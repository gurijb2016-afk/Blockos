#pragma once
#include <stddef.h>

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
