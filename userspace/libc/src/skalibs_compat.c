/* Small POSIX pieces that skalibs needs and BlockOS does not implement yet. */
#include <errno.h>
#include <regex.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int sigismember(const sigset_t *set, int signo) {
    if (signo < 1 || signo > 64) { errno = EINVAL; return -1; }
    return (int)((*set >> (signo - 1)) & 1u);
}

unsigned int alarm(unsigned int seconds) { (void)seconds; return 0; }

int fchdir(int fd) { (void)fd; errno = ENOSYS; return -1; }
int mkfifo(const char *path, mode_t mode) { (void)path; (void)mode; errno = ENOSYS; return -1; }
int mknod(const char *path, mode_t mode, dev_t dev) { (void)path; (void)mode; (void)dev; errno = ENOSYS; return -1; }
int lchown(const char *path, uid_t owner, gid_t group) { (void)path; (void)owner; (void)group; return 0; }
int clock_settime(clockid_t clk, const struct timespec *ts) { (void)clk; (void)ts; errno = EPERM; return -1; }

char *mkdtemp(char *template) {
    static unsigned long counter = 0;
    size_t len = strlen(template);
    if (len < 6 || strcmp(template + len - 6, "XXXXXX") != 0) { errno = EINVAL; return NULL; }
    for (int attempt = 0; attempt < 100; attempt++) {
        unsigned long v = ++counter * 2654435761UL;
        for (int i = 0; i < 6; i++) {
            template[len - 6 + i] = "abcdefghijklmnopqrstuvwxyz0123456789"[(v >> (i * 5)) % 36];
        }
        if (mkdir(template, 0700) == 0) return template;
        if (errno != EEXIST) return NULL;
    }
    errno = EEXIST;
    return NULL;
}

int regcomp(regex_t *preg, const char *pattern, int cflags) {
    (void)pattern; (void)cflags;
    if (preg) { preg->re_nsub = 0; preg->__impl = NULL; }
    return REG_BADPAT;
}
int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags) {
    (void)preg; (void)string; (void)nmatch; (void)pmatch; (void)eflags;
    return REG_NOMATCH;
}
void regfree(regex_t *preg) { (void)preg; }
size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    (void)errcode; (void)preg;
    const char *msg = "regex not supported on BlockOS";
    if (errbuf && errbuf_size) { strncpy(errbuf, msg, errbuf_size - 1); errbuf[errbuf_size - 1] = 0; }
    return strlen(msg) + 1;
}
