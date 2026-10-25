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

int pthread_mutex_init(pthread_mutex_t*m,const pthread_mutexattr_t*a){if(!m)return 22;m->state=0;m->owner=0;m->count=0;m->type=a?a->flags:0;return 0;}
/* Recursive mutexes: the owning thread re-enters and only the outermost unlock releases. */
static uint32_t mutex_self(void){return (uint32_t)pthread_self();}
int pthread_mutex_lock(pthread_mutex_t*m){
    if(!m)return 22;
    uint32_t me=mutex_self();
    if(m->type==PTHREAD_MUTEX_RECURSIVE&&m->count&&m->owner==me){m->count++;return 0;}
    for(;;){
        if(__sync_bool_compare_and_swap(&m->state,0,1)){m->owner=me;m->count=1;return 0;}
        futex_wait(&m->state,1);
    }
}
int pthread_mutex_trylock(pthread_mutex_t*m){
    if(!m)return 22;
    uint32_t me=mutex_self();
    if(m->type==PTHREAD_MUTEX_RECURSIVE&&m->count&&m->owner==me){m->count++;return 0;}
    if(__sync_bool_compare_and_swap(&m->state,0,1)){m->owner=me;m->count=1;return 0;}
    return 16;
}
int pthread_mutex_unlock(pthread_mutex_t*m){
    if(!m)return 22;
    if(m->count>1){m->count--;return 0;}
    m->owner=0;m->count=0;
    __sync_lock_release(&m->state);futex_wake(&m->state,1);return 0;
}
int pthread_mutex_destroy(pthread_mutex_t*m){(void)m;return 0;}
int pthread_cond_init(pthread_cond_t*c,const pthread_condattr_t*a){(void)a;if(c)c->seq=0;return 0;}
int pthread_cond_wait(pthread_cond_t*c,pthread_mutex_t*m){
    if(!c||!m)return 22;
    uint32_t seq=c->seq;
    uint32_t saved=m->count?m->count:1;
    m->owner=0;m->count=0;
    __sync_lock_release(&m->state);futex_wake(&m->state,1);
    futex_wait(&c->seq,seq);
    pthread_mutex_lock(m);
    m->count=saved;
    return 0;
}
int pthread_cond_signal(pthread_cond_t*c){if(!c)return 22;__sync_add_and_fetch(&c->seq,1);futex_wake(&c->seq,1);return 0;}
int pthread_cond_broadcast(pthread_cond_t*c){if(!c)return 22;__sync_add_and_fetch(&c->seq,1);futex_wake(&c->seq,0x7fffffff);return 0;}
int pthread_cond_destroy(pthread_cond_t*c){(void)c;return 0;}
int pthread_attr_init(pthread_attr_t*a){if(!a)return 22;a->stacksize=1024*1024;a->detachstate=PTHREAD_CREATE_JOINABLE;return 0;}
int pthread_attr_destroy(pthread_attr_t*a){(void)a;return 0;}
int pthread_attr_setstacksize(pthread_attr_t*a,size_t n){if(!a||n<16384)return 22;a->stacksize=(n+4095)&~(size_t)4095;return 0;}
int pthread_attr_getstacksize(const pthread_attr_t*a,size_t*n){if(!a||!n)return 22;*n=a->stacksize;return 0;}
int pthread_attr_setdetachstate(pthread_attr_t*a,int s){if(!a||(s!=PTHREAD_CREATE_JOINABLE&&s!=PTHREAD_CREATE_DETACHED))return 22;a->detachstate=s;return 0;}
int pthread_attr_getdetachstate(const pthread_attr_t*a,int*s){if(!a||!s)return 22;*s=a->detachstate;return 0;}

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


int pthread_mutexattr_init(pthread_mutexattr_t*a){if(!a)return 22;a->flags=PTHREAD_MUTEX_NORMAL;return 0;}
int pthread_mutexattr_destroy(pthread_mutexattr_t*a){(void)a;return 0;}
int pthread_mutexattr_settype(pthread_mutexattr_t*a,int t){if(!a||(t<0||t>2))return 22;a->flags=(uint32_t)t;return 0;}
int pthread_mutexattr_gettype(const pthread_mutexattr_t*a,int*t){if(!a||!t)return 22;*t=(int)a->flags;return 0;}
int pthread_condattr_init(pthread_condattr_t*a){if(!a)return 22;a->flags=PTHREAD_PROCESS_PRIVATE;return 0;}
int pthread_condattr_destroy(pthread_condattr_t*a){(void)a;return 0;}

int pthread_cond_timedwait(pthread_cond_t*c,pthread_mutex_t*m,const struct timespec*ab){
    if(!c||!m||!ab)return 22;
    while(1){uint32_t seq=c->seq;pthread_mutex_unlock(m);struct timespec now;clock_gettime(CLOCK_REALTIME,&now);if(now.tv_sec>ab->tv_sec||(now.tv_sec==ab->tv_sec&&now.tv_nsec>=ab->tv_nsec)){pthread_mutex_lock(m);return 110;}long long ds=(long long)ab->tv_sec-now.tv_sec;long long dn=(long long)ab->tv_nsec-now.tv_nsec;if(dn<0){--ds;dn+=1000000000;}struct timespec sl={(int64_t)ds,(long)dn};futex_wait(&c->seq,seq);if(c->seq!=seq){pthread_mutex_lock(m);return 0;}nanosleep(&sl,0);pthread_mutex_lock(m);return 110;}
}

typedef struct { volatile uint32_t readers; volatile uint32_t writer; } rw_internal;
static rw_internal *rw(pthread_rwlock_t*l){return (rw_internal*)l;}
int pthread_rwlock_init(pthread_rwlock_t*l,const pthread_rwlockattr_t*a){(void)a;if(!l)return 22;rw(l)->readers=0;rw(l)->writer=0;return 0;}
int pthread_rwlock_destroy(pthread_rwlock_t*l){(void)l;return 0;}
int pthread_rwlock_rdlock(pthread_rwlock_t*l){if(!l)return 22;for(;;){while(rw(l)->writer) sched_yield();if(__sync_add_and_fetch(&rw(l)->readers,1),rw(l)->writer){__sync_sub_and_fetch(&rw(l)->readers,1);continue;}return 0;}}
int pthread_rwlock_wrlock(pthread_rwlock_t*l){if(!l)return 22;for(;;){if(__sync_bool_compare_and_swap(&rw(l)->writer,0,1)){while(rw(l)->readers)sched_yield();return 0;}sched_yield();}}
int pthread_rwlock_tryrdlock(pthread_rwlock_t*l){if(!l)return 22;if(rw(l)->writer)return 16;__sync_add_and_fetch(&rw(l)->readers,1);if(rw(l)->writer){__sync_sub_and_fetch(&rw(l)->readers,1);return 16;}return 0;}
int pthread_rwlock_trywrlock(pthread_rwlock_t*l){if(!l)return 22;if(!__sync_bool_compare_and_swap(&rw(l)->writer,0,1))return 16;if(rw(l)->readers){rw(l)->writer=0;return 16;}return 0;}
int pthread_rwlock_unlock(pthread_rwlock_t*l){if(!l)return 22;if(rw(l)->writer){rw(l)->writer=0;return 0;}if(rw(l)->readers){__sync_sub_and_fetch(&rw(l)->readers,1);return 0;}return 22;}

int pthread_once(pthread_once_t*o,void(*fn)(void)){if(!o||!fn)return 22;uint32_t old=__sync_val_compare_and_swap(&o->state,0,1);if(old==0){fn();__sync_synchronize();o->state=2;}else while(o->state!=2)sched_yield();return 0;}
#define KEY_MAX 128
static _Thread_local void* key_values[KEY_MAX];
static volatile uint32_t key_used[KEY_MAX];
static void(*key_destruct[KEY_MAX])(void*);
int pthread_key_create(pthread_key_t*k,void(*d)(void*)){if(!k)return 22;for(unsigned i=0;i<KEY_MAX;i++)if(__sync_bool_compare_and_swap(&key_used[i],0,1)){key_destruct[i]=d;key_values[i]=0;*k=i;return 0;}return 11;}
int pthread_key_delete(pthread_key_t k){if(k>=KEY_MAX||!key_used[k])return 22;key_values[k]=0;key_destruct[k]=0;key_used[k]=0;return 0;}
void*pthread_getspecific(pthread_key_t k){return (k<KEY_MAX&&key_used[k])?key_values[k]:0;}
int pthread_setspecific(pthread_key_t k,const void*v){if(k>=KEY_MAX||!key_used[k])return 22;key_values[k]=(void*)v;return 0;}
int pthread_detach(pthread_t tid){struct ThreadStart*rec=find_record(tid);if(!rec)return 3;rec->detached=1;return 0;}
int pthread_setname_np(pthread_t t,const char*n){(void)t;(void)n;return 0;}
int pthread_getname_np(pthread_t t,char*n,size_t sz){(void)t;if(!n||!sz)return 22;const char*s="blockos-thread";size_t i=0;for(;i+1<sz&&s[i];i++)n[i]=s[i];n[i]=0;return 0;}
