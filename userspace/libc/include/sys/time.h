#ifdef __cplusplus
extern "C" {
#endif
#ifndef BLOCKOS_SYS_TIME_H
#define BLOCKOS_SYS_TIME_H
#include <stdint.h>
typedef int64_t time_t;
typedef int64_t suseconds_t;
typedef int clockid_t;
struct timeval { int64_t tv_sec; int64_t tv_usec; };
struct timezone { int tz_minuteswest; int tz_dsttime; };
struct timespec { int64_t tv_sec; long tv_nsec; };
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3

/* BSD timeval helpers (as in glibc <sys/time.h>) */
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#define timercmp(a, b, CMP) \
    (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP (b)->tv_usec) \
                                  : ((a)->tv_sec CMP (b)->tv_sec))
#define timeradd(a, b, result) \
    do { \
        (result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
        (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
        if ((result)->tv_usec >= 1000000) { \
            ++(result)->tv_sec; \
            (result)->tv_usec -= 1000000; \
        } \
    } while (0)
#define timersub(a, b, result) \
    do { \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
        if ((result)->tv_usec < 0) { \
            --(result)->tv_sec; \
            (result)->tv_usec += 1000000; \
        } \
    } while (0)
int gettimeofday(struct timeval*, struct timezone*);
int clock_gettime(int clock_id, struct timespec* ts);
int nanosleep(const struct timespec* req, struct timespec* rem);
#endif

int utimes(const char*, const struct timeval[2]);
int settimeofday(const struct timeval*, const void*);

#ifdef __cplusplus
}
#endif
