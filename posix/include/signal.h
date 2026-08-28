#pragma once
#include <sys/types.h>
typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGABRT 6
#define SIGKILL 9
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
struct sigaction { sighandler_t sa_handler; unsigned long sa_flags; void (*sa_restorer)(void); unsigned long sa_mask[2]; };
#ifdef __cplusplus
extern "C" { sighandler_t signal(int,sighandler_t); int kill(pid_t,int); int tgkill(pid_t,pid_t,int); int sigaction(int,const struct sigaction*,struct sigaction*); int sigprocmask(int,const void*,void*); }
#endif
