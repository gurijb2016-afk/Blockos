/* Small POSIX pieces dinit needs: 64-bit strtol family, stpcpy, sync, tcsetpgrp. */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "blockos_syscall.h"

unsigned long long strtoull(const char* s, char** endp, int base) {
    const char* p = s;
    unsigned long long v = 0;
    int any = 0, neg = 0;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '+') p++; else if (*p == '-') { neg = 1; p++; }
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    else if (base == 0) base = (p[0] == '0') ? 8 : 10;
    for (;; p++) {
        int d;
        unsigned char c = (unsigned char)*p;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * (unsigned long long)base + (unsigned long long)d;
        any = 1;
    }
    if (endp) *endp = (char*)(any ? p : s);
    if (!any) { errno = EINVAL; return 0; }
    return neg ? (unsigned long long)(-(long long)v) : v;
}

long long strtoll(const char* s, char** endp, int base) {
    return (long long)strtoull(s, endp, base);
}

char* stpcpy(char* dst, const char* src) {
    size_t n = strlen(src);
    memcpy(dst, src, n + 1);
    return dst + n;
}

void sync(void) { /* No write-back cache in the BlockOS kernel yet; nothing to flush. */ }

int tcsetpgrp(int fd, pid_t pgrp) { (void)fd; (void)pgrp; return 0; }

#include <stdarg.h>
#include <stdio.h>
#include <sys/syslog.h>

/* chown: BlockOS has no per-file ownership in the initramfs/VFS path dinit uses. */
int chown(const char *path, uid_t owner, gid_t group) { (void)path; (void)owner; (void)group; return 0; }

/* syslog: no daemon, so messages go to stderr (the serial console). */
static const char *log_ident = "";
void openlog(const char *ident, int option, int facility) { (void)option; (void)facility; log_ident = ident ? ident : ""; }
void closelog(void) { }
void syslog(int priority, const char *format, ...) {
    va_list ap;
    (void)priority;
    fprintf(stderr, "%s: ", log_ident);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

/* Credentials and priorities: BlockOS currently runs everything as root (uid/gid 0)
 * with no supplementary groups, so these only accept the no-op/root case. */
#include <sys/resource.h>
#include <grp.h>
#include <sys/ioctl.h>
int fchown(int fd, uid_t owner, gid_t group) { (void)fd; (void)owner; (void)group; return 0; }
int setreuid(uid_t ruid, uid_t euid) { (void)ruid; (void)euid; return 0; }
int setregid(gid_t rgid, gid_t egid) { (void)rgid; (void)egid; return 0; }
int setgroups(size_t size, const gid_t *list) { (void)size; (void)list; return 0; }
int initgroups(const char *user, gid_t group) { (void)user; (void)group; return 0; }
pid_t tcgetsid(int fd) { (void)fd; errno = ENOTTY; return -1; }
int setpriority(int which, int who, int prio) { (void)which; (void)who; (void)prio; return 0; }

/* *at() variants: BlockOS has no directory fds, so only AT_FDCWD (relative to cwd) is supported. */
#include <fcntl.h>
extern int rmdir(const char *path);
int symlink(const char *target, const char *linkpath) {
    long r = __blockos_syscall(__SYS_symlink, (long)target, (long)linkpath, 0, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return -1; }
    return 0;
}
int symlinkat(const char *target, int newdirfd, const char *linkpath) {
    if (newdirfd != AT_FDCWD) { errno = ENOSYS; return -1; }
    return symlink(target, linkpath);
}
int unlinkat(int dirfd, const char *path, int flags) {
    if (dirfd != AT_FDCWD) { errno = ENOSYS; return -1; }
    return (flags & AT_REMOVEDIR) ? rmdir(path) : unlink(path);
}
