#pragma once

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     0100
#define O_EXCL      0200
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_DIRECTORY 0200000
#define O_CLOEXEC   02000000

#define AT_FDCWD (-100)

int open(const char* path, int flags, ...);
int openat(int dirfd, const char* path, int flags, ...);
