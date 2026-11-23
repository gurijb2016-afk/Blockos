// --wrap=_Znwm : log NULL allocations and keep running with a fallback arena.
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
extern "C" void *__real__Znwm(size_t);
static unsigned char fallback[1 << 20];
static size_t fallback_used = 0;
static unsigned long nulls = 0;
extern "C" void *__wrap__Znwm(size_t n) {
    void *p = __real__Znwm(n);
    if (p) return p;
    nulls++;
    printf("NEWNULL n=%zu ra=%p count=%lu\n", n, __builtin_return_address(0), nulls);
    fflush(stdout);
    size_t need = (n + 15) & ~(size_t)15;
    if (fallback_used + need > sizeof(fallback)) return nullptr;
    void *r = fallback + fallback_used;
    fallback_used += need;
    return r;
}
