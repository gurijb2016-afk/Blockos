#ifdef __cplusplus
extern "C" {
#endif
#pragma once
#include <stdint.h>
#include <sys/wait.h>
#define SI_USER 0
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGSYS 31

typedef uint64_t sigset_t;
typedef void (*sighandler_t)(int);
struct sigaction {
    union { uint64_t handler; void (*sa_handler)(int); void (*sa_sigaction)(int, void*, void*); };
    uint64_t flags; uint64_t restorer; sigset_t mask;
};
#define sa_flags flags
#define sa_mask mask
#define sa_restorer restorer
typedef int sig_atomic_t;
#define SIGHUP 1
#define SIGILL 4
#define SIGTRAP 5
#define SIGBUS 7
#define SIGFPE 8
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGALRM 14
#define SA_NOCLDSTOP 1
#define SA_SIGINFO 4
#define SA_ONSTACK 0x08000000
#define SA_RESTART 0x10000000
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000u

#define SIG_ERR ((sighandler_t)-1)
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#define SIGPIPE 13
#define SIGCHLD 17
#define SIGTERM 15
#define SIGINT 2
#define SIGQUIT 3
#define SIGABRT 6
#define SIGSEGV 11
#define SIGKILL 9
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGSTOP 19
#define SIGCONT 18
#define NSIG 65
#define CLD_EXITED 1
#define CLD_KILLED 2
#define CLD_DUMPED 3
#define CLD_TRAPPED 4
#define CLD_STOPPED 5
#define CLD_CONTINUED 6
#define SIGVTALRM 26

int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int sig);
int sigdelset(sigset_t* set, int sig);
int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
int kill(int pid, int sig);
int tgkill(int tgid, int tid, int sig);

sighandler_t signal(int sig, sighandler_t handler);
int raise(int sig);
int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
int sigismember(const sigset_t *set, int signo);
#ifdef __cplusplus
}
#endif
