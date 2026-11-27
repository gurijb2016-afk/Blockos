/*
 * BlockOS X11 Application 3: Shell / Terminal (/bin/x11_terminal)
 * Dark monospace X11 terminal emulator with scrollback ring buffer, blinking cursor,
 * Backspace/Enter handling, and full parity with BlockOS /bin/sh commands:
 *   help, ls, cat, cd, pwd, echo, mkdir, rmdir, touch, write, rm, stat,
 *   uname, hw, free/mem, cpu, pci, uptime, version, clear, reboot
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "x11_client.h"

#define WIN_W       620
#define WIN_H       360
#define TERM_COLS   75
#define TERM_ROWS   31
#define RING_LINES  256

static long sys_sc3(long nr, long a0, long a1, long a2) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(nr), "D"(a0), "S"(a1), "d"(a2)
                     : "rcx", "r11", "memory", "cc");
    return r;
}

static char g_lines[RING_LINES][TERM_COLS + 1];
static int  g_line_head = 0;
static char g_input[160];
static int  g_input_len = 0;
static char g_cwd[128] = "/";

static void term_push_line(const char* s) {
    int slot = g_line_head % RING_LINES;
    strncpy(g_lines[slot], s, TERM_COLS);
    g_lines[slot][TERM_COLS] = '\0';
    g_line_head++;
}

static void term_print_text(const char* txt) {
    char cur[TERM_COLS + 1];
    int c = 0;
    for (const char* p = txt; *p; ++p) {
        /* Skip ANSI SGR escape sequences */
        if (*p == 0x1B && p[1] == '[') {
            p += 2;
            while (*p && !(*p >= '@' && *p <= '~')) ++p;
            if (!*p) break;
            continue;
        }
        if (*p == '\t') {
            int next = (c + 4) & ~3;
            while (c < next && c < TERM_COLS) cur[c++] = ' ';
            continue;
        }
        if (*p == '\n' || c >= TERM_COLS) {
            cur[c] = '\0';
            term_push_line(cur);
            c = 0;
            if (*p == '\n') continue;
        }
        if (*p >= 0x20 && *p <= 0x7E) cur[c++] = *p;
    }
    if (c > 0) { cur[c] = '\0'; term_push_line(cur); }
}

static int starts_cmd(const char* s, const char* prefix) {
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0 && (s[n] == '\0' || s[n] == ' ');
}

static void term_dump_file(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "cat: %s: No such file", path);
        term_push_line(msg);
        return;
    }
    char buf[2048];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        term_print_text(buf);
    }
    close(fd);
}

static void term_ls(const char* arg) {
    while (*arg == ' ') ++arg;
    while (*arg == '-') {
        while (*arg && *arg != ' ') ++arg;
        while (*arg == ' ') ++arg;
    }
    const char* dir = (*arg) ? arg : g_cwd;
    int fd = open(dir, O_RDONLY);
    if (fd < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "ls: cannot access '%s': No such directory", dir);
        term_push_line(msg);
        return;
    }
    uint8_t buf[1024];
    char line[TERM_COLS + 1];
    line[0] = '\0';
    for (;;) {
        long n = sys_sc3(62L /* SYS_getdents64 */, (long)fd, (long)(uintptr_t)buf, (long)sizeof(buf));
        if (n <= 0) break;
        long pos = 0;
        while (pos < n) {
            uint16_t reclen = *(uint16_t*)(buf + pos + 16);
            uint8_t dtype = buf[pos + 18];
            const char* name = (const char*)(buf + pos + 19);
            if (reclen == 0) break;
            char item[48];
            snprintf(item, sizeof(item), dtype == 4 ? "%s/  " : "%s  ", name);
            if (strlen(line) + strlen(item) >= TERM_COLS) {
                term_push_line(line);
                line[0] = '\0';
            }
            strcat(line, item);
            pos += reclen;
        }
    }
    if (line[0]) term_push_line(line);
    close(fd);
}

static void term_stat(const char* arg) {
    while (*arg == ' ') ++arg;
    if (!*arg) { term_push_line("usage: stat <path>"); return; }
    uint64_t st[16];
    memset(st, 0, sizeof(st));
    if (sys_sc3(23L /* SYS_stat */, (long)(uintptr_t)arg, (long)(uintptr_t)st, 0) < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "stat: cannot stat '%s': No such file or directory", arg);
        term_push_line(msg);
        return;
    }
    uint32_t mode = (uint32_t)st[2];
    uint64_t size = st[7];
    const char* kind = "regular file";
    if ((mode & 0170000u) == 0040000u) kind = "directory";
    else if ((mode & 0170000u) == 0140000u) kind = "device";
    char l1[128], l2[128];
    snprintf(l1, sizeof(l1), "  File: %s", arg);
    snprintf(l2, sizeof(l2), "  Size: %llu bytes  Type: %s  Mode: 0%o",
             (unsigned long long)size, kind, (unsigned)(mode & 07777u));
    term_push_line(l1);
    term_push_line(l2);
}

static void term_uname(const char* arg) {
    while (*arg == ' ') ++arg;
    char u[390];
    memset(u, 0, sizeof(u));
    if (sys_sc3(96L /* SYS_uname */, (long)(uintptr_t)u, 0, 0) < 0) {
        term_push_line("BlockOS x86_64");
        return;
    }
    if (*arg == '-' && arg[1] == 'a') {
        char out[160];
        snprintf(out, sizeof(out), "%s %s %s %s %s UEFI",
                 u + 0 * 65, u + 1 * 65, u + 2 * 65, u + 3 * 65, u + 4 * 65);
        term_push_line(out);
    } else {
        term_push_line(u);
    }
}

static void extract_proc_field(const char* buf, const char* key, char* out, size_t cap) {
    const char* p = strstr(buf, key);
    if (!p) return;
    p += strlen(key);
    while (*p == ' ' || *p == '\t' || *p == ':') ++p;
    size_t i = 0;
    while (p[i] && p[i] != '\n' && p[i] != '\r' && i + 1 < cap) {
        out[i] = p[i];
        ++i;
    }
    out[i] = '\0';
}

static void term_sysinfo(X11Conn* c) {
    char cpu_model[48] = "x86_64 Processor";
    char mem_total[32] = "Unknown";
    char mem_free[32]  = "Unknown";
    char os_ver[48]    = "BlockOS 1.0 x86_64";
    int pci_count = 0;
    char buf[2048];
    int fd;
    ssize_t n;

    fd = open("/proc/version", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char* nl = strchr(buf, '\n');
            if (nl) *nl = '\0';
            strncpy(os_ver, buf, sizeof(os_ver) - 1);
        }
    }
    fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            extract_proc_field(buf, "model name", cpu_model, sizeof(cpu_model));
        }
    }
    fd = open("/proc/meminfo", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            extract_proc_field(buf, "MemTotal:", mem_total, sizeof(mem_total));
            extract_proc_field(buf, "MemFree:", mem_free, sizeof(mem_free));
        }
    }
    fd = open("/proc/pci", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            int lines = 0;
            for (ssize_t i = 0; i < n; ++i) if (buf[i] == '\n') ++lines;
            pci_count = lines > 1 ? lines - 1 : 0;
        }
    }

    struct timespec ts;
    uint32_t sec = (clock_gettime(0, &ts) == 0) ? (uint32_t)ts.tv_sec : 0;
    char line[96];

    term_push_line("  +-------+   BlockOS Live System Information");
    snprintf(line, sizeof(line), "  | [] [] |   OS:      %s", os_ver);
    term_push_line(line);
    snprintf(line, sizeof(line), "  | []=== |   Kernel:  BlockOS UEFI x86_64");
    term_push_line(line);
    snprintf(line, sizeof(line), "  | [] [] |   CPU:     %s", cpu_model);
    term_push_line(line);
    snprintf(line, sizeof(line), "  +-------+   Memory:  %s total (%s free)", mem_total, mem_free);
    term_push_line(line);
    snprintf(line, sizeof(line), "   BLOCKOS    Display: %ux%u (32bpp X11 :0)",
             c ? c->screen_w : 0, c ? c->screen_h : 0);
    term_push_line(line);
    snprintf(line, sizeof(line), "              PCI Bus: %d devices | Uptime: %um %02us",
             pci_count, sec / 60u, sec % 60u);
    term_push_line(line);
    term_push_line("");
}

static X11Conn* g_conn = NULL;

static void exec_command(char* cmd) {
    while (*cmd == ' ') ++cmd;
    size_t len = strlen(cmd);
    while (len > 0 && cmd[len - 1] == ' ') cmd[--len] = '\0';
    if (!*cmd) return;

    if (strcmp(cmd, "help") == 0) {
        term_push_line("BlockOS X11 Terminal commands (same as /bin/sh):");
        term_push_line("  help                 Show this command reference");
        term_push_line("  ls [dir]             List directory entries");
        term_push_line("  cat <file>           Print file contents");
        term_push_line("  cd [dir]             Change working directory");
        term_push_line("  pwd                  Print working directory");
        term_push_line("  echo <text>          Print text");
        term_push_line("  mkdir <dir>          Create a directory");
        term_push_line("  rmdir <dir>          Remove an empty directory");
        term_push_line("  touch <file>         Create an empty file");
        term_push_line("  write <file> <text>  Write text to a file");
        term_push_line("  rm <file>            Delete a file");
        term_push_line("  stat <path>          Show file mode and byte size");
        term_push_line("  uname [-a]           Print kernel name and architecture");
        term_push_line("  hw                   Show hardware checklist (/proc/drivers)");
        term_push_line("  free | mem           Show memory stats (/proc/meminfo)");
        term_push_line("  cpu                  Show CPU details (/proc/cpuinfo)");
        term_push_line("  pci                  List PCI/PCIe devices (/proc/pci)");
        term_push_line("  sysinfo | neofetch   Show BlockOS logo and live hardware info");
        term_push_line("  uptime               Show kernel uptime (/proc/uptime)");
        term_push_line("  version              Show OS and terminal version");
        term_push_line("  clear                Clear terminal screen");
        term_push_line("  reboot               Reboot the machine");
        return;
    }
    if (strcmp(cmd, "clear") == 0) {
        memset(g_lines, 0, sizeof(g_lines));
        g_line_head = 0;
        return;
    }
    if (strcmp(cmd, "version") == 0) {
        term_push_line("BlockOS /bin/x11_terminal v1.1 (x86_64 UEFI X11 :0)");
        return;
    }
    if (strcmp(cmd, "pwd") == 0) {
        term_push_line(g_cwd);
        return;
    }
    if (strcmp(cmd, "hw") == 0)     { term_dump_file("/proc/drivers"); return; }
    if (strcmp(cmd, "free") == 0 || strcmp(cmd, "mem") == 0) { term_dump_file("/proc/meminfo"); return; }
    if (strcmp(cmd, "cpu") == 0)    { term_dump_file("/proc/cpuinfo"); return; }
    if (strcmp(cmd, "pci") == 0)    { term_dump_file("/proc/pci");     return; }
    if (strcmp(cmd, "sysinfo") == 0 || strcmp(cmd, "neofetch") == 0 || strcmp(cmd, "fetch") == 0) {
        term_sysinfo(g_conn);
        return;
    }
    if (strcmp(cmd, "uptime") == 0) { term_dump_file("/proc/uptime");  return; }
    if (strcmp(cmd, "reboot") == 0) {
        term_push_line("Rebooting BlockOS...");
        sys_sc3(108L /* SYS_blockos_reboot */, 0, 0, 0);
        return;
    }
    if (starts_cmd(cmd, "uname")) { term_uname(cmd + 5); return; }
    if (starts_cmd(cmd, "ls"))    { term_ls(cmd + 2);    return; }
    if (starts_cmd(cmd, "stat"))  { term_stat(cmd + 4);  return; }
    if (starts_cmd(cmd, "echo")) {
        const char* p = cmd + 4;
        while (*p == ' ') ++p;
        term_push_line(p);
        return;
    }
    if (starts_cmd(cmd, "cat")) {
        char* p = cmd + 3;
        while (*p == ' ') ++p;
        if (!*p) { term_push_line("usage: cat <file>"); return; }
        while (*p) {
            while (*p == ' ') ++p;
            if (!*p) break;
            char* tok = p;
            while (*p && *p != ' ') ++p;
            if (*p) *p++ = '\0';
            term_dump_file(tok);
        }
        return;
    }
    if (starts_cmd(cmd, "cd")) {
        const char* arg = cmd + 2;
        while (*arg == ' ') ++arg;
        if (!*arg) arg = "/";
        if (chdir(arg) == 0) {
            if (!getcwd(g_cwd, sizeof(g_cwd))) strcpy(g_cwd, "/");
        } else {
            term_push_line("cd: no such directory");
        }
        return;
    }
    if (starts_cmd(cmd, "mkdir")) {
        const char* arg = cmd + 5;
        while (*arg == ' ') ++arg;
        if (!*arg) { term_push_line("usage: mkdir <dir>"); return; }
        if (mkdir(arg, 0755) < 0) term_push_line("mkdir: cannot create directory");
        return;
    }
    if (starts_cmd(cmd, "rmdir")) {
        const char* arg = cmd + 5;
        while (*arg == ' ') ++arg;
        if (!*arg) { term_push_line("usage: rmdir <dir>"); return; }
        if (sys_sc3(72L /* SYS_rmdir */, (long)(uintptr_t)arg, 0, 0) < 0)
            term_push_line("rmdir: failed to remove directory");
        return;
    }
    if (starts_cmd(cmd, "touch")) {
        const char* arg = cmd + 5;
        while (*arg == ' ') ++arg;
        if (!*arg) { term_push_line("usage: touch <file>"); return; }
        int fd = open(arg, O_CREAT | O_WRONLY, 0644);
        if (fd < 0) term_push_line("touch: cannot create file");
        else close(fd);
        return;
    }
    if (starts_cmd(cmd, "write")) {
        char* p = cmd + 5;
        while (*p == ' ') ++p;
        if (!*p) { term_push_line("usage: write <file> <text>"); return; }
        char* file = p;
        while (*p && *p != ' ') ++p;
        if (*p) *p++ = '\0';
        while (*p == ' ') ++p;
        int fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) { term_push_line("write: cannot open file"); return; }
        if (*p) {
            (void)write(fd, p, strlen(p));
            (void)write(fd, "\n", 1);
        }
        close(fd);
        return;
    }
    if (starts_cmd(cmd, "rm")) {
        const char* arg = cmd + 2;
        while (*arg == ' ') ++arg;
        if (!*arg) { term_push_line("usage: rm <file>"); return; }
        if (unlink(arg) < 0) term_push_line("rm: cannot remove file");
        return;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "term: unknown command '%s' (type 'help')", cmd);
    term_push_line(msg);
}

static void draw_prompt_line(X11Conn* c, uint32_t win, uint32_t gc, int cursor_on) {
    int max_hist = TERM_ROWS - 1;
    int count = g_line_head < max_hist ? g_line_head : max_hist;
    int py = 8 + count * 11;

    x11_set_gc_colors(c, gc, 0x000D1117u, 0x000D1117u);
    x11_fill_rect(c, win, gc, 0, (int16_t)py, WIN_W, 11);

    char prompt[192];
    snprintf(prompt, sizeof(prompt), "blockos:%s$ %s", g_cwd, g_input);
    x11_set_gc_colors(c, gc, 0x0058A6FFu, 0x000D1117u);
    x11_draw_text(c, win, gc, 8, (int16_t)py, prompt);

    if (cursor_on) {
        int cx = 8 + (int)strlen(prompt) * 8;
        if (cx + 8 < WIN_W) {
            x11_set_gc_colors(c, gc, 0x0058A6FFu, 0x0058A6FFu);
            x11_fill_rect(c, win, gc, (int16_t)cx, (int16_t)py, 7, 9);
        }
    }
    x11_flush(c);
}

static void draw_term(X11Conn* c, uint32_t win, uint32_t gc, int cursor_on) {
    /* Dark terminal canvas */
    x11_set_gc_colors(c, gc, 0x000D1117u, 0x000D1117u);
    x11_fill_rect(c, win, gc, 0, 0, WIN_W, WIN_H);

    int max_hist = TERM_ROWS - 1;
    int count = g_line_head < max_hist ? g_line_head : max_hist;
    int first = g_line_head - count;

    x11_set_gc_colors(c, gc, 0x00C9D1D9u, 0x000D1117u);
    for (int i = 0; i < count; ++i) {
        int slot = (first + i) % RING_LINES;
        x11_draw_text(c, win, gc, 8, (int16_t)(8 + i * 11), g_lines[slot]);
    }

    draw_prompt_line(c, win, gc, cursor_on);
}

int main(void) {
    X11Conn c;
    if (x11_connect(&c, NULL) < 0) return 1;
    g_conn = &c;

    term_sysinfo(&c);
    term_push_line("Type 'help' for all commands, or 'sysinfo' to refresh hardware info.");
    uint32_t win = x11_create_window(&c, 220, 340, WIN_W, WIN_H, 0x000D1117u,
                                     X_ExposureMask | X_KeyPressMask | X_ButtonPressMask);
    x11_set_title(&c, win, "Terminal");
    uint32_t gc = x11_create_gc(&c, win, 0x00C9D1D9u, 0x000D1117u);
    x11_map_window(&c, win);

    int cursor_on = 1;
    int ticks = 0;
    draw_term(&c, win, gc, cursor_on);

    for (;;) {
        int need_full = 0;
        int need_prompt = 0;
        int closed = 0;
        int got_any = 0;

        for (;;) {
            X11Event ev;
            int r = x11_poll_event(&c, &ev);
            if (r < 0) { closed = 1; break; }
            if (r == 0) break;
            got_any = 1;

            if (ev.type == X_EV_DestroyNotify) { closed = 1; break; }
            if (ev.type == X_EV_Expose) {
                need_full = 1;
            } else if (ev.type == X_EV_KeyPress) {
                char ch = (char)ev.ascii;
                if (ch == '\b' || ch == 0x7F) {
                    if (g_input_len > 0) g_input[--g_input_len] = '\0';
                    need_prompt = 1;
                } else if (ch == '\n' || ch == '\r') {
                    char echo[192];
                    snprintf(echo, sizeof(echo), "blockos:%s$ %s", g_cwd, g_input);
                    term_push_line(echo);
                    exec_command(g_input);
                    g_input_len = 0;
                    g_input[0] = '\0';
                    need_full = 1;
                } else if (ch >= 0x20 && ch <= 0x7E) {
                    if (g_input_len < (int)sizeof(g_input) - 1) {
                        g_input[g_input_len++] = ch;
                        g_input[g_input_len] = '\0';
                    }
                    need_prompt = 1;
                }
                cursor_on = 1;
                ticks = 0;
            }
        }
        if (closed) break;

        if (need_full) {
            draw_term(&c, win, gc, cursor_on);
        } else if (need_prompt) {
            draw_prompt_line(&c, win, gc, cursor_on);
        }

        if (got_any) {
            x11_yield();
        } else {
            x11_sleep_ms(4);
            if (++ticks >= 125) {
                ticks = 0;
                cursor_on = !cursor_on;
                draw_prompt_line(&c, win, gc, cursor_on);
            }
        }
    }
    close(c.fd);
    return 0;
}
