#include "spinlock.hpp"

namespace Blockos {

void Spinlock::lock() {
    unsigned long rflags;

    // 1. LÉPÉS: Megszakítások (Interruptok) letiltása és a jelenlegi állapot elmentése.
    // Beolvassuk az RFLAGS regisztert, majd kiadjuk a CLI (Clear Interrupt) utasítást.
    asm volatile(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(rflags)
        :
        : "memory"
    );

    // 2. LÉPÉS: Atomi pörgő-zár (Spinlock) tesztelés.
    // Az x86 'xchg' utasítás atomi módon kicseréli a regiszter tartalmát a memóriával,
    // és automatikusan busz-szintű zárat (LOCK prefix hardveresen) alkalmaz.
    int acquired = 1;
    while (acquired == 1) {
        asm volatile(
            "xchgl %0, %1"
            : "+r"(acquired), "+m"(m_lock_state)
            :
            : "memory"
        );

        if (acquired == 1) {
            // Ha nem sikerült megszerezni (még foglalt volt), PAUSE utasítással 
            // jelezzük a CPU-nak, hogy egy spin-loopban vagyunk. Ez drasztikusan 
            // csökkenti a processzor áramfelvételét és melegedését pörgés közben.
            asm volatile("pause");
        }
    }

    // Csak a sikeres zár megszerzése UTÁN mentjük el az RFLAGS állapotot.
    m_rflags_state = rflags;
}

bool Spinlock::try_lock() {
    unsigned long rflags;

    // Megszakítások ideiglenes letiltása a teszt idejére
    asm volatile("pushfq; popq %0; cli" : "=r"(rflags) :: "memory");

    int acquired = 1;
    asm volatile(
        "xchgl %0, %1"
        : "+r"(acquired), "+m"(m_lock_state)
        :
        : "memory"
    );

    if (acquired == 0) {
        // Sikerült megszerezni! Elmentjük az állapotot.
        m_rflags_state = rflags;
        return true;
    }

    // Nem sikerült megszerezni, azonnal visszaállítjuk az interruptokat és kilépünk
    if (rflags & (1 << 9)) { // Ha az IF (Interrupt Flag) be volt kapcsolva (9. bit)
        asm volatile("sti");
    }
    return false;
}

void Spinlock::unlock() {
    unsigned long rflags = m_rflags_state;

    // 1. LÉPÉS: A zár állapotának atomi visszaállítása 0-ra.
    // A memóriakorlát (memory barrier / "memory") garantálja, hogy a fordító 
    // egyetlen korábbi írási műveletet sem pakolhat át az unlock UTÁNI szakaszra.
    asm volatile(
        "movl $0, %0"
        : "=m"(m_lock_state)
        :
        : "memory"
    );

    // 2. LÉPÉS: A megszakítások eredeti állapotának visszaállítása.
    // Megnézzük, hogy a zár megszerzése előtt engedélyezve voltak-e a megszakítások.
    // Ha igen (az RFLAGS 9. bitje 1-es volt), akkor újra engedélyezzük őket (STI).
    if (rflags & (1 << 9)) {
        asm volatile("sti");
    }
}

} // namespace Blockos
