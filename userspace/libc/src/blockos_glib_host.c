#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>

/*
 * Small host functions used by the BlockOS port layer. Keep ABI-stable and
 * avoid Linux-only syscalls. These are intentionally ordinary C entry points
 * so the existing BlockOS libc can provide the real system calls.
 */
int blockos_clock_gettime_monotonic(struct timespec *ts) {
    if (!ts) { errno = EINVAL; return -1; }
    return clock_gettime(CLOCK_MONOTONIC, ts);
}

uint64_t blockos_monotonic_ns(void) {
    struct timespec ts;
    if (blockos_clock_gettime_monotonic(&ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
