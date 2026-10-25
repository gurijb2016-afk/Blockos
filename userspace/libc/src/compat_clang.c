#include "blockos_syscall.h"
#include "errno.h"
#include "sys/resource.h"
#include "sys/utsname.h"
#include "sys/time.h"
#include <stddef.h>
#include <string.h>
int getrlimit(int x,struct rlimit*r){long v=__blockos_syscall(__SYS_getrlimit,x,(long)r,0,0,0,0);if(v<0){errno=(int)-v;return -1;}return 0;}
int setrlimit(int x,const struct rlimit*r){long v=__blockos_syscall(__SYS_setrlimit,x,(long)r,0,0,0,0);if(v<0){errno=(int)-v;return -1;}return 0;}
int uname(struct utsname*u){long v=__blockos_syscall(__SYS_uname,(long)u,0,0,0,0,0);if(v<0){errno=(int)-v;return -1;}return 0;}
int madvise(void*a,size_t l,int advice){long v=__blockos_syscall(__SYS_madvise,(long)a,(long)l,advice,0,0,0);if(v<0){errno=(int)-v;return -1;}return 0;}
