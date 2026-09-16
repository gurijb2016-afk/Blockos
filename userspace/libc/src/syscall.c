#include "blockos_syscall.h"

long __blockos_syscall(long n, long a0, long a1, long a2, long a3, long a4, long a5) {
    register long r10 __asm__("r10") = a3;
    register long r8  __asm__("r8")  = a4;
    register long r9  __asm__("r9")  = a5;
    long r;
    __asm__ volatile("int $0x80"
        : "=a"(r)
        : "a"(n), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory", "cc");
    return r;
}
