#include "stdio.h"
#include "unistd.h"
#include <stdint.h>

static void putc_buf(char* buf, size_t size, size_t* pos, char c) {
    if (*pos + 1 < size) buf[*pos] = c;
    (*pos)++;
}

static void puts_buf(char* buf, size_t size, size_t* pos, const char* s) {
    while (*s) putc_buf(buf, size, pos, *s++);
}

static void put_uint(char* buf, size_t size, size_t* pos, unsigned long v, int base, int upper) {
    char tmp[32];
    int n = 0;
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (v == 0) { putc_buf(buf, size, pos, '0'); return; }
    while (v && n < (int)sizeof(tmp)) { tmp[n++] = digits[v % (unsigned)base]; v /= (unsigned)base; }
    while (n) putc_buf(buf, size, pos, tmp[--n]);
}

static void put_int(char* buf, size_t size, size_t* pos, long v) {
    if (v < 0) {
        putc_buf(buf, size, pos, '-');
        /* Careful with LONG_MIN: cast to unsigned before negating to
         * avoid signed overflow on the one value where -v overflows. */
        put_uint(buf, size, pos, (unsigned long)(-(unsigned long)v), 10, 0);
    } else {
        put_uint(buf, size, pos, (unsigned long)v, 10, 0);
    }
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
    size_t pos = 0;
    for (const char* f = fmt; *f; f++) {
        if (*f != '%') { putc_buf(buf, size, &pos, *f); continue; }
        f++;
        int is_long = 0;
        if (*f == 'l') { is_long = 1; f++; }
        switch (*f) {
            case 'd': case 'i':
                if (is_long) put_int(buf, size, &pos, va_arg(ap, long));
                else         put_int(buf, size, &pos, (long)va_arg(ap, int));
                break;
            case 'u':
                if (is_long) put_uint(buf, size, &pos, va_arg(ap, unsigned long), 10, 0);
                else         put_uint(buf, size, &pos, (unsigned long)va_arg(ap, unsigned int), 10, 0);
                break;
            case 'x':
                if (is_long) put_uint(buf, size, &pos, va_arg(ap, unsigned long), 16, 0);
                else         put_uint(buf, size, &pos, (unsigned long)va_arg(ap, unsigned int), 16, 0);
                break;
            case 'X':
                if (is_long) put_uint(buf, size, &pos, va_arg(ap, unsigned long), 16, 1);
                else         put_uint(buf, size, &pos, (unsigned long)va_arg(ap, unsigned int), 16, 1);
                break;
            case 'p':
                puts_buf(buf, size, &pos, "0x");
                put_uint(buf, size, &pos, (unsigned long)(uintptr_t)va_arg(ap, void*), 16, 0);
                break;
            case 's': {
                const char* s = va_arg(ap, const char*);
                puts_buf(buf, size, &pos, s ? s : "(null)");
                break;
            }
            case 'c':
                putc_buf(buf, size, &pos, (char)va_arg(ap, int));
                break;
            case '%':
                putc_buf(buf, size, &pos, '%');
                break;
            default:
                /* Unknown conversion - print it literally rather than
                 * silently eating an argument, so a typo is visible. */
                putc_buf(buf, size, &pos, '%');
                if (is_long) putc_buf(buf, size, &pos, 'l');
                if (*f) putc_buf(buf, size, &pos, *f);
                break;
        }
    }
    if (size > 0) buf[pos < size ? pos : size - 1] = 0;
    return (int)pos;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    /* Long output beyond the 512-byte scratch buffer is truncated in this
     * MVP (no dynamic growth) - fine for diagnostics, not for arbitrary-size
     * formatted output. */
    int to_write = n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1;
    if (to_write > 0) write(1, buf, (size_t)to_write);
    return n;
}
