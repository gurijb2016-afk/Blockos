#pragma once
#include <stddef.h>

/*
 * BlockOS has no real thread/clone support wired up in the kernel yet
 * (SYS_clone exists as a syscall NUMBER in kernel/syscall/syscall_numbers.hpp
 * but kernel/user_syscall.cpp's dispatcher has no case for it - it falls
 * through to ENOSYS). These are stubs so that code which merely LINKS
 * against pthread (Xlib does, for its optional thread-safety hooks) can
 * still build and run single-threaded:
 *   - mutex/cond functions are no-ops that always "succeed", which is
 *     correct as long as nothing is actually concurrent.
 *   - pthread_create() always fails - anything that requires a SECOND
 *     thread to actually run will not work until the kernel gets real
 *     thread support and these stubs are replaced with real syscalls.
 */

typedef int pthread_mutex_t;
typedef int pthread_cond_t;
typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;
typedef unsigned long pthread_t;
typedef int pthread_attr_t;

int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* attr);
int pthread_mutex_lock(pthread_mutex_t* m);
int pthread_mutex_trylock(pthread_mutex_t* m);
int pthread_mutex_unlock(pthread_mutex_t* m);
int pthread_mutex_destroy(pthread_mutex_t* m);

int pthread_cond_init(pthread_cond_t* c, const pthread_condattr_t* attr);
int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m);
int pthread_cond_signal(pthread_cond_t* c);
int pthread_cond_broadcast(pthread_cond_t* c);
int pthread_cond_destroy(pthread_cond_t* c);

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                    void* (*start_routine)(void*), void* arg);
int pthread_join(pthread_t thread, void** retval);
pthread_t pthread_self(void);
