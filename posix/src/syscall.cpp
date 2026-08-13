#include "../include/internal/syscall.h"
#include "../include/errno.h"
namespace blockos::abi {
#if defined(__x86_64__)
static inline int64_t raw(Number n, uint64_t a0, uint64_t a1, uint64_t a2,
                          uint64_t a3, uint64_t a4, uint64_t a5) {
    int64_t r;
    register uint64_t r10 asm("r10") = a3;
    register uint64_t r8  asm("r8")  = a4;
    register uint64_t r9  asm("r9")  = a5;
    asm volatile("syscall" : "=a"(r) : "a"((uint64_t)n), "D"(a0), "S"(a1),
                 "d"(a2), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return r;
}
#else
static inline int64_t raw(Number, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { return -ENOSYS; }
#endif
int64_t Dispatcher::call(Number n, uint64_t a0, uint64_t a1, uint64_t a2,
                          uint64_t a3, uint64_t a4, uint64_t a5) {
    const int64_t r = raw(n,a0,a1,a2,a3,a4,a5);
    if (r < 0) errno = static_cast<int>(-r);
    return r < 0 ? -1 : r;
}
}
