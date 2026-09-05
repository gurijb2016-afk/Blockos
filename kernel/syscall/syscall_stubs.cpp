#include "syscall_stubs.hpp"
#include "syscall_numbers.hpp"
namespace blockos::syscall {
#define STUB(n) std::int64_t sys_##n(){return -38;}
STUB(read) STUB(write) STUB(openat) STUB(close) STUB(fstat) STUB(mmap) STUB(munmap) STUB(mprotect) STUB(brk)
STUB(setuid) STUB(setgid) STUB(seteuid) STUB(setegid) STUB(getuid) STUB(getgid) STUB(geteuid) STUB(getegid)
STUB(setgroups) STUB(getgroups) STUB(chdir) STUB(getcwd) STUB(access) STUB(readlink) STUB(stat) STUB(lstat)
STUB(socket) STUB(bind) STUB(listen) STUB(accept) STUB(accept4) STUB(connect) STUB(sendto) STUB(recvfrom) STUB(shutdown)
STUB(setsockopt) STUB(getsockopt) STUB(epoll_create1) STUB(epoll_ctl) STUB(epoll_wait) STUB(poll) STUB(ppoll) STUB(futex)
STUB(clone) STUB(wait4) STUB(exit) STUB(exit_group) STUB(sched_yield) STUB(nanosleep) STUB(execve) STUB(prctl)
STUB(getrandom) STUB(clock_gettime) STUB(getpid) STUB(getppid) STUB(gettid) STUB(set_tid_address) STUB(capget) STUB(capset)
STUB(ioctl) STUB(readv) STUB(writev)
#undef STUB
}
