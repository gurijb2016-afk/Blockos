#include "sys/poll.h"
#include "blockos_syscall.h"
#include "errno.h"
int poll(struct pollfd* fds,size_t nfds,int timeout){long r=__blockos_syscall(__SYS_poll,(long)fds,nfds,timeout,0,0,0);if(r<0){errno=(int)-r;return -1;}return (int)r;}
