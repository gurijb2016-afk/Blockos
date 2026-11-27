/*
 * BlockOS X11 Application 1: Settings (/bin/x11_settings)
 * Displays live hardware & OS details probed from the machine running BlockOS
 * (OS/Kernel, CPUID model & vendor, RAM from /proc/meminfo, display geometry,
 * PCI device count from /proc/pci, and live uptime), plus interactive desktop
 * theme & mouse speed controls persisted to /etc/blockos.settings.
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

#define WIN_W 440
#define WIN_H 330

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
static char g_os_ver[64]    = "BlockOS x86_64";
static char g_cpu_model[64] = "Detecting...";
static char g_cpu_vendor[32]= "Unknown";
static char g_mem_total[48] = "Unknown";
static char g_mem_free[48]  = "Unknown";
static int  g_pci_count     = 0;

static void extract_field(const char* buf, const char* key, char* out, size_t cap) {
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

static void read_system_details(void) {
    char buf[2048];
    int fd;
    ssize_t n;

    /* 1. Kernel / OS version from /proc/version */
    fd = open("/proc/version", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char* nl = strchr(buf, '\n');
            if (nl) *nl = '\0';
            strncpy(g_os_ver, buf, sizeof(g_os_ver) - 1);
            g_os_ver[sizeof(g_os_ver) - 1] = '\0';
        }
    }

    /* 2. Live CPU model & vendor from /proc/cpuinfo (populated via CPUID on this machine) */
    fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            extract_field(buf, "model name", g_cpu_model, sizeof(g_cpu_model));
            extract_field(buf, "vendor_id", g_cpu_vendor, sizeof(g_cpu_vendor));
        }
    }

    /* 3. Live physical RAM stats from /proc/meminfo */
    fd = open("/proc/meminfo", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            extract_field(buf, "MemTotal:", g_mem_total, sizeof(g_mem_total));
            extract_field(buf, "MemFree:", g_mem_free, sizeof(g_mem_free));
        }
    }

    /* 4. Live PCI device count from /proc/pci */
    fd = open("/proc/pci", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            int lines = 0;
            for (ssize_t i = 0; i < n; ++i)
                if (buf[i] == '\n') ++lines;
            g_pci_count = lines > 1 ? lines - 1 : 0; /* minus header row */
        }
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
    x11_draw_text(c, win, gc, 12, 12, "BlockOS System Details & Settings");

    /* Live Hardware / System Info Box */
    x11_set_gc_colors(c, gc, 0x00151A20u, 0x00151A20u);
    x11_fill_rect(c, win, gc, 12, 30, WIN_W - 24, 126);

    char line[64];
    x11_set_gc_colors(c, gc, 0x0068D391u, 0x00151A20u);
    snprintf(line, sizeof(line), "OS:      %.42s", g_os_ver);
    x11_draw_text(c, win, gc, 20, 38, line);

    x11_set_gc_colors(c, gc, 0x00E2E8F0u, 0x00151A20u);
    snprintf(line, sizeof(line), "CPU:     %.42s", g_cpu_model);
    x11_draw_text(c, win, gc, 20, 54, line);

    snprintf(line, sizeof(line), "Vendor:  %.20s | Arch: x86_64 UEFI", g_cpu_vendor);
    x11_draw_text(c, win, gc, 20, 70, line);

    snprintf(line, sizeof(line), "Display: %ux%u (32bpp GOP X11 :0)", c->screen_w, c->screen_h);
    x11_draw_text(c, win, gc, 20, 86, line);

    snprintf(line, sizeof(line), "Memory:  %s total | %s free", g_mem_total, g_mem_free);
    x11_draw_text(c, win, gc, 20, 102, line);

    struct timespec ts;
    uint32_t sec = (clock_gettime(0, &ts) == 0) ? (uint32_t)ts.tv_sec : 0;
    snprintf(line, sizeof(line), "PCI Bus: %d devices detected | Uptime: %um %02us",
             g_pci_count, sec / 60u, sec % 60u);
    x11_draw_text(c, win, gc, 20, 118, line);

    x11_set_gc_colors(c, gc, 0x0090CDf4u, 0x00151A20u);
    x11_draw_text(c, win, gc, 20, 136, "Live probe from /proc (this running machine)");

    /* Desktop Theme Section */
    x11_set_gc_colors(c, gc, 0x00A0AEC0u, 0x001E242Cu);
    x11_draw_text(c, win, gc, 12, 168, "Desktop Theme:");

    for (int i = 0; i < 3; ++i) {
        int bx = 12 + i * 140;
        uint32_t btn_bg = (i == g_theme) ? kThemes[i].accent : 0x002D3748u;
        x11_set_gc_colors(c, gc, btn_bg, btn_bg);
        x11_fill_rect(c, win, gc, (int16_t)bx, 184, 132, 28);
        x11_set_gc_colors(c, gc, 0x00FFFFFFu, btn_bg);
        x11_draw_text(c, win, gc, (int16_t)(bx + 18), 194, kThemes[i].name);
    }

    /* Mouse Cursor Speed Section */
    x11_set_gc_colors(c, gc, 0x00A0AEC0u, 0x001E242Cu);
    x11_draw_text(c, win, gc, 12, 226, "Mouse Cursor Speed:");

    const char* spd_labels[3] = { "1x Slow", "2x Medium", "3x Fast" };
    for (int i = 0; i < 3; ++i) {
        int bx = 12 + i * 140;
        uint32_t btn_bg = ((i + 1) == g_speed) ? kThemes[g_theme].accent : 0x002D3748u;
        x11_set_gc_colors(c, gc, btn_bg, btn_bg);
        x11_fill_rect(c, win, gc, (int16_t)bx, 242, 132, 28);
        x11_set_gc_colors(c, gc, 0x00FFFFFFu, btn_bg);
        x11_draw_text(c, win, gc, (int16_t)(bx + 26), 252, spd_labels[i]);
    }

    /* Persistence notice */
    x11_set_gc_colors(c, gc, 0x00718096u, 0x001E242Cu);
    x11_draw_text(c, win, gc, 12, 290, "Settings saved to /etc/blockos.settings");
    x11_flush(c);
}

int main(void) {
    X11Conn c;
    if (x11_connect(&c, NULL) < 0) return 1;

    load_settings();
    read_system_details();

    uint32_t win = x11_create_window(&c, 40, 50, WIN_W, WIN_H, 0x001E242Cu,
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
        if (r == 0) { x11_sleep_ms(4); continue; }

        if (ev.type == X_EV_DestroyNotify) break;
        if (ev.type == X_EV_Expose) {
            read_system_details();
            draw_ui(&c, win, gc);
        } else if (ev.type == X_EV_ButtonPress) {
            int x = ev.event_x, y = ev.event_y;
            int changed = 0;
            if (y >= 184 && y < 212) {
                for (int i = 0; i < 3; ++i) {
                    int bx = 12 + i * 140;
                    if (x >= bx && x < bx + 132) { g_theme = i; changed = 1; }
                }
            } else if (y >= 242 && y < 270) {
                for (int i = 0; i < 3; ++i) {
                    int bx = 12 + i * 140;
                    if (x >= bx && x < bx + 132) { g_speed = i + 1; changed = 1; }
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
