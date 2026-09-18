#pragma once
#include <stddef.h>
#include <stdint.h>

typedef unsigned long pthread_t;
typedef struct { uint32_t state; } pthread_mutex_t;
typedef struct { uint32_t seq; } pthread_cond_t;
typedef struct { uint32_t flags; } pthread_mutexattr_t;
typedef struct { uint32_t flags; } pthread_condattr_t;
typedef struct { size_t stacksize; } pthread_attr_t;

#define PTHREAD_CREATE_DETACHED 1u
#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER {0}

int pthread_mutex_init(pthread_mutex_t*,const pthread_mutexattr_t*);
int pthread_mutex_lock(pthread_mutex_t*);
int pthread_mutex_trylock(pthread_mutex_t*);
int pthread_mutex_unlock(pthread_mutex_t*);
int pthread_mutex_destroy(pthread_mutex_t*);
int pthread_cond_init(pthread_cond_t*,const pthread_condattr_t*);
int pthread_cond_wait(pthread_cond_t*,pthread_mutex_t*);
int pthread_cond_signal(pthread_cond_t*);
int pthread_cond_broadcast(pthread_cond_t*);
int pthread_cond_destroy(pthread_cond_t*);
int pthread_create(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
int pthread_join(pthread_t,void**);
pthread_t pthread_self(void);
int pthread_equal(pthread_t,pthread_t);
int pthread_attr_init(pthread_attr_t*);
int pthread_attr_destroy(pthread_attr_t*);
int pthread_attr_setstacksize(pthread_attr_t*,size_t);
int pthread_attr_getstacksize(const pthread_attr_t*,size_t*);
