#pragma once
#include <sys/types.h>
struct stat { dev_t st_dev; ino_t st_ino; mode_t st_mode; nlink_t st_nlink; uid_t st_uid; gid_t st_gid; dev_t st_rdev; off_t st_size; blksize_t st_blksize; blkcnt_t st_blocks; time_t st_atime; time_t st_mtime; time_t st_ctime; };
#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFLNK 0120000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_ISREG(m) (((m)&S_IFMT)==S_IFREG)
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#define S_ISLNK(m) (((m)&S_IFMT)==S_IFLNK)
#ifdef __cplusplus
extern "C" { int stat(const char*,struct stat*); int fstat(int,struct stat*); int lstat(const char*,struct stat*); int mkdir(const char*,mode_t); int rmdir(const char*); int unlink(const char*); int rename(const char*,const char*); }
#endif
