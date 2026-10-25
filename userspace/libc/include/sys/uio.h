#ifndef BLOCKOS_SYS_UIO_H
#define BLOCKOS_SYS_UIO_H
#include <stddef.h>
struct iovec { void *iov_base; size_t iov_len; };
#endif

#ifndef BLOCKOS_UIO_X11_COMPAT
#define BLOCKOS_UIO_X11_COMPAT
#include <sys/types.h>
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
#endif
