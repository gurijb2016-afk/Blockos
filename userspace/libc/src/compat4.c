/* More libc completions for X: identity, host name, time strings, net stubs.
 * BlockOS is single-user and has no DNS/NSS yet: name lookups fail cleanly. */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <math.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "blockos_syscall.h"

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }


int fchmod(int fd, unsigned int mode) { (void)fd; (void)mode; return 0; }

static const char *wday_name[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *mon_name[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static char asctime_buf[32];

char *asctime(const struct tm *tm) {
    snprintf(asctime_buf, sizeof(asctime_buf), "%s %s %2d %02d:%02d:%02d %d\n",
             wday_name[tm->tm_wday % 7], mon_name[tm->tm_mon % 12], tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
    return asctime_buf;
}
char *ctime(const time_t *t) {
    struct tm tmv;
    gmtime_r(t, &tmv);
    return asctime(&tmv);
}

double acos(double x) {
    if (x >= 1) return 0;
    if (x <= -1) return M_PI;
    return atan2(sqrt(1 - x * x), x);
}
double cbrt(double x) {
    if (x == 0) return 0;
    double neg = x < 0; double a = neg ? -x : x;
    double g = a > 1 ? a : 1;
    for (int i = 0; i < 80; i++) g = (2 * g + a / (g * g)) / 3;
    return neg ? -g : g;
}

const char *gai_strerror(int errcode) { (void)errcode; return "Name or service not known"; }
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    (void)node; (void)service; (void)hints;
    if (res) *res = NULL;
    return EAI_NONAME; /* no resolver yet */
}
void freeaddrinfo(struct addrinfo *res) { (void)res; }
struct hostent *gethostbyname(const char *name) { (void)name; return NULL; }

#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

int umask(int mask) { (void)mask; return 022; }
int chmod(const char *path, unsigned int mode) { (void)path; (void)mode; return 0; }
int setuid(uid_t uid) { (void)uid; return 0; }
int setgid(gid_t gid) { (void)gid; return 0; }
int seteuid(uid_t uid) { (void)uid; return 0; }
int setegid(gid_t gid) { (void)gid; return 0; }


struct servent *getservbyname(const char *name, const char *proto) { (void)name; (void)proto; return NULL; }

int execv(const char *path, char *const argv[]);
int execl(const char *path, const char *arg, ...) {
    char *argv[64];
    int n = 0;
    va_list ap; va_start(ap, arg);
    argv[n++] = (char *)arg;
    while (n < 63) { char *a = va_arg(ap, char *); argv[n++] = a; if (!a) break; }
    va_end(ap);
    argv[n < 64 ? n : 63] = NULL;
    return execv(path, argv);
}

char *inet_ntoa(struct in_addr in) {
    static char buf[16];
    uint32_t a = ntohl(in.s_addr);
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (a >> 24) & 255, (a >> 16) & 255, (a >> 8) & 255, a & 255);
    return buf;
}

int getsockname(int fd, struct sockaddr *addr, unsigned int *len) {
    long r = __blockos_syscall(__SYS_getsockname, fd, (long)addr, (long)len, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}
int getpeername(int fd, struct sockaddr *addr, unsigned int *len) {
    long r = __blockos_syscall(__SYS_getpeername, fd, (long)addr, (long)len, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}
