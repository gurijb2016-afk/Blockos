#include "sys/mman.h"
#include "blockos_syscall.h"
#include "errno.h"

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    long r = __blockos_syscall(__SYS_mmap, (long)addr, (long)length, prot, flags, fd, offset);
    if (r < 0) { errno = (int)-r; return MAP_FAILED; }
    return (void*)(unsigned long)r;
}

int munmap(void* addr, size_t length) {
    /* Not implemented by the kernel yet (SYS_munmap falls through to the
     * default ENOSYS case in kernel/user_syscall.cpp) - calling this will
     * fail with ENOSYS until that's added. Memory simply isn't reclaimed
     * for now; fine for a bootstrap MVP, not fine for anything long-running. */
    long r = __blockos_syscall(__SYS_munmap, (long)addr, (long)length, 0, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}
