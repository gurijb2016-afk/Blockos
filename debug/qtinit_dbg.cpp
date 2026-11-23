// Debug replacement for qt_initialize_pthread_cond: first definition wins at link time.
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
void qt_initialize_pthread_cond(pthread_cond_t *cond, const char *where)
{
    if ((uintptr_t)cond < 4096) {
        printf("BADCOND where=%s c=%p ra=%p\n", where, (void *)cond, __builtin_return_address(0));
        fflush(stdout);
        return;
    }
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_cond_init(cond, &attr);
    pthread_condattr_destroy(&attr);
}
