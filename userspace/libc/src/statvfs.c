#include "sys/statvfs.h"
#include "sys/stat.h"
#include "errno.h"
#include "string.h"
int statvfs(const char* path, struct statvfs* st){
    (void)path; if(!st){errno=EINVAL;return -1;} memset(st,0,sizeof(*st)); st->f_bsize=4096; st->f_frsize=4096; return 0;
}
int fstatvfs(int fd, struct statvfs* st){ if(fd<0||!st){errno=EBADF;return -1;} memset(st,0,sizeof(*st)); st->f_bsize=4096; st->f_frsize=4096; return 0; }
