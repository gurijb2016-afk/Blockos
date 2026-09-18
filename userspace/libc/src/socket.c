#include "sys/socket.h"
#include "blockos_syscall.h"
#include "errno.h"

typedef long ssize_t;
static long rr(long r){if(r<0){errno=(int)-r;return -1;}return r;}
int socket(int domain,int type,int protocol){return (int)rr(__blockos_syscall(__SYS_socket,domain,type,protocol,0,0,0));}
int socketpair(int domain,int type,int protocol,int sv[2]){return (int)rr(__blockos_syscall(__SYS_socketpair,domain,type,protocol,0,0,(long)sv));}
int bind(int fd,const struct sockaddr*a,socklen_t l){return (int)rr(__blockos_syscall(__SYS_bind,fd,(long)a,l,0,0,0));}
int listen(int fd,int backlog){return (int)rr(__blockos_syscall(__SYS_listen,fd,backlog,0,0,0,0));}
int accept(int fd,struct sockaddr*a,socklen_t*l){return (int)rr(__blockos_syscall(__SYS_accept,fd,(long)a,(long)l,0,0,0));}
int accept4(int fd,struct sockaddr*a,socklen_t*l,int flags){return (int)rr(__blockos_syscall(__SYS_accept4,fd,(long)a,(long)l,flags,0,0));}
int connect(int fd,const struct sockaddr*a,socklen_t l){return (int)rr(__blockos_syscall(__SYS_connect,fd,(long)a,l,0,0,0));}
ssize_t send(int fd,const void*b,size_t n,int flags){return rr(__blockos_syscall(__SYS_sendto,fd,(long)b,n,flags,0,0));}
ssize_t recv(int fd,void*b,size_t n,int flags){return rr(__blockos_syscall(__SYS_recvfrom,fd,(long)b,n,flags,0,0));}
ssize_t sendto(int fd,const void*b,size_t n,int flags,const struct sockaddr*d,socklen_t l){return rr(__blockos_syscall(__SYS_sendto,fd,(long)b,n,flags,(long)d,l));}
ssize_t recvfrom(int fd,void*b,size_t n,int flags,struct sockaddr*s,socklen_t*l){return rr(__blockos_syscall(__SYS_recvfrom,fd,(long)b,n,flags,(long)s,(long)l));}
ssize_t sendmsg(int fd,const struct msghdr*m,int flags){return rr(__blockos_syscall(__SYS_sendmsg,fd,(long)m,flags,0,0,0));}
ssize_t recvmsg(int fd,struct msghdr*m,int flags){return rr(__blockos_syscall(__SYS_recvmsg,fd,(long)m,flags,0,0,0));}
int shutdown(int fd,int how){return (int)rr(__blockos_syscall(__SYS_shutdown,fd,how,0,0,0,0));}
int setsockopt(int fd,int level,int opt,const void*v,socklen_t l){return (int)rr(__blockos_syscall(__SYS_setsockopt,fd,level,opt,(long)v,l,0));}
int getsockopt(int fd,int level,int opt,void*v,socklen_t*l){return (int)rr(__blockos_syscall(__SYS_getsockopt,fd,level,opt,(long)v,(long)l,0));}
