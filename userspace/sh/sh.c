/* Minimal BlockOS /bin/sh: freestanding, static, uses the BlockOS syscall ABI
 * (int 0x80, rax=nr, rdi/rsi/rdx args). Built-ins only. */
#include <stdint.h>
#include <stddef.h>

enum {
    SYS_READ = 0, SYS_WRITE = 1, SYS_CHDIR = 19, SYS_GETCWD = 20,
    SYS_IOCTL = 58, SYS_EXIT = 44, SYS_SCHED_YIELD = 46
};

/* tty termios layout and ioctl requests as defined by the kernel (tty.cpp). */
struct termios_k { uint32_t iflag, oflag, cflag, lflag; uint8_t cc[20]; };
#define TIOC_GETTERMIOS 0x424F5303UL
#define TIOC_SETTERMIOS 0x424F5304UL
#define LFLAG_ECHO   (1u << 0)
#define LFLAG_ICANON (1u << 3)

static long sc3(long n, long a0, long a1, long a2) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(n), "D"(a0), "S"(a1), "d"(a2)
                     : "rcx", "r11", "memory", "cc");
    return r;
}

static size_t slen(const char* s) { size_t n = 0; while (s[n]) n++; return n; }
static int seq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}
static int starts(const char* s, const char* p) {
    while (*p) { if (*s != *p) return 0; s++; p++; }
    return *s == '\0' || *s == ' ';
}
static void outn(const char* s, size_t n) { sc3(SYS_WRITE, 1, (long)(uintptr_t)s, (long)n); }
static void out(const char* s) { outn(s, slen(s)); }

static char cwd[256];

static void cmd_help(void) {
    out("builtins: help, echo <text>, pwd, cd <dir>, version\n");
}

static void print_cwd(void) {
    if (sc3(SYS_GETCWD, (long)(uintptr_t)cwd, sizeof cwd, 0) < 0) { cwd[0] = '/'; cwd[1] = 0; }
    out(cwd);
    out("\n");
}

/* Reads one line. The shell does its own echo (tty ECHO is off), so the
 * echoed text and the prompt are written in order. */
static long readline(char* buf, size_t cap) {
    size_t n = 0;
    for (;;) {
        char c = 0;
        long r = sc3(SYS_READ, 0, (long)(uintptr_t)&c, 1);
        if (r <= 0) {
            /* tty reads are non-blocking: no data yet is not EOF. Yield and retry. */
            sc3(SYS_SCHED_YIELD, 0, 0, 0);
            continue;
        }
        if (c == '\r' || c == '\n') { out("\n"); break; }
        if (c == 0x7f || c == '\b') {
            if (n > 0) { n--; out("\b \b"); }
            continue;
        }
        if (c < 0x20) continue;
        if (n + 1 < cap) {
            buf[n++] = c;
            outn(&c, 1);
        }
    }
    buf[n] = 0;
    return (long)n;
}

/* Turn off kernel echo and line buffering, so input arrives per key and is
 * echoed by this process only. */
static void tty_setup(void) {
    struct termios_k t;
    if (sc3(SYS_IOCTL, 0, (long)TIOC_GETTERMIOS, (long)(uintptr_t)&t) < 0) return;
    t.lflag &= ~(LFLAG_ECHO | LFLAG_ICANON);
    sc3(SYS_IOCTL, 0, (long)TIOC_SETTERMIOS, (long)(uintptr_t)&t);
}

/* Four-argument syscall. The kernel's wait4 reads status from rdx and
 * options from r10, so this helper is needed for that call. */
static long sc4(long n, long a0, long a1, long a2, long a3) {
    long r;
    register long r10 __asm__("r10") = a3;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(n), "D"(a0), "S"(a1), "d"(a2), "r"(r10)
                     : "rcx", "r11", "memory", "cc");
    return r;
}

#define SYS_FORK   91
#define SYS_EXECVE 48
#define SYS_WAIT4  43
#define MAX_ARGS   16

/* Runs /bin/<name> (or <name> when it contains '/') as a child process.
 * argv is split on spaces; the child execs with an empty environment. */
static void run_external(char* line) {
    char* argv[MAX_ARGS + 1];
    int argc = 0;
    char* p = line;
    /* A trailing '&' starts the command in the background (no wait). */
    size_t len = 0;
    while (line[len]) len++;
    while (len > 0 && line[len - 1] == ' ') line[--len] = 0;
    int background = 0;
    if (len > 0 && line[len - 1] == '&') {
        background = 1;
        line[--len] = 0;
        while (len > 0 && line[len - 1] == ' ') line[--len] = 0;
    }
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    argv[argc] = 0;
    if (argc == 0) return;

    const char* name = argv[0];
    int has_slash = 0;
    for (const char* q = name; *q; q++) if (*q == '/') has_slash = 1;

    char path[256];
    size_t k = 0;
    if (!has_slash) {
        const char* pre = "/bin/";
        while (*pre && k < sizeof path - 1) path[k++] = *pre++;
    }
    for (const char* q = name; *q && k < sizeof path - 1; q++) path[k++] = *q;
    path[k] = 0;

    long pid = sc3(SYS_FORK, 0, 0, 0);
    if (pid < 0) { out("sh: fork failed\n"); return; }
    if (pid == 0) {
        char* envp[1] = { 0 };
        sc3(SYS_EXECVE, (long)(uintptr_t)path, (long)(uintptr_t)argv, (long)(uintptr_t)envp);
        /* execve only returns on failure */
        out("sh: "); out(name); out(": command not found\n");
        sc3(SYS_EXIT, 127, 0, 0);
    }
    if (background) return;
    /* Wait for exactly this child (the kernel does not match wait4(-1)). */
    int status = 0;
    sc4(SYS_WAIT4, pid, 0, (long)(uintptr_t)&status, 0);
}

static void run(char* line) {
    while (*line == ' ') line++;
    if (*line == 0) return;

    if (seq(line, "help")) { cmd_help(); return; }
    if (seq(line, "version")) { out("BlockOS /bin/sh (minimal)\n"); return; }
    if (seq(line, "pwd")) { print_cwd(); return; }
    if (starts(line, "echo")) {
        char* p = line + 4;
        while (*p == ' ') p++;
        out(p);
        out("\n");
        return;
    }
    if (starts(line, "cd")) {
        char* p = line + 2;
        while (*p == ' ') p++;
        if (*p == 0) p = "/";
        if (sc3(SYS_CHDIR, (long)(uintptr_t)p, 0, 0) < 0) out("cd: no such directory\n");
        return;
    }
    run_external(line);
}

int main(int argc, char** argv, char** envp) {
    (void)argc; (void)argv; (void)envp;
    char line[256];
    cwd[0] = '/'; cwd[1] = 0;
    tty_setup();
    out("BlockOS shell. Type 'help'.\n");
    for (;;) {
        out("blockos$ ");
        readline(line, sizeof line);
        run(line);
    }
    return 0;
}
