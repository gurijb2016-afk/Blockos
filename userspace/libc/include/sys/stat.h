#pragma once
#include <stdint.h>

/*
 * This layout is BlockOS-specific, not the real glibc x86-64 struct stat
 * ABI - it matches exactly what kernel/user_syscall.cpp's SYS_fstat
 * handler currently writes (q[0]=dev, q[1]=ino, q[2]=mode, q[7]=size,
 * 128 bytes total, everything else zeroed). If you ever change what the
 * kernel writes, update this struct to match, or fstat()'s callers will
 * silently read garbage/zero for whatever field moved.
 */
struct stat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_mode;
    unsigned long __pad3;
    unsigned long __pad4;
    unsigned long __pad5;
    unsigned long __pad6;
    unsigned long st_size;
    unsigned long __pad8;
    unsigned long __pad9;
    unsigned long __pad10;
    unsigned long __pad11;
    unsigned long __pad12;
    unsigned long __pad13;
    unsigned long __pad14;
    unsigned long __pad15;
};

int fstat(int fd, struct stat* buf);
