#include "syscall_names.hpp"

namespace blockos::syscall {

static constexpr const char* NAMES[] = {
    "read",
    "write",
    "openat",
    "close",
    "fstat",
    "mmap",
    "munmap",
    "mprotect",
    "brk",
    "setuid",
    "setgid",
    "seteuid",
    "setegid",
    "getuid",
    "getgid",
    "geteuid",
    "getegid",
    "setgroups",
    "getgroups",
    "chdir",
    "getcwd",
    "access",
    "readlink",
    "stat",
    "lstat",
    "socket",
    "bind",
    "listen",
    "accept",
    "accept4",
    "connect",
    "sendto",
    "recvfrom",
    "shutdown",
    "setsockopt",
    "getsockopt",
    "epoll_create1",
    "epoll_ctl",
    "epoll_wait",
    "poll",
    "ppoll",
    "futex",
    "clone",
    "wait4",
    "exit",
    "exit_group",
    "sched_yield",
    "nanosleep",
    "execve",
    "prctl",
    "getrandom",
    "clock_gettime",
    "getpid",
    "getppid",
    "gettid",
    "set_tid_address",
    "capget",
    "capset",
    "ioctl",
    "readv",
    "writev",
};

const char* syscall_name(std::uint64_t nr) {
    if (nr < (sizeof(NAMES) / sizeof(NAMES[0]))) return NAMES[nr];
    return "unknown";
}

std::size_t syscall_count() {
    return sizeof(NAMES) / sizeof(NAMES[0]);
}

}
