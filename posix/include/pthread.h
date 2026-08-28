#pragma once
#include <stdint.h>
typedef uint64_t pthread_t; typedef struct { uint64_t opaque[4]; } pthread_attr_t; typedef struct { uint64_t opaque[4]; } pthread_mutex_t; typedef struct { uint64_t opaque[4]; } pthread_mutexattr_t;
#ifdef __cplusplus
extern "C" {
#endif
int pthread_create(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
int pthread_join(pthread_t,void**); int pthread_detach(pthread_t); pthread_t pthread_self(void);
int pthread_mutex_init(pthread_mutex_t*,const pthread_mutexattr_t*); int pthread_mutex_lock(pthread_mutex_t*); int pthread_mutex_unlock(pthread_mutex_t*); int pthread_mutex_destroy(pthread_mutex_t*);
#ifdef __cplusplus
}
#endif
