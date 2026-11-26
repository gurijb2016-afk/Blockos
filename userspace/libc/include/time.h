#ifdef __cplusplus
extern "C" {
#endif
#pragma once
#include <sys/types.h>
#include <sys/time.h>
#include <stddef.h>

struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};

typedef long clock_t;
#define CLOCKS_PER_SEC 1000000L
clock_t clock(void);
double difftime(time_t end, time_t beg);

time_t time(time_t *out);
struct tm *gmtime(const time_t *t);
struct tm *localtime(const time_t *t);
time_t mktime(struct tm *tm);
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);

struct tm *localtime_r(const time_t *t, struct tm *out);
struct tm *gmtime_r(const time_t *t, struct tm *out);
char *ctime(const time_t *t);
char *asctime(const struct tm *tm);
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/time.h>
int clock_settime(clockid_t clk, const struct timespec *ts);
#ifdef __cplusplus
}
#endif
