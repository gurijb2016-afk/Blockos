#pragma once
#include <stdint.h>
#include "unistd.h"

typedef struct {
    int64_t si_signo;
    int64_t si_errno;
    int64_t si_code;
    pid_t   si_pid;
    int64_t si_uid;
    int64_t si_status;
    void *si_addr;
    int64_t _pad[8];
} siginfo_t;

#define P_PID 1
#define P_PGID 2
#define P_ALL 0

#define WNOHANG    1
#define WUNTRACED  2
#define WEXITED    4
#define WSTOPPED   2
#define WCONTINUED 8
#define WNOWAIT    0x01000000

#define WIFEXITED(s)   (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) (((s) & 0x7f) != 0 && (((s) & 0x7f) != 0x7f))
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFSTOPPED(s)  (((s) & 0xff) == 0x7f)
#define WSTOPSIG(s)    (((s) >> 8) & 0xff)

pid_t wait(int* status);
pid_t waitpid(pid_t pid, int* status, int options);
pid_t wait4(pid_t pid, int* status, int options, void* rusage);
int waitid(int idtype, pid_t id, siginfo_t* infop, int options);
