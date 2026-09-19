#include "dirent.h"
#include "blockos_syscall.h"
#include "fcntl.h"
#include "errno.h"
#include <string.h>
#include <stdlib.h>

struct linux_dirent64 { uint64_t ino; int64_t off; unsigned short reclen; unsigned char type; char name[]; };

DIR* opendir(const char* path){
    long fd=__blockos_syscall(__SYS_openat,-100,(long)path,O_RDONLY|O_DIRECTORY,0,0,0);
    if(fd<0){errno=(int)-fd;return 0;}
    DIR*d=(DIR*)malloc(sizeof(DIR));if(!d){__blockos_syscall(__SYS_close,fd,0,0,0,0,0);errno=12;return 0;}
    d->fd=(int)fd;d->pos=d->len=0;return d;
}
struct dirent* readdir(DIR*d){
    if(!d){errno=22;return 0;}
    for(;;){
        if(d->pos>=d->len){long n=__blockos_syscall(__SYS_getdents64,d->fd,(long)d->buf,sizeof(d->buf),0,0,0);if(n<=0)return 0;d->len=(size_t)n;d->pos=0;}
        struct linux_dirent64*x=(struct linux_dirent64*)(d->buf+d->pos);if(x->reclen<20||d->pos+x->reclen>d->len){errno=5;return 0;}
        memset(&d->current,0,sizeof(d->current));d->current.d_ino=x->ino;d->current.d_off=x->off;d->current.d_reclen=x->reclen;d->current.d_type=x->type;strncpy(d->current.d_name,x->name,sizeof(d->current.d_name)-1);d->pos+=x->reclen;return &d->current;
    }
}
int closedir(DIR*d){if(!d){errno=22;return -1;}long r=__blockos_syscall(__SYS_close,d->fd,0,0,0,0,0);free(d);if(r<0){errno=(int)-r;return -1;}return 0;}
