/*
 * BlockOS X11 Application 1: Settings (/bin/x11_settings)
 * Displays framebuffer resolution, memory/uptime stats from /proc,
 * and interactive desktop theme & mouse speed controls persisted to /etc/blockos.settings.
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

#define WIN_W 360
#define WIN_H 270

typedef struct {
    const char* name;
    uint32_t bg;
    uint32_t accent;
} Theme;

static const Theme kThemes[3] = {
    { "Ocean Blue",  0x001B2A38u, 0x002B6CB0u },
    { "Slate Dark",  0x00181A20u, 0x004C566Au },
    { "Forest Mint", 0x00142822u, 0x002F855Au }
};

static int g_theme = 0;
static int g_speed = 1; /* 1, 2, 3 */
static char g_mem_total[64] = "Unknown";
static char g_mem_free[64]  = "Unknown";

static void read_meminfo(void) {
    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd < 0) return;
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';

    const char* p = strstr(buf, "MemTotal:");
    if (p) {
        p += 9;
        while (*p == ' ') ++p;
        size_t i = 0;
        while (p[i] && p[i] != '\n' && i < sizeof(g_mem_total) - 1) { g_mem_total[i] = p[i]; ++i; }
        g_mem_total[i] = '\0';
    }
    p = strstr(buf, "MemFree:");
    if (p) {
        p += 8;
        while (*p == ' ') ++p;
        size_t i = 0;
        while (p[i] && p[i] != '\n' && i < sizeof(g_mem_free) - 1) { g_mem_free[i] = p[i]; ++i; }
        g_mem_free[i] = '\0';
    }
}

static void save_settings(void) {
    mkdir("/etc", 0755);
    int fd = open("/etc/blockos.settings", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) return;
    char out[256];
    int len = snprintf(out, sizeof(out),
        "CONFIG_GUI_X11=ON\n"
        "DESKTOP_THEME=%d\n"
        "MOUSE_SPEED=%d\n",
        g_theme, g_speed);
    if (len > 0) (void)write(fd, out, (size_t)len);
    close(fd);
}

static void load_settings(void) {
    int fd = open("/etc/blockos.settings", O_RDONLY);
    if (fd < 0) return;
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';
    const char* p = strstr(buf, "DESKTOP_THEME=");
    if (p) {
        int v = p[14] - '0';
        if (v >= 0 && v < 3) g_theme = v;
    }
    p = strstr(buf, "MOUSE_SPEED=");
    if (p) {
        int v = p[12] - '0';
        if (v >= 1 && v <= 3) g_speed = v;
    }
}

static void draw_ui(X11Conn* c, uint32_t win, uint32_t gc) {
    /* Window background */
    x11_set_gc_colors(c, gc, 0x001E242Cu, 0x001E242Cu);
    x11_fill_rect(c, win, gc, 0, 0, WIN_W, WIN_H);

    /* Header */
    x11_set_gc_colors(c, gc, 0x0063B3EDu, 0x001E242Cu);
    x11_draw_text(c, win, gc, 12, 12, "BlockOS System Settings");

    /* System Info box */
    x11_set_gc_colors(c, gc, 0x00151A20u, 0x00151A20u);
    x11_fill_rect(c, win, gc, 12, 30, WIN_W - 24, 68);

    char line[64];
    x11_set_gc_colors(c, gc, 0x00E2E8F0u, 0x00151A20u);
    snprintf(line, sizeof(line), "Display: %ux%u (32bpp X11)", c->screen_w, c->screen_h);
    x11_draw_text(c, win, gc, 20, 40, line);

    snprintf(line, sizeof(line), "MemTotal: %s", g_mem_total);
    x11_draw_text(c, win, gc, 20, 56, line);

    struct timespec ts;
    uint32_t sec = (clock_gettime(0, &ts) == 0) ? (uint32_t)ts.tv_sec : 0;
    snprintf(line, sizeof(line), "MemFree:  %s | Up %us", g_mem_free, sec);
    x11_draw_text(c, win, gc, 20, 72, line);

    /* Desktop Theme Section */
    x11_set_gc_colors(c, gc, 0x00A0AEC0u, 0x001E242Cu);
    x11_draw_text(c, win, gc, 12, 110, "Desktop Theme:");

    for (int i = 0; i < 3; ++i) {
        int bx = 12 + i * 112;
        uint32_t btn_bg = (i == g_theme) ? kThemes[i].accent : 0x002D3748u;
        x11_set_gc_colors(c, gc, btn_bg, btn_bg);
        x11_fill_rect(c, win, gc, (int16_t)bx, 126, 104, 28);
        x11_set_gc_colors(c, gc, 0x00FFFFFFu, btn_bg);
        x11_draw_text(c, win, gc, (int16_t)(bx + 8), 136, kThemes[i].name);
    }

    /* Mouse Cursor Speed Section */
    x11_set_gc_colors(c, gc, 0x00A0AEC0u, 0x001E242Cu);
    x11_draw_text(c, win, gc, 12, 170, "Mouse Cursor Speed:");

    const char* spd_labels[3] = { "1x Slow", "2x Medium", "3x Fast" };
    for (int i = 0; i < 3; ++i) {
        int bx = 12 + i * 112;
        uint32_t btn_bg = ((i + 1) == g_speed) ? kThemes[g_theme].accent : 0x002D3748u;
        x11_set_gc_colors(c, gc, btn_bg, btn_bg);
        x11_fill_rect(c, win, gc, (int16_t)bx, 186, 104, 28);
        x11_set_gc_colors(c, gc, 0x00FFFFFFu, btn_bg);
        x11_draw_text(c, win, gc, (int16_t)(bx + 14), 196, spd_labels[i]);
    }

    /* Persistence notice */
    x11_set_gc_colors(c, gc, 0x00718096u, 0x001E242Cu);
    x11_draw_text(c, win, gc, 12, 232, "Saved to /etc/blockos.settings");
}

int main(void) {
    X11Conn c;
    if (x11_connect(&c, NULL) < 0) return 1;

    load_settings();
    read_meminfo();

    uint32_t win = x11_create_window(&c, 28, 42, WIN_W, WIN_H, 0x001E242Cu,
                                     X_ExposureMask | X_ButtonPressMask);
    x11_set_title(&c, win, "Settings");
    uint32_t gc = x11_create_gc(&c, win, 0x00FFFFFFu, 0x001E242Cu);
    x11_map_window(&c, win);

    /* Apply persisted settings on startup */
    x11_send_desktop_cfg(&c, kThemes[g_theme].bg, kThemes[g_theme].accent, (uint32_t)g_speed);
    draw_ui(&c, win, gc);

    for (;;) {
        X11Event ev;
        int r = x11_poll_event(&c, &ev);
        if (r < 0) break;
        if (r == 0) { x11_sleep_ms(20); continue; }

        if (ev.type == X_EV_DestroyNotify) break;
        if (ev.type == X_EV_Expose) {
            read_meminfo();
            draw_ui(&c, win, gc);
        } else if (ev.type == X_EV_ButtonPress) {
            int x = ev.event_x, y = ev.event_y;
            int changed = 0;
            if (y >= 126 && y < 154) {
                for (int i = 0; i < 3; ++i) {
                    int bx = 12 + i * 112;
                    if (x >= bx && x < bx + 104) { g_theme = i; changed = 1; }
                }
            } else if (y >= 186 && y < 214) {
                for (int i = 0; i < 3; ++i) {
                    int bx = 12 + i * 112;
                    if (x >= bx && x < bx + 104) { g_speed = i + 1; changed = 1; }
                }
            }
            if (changed) {
                save_settings();
                x11_send_desktop_cfg(&c, kThemes[g_theme].bg, kThemes[g_theme].accent, (uint32_t)g_speed);
                draw_ui(&c, win, gc);
            }
        }
    }
    close(c.fd);
    return 0;
}
