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
    /* unknown: print the word */
    char* w = line;
    while (*w && *w != ' ') w++;
    *w = 0;
    out("sh: ");
    out(line);
    out(": command not found\n");
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
