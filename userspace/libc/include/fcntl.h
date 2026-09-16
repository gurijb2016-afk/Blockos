#pragma once

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0100
#define O_TRUNC  01000
#define O_APPEND 02000

#define AT_FDCWD (-100)

/* Note: the kernel's current SYS_openat handler only reads the path
 * (it ignores dirfd/flags/mode), so O_CREAT etc. are accepted here for
 * source compatibility but don't yet do anything on the kernel side -
 * every open() is effectively read-only. Extend kernel/user_syscall.cpp's
 * SYS_openat case if you need real write/create support. */
int open(const char* path, int flags, ...);
