#pragma once
#include <sys/types.h>
using sighandler_t = void (*)(int);
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
#ifdef __cplusplus
extern "C" {
#endif
sighandler_t signal(int sig, sighandler_t handler);
int kill(pid_t pid, int sig);
unsigned alarm(unsigned seconds);
#ifdef __cplusplus
}
#endif
