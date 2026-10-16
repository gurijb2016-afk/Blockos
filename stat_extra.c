#include "sys/stat.h"
#include "blockos_syscall.h"
#include "errno.h"

static int stat_call(long nr, const char* path, struct stat* st) {
    long r = __blockos_syscall(nr, (long)path, (long)st, 0, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}
int stat(const char* path, struct stat* st)  { return stat_call(__SYS_stat, path, st); }
int lstat(const char* path, struct stat* st) { return stat_call(__SYS_lstat, path, st); }
int fstatat(int dirfd, const char* path, struct stat* st, int flags) {
    long r = __blockos_syscall(__SYS_fstatat, dirfd, (long)path, (long)st, flags, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}
