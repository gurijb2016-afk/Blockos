#pragma once
#include <stdint.h>
struct statvfs {
    unsigned long f_bsize, f_frsize;
    uint64_t f_blocks, f_bfree, f_bavail;
    uint64_t f_files, f_ffree, f_favail;
    unsigned long f_fsid, f_flag, f_namemax;
};
int statvfs(const char*, struct statvfs*);
int fstatvfs(int, struct statvfs*);
