/* Small C-locale and POSIX helpers needed by libX11 / libXt / twm on BlockOS.
 * Locale: only the "C" locale exists, so every character is one byte. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

void *reallocarray(void *p, size_t nmemb, size_t size) {
    if (size && nmemb > (size_t)-1 / size) { errno = ENOMEM; return 0; }
    return realloc(p, nmemb * size);
}

int mblen(const char *s, size_t n) {
    if (!s) return 0;
    if (n == 0) return -1;
    if (*s == 0) return 0;
    return 1;
}

int mbtowc(wchar_t *pwc, const char *s, size_t n) {
    if (!s) return 0;
    if (n == 0) return -1;
    if (pwc) *pwc = (unsigned char)*s;
    return *s ? 1 : 0;
}

int wctomb(char *s, wchar_t wc) {
    if (!s) return 0;
    if ((unsigned)wc > 0x7f) { errno = 84; /* EILSEQ */ return -1; }
    *s = (char)wc;
    return 1;
}

size_t mbstowcs(wchar_t *dst, const char *src, size_t n) {
    size_t i = 0;
    while (src[i] && (!dst || i < n)) {
        if (dst) dst[i] = (unsigned char)src[i];
        i++;
    }
    if (dst && i < n) dst[i] = 0;
    return i;
}

int isascii(int c) { return (c & ~0x7f) == 0; }

int getresuid(unsigned int *ruid, unsigned int *euid, unsigned int *suid) {
    unsigned int u = (unsigned int) getuid();
    unsigned int e = (unsigned int) geteuid();
    if (ruid) *ruid = u;
    if (euid) *euid = e;
    if (suid) *suid = e;
    return 0;
}

int getresgid(unsigned int *rgid, unsigned int *egid, unsigned int *sgid) {
    unsigned int g = (unsigned int) getgid();
    unsigned int e = (unsigned int) getegid();
    if (rgid) *rgid = g;
    if (egid) *egid = e;
    if (sgid) *sgid = e;
    return 0;
}

/* Reentrant passwd lookup. Copies the fields of the non-reentrant result into buf. */
static int pw_copy(const struct passwd *src, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    size_t need = 0;
    const char *parts[] = { src->pw_name, src->pw_passwd, src->pw_gecos, src->pw_dir, src->pw_shell };
    for (int i = 0; i < 5; i++) need += (parts[i] ? strlen(parts[i]) : 0) + 1;
    if (need > buflen) { *result = 0; return ERANGE; }
    char *q = buf;
    char *fields[5];
    for (int i = 0; i < 5; i++) {
        fields[i] = q;
        if (parts[i]) { size_t l = strlen(parts[i]); memcpy(q, parts[i], l); q += l; }
        *q++ = 0;
    }
    pwd->pw_name = fields[0];
    pwd->pw_passwd = fields[1];
    pwd->pw_uid = src->pw_uid;
    pwd->pw_gid = src->pw_gid;
    pwd->pw_gecos = fields[2];
    pwd->pw_dir = fields[3];
    pwd->pw_shell = fields[4];
    *result = pwd;
    return 0;
}

int getpwuid_r(unsigned long uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    struct passwd *p = getpwuid(uid);
    if (!p) { *result = 0; return 0; }
    return pw_copy(p, pwd, buf, buflen, result);
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    struct passwd *p = getpwnam(name);
    if (!p) { *result = 0; return 0; }
    return pw_copy(p, pwd, buf, buflen, result);
}

int inet_pton(int af, const char *src, void *dst) {
    if (af != AF_INET) { errno = EAFNOSUPPORT; return -1; }
    unsigned int parts[4];
    int n = 0;
    const char *p = src;
    while (n < 4) {
        if (!isdigit((unsigned char)*p)) return 0;
        unsigned int v = 0;
        int digits = 0;
        while (isdigit((unsigned char)*p)) {
            v = v * 10 + (unsigned)(*p - '0');
            if (v > 255) return 0;
            p++; digits++;
        }
        if (digits > 3) return 0;
        parts[n++] = v;
        if (n < 4) { if (*p != '.') return 0; p++; }
    }
    if (*p) return 0;
    uint8_t *o = dst;
    for (int i = 0; i < 4; i++) o[i] = (uint8_t) parts[i];
    return 1;
}

const char *inet_ntop(int af, const void *src, char *dst, unsigned int size) {
    if (af != AF_INET) { errno = EAFNOSUPPORT; return 0; }
    const uint8_t *a = src;
    char tmp[16];
    int len = 0;
    for (int i = 0; i < 4; i++) {
        int v = a[i];
        char d[4]; int k = 0;
        do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v);
        while (k) tmp[len++] = d[--k];
        if (i < 3) tmp[len++] = '.';
    }
    tmp[len] = 0;
    if ((unsigned)len + 1 > size) { errno = ENOSPC; return 0; }
    memcpy(dst, tmp, (size_t)len + 1);
    return dst;
}

#include <netinet/in.h>
const struct in6_addr in6addr_any = {{{0}}};
const struct in6_addr in6addr_loopback = {{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}}};

/* ---- additional POSIX pieces needed by twm / libXt / libX11 ---- */
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <netdb.h>
#include <time.h>

int vprintf(const char *fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }
int putchar(int c) { return fputc(c, stdout); }

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        const char *p = iov[i].iov_base;
        size_t n = iov[i].iov_len;
        while (n) {
            ssize_t w = write(fd, p, n);
            if (w < 0) return total ? total : -1;
            p += w; n -= (size_t)w; total += w;
        }
    }
    return total;
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        char *p = iov[i].iov_base;
        size_t n = iov[i].iov_len;
        while (n) {
            ssize_t r = read(fd, p, n);
            if (r < 0) return total ? total : -1;
            if (r == 0) return total;
            p += r; n -= (size_t)r; total += r;
        }
    }
    return total;
}

int mkstemp(char *tmpl) {
    size_t len = strlen(tmpl);
    if (len < 6 || strcmp(tmpl + len - 6, "XXXXXX") != 0) { errno = EINVAL; return -1; }
    static unsigned long seq = 0;
    for (int attempt = 0; attempt < 100; attempt++) {
        unsigned long v = (unsigned long)time(0) * 2654435761UL + (++seq) * 40503UL + (unsigned long)attempt;
        for (int i = 0; i < 6; i++) {
            tmpl[len - 6 + i] = (char)('a' + (v % 26));
            v /= 26;
        }
        int fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) return fd;
        if (errno != EEXIST) return -1;
    }
    errno = EEXIST;
    return -1;
}

int execlp(const char *file, const char *arg, ...) {
    char *argv[64];
    int n = 0;
    va_list ap;
    argv[n++] = (char *)arg;
    va_start(ap, arg);
    while (n < 63) {
        char *a = va_arg(ap, char *);
        argv[n++] = a;
        if (!a) break;
    }
    va_end(ap);
    argv[n < 64 ? n : 63] = 0;
    return execvp(file, argv);
}

struct hostent *gethostbyaddr(const void *addr, unsigned int len, int type) {
    (void)addr; (void)len; (void)type;
    return 0;
}

/* Not cryptographic: /dev/urandom if it can be opened, otherwise a time-based stream. */
void arc4random_buf(void *buf, size_t n) {
    unsigned char *p = buf;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        size_t got = 0;
        while (got < n) {
            ssize_t r = read(fd, p + got, n - got);
            if (r <= 0) break;
            got += (size_t)r;
        }
        close(fd);
        if (got == n) return;
        p += got; n -= got;
    }
    unsigned long x = (unsigned long)time(0) ^ 0x9e3779b97f4a7c15UL;
    for (size_t i = 0; i < n; i++) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        p[i] = (unsigned char)x;
    }
}

int strcoll(const char *a, const char *b) { return strcmp(a, b); }

#define BLOCKOS_ATEXIT_MAX 32
static void (*atexit_fns[BLOCKOS_ATEXIT_MAX])(void);
static int atexit_count;

int atexit(void (*fn)(void)) {
    if (atexit_count >= BLOCKOS_ATEXIT_MAX) return -1;
    atexit_fns[atexit_count++] = fn;
    return 0;
}

void __blockos_run_atexit(void) {
    while (atexit_count > 0) {
        void (*fn)(void) = atexit_fns[--atexit_count];
        fn();
    }
}

extern char **environ;

int putenv(char *s) {
    if (!s || !strchr(s, '=')) { errno = EINVAL; return -1; }
    size_t nlen = (size_t)(strchr(s, '=') - s);
    size_t count = 0;
    if (environ) while (environ[count]) count++;
    char **arr = (char **)malloc((count + 2) * sizeof(char *));
    if (!arr) { errno = ENOMEM; return -1; }
    size_t j = 0;
    int replaced = 0;
    for (size_t i = 0; i < count; i++) {
        char *e = environ[i];
        if (!replaced && strncmp(e, s, nlen) == 0 && e[nlen] == '=') {
            arr[j++] = s; replaced = 1;
        } else {
            arr[j++] = e;
        }
    }
    if (!replaced) arr[j++] = s;
    arr[j] = 0;
    environ = arr;
    return 0;
}
