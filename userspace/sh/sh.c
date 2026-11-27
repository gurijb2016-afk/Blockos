/* BlockOS /bin/sh: freestanding, static, uses the BlockOS syscall ABI
 * (int 0x80, rax=nr, rdi/rsi/rdx/r10 args). */
#include <stdint.h>
#include <stddef.h>

enum {
    SYS_READ          = 0,
    SYS_WRITE         = 1,
    SYS_OPENAT        = 2,
    SYS_CLOSE         = 3,
    SYS_CHDIR         = 19,
    SYS_GETCWD        = 20,
    SYS_STAT          = 23,
    SYS_WAIT4         = 43,
    SYS_EXIT          = 44,
    SYS_SCHED_YIELD   = 46,
    SYS_EXECVE        = 48,
    SYS_GETPID        = 52,
    SYS_IOCTL         = 58,
    SYS_GETDENTS64    = 62,
    SYS_UNLINK        = 70,
    SYS_MKDIR         = 71,
    SYS_RMDIR         = 72,
    SYS_FORK          = 91,
    SYS_UNAME         = 96,
    SYS_BLOCKOS_REBOOT= 108
};

#define AT_FDCWD   (-100L)
#define O_RDONLY   00u
#define O_WRONLY   01u
#define O_RDWR     02u
#define O_CREAT    0100u
#define O_TRUNC    01000u

/* tty termios layout and ioctl requests as defined by the kernel (tty.cpp). */
struct termios_k { uint32_t iflag, oflag, cflag, lflag; uint8_t cc[20]; };
#define TIOC_GETTERMIOS 0x424F5303UL
#define TIOC_SETTERMIOS 0x424F5304UL
#define LFLAG_ECHO   8u      /* Linux ECHO */
#define LFLAG_ICANON 2u      /* Linux ICANON */

static long sc3(long n, long a0, long a1, long a2) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(n), "D"(a0), "S"(a1), "d"(a2)
                     : "rcx", "r11", "memory", "cc");
    return r;
}

static long sc4(long n, long a0, long a1, long a2, long a3) {
    long r;
    register long r10 __asm__("r10") = a3;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(n), "D"(a0), "S"(a1), "d"(a2), "r"(r10)
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

static void out_u64(uint64_t v) {
    char tmp[24];
    int i = 0;
    if (v == 0) { out("0"); return; }
    while (v > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (i > 0) outn(&tmp[--i], 1);
}

static void out_oct(uint32_t v) {
    char tmp[16];
    int i = 0;
    if (v == 0) { out("0"); return; }
    while (v > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = (char)('0' + (v & 7u));
        v >>= 3;
    }
    out("0");
    while (i > 0) outn(&tmp[--i], 1);
}

static char cwd[256];

static void refresh_cwd(void) {
    if (sc3(SYS_GETCWD, (long)(uintptr_t)cwd, sizeof cwd, 0) < 0 || cwd[0] == 0) {
        cwd[0] = '/';
        cwd[1] = 0;
    }
}

static void cmd_help(void) {
    out("BlockOS /bin/sh built-in commands:\n");
    out("  help                 Show this command reference\n");
    out("  ls [dir]             List directory entries\n");
    out("  cat <file>           Print file contents\n");
    out("  cd [dir]             Change working directory\n");
    out("  pwd                  Print working directory\n");
    out("  echo <text>          Print text to console\n");
    out("  mkdir <dir>          Create a directory\n");
    out("  rmdir <dir>          Remove an empty directory\n");
    out("  touch <file>         Create an empty file\n");
    out("  write <file> <text>  Write text to a file\n");
    out("  rm <file>            Delete a file\n");
    out("  stat <path>          Show file mode and byte size\n");
    out("  uname [-a]           Print kernel name and architecture\n");
    out("  hw                   Show hardware detection checklist (/proc/drivers)\n");
    out("  free                 Show system & kernel heap memory (/proc/meminfo)\n");
    out("  cpu                  Show processor details (/proc/cpuinfo)\n");
    out("  pci                  List discovered PCI/PCIe devices (/proc/pci)\n");
    out("  clear                Clear the terminal screen\n");
    out("  reboot               Reboot the machine\n");
    out("  version              Show shell and OS version\n");
}

static void print_cwd(void) {
    refresh_cwd();
    out(cwd);
    out("\n");
}

static int dump_file(const char* path) {
    long fd = sc4(SYS_OPENAT, AT_FDCWD, (long)(uintptr_t)path, O_RDONLY, 0);
    if (fd < 0) return -1;
    char buf[512];
    for (;;) {
        long n = sc3(SYS_READ, fd, (long)(uintptr_t)buf, sizeof(buf));
        if (n <= 0) break;
        outn(buf, (size_t)n);
    }
    sc3(SYS_CLOSE, fd, 0, 0);
    return 0;
}

static void cmd_cat(char* arg) {
    while (*arg == ' ') arg++;
    if (!*arg) { out("usage: cat <file>\n"); return; }
    while (*arg) {
        while (*arg == ' ') arg++;
        if (!*arg) break;
        char* tok = arg;
        while (*arg && *arg != ' ') arg++;
        if (*arg) *arg++ = 0;
        if (dump_file(tok) < 0) {
            out("cat: "); out(tok); out(": No such file\n");
        }
    }
}

static void cmd_ls(char* arg) {
    while (*arg == ' ') arg++;
    /* Ignore leading flags like -l / -a if passed */
    while (*arg == '-') {
        while (*arg && *arg != ' ') arg++;
        while (*arg == ' ') arg++;
    }
    const char* target = (*arg) ? arg : cwd;
    long fd = sc4(SYS_OPENAT, AT_FDCWD, (long)(uintptr_t)target, O_RDONLY, 0);
    if (fd < 0) {
        out("ls: cannot access '"); out(target); out("': No such directory\n");
        return;
    }
    uint8_t buf[1024];
    int col = 0;
    for (;;) {
        long n = sc3(SYS_GETDENTS64, fd, (long)(uintptr_t)buf, sizeof(buf));
        if (n <= 0) break;
        long pos = 0;
        while (pos < n) {
            uint16_t reclen = *(uint16_t*)(buf + pos + 16);
            uint8_t dtype = buf[pos + 18];
            const char* name = (const char*)(buf + pos + 19);
            if (reclen == 0) break;
            if (dtype == 4) {
                out("\x1b[36m"); out(name); out("/\x1b[0m  ");
            } else {
                out(name); out("  ");
            }
            col++;
            if (col >= 6) { out("\n"); col = 0; }
            pos += reclen;
        }
    }
    if (col > 0) out("\n");
    sc3(SYS_CLOSE, fd, 0, 0);
}

static void cmd_mkdir(char* arg) {
    while (*arg == ' ') arg++;
    if (!*arg) { out("usage: mkdir <dir>\n"); return; }
    if (sc3(SYS_MKDIR, (long)(uintptr_t)arg, 0755, 0) < 0) {
        out("mkdir: cannot create directory '"); out(arg); out("'\n");
    }
}

static void cmd_rmdir(char* arg) {
    while (*arg == ' ') arg++;
    if (!*arg) { out("usage: rmdir <dir>\n"); return; }
    if (sc3(SYS_RMDIR, (long)(uintptr_t)arg, 0, 0) < 0) {
        out("rmdir: failed to remove '"); out(arg); out("'\n");
    }
}

static void cmd_rm(char* arg) {
    while (*arg == ' ') arg++;
    if (!*arg) { out("usage: rm <file>\n"); return; }
    if (sc3(SYS_UNLINK, (long)(uintptr_t)arg, 0, 0) < 0) {
        out("rm: cannot remove '"); out(arg); out("'\n");
    }
}

static void cmd_touch(char* arg) {
    while (*arg == ' ') arg++;
    if (!*arg) { out("usage: touch <file>\n"); return; }
    long fd = sc4(SYS_OPENAT, AT_FDCWD, (long)(uintptr_t)arg, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        out("touch: cannot touch '"); out(arg); out("'\n");
        return;
    }
    sc3(SYS_CLOSE, fd, 0, 0);
}

static void cmd_write(char* arg) {
    while (*arg == ' ') arg++;
    if (!*arg) { out("usage: write <file> <text>\n"); return; }
    char* file = arg;
    while (*arg && *arg != ' ') arg++;
    if (*arg) *arg++ = 0;
    while (*arg == ' ') arg++;
    long fd = sc4(SYS_OPENAT, AT_FDCWD, (long)(uintptr_t)file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        out("write: cannot open '"); out(file); out("'\n");
        return;
    }
    if (*arg) {
        sc3(SYS_WRITE, fd, (long)(uintptr_t)arg, (long)slen(arg));
        sc3(SYS_WRITE, fd, (long)(uintptr_t)"\n", 1);
    }
    sc3(SYS_CLOSE, fd, 0, 0);
}

static void cmd_stat(char* arg) {
    while (*arg == ' ') arg++;
    if (!*arg) { out("usage: stat <path>\n"); return; }
    uint64_t st[16];
    for (int i = 0; i < 16; ++i) st[i] = 0;
    if (sc3(SYS_STAT, (long)(uintptr_t)arg, (long)(uintptr_t)st, 0) < 0) {
        out("stat: cannot stat '"); out(arg); out("': No such file or directory\n");
        return;
    }
    uint32_t mode = (uint32_t)st[2];
    uint64_t size = st[7];
    out("  File: "); out(arg); out("\n");
    out("  Size: "); out_u64(size); out(" bytes\tType: ");
    if ((mode & 0170000u) == 0040000u) out("directory");
    else if ((mode & 0170000u) == 0140000u) out("device");
    else out("regular file");
    out("\tMode: "); out_oct(mode & 07777u); out("\n");
}

static void cmd_uname(char* arg) {
    while (*arg == ' ') arg++;
    char u[390];
    for (size_t i = 0; i < sizeof(u); ++i) u[i] = 0;
    if (sc3(SYS_UNAME, (long)(uintptr_t)u, 0, 0) < 0) {
        out("BlockOS x86_64\n");
        return;
    }
    if (*arg == '-' && arg[1] == 'a') {
        out(u + 0 * 65); out(" ");
        out(u + 1 * 65); out(" ");
        out(u + 2 * 65); out(" ");
        out(u + 3 * 65); out(" ");
        out(u + 4 * 65); out(" UEFI\n");
    } else {
        out(u + 0 * 65); out("\n");
    }
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
    size_t l = slen(line);
    while (l > 0 && line[l - 1] == ' ') line[--l] = 0;
    if (*line == 0) return;

    if (seq(line, "help")) { cmd_help(); return; }
    if (seq(line, "version")) { out("BlockOS /bin/sh v1.1 (x86_64 UEFI)\n"); return; }
    if (seq(line, "pwd")) { print_cwd(); return; }
    if (seq(line, "clear")) { out("\x1b[2J\x1b[H"); return; }
    if (seq(line, "hw")) { dump_file("/proc/drivers"); return; }
    if (seq(line, "free") || seq(line, "mem")) { dump_file("/proc/meminfo"); return; }
    if (seq(line, "cpu")) { dump_file("/proc/cpuinfo"); return; }
    if (seq(line, "pci")) { dump_file("/proc/pci"); return; }
    if (seq(line, "reboot")) {
        out("Rebooting BlockOS...\n");
        sc3(SYS_BLOCKOS_REBOOT, 0, 0, 0);
        return;
    }
    if (starts(line, "uname")) { cmd_uname(line + 5); return; }
    if (starts(line, "ls")) { cmd_ls(line + 2); return; }
    if (starts(line, "cat")) { cmd_cat(line + 3); return; }
    if (starts(line, "mkdir")) { cmd_mkdir(line + 5); return; }
    if (starts(line, "rmdir")) { cmd_rmdir(line + 5); return; }
    if (starts(line, "rm")) { cmd_rm(line + 2); return; }
    if (starts(line, "touch")) { cmd_touch(line + 5); return; }
    if (starts(line, "write")) { cmd_write(line + 5); return; }
    if (starts(line, "stat")) { cmd_stat(line + 4); return; }
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
        else refresh_cwd();
        return;
    }
    run_external(line);
}

int main(int argc, char** argv, char** envp) {
    (void)argc; (void)argv; (void)envp;
    char line[256];
    refresh_cwd();
    tty_setup();
    out("BlockOS shell. Type 'help'.\n");
    for (;;) {
        /* green host name, grey-blue working directory, then the usual "$ " */
        out("\x1b[?1049l");   /* leave the alternate screen if a full-screen program left it */
        out("\x1b[32mblockos\x1b[0m:\x1b[34m");
        out(cwd);
        out("\x1b[0m$ ");
        readline(line, sizeof line);
        run(line);
    }
    return 0;
}
