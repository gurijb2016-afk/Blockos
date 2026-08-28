#pragma once
#include <sys/types.h>
#define AT_FDCWD (-100)
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define O_EXCL 0x0080
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#define O_NONBLOCK 0x0800
#define O_CLOEXEC 0x80000
#define AT_SYMLINK_NOFOLLOW 0x100
#define FD_CLOEXEC 1
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#ifdef __cplusplus
extern "C" { int open(const char*,int,...); int openat(int,const char*,int,...); int fcntl(int,int,...); }
#endif
