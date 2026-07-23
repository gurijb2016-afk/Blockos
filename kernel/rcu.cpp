#include "kernel/scheduler.hpp"
#include "spinlock.hpp"

namespace Blockos {

// RCU-val védett objektumok alapstruktúrája (Láncolt lista a törlendő elemeknek)
struct rcu_head {
    rcu_head* next;
    void (*callback)(void* obj); // Felszabadító függvény pointer
    void* obj_ptr;
};

class RCU {
private:
    Spinlock m_lock;
    
    // Globális RCU korszak-számláló (Grace Period követéshez)
    volatile uint64_t m_global_generation;
    
    // CPU magok egyéni generációs állapotai (Hobby OS szinten max 64 magra méretezve)
    volatile uint64_t m_cpu_generations[64];
    size_t m_registered_cpus;

    // A törlésre váró (elavult) memóriablokkok listája
    rcu_head* m_pending_deletions;

public:
    RCU() : m_global_generation(0), m_registered_cpus(0), m_pending_deletions(nullptr) {
        for (int i = 0; i < 64; ++i) m_cpu_generations[i] = 0;
    }

    // --- OLVASÓ OLDAL (Ring 0 / Ring 3 VFS olvasáshoz) ---
    // Elképesztően gyors: zéró Spinlock, csak egy sima CPU-szintű regisztráció! [source: 1.3.3, 1.3.7]
    void read_lock() {
        size_t cpu_id = Scheduler::get_current_cpu_id();
        
        // Beolvassuk a jelenlegi globális generációt memóriakorláttal (Acquire)
        uint64_t gen = __atomic_load_n(&m_global_generation, __ATOMIC_ACQUIRE);
        
        // Jelezzük a többi magnak, hogy ezen a generáción állunk (Bementünk az RCU kritikus szakaszba)
        __atomic_store_n(&m_cpu_generations[cpu_id], gen, __ATOMIC_RELEASE);
        
        // Megakadályozzuk, hogy a pre-emptív ütemező elváltson minket RCU olvasás közben
        Scheduler::disable_preemption();
    }

    void read_unlock() {
        size_t cpu_id = Scheduler::get_current_cpu_id();
        
        // Jelezzük, hogy kiléptünk az RCU szakaszból (0 = inaktív olvasó)
        __atomic_store_n(&m_cpu_generations[cpu_id], 0, __ATOMIC_RELEASE);
        
        Scheduler::enable_preemption();
    }

    // --- ÍRÓ OLDAL (Fájl/Adat módosítás után) ---
    // Az író nem törölheti azonnal a régi struktúrát, mert az olvasók még láthatják. 
    // Beregisztrálja a törlendő memóriát egy callback-kel. [source: 1.3.6, 1.3.7]
    void call_rcu(rcu_head* head, void* obj, void (*cleanup_func)(void*)) {
        head->obj_ptr = obj;
        head->callback = cleanup_func;

        ScopedLock guard(m_lock);
        // Beillesztjük a törlésre váró láncolt lista elejére
        head->next = m_pending_deletions;
        m_pending_deletions = head;
    }

    // Grace Period (Türelmi idő) kényszerítése. 
    // Addig blokkolja az író szálat, amíg az ÖSSZES CPU mag le nem zárta a régi olvasásait. [source: 1.3.6]
    void synchronize_rcu() {
        // 1. Globális generáció léptetése (Új korszak kezdődik)
        uint64_t old_gen = __atomic_fetch_add(&m_global_generation, 1, __ATOMIC_SEQ_CST);

        // 2. Várjuk meg, amíg az összes CPU mag átlép az új korszakba, vagy inaktívvá válik
        for (size_t i = 0; i < m_registered_cpus; ++i) {
            while (true) {
                uint64_t cpu_gen = __atomic_load_n(&m_cpu_generations[i], __ATOMIC_ACQUIRE);
                
                // Ha a CPU mag épp nem olvas (0), vagy már az új generáción jár (> old_gen), 
                // akkor rajta biztonságosan túlhaladtunk.
                if (cpu_gen == 0 || cpu_gen > old_gen) {
                    break;
                }
                
                // Ha még a régi adaton pörög, a CPU pihen egy ciklust (PAUSE assembly)
                asm volatile("pause");
            }
        }

        // 3. FELSZABADÍTÁS FÁZIS: Mivel a türelmi idő lejárt, senki nem láthatja a régi struktúrákat.
        // Biztonságosan végrehajtjuk az összes felgyülemlett törlési callbacket.
        rcu_head* to_delete = nullptr;
        {
            ScopedLock guard(m_lock);
            to_delete = m_pending_deletions;
            m_pending_deletions = nullptr;
        }

        while (to_delete) {
            rcu_head* next = to_delete->next;
            to_delete->callback(to_delete->obj_ptr); // Tényleges KernelAllocator::free() fut le [source: 1]
            to_delete = next;
        }
    }
};

} // namespace Blockos
