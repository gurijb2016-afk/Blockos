#pragma once
#include <stddef.h>
struct pollfd { int fd; short events; short revents; };
#define POLLIN 0x001
#define POLLOUT 0x004
#define POLLERR 0x008
#define POLLHUP 0x010
int poll(struct pollfd* fds, size_t nfds, int timeout);
