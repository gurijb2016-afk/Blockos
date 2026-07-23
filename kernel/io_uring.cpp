#include "kernel/allocator.hpp"
#include "kernel/scheduler.hpp"
#include "spinlock.hpp"

namespace Blockos {

// io_uring struktúrák a Ring 3 (User) és Ring 0 (Kernel) közötti megosztott memóriához
struct io_uring_sqe { // Submission Queue Entry (Kérés)
    uint8_t  opcode;    // Pl. 0 = READ, 1 = WRITE, 2 = NET_RECV
    int      fd;        // Fájl vagy socket leíró
    uint64_t addr;      // Felhasználói puffer címe (Ring 3)
    uint32_t len;       // Adat hossza bájtokban
    uint64_t user_data; // Egyedi azonosító a visszakövetéshez
};

struct io_uring_cqe { // Completion Queue Entry (Eredmény)
    uint64_t user_data; // A kérésből visszakapott azonosító
    int32_t  res;       // Eredmény bájtok száma, vagy negatív hibakód
};

class io_uring {
private:
    Spinlock m_lock;
    
    // Osztott memóriás körkörös pufferek (Ring Buffers)
    io_uring_sqe* m_sqes;
    io_uring_cqe* m_cqes;
    
    uint32_t m_queue_depth;
    
    // Atomi indexek a lockless működéshez (User és Kernel egyszerre írhatja/olvashatja)
    volatile uint32_t* m_sq_head;
    volatile uint32_t* m_sq_tail;
    volatile uint32_t* m_cq_head;
    volatile uint32_t* m_cq_tail;

    bool m_running;
    Task* m_io_kthread;

public:
    io_uring(uint32_t depth) : m_queue_depth(depth), m_running(false), m_io_kthread(nullptr) {
        // Nagy, laphoz igazított (Page-aligned) memóriát foglalunk, amit az mmap át tud adni Ring 3-nak
        m_sqes = reinterpret_cast<io_uring_sqe*>(KernelAllocator::alloc(depth * sizeof(io_uring_sqe)));
        m_cqes = reinterpret_cast<io_uring_cqe*>(KernelAllocator::alloc(depth * sizeof(io_uring_cqe)));
        
        // Atomi kontroll indexek allokálása
        m_sq_head = reinterpret_cast<volatile uint32_t*>(KernelAllocator::alloc(sizeof(uint32_t)));
        m_sq_tail = reinterpret_cast<volatile uint32_t*>(KernelAllocator::alloc(sizeof(uint32_t)));
        m_cq_head = reinterpret_cast<volatile uint32_t*>(KernelAllocator::alloc(sizeof(uint32_t)));
        m_cq_tail = reinterpret_cast<volatile uint32_t*>(KernelAllocator::alloc(sizeof(uint32_t)));

        *m_sq_head = 0; *m_sq_tail = 0;
        *m_cq_head = 0; *m_cq_tail = 0;
    }

    // A kernel belső aszinkron I/O szállának magja (Kernel Thread) [source: 1]
    static void io_worker_routine(void* arg) {
        auto* self = reinterpret_cast<io_uring*>(arg);
        
        while (self->m_running) {
            uint32_t sq_head = *self->m_sq_head;
            uint32_t sq_tail = *self->m_sq_tail;

            // Ellenőrizzük, hogy van-e új kérés a Submission Queue-ban (Lockless olvasás)
            if (sq_head != sq_tail) {
                uint32_t index = sq_head % self->m_queue_depth;
                io_uring_sqe sqe = self->m_sqes[index];

                // 1. Kérés kivétele (Atomi léptetés memóriakorláttal)
                __atomic_store_n(self->m_sq_head, sq_head + 1, __ATOMIC_RELEASE);

                // 2. Aszinkron végrehajtás a hardveres drivereken keresztül
                int32_t result = 0;
                if (sqe.opcode == 0) { // ASYNC READ
                    // Itt meghívódik a te virtuális VFS / Ext4 drivered [source: 1]
                    // result = VFS::read(sqe.fd, reinterpret_cast<void*>(sqe.addr), sqe.len);
                }

                // 3. Eredmény beírása a Completion Queue-ba (CQ)
                uint32_t cq_tail = *self->m_cq_tail;
                uint32_t cq_index = cq_tail % self->m_queue_depth;
                
                self->m_cqes[cq_index].user_data = sqe.user_data;
                self->m_cqes[cq_index].res = result;

                // CQ tail frissítése, hogy a felhasználói program (Ring 3) lássa a kész eredményt
                __atomic_store_n(self->m_cq_tail, cq_tail + 1, __ATOMIC_RELEASE);
            } else {
                // Ha nincs dolga az I/O motornak, átadja a futást, nem terheli a CPU-t
                Scheduler::yield();
            }
        }
    }

    void start() {
        m_running = true;
        m_io_kthread = Scheduler::create_kernel_thread("kio_uringd", &io_uring::io_worker_routine, this);
    }
};

} // namespace Blockos
