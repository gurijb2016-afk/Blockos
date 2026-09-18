#pragma once
#include <stddef.h>
#include <stdint.h>
struct timespec { int64_t tv_sec; long tv_nsec; };

typedef long ssize_t;
typedef long off_t;
typedef int  pid_t;

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int     close(int fd);
off_t   lseek(int fd, off_t offset, int whence);
pid_t   getpid(void);
void*   sbrk(long increment);   /* returns (void*)-1 on failure */
__attribute__((noreturn)) void _exit(int code);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int open(const char* path, int flags, ...);
int openat(int dirfd, const char* path, int flags, ...);

int sched_yield(void);
int nanosleep(const struct timespec*, struct timespec*);

int unlink(const char* path);
int rename(const char* oldpath, const char* newpath);
int mkdir(const char* path, unsigned mode);
