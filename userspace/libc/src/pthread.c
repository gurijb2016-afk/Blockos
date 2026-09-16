#include "pthread.h"
#include "errno.h"

int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* attr) { (void)attr; if (m) *m = 0; return 0; }
int pthread_mutex_lock(pthread_mutex_t* m)    { (void)m; return 0; }
int pthread_mutex_trylock(pthread_mutex_t* m) { (void)m; return 0; }
int pthread_mutex_unlock(pthread_mutex_t* m)  { (void)m; return 0; }
int pthread_mutex_destroy(pthread_mutex_t* m) { (void)m; return 0; }

int pthread_cond_init(pthread_cond_t* c, const pthread_condattr_t* attr) { (void)attr; if (c) *c = 0; return 0; }
int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m) {
    (void)c; (void)m;
    /* Single-threaded: nothing else will ever signal us, so a real wait
     * would hang forever. Returning immediately is wrong in general but
     * at least doesn't deadlock a single-threaded program that happens to
     * call this. */
    return 0;
}
int pthread_cond_signal(pthread_cond_t* c)    { (void)c; return 0; }
int pthread_cond_broadcast(pthread_cond_t* c) { (void)c; return 0; }
int pthread_cond_destroy(pthread_cond_t* c)   { (void)c; return 0; }

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                    void* (*start_routine)(void*), void* arg) {
    (void)thread; (void)attr; (void)start_routine; (void)arg;
    errno = ENOSYS;
    return ENOSYS; /* pthread_create returns the error code directly, not -1 */
}

int pthread_join(pthread_t thread, void** retval) {
    (void)thread;
    if (retval) *retval = 0;
    return ENOSYS; /* no thread ever actually started - see pthread_create */
}

pthread_t pthread_self(void) { return 1; /* dummy id - there is only ever one thread */ }
