/* Libc completions needed by X libraries/server (static BlockOS userland).
 * Small, dependency-light implementations; not locale-aware. */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/types.h>
#include "blockos_syscall.h"

static int c_isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
static int c_isdigit(int c) { return c >= '0' && c <= '9'; }

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }

static unsigned long rand_state = 1;
void srand(unsigned int seed) { rand_state = seed; }
int rand(void) {
    rand_state = rand_state * 1103515245UL + 12345UL;
    return (int)((rand_state >> 16) & 0x7fff);
}

static const char *errmsg(int e) {
    switch (e) {
    case 0: return "Success";
    case 1: return "Operation not permitted";
    case 2: return "No such file or directory";
    case 4: return "Interrupted system call";
    case 5: return "Input/output error";
    case 9: return "Bad file descriptor";
    case 11: return "Resource temporarily unavailable";
    case 12: return "Cannot allocate memory";
    case 13: return "Permission denied";
    case 14: return "Bad address";
    case 17: return "File exists";
    case 20: return "Not a directory";
    case 21: return "Is a directory";
    case 22: return "Invalid argument";
    case 24: return "Too many open files";
    case 28: return "No space left on device";
    case 32: return "Broken pipe";
    case 38: return "Function not implemented";
    case 110: return "Connection timed out";
    case 111: return "Connection refused";
    default: return "Unknown error";
    }
}
char *strerror(int errnum) { return (char *)errmsg(errnum); }
int strerror_r(int errnum, char *buf, size_t buflen) {
    const char *m = errmsg(errnum);
    size_t n = strlen(m);
    if (!buf || buflen == 0) return ERANGE;
    if (n >= buflen) { memcpy(buf, m, buflen - 1); buf[buflen - 1] = 0; return ERANGE; }
    memcpy(buf, m, n + 1);
    return 0;
}
char *strsignal(int sig) { (void)sig; return (char *)"Unknown signal"; }

size_t strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n]) n++;
    return n;
}

char *strtok_r(char *s, const char *delim, char **save) {
    char *p = s ? s : *save;
    if (!p) return NULL;
    p += strspn(p, delim);
    if (!*p) { *save = NULL; return NULL; }
    char *end = p + strcspn(p, delim);
    if (*end) { *end = 0; *save = end + 1; } else *save = NULL;
    return p;
}
static char *strtok_state;
char *strtok(char *s, const char *delim) { return strtok_r(s, delim, &strtok_state); }

int sprintf(char *s, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(s, (size_t)-1, fmt, ap);
    va_end(ap);
    return r;
}
int vsprintf(char *s, const char *fmt, va_list ap) { return vsnprintf(s, (size_t)-1, fmt, ap); }

int vasprintf(char **out, const char *fmt, va_list ap) {
    va_list cp; va_copy(cp, ap);
    int n = vsnprintf(NULL, 0, fmt, cp);
    va_end(cp);
    if (n < 0) return -1;
    char *p = malloc((size_t)n + 1);
    if (!p) return -1;
    vsnprintf(p, (size_t)n + 1, fmt, ap);
    *out = p;
    return n;
}
int asprintf(char **out, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vasprintf(out, fmt, ap);
    va_end(ap);
    return r;
}

/* Minimal sscanf: %d %i %u %x %o %ld %lu %lx %s %c %n %%, widths, whitespace. */
int vsscanf(const char *s, const char *fmt, va_list ap) {
    int count = 0;
    const char *p = s;
    while (*fmt) {
        if (c_isspace((unsigned char)*fmt)) { while (c_isspace((unsigned char)*p)) p++; fmt++; continue; }
        if (*fmt != '%') { if (*p != *fmt) break; p++; fmt++; continue; }
        fmt++;
        if (*fmt == '%') { while (c_isspace((unsigned char)*p)) p++; if (*p != '%') break; p++; fmt++; continue; }
        int suppress = 0;
        if (*fmt == '*') { suppress = 1; fmt++; }
        int width = 0;
        while (c_isdigit((unsigned char)*fmt)) width = width * 10 + (*fmt++ - '0');
        int lng = 0;
        if (*fmt == 'l') { lng = 1; fmt++; if (*fmt == 'l') { lng = 2; fmt++; } }
        char conv = *fmt++;
        if (conv == 'n') { if (!suppress) *va_arg(ap, int *) = (int)(p - s); continue; }
        if (conv != 'c') while (c_isspace((unsigned char)*p)) p++;
        if (!*p) break;
        int maxw = width ? width : 1 << 30;
        if (conv == 's') {
            char *dst = suppress ? NULL : va_arg(ap, char *);
            int n = 0;
            while (*p && !c_isspace((unsigned char)*p) && n < maxw) { if (dst) dst[n] = *p; p++; n++; }
            if (!n) break;
            if (dst) dst[n] = 0;
            if (!suppress) count++;
        } else if (conv == 'c') {
            char *dst = suppress ? NULL : va_arg(ap, char *);
            int n = width ? width : 1;
            if (!*p) break;
            if (dst) memcpy(dst, p, (size_t)n);
            p += n;
            if (!suppress) count++;
        } else if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'x' || conv == 'X' || conv == 'o') {
            int base = (conv == 'x' || conv == 'X') ? 16 : (conv == 'o') ? 8 : 10;
            int neg = 0, n = 0;
            unsigned long long v = 0;
            if ((conv == 'd' || conv == 'i') && (*p == '-' || *p == '+')) { neg = (*p == '-'); p++; n++; }
            if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { p += 2; n += 2; }
            int any = 0;
            while (n < maxw) {
                int d;
                char c = *p;
                if (c_isdigit((unsigned char)c)) d = c - '0';
                else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else break;
                if (d >= base) break;
                v = v * (unsigned long long)base + (unsigned long long)d;
                p++; n++; any = 1;
            }
            if (!any) break;
            if (!suppress) {
                if (neg) v = (unsigned long long)(-(long long)v);
                if (lng == 2) *va_arg(ap, long long *) = (long long)v;
                else if (lng == 1) *va_arg(ap, long *) = (long)v;
                else *va_arg(ap, int *) = (int)v;
                count++;
            }
        } else {
            break; /* unsupported conversion (floats) */
        }
    }
    return count;
}
int sscanf(const char *s, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}


int remove(const char *path) {
    if (unlink(path) == 0) return 0;
    if (rmdir(path) == 0) return 0;
    return -1;
}

int pipe(int fds[2]) {
    long r = __blockos_syscall(__SYS_pipe2, (long)fds, 0, 0, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}

/* BlockOS has no hard links. link() is emulated as an exclusive create of
 * newpath (O_CREAT|O_EXCL, so it fails with EEXIST exactly like link() does)
 * followed by a copy of oldpath's contents. That is enough for the lock-file
 * protocol X uses (write tmp file, link to lock name, unlink tmp). */
int link(const char *oldpath, const char *newpath) {
    char buf[1024];
    int in = open(oldpath, O_RDONLY);
    if (in < 0) return -1;
    int out = open(newpath, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (out < 0) { int e = errno; close(in); errno = e; return -1; }
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n < 0) { int e = errno; close(in); close(out); unlink(newpath); errno = e; return -1; }
        if (n == 0) break;
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(out, buf + off, (size_t)(n - off));
            if (w <= 0) { int e = errno ? errno : EIO; close(in); close(out); unlink(newpath); errno = e; return -1; }
            off += w;
        }
    }
    close(in);
    close(out);
    return 0;
}

int flock(int fd, int op) { (void)fd; (void)op; return 0; }

int pthread_attr_setscope(pthread_attr_t *a, int scope) { (void)a; (void)scope; return 0; }

char *basename(char *path) {
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') path[--n] = 0;
    char *slash = strrchr(path, '/');
    return (slash && slash[1]) ? slash + 1 : (slash && n == 1 ? path : path);
}
char *dirname(char *path) {
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') path[--n] = 0;
    char *slash = strrchr(path, '/');
    if (!slash) return dot;
    if (slash == path) { path[1] = 0; return path; }
    *slash = 0;
    return path;
}

/* UTC calendar conversion (no timezone database on BlockOS yet). */
struct tm *gmtime_r(const time_t *t, struct tm *out) {
    int64_t secs = (int64_t)*t;
    int64_t days = secs / 86400;
    int64_t rem = secs % 86400;
    if (rem < 0) { rem += 86400; days--; }
    out->tm_hour = (int)(rem / 3600);
    out->tm_min = (int)((rem % 3600) / 60);
    out->tm_sec = (int)(rem % 60);
    out->tm_wday = (int)((days + 4) % 7);
    if (out->tm_wday < 0) out->tm_wday += 7;
    /* civil-from-days (Howard Hinnant) */
    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int64_t d = doy - (153 * mp + 2) / 5 + 1;
    int64_t m = mp < 10 ? mp + 3 : mp - 9;
    y += (m <= 2);
    out->tm_year = (int)(y - 1900);
    out->tm_mon = (int)(m - 1);
    out->tm_mday = (int)d;
    static const int cum[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) && m > 2;
    out->tm_yday = cum[out->tm_mon] + (int)d - 1 + leap;
    out->tm_isdst = 0;
    return out;
}
static struct tm gm_buf;
struct tm *gmtime(const time_t *t) { return gmtime_r(t, &gm_buf); }
struct tm *localtime_r(const time_t *t, struct tm *out) { return gmtime_r(t, out); }
struct tm *localtime(const time_t *t) { return gmtime_r(t, &gm_buf); }
