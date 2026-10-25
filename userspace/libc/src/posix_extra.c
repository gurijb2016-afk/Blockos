#include "unistd.h"
#include "sys/time.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/statvfs.h"
#include "blockos_syscall.h"
#include "errno.h"
#include "string.h"
#include "limits.h"
#include "stdlib.h"
#include <stdint.h>
#include <stddef.h>

unsigned int sleep(unsigned int sec){struct timespec r={0,0};struct timespec q={(int64_t)sec,0};if(nanosleep(&q,&r)<0)return sec;return (unsigned)r.tv_sec;}
int usleep(unsigned int usec){struct timespec q={(int64_t)(usec/1000000),(long)((usec%1000000)*1000)};return nanosleep(&q,0);}
long sysconf(int name){switch(name){case 2: return 4096; /* _SC_PAGESIZE */ case 4:return 1024; /* _SC_OPEN_MAX */ case 84:return 1; /* _SC_NPROCESSORS_ONLN */ case 85:return 1; /* _SC_NPROCESSORS_CONF */ default:return -1;}}
long getpagesize(void){return 4096;}
int gethostname(char*name,size_t len){if(!name||!len){errno=22;return -1;}const char*h="blockos";size_t n=strlen(h);if(n>=len)n=len-1;memcpy(name,h,n);name[n]=0;return 0;}
int stat(const char*path,struct stat*st){long r=__blockos_syscall(__SYS_stat,(long)path,(long)st,0,0,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
int lstat(const char*path,struct stat*st){long r=__blockos_syscall(__SYS_lstat,(long)path,(long)st,0,0,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
int access(const char*path,int mode){(void)mode;long r=__blockos_syscall(__SYS_access,(long)path,0,0,0,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
int rmdir(const char*path){long r=__blockos_syscall(__SYS_rmdir,(long)path,0,0,0,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
int ftruncate(int fd,off_t n){long r=__blockos_syscall(__SYS_ftruncate,fd,(long)n,0,0,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
int fsync(int fd){long r=__blockos_syscall(__SYS_fsync,fd,0,0,0,0,0);if(r<0){errno=(int)-r;return -1;}return 0;}
char* realpath(const char*path,char*out){if(!path){errno=22;return 0;}char local[PATH_MAX];if(path[0]=='/')strncpy(local,path,sizeof(local)-1);else{if(!getcwd(local,sizeof(local)))return 0;size_t n=strlen(local);if(n+1+strlen(path)+1>=sizeof(local)){errno=36;return 0;}if(n&&local[n-1]!='/')local[n++]='/';strcpy(local+n,path);}local[sizeof(local)-1]=0;if(!out)out=(char*)malloc(strlen(local)+1);if(!out)return 0;strcpy(out,local);return out;}
