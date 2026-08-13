#include "init/process.hpp"
#include "init/logger.hpp"

#include <errno.h>
#include <signal.h>
#include <unistd.h>

namespace blockos::init {

int process_spawn(
    const char* path,
    const char* const argv[],
    const char* const envp[])
{
    if (!path || !argv || !argv[0])
        return -1;

    pid_t pid = fork();

    if (pid < 0)
    {
        log(LogLevel::Error, "fork() failed for %s", path);
        return -1;
    }

    if (pid == 0)
    {
        execve(
            path,
            const_cast<char* const*>(argv),
            const_cast<char* const*>(envp)
        );

        _exit(127);
    }

    return static_cast<int>(pid);
}

bool process_alive(int pid)
{
    if (pid <= 0)
        return false;

    if (kill(pid, 0) == 0)
        return true;

    return errno == EPERM;
}

int process_signal(int pid, int signal_number)
{
    if (pid <= 0)
        return -1;

    return kill(pid, signal_number);
}

} // namespace blockos::init
