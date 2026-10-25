/* Libc completions for X font/config code: sorting, number parsing, string
 * scanning, stream scanf, strftime subset, assert backend. */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include "blockos_syscall.h"

/* ---- qsort: in-place heapsort (O(n log n), not stable) ---- */
static void swap_bytes(char *a, char *b, size_t n) {
    while (n--) { char t = *a; *a++ = *b; *b++ = t; }
}
static void sift(char *base, size_t size, size_t root, size_t end,
                 int (*cmp)(const void *, const void *)) {
    for (;;) {
        size_t child = 2 * root + 1;
        if (child > end) break;
        if (child + 1 <= end && cmp(base + child * size, base + (child + 1) * size) < 0) child++;
        if (cmp(base + root * size, base + child * size) < 0) {
            swap_bytes(base + root * size, base + child * size, size);
            root = child;
        } else break;
    }
}
void qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *)) {
    char *b = base;
    if (nmemb < 2 || size == 0) return;
    for (size_t i = (nmemb - 2) / 2 + 1; i-- > 0;) sift(b, size, i, nmemb - 1, cmp);
    for (size_t end = nmemb - 1; end > 0; end--) {
        swap_bytes(b, b + end * size, size);
        sift(b, size, 0, end - 1, cmp);
    }
}

/* ---- strings ---- */
size_t strspn(const char *s, const char *accept) {
    size_t n = 0;
    while (s[n] && strchr(accept, s[n])) n++;
    return n;
}
size_t strcspn(const char *s, const char *reject) {
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n])) n++;
    return n;
}

/* ---- numbers ---- */
double strtod(const char *s, char **end) {
    const char *p = s;
    int neg = 0;
    double v = 0;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
    if (*p == '-' || *p == '+') { neg = (*p == '-'); p++; }
    int any = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; any = 1; }
    if (*p == '.') {
        p++;
        double scale = 0.1;
        while (*p >= '0' && *p <= '9') { v += (*p - '0') * scale; scale *= 0.1; p++; any = 1; }
    }
    if (any && (*p == 'e' || *p == 'E')) {
        const char *q = p + 1;
        int eneg = 0, ex = 0, eany = 0;
        if (*q == '-' || *q == '+') { eneg = (*q == '-'); q++; }
        while (*q >= '0' && *q <= '9') { ex = ex * 10 + (*q - '0'); q++; eany = 1; }
        if (eany) {
            p = q;
            double m = 1;
            for (int i = 0; i < ex && i < 400; i++) m *= 10;
            v = eneg ? v / m : v * m;
        }
    }
    if (end) *end = (char *)(any ? p : s);
    return neg ? -v : v;
}
double atof(const char *s) { return strtod(s, NULL); }
float sqrtf(float x) { return (float)sqrt((double)x); }

static unsigned long rnd_state = 1;
void srandom(unsigned int seed) { rnd_state = seed; }
long random(void) {
    rnd_state = rnd_state * 1103515245UL + 12345UL;
    return (long)((rnd_state >> 16) & 0x7fffffff);
}

/* ---- strftime (UTC; common conversions only) ---- */
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *t) {
    size_t n = 0;
    char tmp[32];
    if (!max) return 0;
    for (const char *f = fmt; *f; f++) {
        const char *piece = tmp;
        if (*f != '%') { tmp[0] = *f; tmp[1] = 0; }
        else {
            f++;
            switch (*f) {
            case 'Y': snprintf(tmp, sizeof tmp, "%d", t->tm_year + 1900); break;
            case 'm': snprintf(tmp, sizeof tmp, "%02d", t->tm_mon + 1); break;
            case 'd': snprintf(tmp, sizeof tmp, "%02d", t->tm_mday); break;
            case 'H': snprintf(tmp, sizeof tmp, "%02d", t->tm_hour); break;
            case 'M': snprintf(tmp, sizeof tmp, "%02d", t->tm_min); break;
            case 'S': snprintf(tmp, sizeof tmp, "%02d", t->tm_sec); break;
            case 'j': snprintf(tmp, sizeof tmp, "%03d", t->tm_yday + 1); break;
            case '%': tmp[0] = '%'; tmp[1] = 0; break;
            default:  tmp[0] = '%'; tmp[1] = *f; tmp[2] = 0; break;
            }
        }
        for (; *piece; piece++) {
            if (n + 1 >= max) { s[n] = 0; return 0; }
            s[n++] = *piece;
        }
    }
    s[n] = 0;
    return n;
}

/* ---- stream scanf (subset) built on getc/ungetc ---- */
static int c_isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }

int vfscanf(FILE *f, const char *fmt, va_list ap) {
    int count = 0;
    while (*fmt) {
        if (c_isspace((unsigned char)*fmt)) {
            int c;
            while ((c = getc(f)) != EOF && c_isspace(c)) ;
            if (c != EOF) ungetc(c, f);
            fmt++;
            continue;
        }
        if (*fmt != '%') {
            int c = getc(f);
            if (c != *fmt) { if (c != EOF) ungetc(c, f); break; }
            fmt++;
            continue;
        }
        fmt++;
        if (*fmt == '%') {
            int c;
            while ((c = getc(f)) != EOF && c_isspace(c)) ;
            if (c != '%') { if (c != EOF) ungetc(c, f); break; }
            fmt++;
            continue;
        }
        int suppress = 0;
        if (*fmt == '*') { suppress = 1; fmt++; }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        int lng = 0;
        if (*fmt == 'l') { lng = 1; fmt++; if (*fmt == 'l') { lng = 2; fmt++; } }
        char conv = *fmt++;
        if (conv == 'n') { if (!suppress) *va_arg(ap, int *) = count; continue; }
        int maxw = width ? width : 1 << 30;
        int c;
        if (conv != 'c') while ((c = getc(f)) != EOF && c_isspace(c)) ;
        else c = getc(f);
        if (c == EOF) break;
        if (conv == 's') {
            char *dst = suppress ? NULL : va_arg(ap, char *);
            int n = 0;
            while (c != EOF && !c_isspace(c) && n < maxw) {
                if (dst) dst[n] = (char)c;
                n++;
                c = getc(f);
            }
            if (c != EOF) ungetc(c, f);
            if (!n) break;
            if (dst) dst[n] = 0;
            if (!suppress) count++;
        } else if (conv == 'c') {
            char *dst = suppress ? NULL : va_arg(ap, char *);
            if (dst) *dst = (char)c;
            if (!suppress) count++;
        } else if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'x' || conv == 'X') {
            int base = (conv == 'x' || conv == 'X') ? 16 : 10;
            int neg = 0, n = 0, any = 0;
            unsigned long long v = 0;
            if (c == '-' || c == '+') { neg = (c == '-'); c = getc(f); n++; }
            while (c != EOF && n < maxw) {
                int d;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else break;
                v = v * base + d; any = 1; n++;
                c = getc(f);
            }
            if (c != EOF) ungetc(c, f);
            if (!any) break;
            if (!suppress) {
                if (neg) v = (unsigned long long)(-(long long)v);
                if (lng == 2) *va_arg(ap, long long *) = (long long)v;
                else if (lng == 1) *va_arg(ap, long *) = (long)v;
                else *va_arg(ap, int *) = (int)v;
                count++;
            }
        } else {
            if (c != EOF) ungetc(c, f);
            break;
        }
    }
    return count;
}
int fscanf(FILE *f, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vfscanf(f, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- assert backend ---- */
void __blockos_assert_fail(const char *expr, const char *file, int line, const char *func) {
    fprintf(stderr, "assertion failed: %s (%s:%d, %s)\n", expr, file, line, func);
    fflush(stderr);
    __blockos_syscall(__SYS_exit_group, 134, 0, 0, 0, 0, 0);
    for (;;) ;
}
