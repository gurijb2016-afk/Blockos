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
    __SYS_nanosleep = 47, __SYS_lseek = 63, __SYS_exit = 44, __SYS_exit_group = 45
};
