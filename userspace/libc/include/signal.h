#pragma once
#include <stdint.h>

typedef uint64_t sigset_t;
typedef void (*sighandler_t)(int);
struct sigaction { uint64_t handler; uint64_t flags; uint64_t restorer; sigset_t mask; };

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

int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int sig);
int sigdelset(sigset_t* set, int sig);
int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
int kill(int pid, int sig);
int tgkill(int tgid, int tid, int sig);
