#include "sys/epoll.h"
#include "blockos_syscall.h"
#include "errno.h"
int epoll_create1(int flags){long r=__blockos_syscall(__SYS_epoll_create1,flags,0,0,0,0,0);if(r<0){errno=(int)-r;return -1;}return (int)r;}
int epoll_ctl(int e,int op,int fd,struct epoll_event*ev){long r=__blockos_syscall(__SYS_epoll_ctl,e,op,fd,0,0,(long)ev);if(r<0){errno=(int)-r;return -1;}return 0;}
int epoll_wait(int e,struct epoll_event*ev,int n,int timeout){long r=__blockos_syscall(__SYS_epoll_wait,e,(long)ev,n,timeout,0,0);if(r<0){errno=(int)-r;return -1;}return (int)r;}
