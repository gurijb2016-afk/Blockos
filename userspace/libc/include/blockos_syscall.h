#pragma once
/*
 * Internal raw syscall entry point. Every libc wrapper funnels through
 * this - it ALWAYS pins r10/r8/r9 explicitly (not just for mmap), because
 * that's the real x86-64 syscall argument convention the kernel now
 * expects consistently (see kernel/user_syscall.cpp's do_mmap for why this
 * matters: relying on "whatever register the compiler happened to pick"
 * is unsafe once a syscall handler reads more than 3 arguments).
 *
 * Returns the kernel's raw value: 0 or positive on success, a small
 * negative errno-style value on failure (e.g. -22 for EINVAL). Wrappers
 * translate that into the usual libc convention (-1 + errno).
 */
long __blockos_syscall(long n, long a0, long a1, long a2, long a3, long a4, long a5);

/* BlockOS syscall numbers - kept in sync with kernel/syscall/syscall_numbers.hpp */
enum {
    __SYS_read = 0, __SYS_write = 1, __SYS_openat = 2, __SYS_close = 3,
    __SYS_fstat = 4, __SYS_mmap = 5, __SYS_munmap = 6, __SYS_mprotect = 7,
    __SYS_brk = 8, __SYS_getpid = 52, __SYS_clock_gettime = 51,
    __SYS_nanosleep = 47, __SYS_lseek = 63, __SYS_exit = 44, __SYS_exit_group = 45,
    __SYS_sched_yield = 46, __SYS_futex = 41, __SYS_rt_sigaction = 76, __SYS_rt_sigprocmask = 77, __SYS_rt_sigreturn = 78, __SYS_kill = 74, __SYS_tgkill = 75, __SYS_clone = 42, __SYS_wait4 = 43,
    __SYS_getppid = 53, __SYS_gettid = 54, __SYS_set_tid_address = 55,
    __SYS_prctl = 49, __SYS_socket = 25, __SYS_bind = 26, __SYS_listen = 27,
    __SYS_accept = 28, __SYS_accept4 = 29, __SYS_connect = 30, __SYS_sendto = 31,
    __SYS_recvfrom = 32, __SYS_shutdown = 33, __SYS_setsockopt = 34, __SYS_getsockopt = 35,
    __SYS_epoll_create1 = 36, __SYS_epoll_ctl = 37, __SYS_epoll_wait = 38,
    __SYS_poll = 39, __SYS_fcntl = 65, __SYS_dup = 66, __SYS_dup2 = 67,
    __SYS_arch_prctl = 83, __SYS_socketpair = 84, __SYS_sendmsg = 85, __SYS_recvmsg = 86,
    __SYS_rseq = 87, __SYS_stat = 23, __SYS_lstat = 24,
    __SYS_access = 21, __SYS_getcwd = 20, __SYS_chdir = 19, __SYS_execve = 48,
    __SYS_getdents64 = 62, __SYS_pipe2 = 68, __SYS_mkdir = 71, __SYS_unlink = 70,
    __SYS_rename = 69, __SYS_getrandom = 50, __SYS_readv = 59, __SYS_writev = 60,
    __SYS_getuid = 13, __SYS_getgid = 14, __SYS_geteuid = 15, __SYS_getegid = 16
};
