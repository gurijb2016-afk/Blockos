#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include "blockos_syscall.h"

/* ABI-aware scaffolding: BlockOS exposes clone(42) and futex(41), but the
   exact thread descriptor/scheduling contract must be defined by the kernel.
   Returning ENOSYS is safer than silently inventing incompatible semantics. */
int pthread_create(pthread_t *t,const pthread_attr_t *a,void *(*fn)(void*),void *arg){
    (void)t;(void)a;(void)fn;(void)arg; errno=ENOSYS; return ENOSYS;
}
int pthread_join(pthread_t t,void **r){(void)t;(void)r;errno=ENOSYS;return ENOSYS;}
int pthread_detach(pthread_t t){(void)t;errno=ENOSYS;return ENOSYS;}
pthread_t pthread_self(void){return 0;}
int pthread_mutex_init(pthread_mutex_t*m,const pthread_mutexattr_t*a){(void)m;(void)a;errno=ENOSYS;return ENOSYS;}
int pthread_mutex_lock(pthread_mutex_t*m){(void)m;errno=ENOSYS;return ENOSYS;}
int pthread_mutex_unlock(pthread_mutex_t*m){(void)m;errno=ENOSYS;return ENOSYS;}
int pthread_mutex_destroy(pthread_mutex_t*m){(void)m;errno=ENOSYS;return ENOSYS;}
