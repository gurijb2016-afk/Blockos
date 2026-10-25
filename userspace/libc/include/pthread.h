#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

typedef unsigned long pthread_t;
typedef unsigned long pthread_key_t;
typedef struct { volatile uint32_t state; uint32_t owner; uint32_t count; uint32_t type; } pthread_mutex_t;
typedef struct { volatile uint32_t seq; } pthread_cond_t;
typedef struct { uint32_t flags; } pthread_mutexattr_t;
typedef struct { uint32_t flags; } pthread_condattr_t;
typedef struct { size_t stacksize; int detachstate; } pthread_attr_t;
typedef struct { volatile uint32_t state; } pthread_rwlock_t;
typedef struct { int dummy; } pthread_rwlockattr_t;
typedef struct { volatile uint32_t state; } pthread_once_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1
#define PTHREAD_ONCE_INIT {0}
#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER {0}
#define PTHREAD_RWLOCK_INITIALIZER {0}

int pthread_mutex_init(pthread_mutex_t*,const pthread_mutexattr_t*);
int pthread_mutex_lock(pthread_mutex_t*);
int pthread_mutex_trylock(pthread_mutex_t*);
int pthread_mutex_unlock(pthread_mutex_t*);
int pthread_mutex_destroy(pthread_mutex_t*);
int pthread_mutexattr_init(pthread_mutexattr_t*);
int pthread_mutexattr_destroy(pthread_mutexattr_t*);
int pthread_mutexattr_settype(pthread_mutexattr_t*,int);
int pthread_mutexattr_gettype(const pthread_mutexattr_t*,int*);
int pthread_cond_init(pthread_cond_t*,const pthread_condattr_t*);
int pthread_cond_wait(pthread_cond_t*,pthread_mutex_t*);
int pthread_cond_timedwait(pthread_cond_t*,pthread_mutex_t*,const struct timespec*);
int pthread_cond_signal(pthread_cond_t*);
int pthread_cond_broadcast(pthread_cond_t*);
int pthread_cond_destroy(pthread_cond_t*);
int pthread_condattr_init(pthread_condattr_t*);
int pthread_condattr_destroy(pthread_condattr_t*);
int pthread_create(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
int pthread_join(pthread_t,void**);
int pthread_detach(pthread_t);
pthread_t pthread_self(void);
int pthread_equal(pthread_t,pthread_t);
int pthread_attr_init(pthread_attr_t*);
int pthread_attr_destroy(pthread_attr_t*);
int pthread_attr_setstacksize(pthread_attr_t*,size_t);
int pthread_attr_getstacksize(const pthread_attr_t*,size_t*);
int pthread_attr_setdetachstate(pthread_attr_t*,int);
int pthread_attr_getdetachstate(const pthread_attr_t*,int*);
int pthread_rwlock_init(pthread_rwlock_t*,const pthread_rwlockattr_t*);
int pthread_rwlock_destroy(pthread_rwlock_t*);
int pthread_rwlock_rdlock(pthread_rwlock_t*);
int pthread_rwlock_wrlock(pthread_rwlock_t*);
int pthread_rwlock_tryrdlock(pthread_rwlock_t*);
int pthread_rwlock_trywrlock(pthread_rwlock_t*);
int pthread_rwlock_unlock(pthread_rwlock_t*);
int pthread_once(pthread_once_t*,void(*)(void));
int pthread_key_create(pthread_key_t*,void(*)(void*));
int pthread_key_delete(pthread_key_t);
void* pthread_getspecific(pthread_key_t);
int pthread_setspecific(pthread_key_t,const void*);
int pthread_setname_np(pthread_t,const char*);
int pthread_getname_np(pthread_t,char*,size_t);

#define PTHREAD_SCOPE_SYSTEM 0
#define PTHREAD_SCOPE_PROCESS 1
int pthread_attr_setscope(pthread_attr_t *a, int scope);
