#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
long blockos_syscall6(long nr, long a0, long a1, long a2, long a3, long a4, long a5);
#ifdef __cplusplus
}
#endif

/* BlockOS syscall ABI, synchronized with syscalls.tbl (uefi-kernel-scaffold). */
#define BOS_SYS_READ 0
#define BOS_SYS_WRITE 1
#define BOS_SYS_OPENAT 2
#define BOS_SYS_CLOSE 3
#define BOS_SYS_FSTAT 4
#define BOS_SYS_MMAP 5
#define BOS_SYS_MUNMAP 6
#define BOS_SYS_MPROTECT 7
#define BOS_SYS_BRK 8
#define BOS_SYS_SETUID 9
#define BOS_SYS_SETGID 10
#define BOS_SYS_SETEUID 11
#define BOS_SYS_SETEGID 12
#define BOS_SYS_GETUID 13
#define BOS_SYS_GETGID 14
#define BOS_SYS_GETEUID 15
#define BOS_SYS_GETEGID 16
#define BOS_SYS_SETGROUPS 17
#define BOS_SYS_GETGROUPS 18
#define BOS_SYS_CHDIR 19
#define BOS_SYS_GETCWD 20
#define BOS_SYS_ACCESS 21
#define BOS_SYS_READLINK 22
#define BOS_SYS_STAT 23
#define BOS_SYS_LSTAT 24
#define BOS_SYS_SOCKET 25
#define BOS_SYS_BIND 26
#define BOS_SYS_LISTEN 27
#define BOS_SYS_ACCEPT 28
#define BOS_SYS_ACCEPT4 29
#define BOS_SYS_CONNECT 30
#define BOS_SYS_SENDTO 31
#define BOS_SYS_RECVFROM 32
#define BOS_SYS_SHUTDOWN 33
#define BOS_SYS_SETSOCKOPT 34
#define BOS_SYS_GETSOCKOPT 35
#define BOS_SYS_EPOLL_CREATE1 36
#define BOS_SYS_EPOLL_CTL 37
#define BOS_SYS_EPOLL_WAIT 38
#define BOS_SYS_POLL 39
#define BOS_SYS_PPOLL 40
#define BOS_SYS_FUTEX 41
#define BOS_SYS_CLONE 42
#define BOS_SYS_WAIT4 43
#define BOS_SYS_EXIT 44
#define BOS_SYS_EXIT_GROUP 45
#define BOS_SYS_SCHED_YIELD 46
#define BOS_SYS_NANOSLEEP 47
#define BOS_SYS_EXECVE 48
#define BOS_SYS_PRCTL 49
#define BOS_SYS_GETRANDOM 50
#define BOS_SYS_CLOCK_GETTIME 51
#define BOS_SYS_GETPID 52
#define BOS_SYS_GETPPID 53
#define BOS_SYS_GETTID 54
#define BOS_SYS_SET_TID_ADDRESS 55
#define BOS_SYS_CAPGET 56
#define BOS_SYS_CAPSET 57
#define BOS_SYS_IOCTL 58
#define BOS_SYS_READV 59
#define BOS_SYS_WRITEV 60
#define BOS_SYS_SERVICE 61
#define BOS_SYS_GETDENTS64 62
#define BOS_SYS_LSEEK 63
#define BOS_SYS_IOCTL_DUP 64
#define BOS_SYS_FCNTL 65
#define BOS_SYS_DUP 66
#define BOS_SYS_DUP2 67
#define BOS_SYS_PIPE2 68
#define BOS_SYS_RENAME 69
#define BOS_SYS_UNLINK 70
#define BOS_SYS_MKDIR 71
#define BOS_SYS_RMDIR 72
#define BOS_SYS_GETPID_DUP 73
#define BOS_SYS_KILL 74
#define BOS_SYS_TGKILL 75
#define BOS_SYS_RT_SIGACTION 76
#define BOS_SYS_RT_SIGPROCMASK 77
#define BOS_SYS_RT_SIGRETURN 78
#define BOS_SYS_SET_ROBUST_LIST 79
#define BOS_SYS_FTRUNCATE 80
#define BOS_SYS_FSYNC 81
#define BOS_SYS_STATX 82
