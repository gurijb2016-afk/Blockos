#include "syscall_stubs.hpp"
#include "syscall_numbers.hpp"
#include "syscall_runtime.hpp"

namespace blockos::syscall {

extern SyscallRuntime* g_syscall_runtime;

std::int64_t sys_read() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_read) : -38;
}

std::int64_t sys_write() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_write) : -38;
}

std::int64_t sys_openat() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_openat) : -38;
}

std::int64_t sys_close() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_close) : -38;
}

std::int64_t sys_fstat() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_fstat) : -38;
}

std::int64_t sys_mmap() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_mmap) : -38;
}

std::int64_t sys_munmap() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_munmap) : -38;
}

std::int64_t sys_mprotect() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_mprotect) : -38;
}

std::int64_t sys_brk() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_brk) : -38;
}

std::int64_t sys_setuid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_setuid) : -38;
}

std::int64_t sys_setgid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_setgid) : -38;
}

std::int64_t sys_seteuid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_seteuid) : -38;
}

std::int64_t sys_setegid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_setegid) : -38;
}

std::int64_t sys_getuid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getuid) : -38;
}

std::int64_t sys_getgid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getgid) : -38;
}

std::int64_t sys_geteuid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_geteuid) : -38;
}

std::int64_t sys_getegid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getegid) : -38;
}

std::int64_t sys_setgroups() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_setgroups) : -38;
}

std::int64_t sys_getgroups() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getgroups) : -38;
}

std::int64_t sys_chdir() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_chdir) : -38;
}

std::int64_t sys_getcwd() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getcwd) : -38;
}

std::int64_t sys_access() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_access) : -38;
}

std::int64_t sys_readlink() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_readlink) : -38;
}

std::int64_t sys_stat() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_stat) : -38;
}

std::int64_t sys_lstat() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_lstat) : -38;
}

std::int64_t sys_socket() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_socket) : -38;
}

std::int64_t sys_bind() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_bind) : -38;
}

std::int64_t sys_listen() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_listen) : -38;
}

std::int64_t sys_accept() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_accept) : -38;
}

std::int64_t sys_accept4() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_accept4) : -38;
}

std::int64_t sys_connect() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_connect) : -38;
}

std::int64_t sys_sendto() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_sendto) : -38;
}

std::int64_t sys_recvfrom() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_recvfrom) : -38;
}

std::int64_t sys_shutdown() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_shutdown) : -38;
}

std::int64_t sys_setsockopt() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_setsockopt) : -38;
}

std::int64_t sys_getsockopt() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getsockopt) : -38;
}

std::int64_t sys_epoll_create1() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_epoll_create1) : -38;
}

std::int64_t sys_epoll_ctl() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_epoll_ctl) : -38;
}

std::int64_t sys_epoll_wait() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_epoll_wait) : -38;
}

std::int64_t sys_poll() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_poll) : -38;
}

std::int64_t sys_ppoll() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_ppoll) : -38;
}

std::int64_t sys_futex() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_futex) : -38;
}

std::int64_t sys_clone() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_clone) : -38;
}

std::int64_t sys_wait4() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_wait4) : -38;
}

std::int64_t sys_exit() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_exit) : -38;
}

std::int64_t sys_exit_group() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_exit_group) : -38;
}

std::int64_t sys_sched_yield() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_sched_yield) : -38;
}

std::int64_t sys_nanosleep() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_nanosleep) : -38;
}

std::int64_t sys_execve() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_execve) : -38;
}

std::int64_t sys_prctl() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_prctl) : -38;
}

std::int64_t sys_getrandom() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getrandom) : -38;
}

std::int64_t sys_clock_gettime() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_clock_gettime) : -38;
}

std::int64_t sys_getpid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getpid) : -38;
}

std::int64_t sys_getppid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_getppid) : -38;
}

std::int64_t sys_gettid() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_gettid) : -38;
}

std::int64_t sys_set_tid_address() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_set_tid_address) : -38;
}

std::int64_t sys_capget() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_capget) : -38;
}

std::int64_t sys_capset() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_capset) : -38;
}

std::int64_t sys_ioctl() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_ioctl) : -38;
}

std::int64_t sys_readv() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_readv) : -38;
}

std::int64_t sys_writev() {
    return g_syscall_runtime ? g_syscall_runtime->call(SYS_writev) : -38;
}

}
