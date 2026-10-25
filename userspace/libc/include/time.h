#pragma once
#include <sys/time.h>
#include <stddef.h>

struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};

time_t time(time_t *out);
struct tm *gmtime(const time_t *t);
struct tm *localtime(const time_t *t);
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);

struct tm *localtime_r(const time_t *t, struct tm *out);
struct tm *gmtime_r(const time_t *t, struct tm *out);
char *ctime(const time_t *t);
char *asctime(const struct tm *tm);
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);
