#pragma once
#include <sys/types.h>
struct timeval { time_t tv_sec; suseconds_t tv_usec; };
struct timespec { time_t tv_sec; long tv_nsec; };
#ifdef __cplusplus
extern "C" { int nanosleep(const struct timespec*,struct timespec*); int clock_gettime(clockid_t,struct timespec*); }
#endif
