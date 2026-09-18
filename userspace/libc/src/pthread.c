#include "pthread.h"
#include "blockos_syscall.h"
#include "errno.h"
#include "sys/mman.h"
#include "blockos_tls.h"
#include "unistd.h"
#include <stdint.h>

extern long __blockos_clone_and_start(void* start, void* arg, void* stack_top, void* tls, void* record);

struct ThreadStart {
    void* (*start)(void*);
    void* arg;
    volatile uint32_t done;
    uint32_t detached;
    void* retval;
    pthread_t tid;
    void* stack;
    size_t stack_size;
    void* tls;
};

#define MAX_THREADS 256
static struct ThreadStart* records[MAX_THREADS];
static volatile uint32_t next_slot;

static struct ThreadStart* find_record(pthread_t tid) {
    for (size_t i=0;i<MAX_THREADS;i++) if(records[i] && records[i]->tid==tid) return records[i];
    return 0;
}

static int futex_wait(volatile uint32_t* p,uint32_t v){
    long r=__blockos_syscall(__SYS_futex,(long)p,0,v,0,0,0);
    if(r<0 && r!=-11) return (int)-r;
    return 0;
}
static int futex_wake(volatile uint32_t* p,int n){
    long r=__blockos_syscall(__SYS_futex,(long)p,1,(uint32_t)n,0,0,0);
    return r<0?(int)-r:0;
}

int pthread_mutex_init(pthread_mutex_t*m,const pthread_mutexattr_t*a){(void)a;if(m)m->state=0;return 0;}
int pthread_mutex_lock(pthread_mutex_t*m){if(!m)return 22;for(;;){if(__sync_bool_compare_and_swap(&m->state,0,1))return 0;futex_wait(&m->state,1);}}
int pthread_mutex_trylock(pthread_mutex_t*m){if(!m)return 22;return __sync_bool_compare_and_swap(&m->state,0,1)?0:16;}
int pthread_mutex_unlock(pthread_mutex_t*m){if(!m)return 22;__sync_lock_release(&m->state);futex_wake(&m->state,1);return 0;}
int pthread_mutex_destroy(pthread_mutex_t*m){(void)m;return 0;}
int pthread_cond_init(pthread_cond_t*c,const pthread_condattr_t*a){(void)a;if(c)c->seq=0;return 0;}
int pthread_cond_wait(pthread_cond_t*c,pthread_mutex_t*m){if(!c||!m)return 22;uint32_t seq=c->seq;pthread_mutex_unlock(m);futex_wait(&c->seq,seq);pthread_mutex_lock(m);return 0;}
int pthread_cond_signal(pthread_cond_t*c){if(!c)return 22;__sync_add_and_fetch(&c->seq,1);futex_wake(&c->seq,1);return 0;}
int pthread_cond_broadcast(pthread_cond_t*c){if(!c)return 22;__sync_add_and_fetch(&c->seq,1);futex_wake(&c->seq,0x7fffffff);return 0;}
int pthread_cond_destroy(pthread_cond_t*c){(void)c;return 0;}
int pthread_attr_init(pthread_attr_t*a){if(!a)return 22;a->stacksize=1024*1024;return 0;}
int pthread_attr_destroy(pthread_attr_t*a){(void)a;return 0;}
int pthread_attr_setstacksize(pthread_attr_t*a,size_t n){if(!a||n<16384)return 22;a->stacksize=(n+4095)&~(size_t)4095;return 0;}
int pthread_attr_getstacksize(const pthread_attr_t*a,size_t*n){if(!a||!n)return 22;*n=a->stacksize;return 0;}

int pthread_create(pthread_t*out,const pthread_attr_t*attr,void*(*start)(void*),void*arg){
    if(!out||!start)return 22;
    size_t stack_size=(attr&&attr->stacksize)?attr->stacksize:1024*1024;
    void* stack=mmap(0,stack_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(stack==MAP_FAILED)return 12;
    void* tls= (void*)__blockos_tls_clone_current();
    if(!tls)return 12;
    struct ThreadStart* rec=(struct ThreadStart*)mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(rec==MAP_FAILED)return 12;
    rec->start=start;rec->arg=arg;rec->done=0;rec->detached=0;rec->retval=0;rec->tid=0;rec->stack=stack;rec->stack_size=stack_size;rec->tls=tls;
    size_t slot=__sync_fetch_and_add(&next_slot,1)%MAX_THREADS;
    while(records[slot]) slot=(slot+1)%MAX_THREADS;
    records[slot]=rec;
    void* top=(uint8_t*)stack+stack_size;
    long tid=__blockos_clone_and_start(start,arg,top,tls,rec);
    if(tid<0){records[slot]=0;return (int)-tid;}
    rec->tid=(pthread_t)tid;
    *out=(pthread_t)tid;
    return 0;
}

int pthread_join(pthread_t tid,void**retval){
    struct ThreadStart* rec=find_record(tid);if(!rec)return 3;
    while(!rec->done){__blockos_syscall(__SYS_sched_yield,0,0,0,0,0,0);}
    if(retval)*retval=rec->retval;
    return 0;
}
pthread_t pthread_self(void){long r=__blockos_syscall(__SYS_gettid,0,0,0,0,0,0);return r<0?0:(pthread_t)r;}
int pthread_equal(pthread_t a,pthread_t b){return a==b;}
