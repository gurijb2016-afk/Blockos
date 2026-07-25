#include "syscall_dispatcher.hpp"
#include "syscall_context.hpp"
#include "syscall_names.hpp"

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace blockos::syscall {

static inline void* p64(std::uint64_t v) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(v));
}
static inline const char* c64(std::uint64_t v) {
    return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(v));
}
static inline char* cw64(std::uint64_t v) {
    return reinterpret_cast<char*>(static_cast<std::uintptr_t>(v));
}

static void log(const SyscallHooks& hooks, const char* msg) {
    if (hooks.log) hooks.log(hooks.user, msg);
}

std::int64_t SyscallDispatcher::dispatch(std::uint64_t nr,
                                         std::uint64_t a1,
                                         std::uint64_t a2,
                                         std::uint64_t a3,
                                         std::uint64_t a4,
                                         std::uint64_t a5,
                                         std::uint64_t a6,
                                         SyscallContext& ctx,
                                         const SyscallHooks& hooks)
{
    auto* p = ctx.process;
    switch (nr) {
        case SYS_read:
            return hooks.do_read ? hooks.do_read(hooks.user, static_cast<int>(a1), p64(a2), static_cast<std::size_t>(a3)) : -38;
        case SYS_write:
            return hooks.do_write ? hooks.do_write(hooks.user, static_cast<int>(a1), p64(a2), static_cast<std::size_t>(a3)) : -38;
        case SYS_openat:
            return hooks.do_openat ? hooks.do_openat(hooks.user, static_cast<int>(a1), c64(a2), static_cast<int>(a3), static_cast<int>(a4)) : -38;
        case SYS_close:
            return hooks.do_close ? hooks.do_close(hooks.user, static_cast<int>(a1)) : -38;
        case SYS_fstat:
            return hooks.do_fstat ? hooks.do_fstat(hooks.user, static_cast<int>(a1), p64(a2)) : -38;
        case SYS_mmap:
            return hooks.do_mmap ? static_cast<std::int64_t>(hooks.do_mmap(hooks.user, p64(a1), static_cast<std::size_t>(a2), static_cast<int>(a3), static_cast<int>(a4), static_cast<int>(a5), static_cast<std::int64_t>(a6))) : -38;
        case SYS_munmap:
            return hooks.do_munmap ? hooks.do_munmap(hooks.user, p64(a1), static_cast<std::size_t>(a2)) : -38;
        case SYS_mprotect:
            return hooks.do_mprotect ? hooks.do_mprotect(hooks.user, p64(a1), static_cast<std::size_t>(a2), static_cast<int>(a3)) : -38;
        case SYS_brk:
            return hooks.do_brk ? static_cast<std::int64_t>(hooks.do_brk(hooks.user, p64(a1))) : -38;
        case SYS_setuid:
            return hooks.setuid && p ? (hooks.setuid(hooks.user, p, static_cast<std::uint32_t>(a1)) ? 0 : -1) : -38;
        case SYS_setgid:
            return hooks.setgid && p ? (hooks.setgid(hooks.user, p, static_cast<std::uint32_t>(a1)) ? 0 : -1) : -38;
        case SYS_seteuid:
            return hooks.seteuid && p ? (hooks.seteuid(hooks.user, p, static_cast<std::uint32_t>(a1)) ? 0 : -1) : -38;
        case SYS_setegid:
            return hooks.setegid && p ? (hooks.setegid(hooks.user, p, static_cast<std::uint32_t>(a1)) ? 0 : -1) : -38;
        case SYS_getuid:
            return hooks.getuid && p ? hooks.getuid(hooks.user, p) : (p ? p->creds.uid : 0);
        case SYS_getgid:
            return hooks.getgid && p ? hooks.getgid(hooks.user, p) : (p ? p->creds.gid : 0);
        case SYS_geteuid:
            return hooks.geteuid && p ? hooks.geteuid(hooks.user, p) : (p ? p->creds.euid : 0);
        case SYS_getegid:
            return hooks.getegid && p ? hooks.getegid(hooks.user, p) : (p ? p->creds.egid : 0);
        case SYS_setgroups:
            return hooks.setgroups && p ? (hooks.setgroups(hooks.user, p, reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(a1)), static_cast<std::size_t>(a2)) ? 0 : -1) : -38;
        case SYS_getgroups:
            return hooks.getgroups && p ? static_cast<std::int64_t>(hooks.getgroups(hooks.user, p, reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a1)), static_cast<std::size_t>(a2))) : -38;
        case SYS_chdir:
            return hooks.chdir && p ? (hooks.chdir(hooks.user, p, c64(a1)) ? 0 : -1) : -38;
        case SYS_getcwd:
            return hooks.getcwd && p ? (hooks.getcwd(hooks.user, p, cw64(a1), static_cast<std::size_t>(a2)) ? 0 : -1) : -38;
        case SYS_access:
            return hooks.do_access ? hooks.do_access(hooks.user, c64(a1), static_cast<int>(a2)) : -38;
        case SYS_readlink:
            return hooks.do_readlink ? hooks.do_readlink(hooks.user, c64(a1), cw64(a2), static_cast<std::size_t>(a3)) : -38;
        case SYS_stat:
            return hooks.do_stat ? hooks.do_stat(hooks.user, c64(a1), p64(a2)) : -38;
        case SYS_lstat:
            return hooks.do_lstat ? hooks.do_lstat(hooks.user, c64(a1), p64(a2)) : -38;
        case SYS_socket:
            return hooks.do_socket ? hooks.do_socket(hooks.user, static_cast<int>(a1), static_cast<int>(a2), static_cast<int>(a3)) : -38;
        case SYS_bind:
            return hooks.do_bind ? hooks.do_bind(hooks.user, static_cast<int>(a1), p64(a2), static_cast<std::size_t>(a3)) : -38;
        case SYS_listen:
            return hooks.do_listen ? hooks.do_listen(hooks.user, static_cast<int>(a1), static_cast<int>(a2)) : -38;
        case SYS_accept:
            return hooks.do_accept ? hooks.do_accept(hooks.user, static_cast<int>(a1), p64(a2), reinterpret_cast<std::size_t*>(static_cast<std::uintptr_t>(a3))) : -38;
        case SYS_accept4:
            return hooks.do_accept4 ? hooks.do_accept4(hooks.user, static_cast<int>(a1), p64(a2), reinterpret_cast<std::size_t*>(static_cast<std::uintptr_t>(a3)), static_cast<int>(a4)) : -38;
        case SYS_connect:
            return hooks.do_connect ? hooks.do_connect(hooks.user, static_cast<int>(a1), p64(a2), static_cast<std::size_t>(a3)) : -38;
        case SYS_sendto:
            return hooks.do_sendto ? hooks.do_sendto(hooks.user, static_cast<int>(a1), p64(a2), static_cast<std::size_t>(a3), static_cast<int>(a4), p64(a5), static_cast<std::size_t>(a6)) : -38;
        case SYS_recvfrom:
            return hooks.do_recvfrom ? hooks.do_recvfrom(hooks.user, static_cast<int>(a1), p64(a2), static_cast<std::size_t>(a3), static_cast<int>(a4), p64(a5), reinterpret_cast<std::size_t*>(static_cast<std::uintptr_t>(a6))) : -38;
        case SYS_shutdown:
            return hooks.do_shutdown ? hooks.do_shutdown(hooks.user, static_cast<int>(a1), static_cast<int>(a2)) : -38;
        case SYS_setsockopt:
            return hooks.do_setsockopt ? hooks.do_setsockopt(hooks.user, static_cast<int>(a1), static_cast<int>(a2), static_cast<int>(a3), p64(a4), static_cast<std::size_t>(a5)) : -38;
        case SYS_getsockopt:
            return hooks.do_getsockopt ? hooks.do_getsockopt(hooks.user, static_cast<int>(a1), static_cast<int>(a2), static_cast<int>(a3), p64(a4), reinterpret_cast<std::size_t*>(static_cast<std::uintptr_t>(a5))) : -38;
        case SYS_epoll_create1:
            return hooks.do_epoll_create1 ? hooks.do_epoll_create1(hooks.user, static_cast<int>(a1)) : -38;
        case SYS_epoll_ctl:
            return hooks.do_epoll_ctl ? hooks.do_epoll_ctl(hooks.user, static_cast<int>(a1), static_cast<int>(a2), static_cast<int>(a3), p64(a4)) : -38;
        case SYS_epoll_wait:
            return hooks.do_epoll_wait ? hooks.do_epoll_wait(hooks.user, static_cast<int>(a1), p64(a2), static_cast<int>(a3), static_cast<int>(a4)) : -38;
        case SYS_poll:
            return hooks.do_poll ? hooks.do_poll(hooks.user, p64(a1), static_cast<std::size_t>(a2), static_cast<int>(a3)) : -38;
        case SYS_ppoll:
            return hooks.do_ppoll ? hooks.do_ppoll(hooks.user, p64(a1), static_cast<std::size_t>(a2), p64(a3), p64(a4), static_cast<std::size_t>(a5)) : -38;
        case SYS_futex:
            return hooks.do_futex ? hooks.do_futex(hooks.user, reinterpret_cast<int*>(static_cast<std::uintptr_t>(a1)), static_cast<int>(a2), static_cast<int>(a3), p64(a4), reinterpret_cast<int*>(static_cast<std::uintptr_t>(a5)), static_cast<int>(a6)) : -38;
        case SYS_clone:
            return hooks.do_clone ? hooks.do_clone(hooks.user, a1, p64(a2), p64(a3), p64(a4), a5) : -38;
        case SYS_wait4:
            return hooks.do_wait4 ? hooks.do_wait4(hooks.user, static_cast<int>(a1), reinterpret_cast<int*>(static_cast<std::uintptr_t>(a2)), static_cast<int>(a3), p64(a4)) : -38;
        case SYS_exit:
            if (hooks.do_exit) hooks.do_exit(hooks.user, static_cast<std::int64_t>(a1));
            return 0;
        case SYS_exit_group:
            if (hooks.do_exit_group) hooks.do_exit_group(hooks.user, static_cast<std::int64_t>(a1));
            return 0;
        case SYS_sched_yield:
            return hooks.do_sched_yield ? hooks.do_sched_yield(hooks.user) : -38;
        case SYS_nanosleep:
            return hooks.do_nanosleep ? hooks.do_nanosleep(hooks.user, p64(a1), p64(a2)) : -38;
        case SYS_execve:
            return hooks.do_execve ? hooks.do_execve(hooks.user, c64(a1), reinterpret_cast<char* const*>(static_cast<std::uintptr_t>(a2)), reinterpret_cast<char* const*>(static_cast<std::uintptr_t>(a3))) : -38;
        case SYS_prctl:
            return hooks.do_prctl ? hooks.do_prctl(hooks.user, static_cast<int>(a1), a2, a3, a4, a5) : -38;
        case SYS_getrandom:
            return hooks.do_getrandom ? hooks.do_getrandom(hooks.user, p64(a1), static_cast<std::size_t>(a2), static_cast<unsigned int>(a3)) : -38;
        case SYS_clock_gettime:
            return hooks.do_clock_gettime ? hooks.do_clock_gettime(hooks.user, static_cast<int>(a1), p64(a2)) : -38;
        case SYS_getpid:
            return hooks.do_getpid ? hooks.do_getpid(hooks.user) : (p ? static_cast<std::int64_t>(p->pid) : 0);
        case SYS_getppid:
            return hooks.do_getppid ? hooks.do_getppid(hooks.user) : (p ? static_cast<std::int64_t>(p->ppid) : 0);
        case SYS_gettid:
            return hooks.do_gettid ? hooks.do_gettid(hooks.user) : static_cast<std::int64_t>(ctx.tid);
        case SYS_set_tid_address:
            return hooks.do_set_tid_address ? hooks.do_set_tid_address(hooks.user, reinterpret_cast<int*>(static_cast<std::uintptr_t>(a1))) : -38;
        case SYS_capget:
            return hooks.do_capget && p ? (hooks.do_capget(hooks.user, p, p64(a1), p64(a2)) ? 0 : -1) : -38;
        case SYS_capset:
            return hooks.do_capset && p ? (hooks.do_capset(hooks.user, p, p64(a1), p64(a2)) ? 0 : -1) : -38;
        case SYS_ioctl:
            return -38;
        case SYS_readv:
            return -38;
        case SYS_writev:
            return -38;
        default:
            log(hooks, "[SYSCALL] unknown");
            return -38;
    }
}

}
