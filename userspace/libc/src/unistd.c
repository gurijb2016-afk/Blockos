#include "unistd.h"
#include "blockos_syscall.h"
#include "errno.h"

static long ret_or_errno(long r) {
    if (r < 0) { errno = (int)-r; return -1; }
    return r;
}

ssize_t read(int fd, void* buf, size_t count) {
    return (ssize_t)ret_or_errno(__blockos_syscall(__SYS_read, fd, (long)buf, (long)count, 0, 0, 0));
}

ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)ret_or_errno(__blockos_syscall(__SYS_write, fd, (long)buf, (long)count, 0, 0, 0));
}

int close(int fd) {
    return (int)ret_or_errno(__blockos_syscall(__SYS_close, fd, 0, 0, 0, 0, 0));
}

off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)ret_or_errno(__blockos_syscall(__SYS_lseek, fd, (long)offset, whence, 0, 0, 0));
}

pid_t getpid(void) {
    long r = __blockos_syscall(__SYS_getpid, 0, 0, 0, 0, 0, 0);
    return (pid_t)r; /* getpid has no real failure mode worth surfacing here */
}

/* sbrk on top of the kernel's brk(): brk(0) queries the current break,
 * brk(new) tries to set it. Matches classic sbrk() semantics closely
 * enough for malloc's purposes (see stdlib.c). */
void* sbrk(long increment) {
    long cur = __blockos_syscall(__SYS_brk, 0, 0, 0, 0, 0, 0);
    if (cur < 0) { errno = (int)-cur; return (void*)-1; }
    if (increment == 0) return (void*)(unsigned long)cur;
    long want = cur + increment;
    long got = __blockos_syscall(__SYS_brk, want, 0, 0, 0, 0, 0);
    if (got < 0 || got != want) { errno = ENOMEM; return (void*)-1; }
    return (void*)(unsigned long)cur; /* sbrk returns the OLD break on success */
}

void _exit(int code) {
    __blockos_syscall(__SYS_exit_group, code, 0, 0, 0, 0, 0);
    for (;;) __asm__ volatile("hlt"); /* not reached */
}
