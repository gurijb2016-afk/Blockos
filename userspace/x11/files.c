/*
 * BlockOS X11 Application 2: Files (/bin/x11_files)
 * Connects to BlockOS VFS via POSIX openat/getdents64/read syscalls.
 * Displays current path, clickable Up (..) button, scrollable folder/file list,
 * and an inline read-only text preview pane when clicking a file.
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x11_client.h"

#define WIN_W 420
#define WIN_H 340
#define MAX_ENTRIES 64
#define VISIBLE_ROWS 12

/* Direct SYS_getdents64 (syscall 62) */
static long sys_getdents64(int fd, void* buf, size_t count) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(62L), "D"((long)fd), "S"((long)(uintptr_t)buf), "d"((long)count)
                     : "rcx", "r11", "memory", "cc");
    return r;
}

typedef struct {
    char name[64];
    int  is_dir;
} DirEntry;

static char     g_cwd[256] = "/";
static DirEntry g_entries[MAX_ENTRIES];
static int      g_entry_count = 0;
static int      g_scroll = 0;
static char     g_preview_title[64] = "(Click a file to preview)";
static char     g_preview_lines[6][50];
static int      g_preview_count = 0;

static void load_directory(const char* path) {
    strncpy(g_cwd, path, sizeof(g_cwd) - 1);
    g_cwd[sizeof(g_cwd) - 1] = '\0';
    g_entry_count = 0;
    g_scroll = 0;

    int fd = open(g_cwd, O_RDONLY);
    if (fd < 0) return;

    uint8_t buf[1024];
    for (;;) {
        long n = sys_getdents64(fd, buf, sizeof(buf));
        if (n <= 0) break;
        long pos = 0;
        while (pos < n && g_entry_count < MAX_ENTRIES) {
            uint16_t reclen = *(uint16_t*)(buf + pos + 16);
            uint8_t dtype = buf[pos + 18];
            const char* name = (const char*)(buf + pos + 19);
            if (reclen == 0) break;
            strncpy(g_entries[g_entry_count].name, name, 63);
            g_entries[g_entry_count].name[63] = '\0';
            g_entries[g_entry_count].is_dir = (dtype == 4) ? 1 : 0;
            g_entry_count++;
            pos += reclen;
        }
    }
    close(fd);
}

static void load_preview(const char* full_path, const char* name) {
    strncpy(g_preview_title, name, sizeof(g_preview_title) - 1);
    g_preview_title[sizeof(g_preview_title) - 1] = '\0';
    g_preview_count = 0;
    memset(g_preview_lines, 0, sizeof(g_preview_lines));

    int fd = open(full_path, O_RDONLY);
    if (fd < 0) {
        strcpy(g_preview_lines[0], "(Cannot open file)");
        g_preview_count = 1;
        return;
    }
    char raw[320];
    ssize_t n = read(fd, raw, sizeof(raw) - 1);
    close(fd);
    if (n <= 0) {
        strcpy(g_preview_lines[0], "(Empty or binary/device node)");
        g_preview_count = 1;
        return;
    }
    raw[n] = '\0';

    int row = 0, col = 0;
    for (ssize_t i = 0; i < n && row < 6; ++i) {
        unsigned char ch = (unsigned char)raw[i];
        if (ch == '\n') {
            g_preview_lines[row][col] = '\0';
            row++; col = 0;
            continue;
        }
        if (ch < 0x20 || ch > 0x7E) ch = '.';
        if (col < 48) g_preview_lines[row][col++] = (char)ch;
    }
    if (col > 0 && row < 6) {
        g_preview_lines[row][col] = '\0';
        row++;
    }
    g_preview_count = row;
}

static void go_up(void) {
    if (strcmp(g_cwd, "/") == 0) return;
    size_t len = strlen(g_cwd);
    while (len > 1 && g_cwd[len - 1] == '/') g_cwd[--len] = '\0';
    char* slash = strrchr(g_cwd, '/');
    if (!slash || slash == g_cwd) load_directory("/");
    else { *slash = '\0'; load_directory(g_cwd); }
}

static void draw_ui(X11Conn* c, uint32_t win, uint32_t gc) {
    x11_set_gc_colors(c, gc, 0x001E242Cu, 0x001E242Cu);
    x11_fill_rect(c, win, gc, 0, 0, WIN_W, WIN_H);

    /* Top bar: [Up ..] button + Path + [PgUp]/[PgDn] */
    x11_set_gc_colors(c, gc, 0x002B6CB0u, 0x002B6CB0u);
    x11_fill_rect(c, win, gc, 8, 8, 56, 22);
    x11_set_gc_colors(c, gc, 0x00FFFFFFu, 0x002B6CB0u);
    x11_draw_text(c, win, gc, 14, 15, "Up ..");

    x11_set_gc_colors(c, gc, 0x00151A20u, 0x00151A20u);
    x11_fill_rect(c, win, gc, 70, 8, 250, 22);
    x11_set_gc_colors(c, gc, 0x00E2E8F0u, 0x00151A20u);
    char pdisp[32];
    strncpy(pdisp, g_cwd, 30); pdisp[30] = '\0';
    x11_draw_text(c, win, gc, 76, 15, pdisp);

    /* Scroll buttons */
    x11_set_gc_colors(c, gc, 0x002D3748u, 0x002D3748u);
    x11_fill_rect(c, win, gc, 326, 8, 40, 22);
    x11_fill_rect(c, win, gc, 372, 8, 40, 22);
    x11_set_gc_colors(c, gc, 0x00FFFFFFu, 0x002D3748u);
    x11_draw_text(c, win, gc, 338, 15, "/\\");
    x11_draw_text(c, win, gc, 384, 15, "\\/");

    /* Directory listing box */
    x11_set_gc_colors(c, gc, 0x00151A20u, 0x00151A20u);
    x11_fill_rect(c, win, gc, 8, 38, WIN_W - 16, VISIBLE_ROWS * 16 + 4);

    for (int i = 0; i < VISIBLE_ROWS; ++i) {
        int idx = g_scroll + i;
        if (idx >= g_entry_count) break;
        int ry = 42 + i * 16;
        char row_txt[52];
        if (g_entries[idx].is_dir) {
            snprintf(row_txt, sizeof(row_txt), "[DIR]  %s/", g_entries[idx].name);
            x11_set_gc_colors(c, gc, 0x0063B3EDu, 0x00151A20u);
        } else {
            snprintf(row_txt, sizeof(row_txt), "[FILE] %s", g_entries[idx].name);
            x11_set_gc_colors(c, gc, 0x00E2E8F0u, 0x00151A20u);
        }
        x11_draw_text(c, win, gc, 14, (int16_t)ry, row_txt);
    }

    /* Inline read-only preview pane */
    x11_set_gc_colors(c, gc, 0x0011161Cu, 0x0011161Cu);
    x11_fill_rect(c, win, gc, 8, 240, WIN_W - 16, 92);

    x11_set_gc_colors(c, gc, 0x0068D391u, 0x0011161Cu);
    char hdr[52];
    snprintf(hdr, sizeof(hdr), "Preview: %s", g_preview_title);
    x11_draw_text(c, win, gc, 14, 246, hdr);

    x11_set_gc_colors(c, gc, 0x00CBD5E0u, 0x0011161Cu);
    for (int r = 0; r < g_preview_count && r < 6; ++r) {
        x11_draw_text(c, win, gc, 14, (int16_t)(260 + r * 11), g_preview_lines[r]);
    }
}

int main(void) {
    X11Conn c;
    if (x11_connect(&c, NULL) < 0) return 1;

    load_directory("/proc");
    load_preview("/proc/version", "version");

    uint32_t win = x11_create_window(&c, 412, 42, WIN_W, WIN_H, 0x001E242Cu,
                                     X_ExposureMask | X_ButtonPressMask);
    x11_set_title(&c, win, "Files (VFS Browser)");
    uint32_t gc = x11_create_gc(&c, win, 0x00FFFFFFu, 0x001E242Cu);
    x11_map_window(&c, win);
    draw_ui(&c, win, gc);

    for (;;) {
        X11Event ev;
        int r = x11_poll_event(&c, &ev);
        if (r < 0) break;
        if (r == 0) { x11_sleep_ms(20); continue; }

        if (ev.type == X_EV_DestroyNotify) break;
        if (ev.type == X_EV_Expose) {
            draw_ui(&c, win, gc);
        } else if (ev.type == X_EV_ButtonPress) {
            int x = ev.event_x, y = ev.event_y;
            if (y >= 8 && y < 30) {
                if (x >= 8 && x < 64) { go_up(); draw_ui(&c, win, gc); }
                else if (x >= 326 && x < 366 && g_scroll > 0) { g_scroll -= 4; if (g_scroll < 0) g_scroll = 0; draw_ui(&c, win, gc); }
                else if (x >= 372 && x < 412 && g_scroll + VISIBLE_ROWS < g_entry_count) { g_scroll += 4; draw_ui(&c, win, gc); }
            } else if (y >= 40 && y < 40 + VISIBLE_ROWS * 16) {
                int row = (y - 40) / 16;
                int idx = g_scroll + row;
                if (idx >= 0 && idx < g_entry_count) {
                    char next[256];
                    if (strcmp(g_cwd, "/") == 0)
                        snprintf(next, sizeof(next), "/%s", g_entries[idx].name);
                    else
                        snprintf(next, sizeof(next), "%s/%s", g_cwd, g_entries[idx].name);
                    if (g_entries[idx].is_dir) load_directory(next);
                    else load_preview(next, g_entries[idx].name);
                    draw_ui(&c, win, gc);
                }
            }
        }
    }
    close(c.fd);
    return 0;
}
