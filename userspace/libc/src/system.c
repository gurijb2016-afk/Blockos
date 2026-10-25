/* system(): fork, exec "/bin/sh -c <cmd>", wait. Returns wait status (POSIX-style). */
#include "stdlib.h"
#include "unistd.h"
#include "blockos_syscall.h"
#include "errno.h"

int system(const char *cmd) {
    if (!cmd) return 1; /* shell available */
    long pid = __blockos_syscall(__SYS_fork, 0, 0, 0, 0, 0, 0);
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv2[4];
        argv2[0] = "sh"; argv2[1] = "-c"; argv2[2] = (char *)cmd; argv2[3] = 0;
        char *const *av = argv2;
        char *envp[] = { 0 };
        __blockos_syscall(__SYS_execve, (long)"/bin/sh", (long)av, (long)envp, 0, 0, 0);
        __blockos_syscall(__SYS_exit, 127, 0, 0, 0, 0, 0);
        return 127;
    }
    int status = 0;
    __blockos_syscall(__SYS_wait4, pid, 0, (long)&status, 0, 0, 0);
    return status;
}
