#pragma once
#include <stdint.h>
struct epoll_event { uint32_t events; uint64_t data; };
#define EPOLLIN 0x001
#define EPOLLOUT 0x004
#define EPOLLERR 0x008
#define EPOLLHUP 0x010
#define EPOLLET (1u<<31)
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3
#ifdef __cplusplus
extern "C" { int epoll_create1(int); int epoll_ctl(int,int,int,struct epoll_event*); int epoll_wait(int,struct epoll_event*,int,int); }
#endif
