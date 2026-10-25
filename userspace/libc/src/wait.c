#include "sys/wait.h"
#include "blockos_syscall.h"
#include "errno.h"
#include <stddef.h>

static int call_wait4(pid_t pid, int *status, int options) {
    /* kernel ABI: rdi=pid, rdx=status ptr, r10=options */
    long r = __blockos_syscall(__SYS_wait4, pid, 0, (long)status, options, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return (int)r;
}

pid_t wait4(pid_t pid, int *status, int options, void *rusage) {
    (void)rusage;
    return (pid_t)call_wait4(pid, status, options);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    int r;
    for (;;) {
        r = call_wait4(pid, status, options | WNOHANG);
        if (r > 0 || r < 0) return (pid_t)r;
        if (options & WNOHANG) return 0;
        /* The kernel wait path is non-blocking; use the same userspace
         * scheduler-friendly sleep primitive as other BlockOS runtimes. */
        struct timespec ts = {0, 1000000};
        nanosleep(&ts, 0);
    }
}

int waitid(int idtype, pid_t id, siginfo_t *infop, int options) {
    if (!infop || idtype != P_PID && idtype != P_ALL) { errno = 22; return -1; }
    pid_t wanted = (idtype == P_PID) ? id : -1;
    int status = 0;
    pid_t got = waitpid(wanted, &status, (options & WNOHANG) ? WNOHANG : 0);
    if (got < 0) return -1;
    if (got == 0) { return 0; }
    infop->si_signo = 17; /* SIGCHLD */
    infop->si_errno = 0;
    infop->si_code = WIFEXITED(status) ? 1 : 2;
    infop->si_pid = got;
    infop->si_uid = 0;
    infop->si_status = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status);
    return 0;
}
