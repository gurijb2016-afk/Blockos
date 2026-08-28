#pragma once
#include <sys/types.h>
struct pollfd { int fd; short events; short revents; };
#define POLLIN 0x001
#define POLLOUT 0x004
#define POLLERR 0x008
#define POLLHUP 0x010
#define POLLNVAL 0x020
#ifdef __cplusplus
extern "C" { int poll(struct pollfd*,unsigned long,int); }
#endif
