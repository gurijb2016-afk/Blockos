#pragma once
#include <stdint.h>
struct epoll_event { uint32_t events; uint32_t _pad; uint64_t data; };
#define EPOLLIN 0x001
#define EPOLLOUT 0x004
#define EPOLLERR 0x008
#define EPOLLHUP 0x010
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3
int epoll_create1(int flags);
int epoll_ctl(int epfd,int op,int fd,struct epoll_event* event);
int epoll_wait(int epfd,struct epoll_event* events,int maxevents,int timeout);
