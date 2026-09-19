#pragma once
#include <stddef.h>
#include <stdint.h>

struct dirent { uint64_t d_ino; int64_t d_off; unsigned short d_reclen; unsigned char d_type; char d_name[256]; };
typedef struct DIR { int fd; unsigned char buf[8192]; size_t pos; size_t len; struct dirent current; } DIR;
DIR* opendir(const char* path);
struct dirent* readdir(DIR* d);
int closedir(DIR* d);
