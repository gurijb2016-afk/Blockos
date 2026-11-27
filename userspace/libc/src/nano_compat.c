/* getopt and getopt_long for BlockOS. (glob comes from the gnulib copy that nano builds.) */
#include "getopt.h"
#include "glob.h"
#include <string.h>
#include <stdio.h>

char *optarg = 0;
int optind = 1, opterr = 1, optopt = '?';
static int optpos = 1;

int getopt(int argc, char *const argv[], const char *optstring) {
    return getopt_long(argc, argv, optstring, 0, 0);
}

static void opt_report(int argc, char *const argv[], const char *msg, int c) {
    (void)argc;
    if (opterr) fprintf(stderr, "%s: %s -- %c\n", argv[0], msg, c);
}

int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex) {
    const char *p;
    optarg = 0;
    if (optind >= argc || !argv[optind]) return -1;

    /* long options: "--name" or "--name=value" */
    if (longopts && argv[optind][0] == '-' && argv[optind][1] == '-') {
        const char *arg = argv[optind] + 2;
        if (!*arg) { optind++; return -1; }  /* "--" ends options */
        const char *eq = strchr(arg, '=');
        size_t nlen = eq ? (size_t)(eq - arg) : strlen(arg);
        for (int i = 0; longopts[i].name; i++) {
            if (strlen(longopts[i].name) == nlen && strncmp(longopts[i].name, arg, nlen) == 0) {
                if (longindex) *longindex = i;
                optind++;
                if (longopts[i].has_arg == required_argument) {
                    if (eq) optarg = (char *)eq + 1;
                    else if (optind < argc) optarg = argv[optind++];
                    else { opt_report(argc, argv, "option requires an argument", 0); optopt = longopts[i].val; return '?'; }
                } else if (eq) {
                    opt_report(argc, argv, "option takes no argument", 0);
                    return '?';
                }
                if (longopts[i].flag) { *longopts[i].flag = longopts[i].val; return 0; }
                return longopts[i].val;
            }
        }
        optind++;
        if (opterr) fprintf(stderr, "%s: unrecognized option '--%.*s'\n", argv[0], (int)nlen, arg);
        return '?';
    }

    /* short options */
    if (argv[optind][0] != '-' || argv[optind][1] == 0) return -1;
    char c = argv[optind][optpos];
    p = optstring ? strchr(optstring, c) : 0;
    if (!p || c == ':') {
        optopt = c;
        if (argv[optind][++optpos] == 0) { optind++; optpos = 1; }
        opt_report(argc, argv, "invalid option", c);
        return '?';
    }
    if (p[1] == ':') {
        if (argv[optind][optpos + 1]) {
            optarg = &argv[optind][optpos + 1];
        } else if (optind + 1 < argc) {
            optarg = argv[++optind];
        } else {
            optopt = c; optind++; optpos = 1;
            opt_report(argc, argv, "option requires an argument", c);
            return optstring[0] == ':' ? ':' : '?';
        }
        optind++; optpos = 1;
        return c;
    }
    if (argv[optind][++optpos] == 0) { optind++; optpos = 1; }
    return c;
}


/* ---- more functions nano needs ---- */
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>

#undef assert
void assert(int ok) { if (!ok) abort(); }

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    size_t len = 0;
    int c;
    if (!lineptr || !n || !stream) { errno = EINVAL; return -1; }
    if (!*lineptr || *n == 0) {
        *n = 128;
        *lineptr = (char *)malloc(*n);
        if (!*lineptr) { errno = ENOMEM; return -1; }
    }
    while ((c = fgetc(stream)) != EOF) {
        if (len + 2 > *n) {
            size_t nn = *n * 2;
            char *p = (char *)realloc(*lineptr, nn);
            if (!p) { errno = ENOMEM; return -1; }
            *lineptr = p; *n = nn;
        }
        (*lineptr)[len++] = (char)c;
        if (c == '\n') break;
    }
    if (len == 0 && c == EOF) return -1;
    (*lineptr)[len] = '\0';
    return (ssize_t)len;
}

/* No user database yet: the password enumeration is empty. */
struct passwd *getpwent(void) { return 0; }
void endpwent(void) {}
char *getlogin(void) { return 0; }

long fpathconf(int fd, int name) {
    (void)fd;
    if (name == _PC_PIPE_BUF) return 4096;
    errno = EINVAL;
    return -1;
}

/* No wide-character classes: every class is empty. */
unsigned long wctype(const char *property) { (void)property; return 0; }
int iswctype(unsigned int wc, unsigned long desc) { (void)wc; (void)desc; return 0; }
