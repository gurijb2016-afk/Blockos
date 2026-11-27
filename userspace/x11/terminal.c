/*
 * BlockOS X11 Application 3: Shell / Terminal (/bin/x11_terminal)
 * Dark monospace X11 terminal emulator with scrollback ring buffer, blinking cursor,
 * Backspace/Enter handling, and live BlockOS command execution (ls, cat, pwd, cd,
 * uname, hw, free, cpu, pci, mkdir, touch, write, rm, clear, help).
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

#define WIN_W       560
#define WIN_H       320
#define TERM_COLS   68
#define TERM_ROWS   28
#define RING_LINES  128

static long sys_getdents64(int fd, void* buf, size_t count) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(62L), "D"((long)fd), "S"((long)(uintptr_t)buf), "d"((long)count)
                     : "rcx", "r11", "memory", "cc");
    return r;
}

static char g_lines[RING_LINES][TERM_COLS + 1];
static int  g_line_head = 0;
static char g_input[128];
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

static void term_dump_file(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { term_push_line("error: cannot open file"); return; }
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = '\0';
        term_print_text(buf);
    }
}

static void term_ls(const char* dir) {
    int fd = open(dir, O_RDONLY);
    if (fd < 0) { term_push_line("ls: cannot open directory"); return; }
    uint8_t buf[1024];
    char line[TERM_COLS + 1];
    line[0] = '\0';
    for (;;) {
        long n = sys_getdents64(fd, buf, sizeof(buf));
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

static void exec_command(char* cmd) {
    while (*cmd == ' ') ++cmd;
    if (!*cmd) return;

    if (strcmp(cmd, "help") == 0) {
        term_push_line("X11 Terminal commands:");
        term_push_line("  help  ls [dir]  cat <file>  pwd  cd <dir>  clear");
        term_push_line("  uname  hw  free  cpu  pci  mkdir <d>  rm <f>");
        return;
    }
    if (strcmp(cmd, "clear") == 0) {
        memset(g_lines, 0, sizeof(g_lines));
        g_line_head = 0;
        return;
    }
    if (strcmp(cmd, "pwd") == 0) {
        term_push_line(g_cwd);
        return;
    }
    if (strcmp(cmd, "uname") == 0 || strncmp(cmd, "uname ", 6) == 0) {
        term_push_line("BlockOS blockos 1.0 x86_64 UEFI (X11 :0)");
        return;
    }
    if (strcmp(cmd, "hw") == 0)   { term_dump_file("/proc/drivers"); return; }
    if (strcmp(cmd, "free") == 0) { term_dump_file("/proc/meminfo"); return; }
    if (strcmp(cmd, "cpu") == 0)  { term_dump_file("/proc/cpuinfo"); return; }
    if (strcmp(cmd, "pci") == 0)  { term_dump_file("/proc/pci");     return; }
    if (strcmp(cmd, "ls") == 0 || strncmp(cmd, "ls ", 3) == 0) {
        const char* arg = (cmd[2] == ' ') ? cmd + 3 : g_cwd;
        while (*arg == ' ') ++arg;
        term_ls(*arg ? arg : g_cwd);
        return;
    }
    if (strncmp(cmd, "cat ", 4) == 0) {
        const char* arg = cmd + 4;
        while (*arg == ' ') ++arg;
        term_dump_file(arg);
        return;
    }
    if (strncmp(cmd, "cd ", 3) == 0) {
        const char* arg = cmd + 3;
        while (*arg == ' ') ++arg;
        if (chdir(arg) == 0) {
            if (!getcwd(g_cwd, sizeof(g_cwd))) strcpy(g_cwd, "/");
        } else {
            term_push_line("cd: no such directory");
        }
        return;
    }
    if (strncmp(cmd, "mkdir ", 6) == 0) {
        const char* arg = cmd + 6;
        while (*arg == ' ') ++arg;
        if (mkdir(arg, 0755) < 0) term_push_line("mkdir: failed");
        return;
    }
    if (strncmp(cmd, "rm ", 3) == 0) {
        const char* arg = cmd + 3;
        while (*arg == ' ') ++arg;
        if (unlink(arg) < 0) term_push_line("rm: failed");
        return;
    }
    char msg[80];
    snprintf(msg, sizeof(msg), "term: unknown command '%s' (type 'help')", cmd);
    term_push_line(msg);
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

    /* Prompt + active input line */
    char prompt[160];
    snprintf(prompt, sizeof(prompt), "blockos:%s$ %s", g_cwd, g_input);
    int py = 8 + count * 11;
    x11_set_gc_colors(c, gc, 0x0058A6FFu, 0x000D1117u);
    x11_draw_text(c, win, gc, 8, (int16_t)py, prompt);

    if (cursor_on) {
        int cx = 8 + (int)strlen(prompt) * 8;
        if (cx + 8 < WIN_W) {
            x11_set_gc_colors(c, gc, 0x0058A6FFu, 0x0058A6FFu);
            x11_fill_rect(c, win, gc, (int16_t)cx, (int16_t)py, 7, 9);
        }
    }
}

int main(void) {
    X11Conn c;
    if (x11_connect(&c, NULL) < 0) return 1;

    term_push_line("BlockOS X11 Terminal - type 'help', 'hw', 'ls /proc', 'pci'");
    uint32_t win = x11_create_window(&c, 220, 340, WIN_W, WIN_H, 0x000D1117u,
                                     X_ExposureMask | X_KeyPressMask | X_ButtonPressMask);
    x11_set_title(&c, win, "Terminal");
    uint32_t gc = x11_create_gc(&c, win, 0x00C9D1D9u, 0x000D1117u);
    x11_map_window(&c, win);

    int cursor_on = 1;
    int ticks = 0;
    draw_term(&c, win, gc, cursor_on);

    for (;;) {
        X11Event ev;
        int r = x11_poll_event(&c, &ev);
        if (r < 0) break;
        if (r == 0) {
            x11_sleep_ms(20);
            if (++ticks >= 25) {
                ticks = 0;
                cursor_on = !cursor_on;
                draw_term(&c, win, gc, cursor_on);
            }
            continue;
        }

        if (ev.type == X_EV_DestroyNotify) break;
        if (ev.type == X_EV_Expose) {
            draw_term(&c, win, gc, cursor_on);
        } else if (ev.type == X_EV_KeyPress) {
            char ch = (char)ev.ascii;
            if (ch == '\b' || ch == 0x7F) {
                if (g_input_len > 0) g_input[--g_input_len] = '\0';
            } else if (ch == '\n' || ch == '\r') {
                char echo[160];
                snprintf(echo, sizeof(echo), "blockos:%s$ %s", g_cwd, g_input);
                term_push_line(echo);
                exec_command(g_input);
                g_input_len = 0;
                g_input[0] = '\0';
            } else if (ch >= 0x20 && ch <= 0x7E) {
                if (g_input_len < (int)sizeof(g_input) - 1) {
                    g_input[g_input_len++] = ch;
                    g_input[g_input_len] = '\0';
                }
            }
            cursor_on = 1;
            ticks = 0;
            draw_term(&c, win, gc, cursor_on);
        }
    }
    close(c.fd);
    return 0;
}
