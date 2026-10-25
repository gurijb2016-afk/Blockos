#pragma once

/* TLS-backed errno.  The BlockOS dynamic linker provides PT_TLS/FS_BASE
 * support, so errno is independent for each pthread. */
extern _Thread_local int errno;

#define EPERM   1
#define ENOENT  2
#define ESRCH   3
#define EINTR   4
#define EIO     5
#define ENXIO   6
#define E2BIG   7
#define EBADF   9
#define ECHILD  10
#define EAGAIN  11
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EBUSY   16
#define EEXIST  17
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define ENFILE  23
#define EMFILE  24
#define ENOTTY  25
#define EFBIG   27
#define ENOSPC  28
#define ESPIPE  29
#define EROFS   30
#define EMLINK  31
#define EPIPE   32
#define ERANGE  34
#define ENOSYS  38
#define ENOTSOCK 88
#define EHOSTUNREACH 113

#define EDEADLK 35
#define ENAMETOOLONG 36
#define ENOLCK 37
#define ENOTEMPTY 39
#define ELOOP 40
#define EOVERFLOW 75
#define ENOTSUP 95
#define EOPNOTSUPP 95
#define EAFNOSUPPORT 97
#define EADDRINUSE 98
#define EADDRNOTAVAIL 99
#define ENETDOWN 100
#define ENETUNREACH 101
#define ENETRESET 102
#define ECONNABORTED 103
#define ECONNRESET 104
#define ENOBUFS 105
#define EISCONN 106
#define ENOTCONN 107
#define ETIMEDOUT 110
#define ECONNREFUSED 111
#define EHOSTDOWN 112
#define EINPROGRESS 115
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif
