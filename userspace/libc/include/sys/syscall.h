#ifndef BLOCKOS_SYS_SYSCALL_H
#define BLOCKOS_SYS_SYSCALL_H
#include <blockos_syscall.h>
#define SYS_read __SYS_read
#define SYS_write __SYS_write
#define SYS_futex __SYS_futex
#define SYS_mmap __SYS_mmap
#define SYS_munmap __SYS_munmap
#define SYS_getpid __SYS_getpid
long syscall(long n, ...);
#endif
