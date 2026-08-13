#pragma once
#include <sys/types.h>
struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};
#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFLNK 0120000
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#ifdef __cplusplus
extern "C" {
#endif
int stat(const char* path, struct stat* st);
int fstat(int fd, struct stat* st);
int mkdir(const char* path, mode_t mode);
int unlink(const char* path);
int rename(const char* oldp, const char* newp);
#ifdef __cplusplus
}
#endif
