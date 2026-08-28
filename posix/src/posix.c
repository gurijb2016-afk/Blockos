#include "blockos_syscall.h"
#include "errno.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>

_Thread_local int errno;

static long sc(long n,long a0,long a1,long a2,long a3,long a4,long a5){
    long r=blockos_syscall6(n,a0,a1,a2,a3,a4,a5);
    if(r<0) errno=(int)-r;
    return r;
}

ssize_t read(int fd,const void*b,size_t n){return sc(BOS_SYS_READ,fd,(long)b,n,0,0,0);}
ssize_t write(int fd,const void*b,size_t n){return sc(BOS_SYS_WRITE,fd,(long)b,n,0,0,0);}
int close(int fd){return (int)sc(BOS_SYS_CLOSE,fd,0,0,0,0,0);}
int openat(int d,const char*p,int f,...){mode_t m=0; if(f&0x0040){va_list ap;va_start(ap,f);m=(mode_t)va_arg(ap,int);va_end(ap);} return (int)sc(BOS_SYS_OPENAT,d,(long)p,f,m,0,0);}
int open(const char*p,int f,...){mode_t m=0;if(f&0x0040){va_list ap;va_start(ap,f);m=(mode_t)va_arg(ap,int);va_end(ap);}return (int)sc(BOS_SYS_OPENAT,-100,(long)p,f,m,0,0);}
off_t lseek(int f,off_t o,int w){return sc(BOS_SYS_LSEEK,f,o,w,0,0,0);}
ssize_t readv(int f,const void*i,int c){return sc(BOS_SYS_READV,f,(long)i,c,0,0,0);}
ssize_t writev(int f,const void*i,int c){return sc(BOS_SYS_WRITEV,f,(long)i,c,0,0,0);}
int fstat(int f,struct stat*s){return (int)sc(BOS_SYS_FSTAT,f,(long)s,0,0,0,0);}
int stat(const char*p,struct stat*s){return (int)sc(BOS_SYS_STAT,(long)p,(long)s,0,0,0,0);}
int lstat(const char*p,struct stat*s){return (int)sc(BOS_SYS_LSTAT,(long)p,(long)s,0,0,0,0);}
int mkdir(const char*p,mode_t m){return (int)sc(BOS_SYS_MKDIR,(long)p,m,0,0,0,0);}
int rmdir(const char*p){return (int)sc(BOS_SYS_RMDIR,(long)p,0,0,0,0,0);}
int unlink(const char*p){return (int)sc(BOS_SYS_UNLINK,(long)p,0,0,0,0,0);}
int rename(const char*a,const char*b){return (int)sc(BOS_SYS_RENAME,(long)a,(long)b,0,0,0,0);}
int chdir(const char*p){return (int)sc(BOS_SYS_CHDIR,(long)p,0,0,0,0,0);}
char*getcwd(char*b,size_t n){long r=sc(BOS_SYS_GETCWD,(long)b,n,0,0,0,0);return r<0?0:b;}
int access(const char*p,int m){return (int)sc(BOS_SYS_ACCESS,(long)p,m,0,0,0,0);}
ssize_t readlink(const char*p,char*b,size_t n){return sc(BOS_SYS_READLINK,(long)p,(long)b,n,0,0,0);}
int dup(int f){return (int)sc(BOS_SYS_DUP,f,0,0,0,0,0);}
int dup2(int a,int b){return (int)sc(BOS_SYS_DUP2,a,b,0,0,0,0);}
int fcntl(int f,int c,...){long a=0;va_list ap;va_start(ap,c);a=va_arg(ap,long);va_end(ap);return (int)sc(BOS_SYS_FCNTL,f,c,a,0,0,0);}
int pipe2(int f[2],int fl){return (int)sc(BOS_SYS_PIPE2,(long)f,fl,0,0,0,0);}
int pipe(int f[2]){return pipe2(f,0);}
pid_t getpid(void){return sc(BOS_SYS_GETPID,0,0,0,0,0,0);} pid_t getppid(void){return sc(BOS_SYS_GETPPID,0,0,0,0,0,0);} pid_t gettid(void){return sc(BOS_SYS_GETTID,0,0,0,0,0,0);}
pid_t fork(void){return sc(BOS_SYS_CLONE,0,0,0,0,0,0);} /* BlockOS may define clone flags differently; replace with a dedicated fork ABI when available. */
int execve(const char*p,char*const a[],char*const e[]){return (int)sc(BOS_SYS_EXECVE,(long)p,(long)a,(long)e,0,0,0);}
pid_t waitpid(pid_t p,int*s,int o){return sc(BOS_SYS_WAIT4,p,(long)s,o,0,0,0);}
void _exit(int s){sc(BOS_SYS_EXIT,s,0,0,0,0,0);for(;;){} }
uid_t getuid(void){return sc(BOS_SYS_GETUID,0,0,0,0,0,0);} gid_t getgid(void){return sc(BOS_SYS_GETGID,0,0,0,0,0,0);} uid_t geteuid(void){return sc(BOS_SYS_GETEUID,0,0,0,0,0,0);} gid_t getegid(void){return sc(BOS_SYS_GETEGID,0,0,0,0,0,0);}
int setuid(uid_t u){return (int)sc(BOS_SYS_SETUID,u,0,0,0,0,0);}int setgid(gid_t g){return (int)sc(BOS_SYS_SETGID,g,0,0,0,0,0);}int seteuid(uid_t u){return (int)sc(BOS_SYS_SETEUID,u,0,0,0,0,0);}int setegid(gid_t g){return (int)sc(BOS_SYS_SETEGID,g,0,0,0,0,0);}
int ftruncate(int f,off_t n){return (int)sc(BOS_SYS_FTRUNCATE,f,n,0,0,0,0);}int fsync(int f){return (int)sc(BOS_SYS_FSYNC,f,0,0,0,0,0);}
int ioctl(int f,unsigned long r,...){va_list ap;va_start(ap,r);long a=va_arg(ap,long);va_end(ap);return (int)sc(BOS_SYS_IOCTL,f,r,a,0,0,0);}
void*mmap(void*a,size_t l,int p,int fl,int f,off_t o){return(void*)sc(BOS_SYS_MMAP,(long)a,l,p,fl,f,o);}int munmap(void*a,size_t l){return(int)sc(BOS_SYS_MUNMAP,(long)a,l,0,0,0,0);}int mprotect(void*a,size_t l,int p){return(int)sc(BOS_SYS_MPROTECT,(long)a,l,p,0,0,0);}int brk(void*e){return(int)sc(BOS_SYS_BRK,(long)e,0,0,0,0,0);}
int nanosleep(const struct timespec*r,struct timespec*x){return(int)sc(BOS_SYS_NANOSLEEP,(long)r,(long)x,0,0,0,0);}int clock_gettime(clockid_t c,struct timespec*t){return(int)sc(BOS_SYS_CLOCK_GETTIME,c,(long)t,0,0,0,0);}int sched_yield(void){return(int)sc(BOS_SYS_SCHED_YIELD,0,0,0,0,0,0);}
int kill(pid_t p,int s){return(int)sc(BOS_SYS_KILL,p,s,0,0,0,0);}int tgkill(pid_t p,pid_t t,int s){return(int)sc(BOS_SYS_TGKILL,p,t,s,0,0,0);}
int sigaction(int s,const struct sigaction*a,struct sigaction*o){return(int)sc(BOS_SYS_RT_SIGACTION,s,(long)a,(long)o,0,0,0);}int sigprocmask(int w,const void*a,void*o){return(int)sc(BOS_SYS_RT_SIGPROCMASK,w,(long)a,(long)o,0,0,0);}
sighandler_t signal(int s,sighandler_t h){struct sigaction a={h,0,0,{0,0}};struct sigaction o={0};if(sigaction(s,&a,&o)<0)return SIG_DFL;return o.sa_handler;}
int socket(int d,int t,int p){return(int)sc(BOS_SYS_SOCKET,d,t,p,0,0,0);}int bind(int s,const struct sockaddr*a,socklen_t n){return(int)sc(BOS_SYS_BIND,s,(long)a,n,0,0,0);}int listen(int s,int b){return(int)sc(BOS_SYS_LISTEN,s,b,0,0,0,0);}int accept(int s,struct sockaddr*a,socklen_t*n){return(int)sc(BOS_SYS_ACCEPT,s,(long)a,(long)n,0,0,0);}int accept4(int s,struct sockaddr*a,socklen_t*n,int f){return(int)sc(BOS_SYS_ACCEPT4,s,(long)a,(long)n,f,0,0);}int connect(int s,const struct sockaddr*a,socklen_t n){return(int)sc(BOS_SYS_CONNECT,s,(long)a,n,0,0,0);}ssize_t sendto(int s,const void*b,size_t n,int f,const struct sockaddr*a,socklen_t al){return sc(BOS_SYS_SENDTO,s,(long)b,n,f,(long)a,al);}ssize_t recvfrom(int s,void*b,size_t n,int f,struct sockaddr*a,socklen_t*al){return sc(BOS_SYS_RECVFROM,s,(long)b,n,f,(long)a,(long)al);}int shutdown(int s,int h){return(int)sc(BOS_SYS_SHUTDOWN,s,h,0,0,0,0);}int setsockopt(int s,int l,int o,const void*v,socklen_t n){return(int)sc(BOS_SYS_SETSOCKOPT,s,l,o,(long)v,n,0);}int getsockopt(int s,int l,int o,void*v,socklen_t*n){return(int)sc(BOS_SYS_GETSOCKOPT,s,l,o,(long)v,(long)n,0);}
int poll(struct pollfd*p,unsigned long n,int t){return(int)sc(BOS_SYS_POLL,(long)p,n,t,0,0,0);}int epoll_create1(int f){return(int)sc(BOS_SYS_EPOLL_CREATE1,f,0,0,0,0,0);}int epoll_ctl(int e,int op,int fd,struct epoll_event*ev){return(int)sc(BOS_SYS_EPOLL_CTL,e,op,fd,(long)ev,0,0);}int epoll_wait(int e,struct epoll_event*ev,int m,int t){return(int)sc(BOS_SYS_EPOLL_WAIT,e,(long)ev,m,t,0,0);}
uint16_t htons(uint16_t x){return(uint16_t)((x<<8)|(x>>8));}uint16_t ntohs(uint16_t x){return htons(x);}uint32_t htonl(uint32_t x){return((x&0xff)<<24)|((x&0xff00)<<8)|((x&0xff0000)>>8)|((x>>24)&0xff);}uint32_t ntohl(uint32_t x){return htonl(x);}
