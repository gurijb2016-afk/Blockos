#include <stddef.h>
#include <stdint.h>

// Külső hivatkozások a te már meglévő Blockos kernel funkcióidra
extern "C" void* blockos_mmap_allocate(size_t length);
extern "C" int blockos_scheduler_fork();
extern "C" void blockos_terminal_print(const char* text, size_t length);

// Linux-specifikus rendszerhívás számok x86_64 architektúrán
#define SYS_WRITE      1
#define SYS_MMAP       9
#define SYS_FORK      57
#define SYS_EXIT      60

// Ez a fő belépési pont, amit az x86_64 MSR (Model Specific Register) hív meg, ha a Brave 'syscall'-t futtat
extern "C" int64_t blockos_linux_syscall_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (syscall_num) {
        
        case SYS_WRITE: { // A Brave konzolos üzeneteinek átirányítása a Blockos terminálra
            int fd = (int)arg1;
            const char* buf = (const char*)arg2;
            size_t count = (size_t)arg3;
            
            if (fd == 1 || fd == 2) { // stdout vagy stderr
                blockos_terminal_print(buf, count);
                return count;
            }
            return -1;
        }

        case SYS_MMAP: { // A Brave JavaScript (V8) motorjának memóriafoglalása
            size_t length = (size_t)arg2;
            void* allocated_memory = blockos_mmap_allocate(length);
            if (!allocated_memory) {
                return -1; // Memóriahiba esetén
            }
            return (int64_t)allocated_memory;
        }

        case SYS_FORK: { // A Brave többfolyamatos (Sandbox/Tab) működésének kiszolgálása
            return blockos_scheduler_fork(); 
        }

        case SYS_EXIT: { // Ha a Brave vagy egy füle bezárul
            // Itt hívd meg a te belső szál-megsemmisítő (kill) függvényedet
            return 0;
        }

        default:
            // Minden egyéb nem támogatott Linux hívás elengedése (dummy sikeres visszatérés a fagyás ellen)
            return 0; 
    }
}
