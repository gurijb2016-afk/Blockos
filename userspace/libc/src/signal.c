#include "signal.h"
#include "blockos_syscall.h"
#include "errno.h"

int sigemptyset(sigset_t* s){ if(!s){errno=EFAULT;return -1;} *s=0; return 0; }
int sigfillset(sigset_t* s){ if(!s){errno=EFAULT;return -1;} *s=~(sigset_t)0; return 0; }
int sigaddset(sigset_t* s,int sig){ if(!s||sig<1||sig>64){errno=EINVAL;return -1;} *s|=1ULL<<(sig-1);return 0; }
int sigdelset(sigset_t* s,int sig){ if(!s||sig<1||sig>64){errno=EINVAL;return -1;} *s&=~(1ULL<<(sig-1));return 0; }
int sigaction(int sig,const struct sigaction* a,struct sigaction* o){long r=__blockos_syscall(__SYS_rt_sigaction,sig,(long)a,(long)o,8,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
int sigprocmask(int how,const sigset_t* s,sigset_t* o){long r=__blockos_syscall(__SYS_rt_sigprocmask,how,(long)s,(long)o,8,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
int kill(int pid,int sig){long r=__blockos_syscall(__SYS_kill,pid,sig,0,0,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
int tgkill(int tgid,int tid,int sig){long r=__blockos_syscall(__SYS_tgkill,tgid,tid,sig,0,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}


sighandler_t signal(int sig, sighandler_t handler) {
    struct sigaction a, o;
    a.handler = (uint64_t)handler;
    a.flags = 0;
    a.restorer = 0;
    a.mask = 0;
    if (sigaction(sig, &a, &o) < 0) return SIG_ERR;
    return (sighandler_t)(uintptr_t)o.handler;
}

int raise(int sig) {
    long tid = __blockos_syscall(__SYS_gettid, 0, 0, 0, 0, 0, 0);
    long pid = __blockos_syscall(__SYS_getpid, 0, 0, 0, 0, 0, 0);
    long r = __blockos_syscall(__SYS_tgkill, pid, tid, sig, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
    return sigprocmask(how, set, oldset);
}
