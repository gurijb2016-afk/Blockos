/*
 * BlockOS Userspace X11 Display Server & Compositing Window Manager (/system/bin/X)
 *
 * Follows BlockOS architecture (README.md):
 *   - Strictly userspace X11 server over AF_UNIX /tmp/.X11-unix/X0
 *   - Reads GOP framebuffer geometry from /system/display.info and mmaps /devices/display
 *   - Reads PS/2, VirtIO-Input, and USB xHCI mouse/keyboard events from /devices/x11-input
 *   - Double-buffered composition with bitmap font8x8.h text rendering and software cursor
 *   - Built-in X11 desktop panel with 3 launchers: Settings, Files, Terminal + live uptime
 *   - Supports standard X11 wire clients (xclient, x11_settings, x11_files, x11_terminal)
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "x11_client.h"

/* Pull 8x8 bitmap font table directly from repository root */
#define constexpr const
#include "../../font8x8.h"
#undef constexpr

extern int execve(const char *path, char *const argv[], char *const envp[]);

/* Kernel ABI structs from kernel/kernel.cpp & kernel/input_bridge.hpp */
struct BlockOSDisplayInfo {
    uint32_t magic;
    uint32_t version;
    uint64_t framebuffer_phys;
    uint64_t framebuffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t bpp;
    uint32_t depth;
};

struct BlockOSInputEvent {
    uint32_t magic;
    uint32_t version;
    uint32_t type;   /* 1 = mouse, 2 = keyboard */
    int32_t  dx;
    int32_t  dy;
    int32_t  wheel;
    uint32_t buttons;
    uint32_t scancode;
    uint32_t pressed;
    uint32_t extended;
};

#define MAX_CLIENTS  16
#define MAX_WINDOWS  24
#define MAX_GCS      64
#define TITLEBAR_H   24
#define TASKBAR_H    28

typedef struct {
    int used;
    uint32_t id;
    uint32_t drawable;
    uint32_t fg;
    uint32_t bg;
} XGC;

typedef struct {
    int used;
    int mapped;
    uint32_t id;
    int client_idx;
    int x, y;
    int w, h;
    uint32_t bg;
    uint32_t event_mask;
    char title[64];
    uint32_t* pixels; /* client backing store (w * h) */
} XWin;

typedef struct {
    int used;
    int fd;
    int handshaked;
    uint32_t rid_base;
    uint16_t seq;
    uint8_t rx[8192];
    size_t rx_len;
} XClient;

static uint32_t* g_fb = NULL;
static uint32_t* g_back = NULL;
static uint32_t  g_fb_w = 0, g_fb_h = 0, g_fb_stride = 0;

static XClient g_clients[MAX_CLIENTS];
static XWin    g_wins[MAX_WINDOWS];
static int     g_zorder[MAX_WINDOWS];
static int     g_zcount = 0;
static XGC     g_gcs[MAX_GCS];

static int      g_focused_win = -1;
static int      g_drag_win = -1;
static int      g_drag_ox = 0, g_drag_oy = 0;

static int      g_mouse_x = 320, g_mouse_y = 240;
static uint32_t g_mouse_buttons = 0;
static uint32_t g_mouse_speed = 1; /* 1, 2, or 3 */

static uint32_t g_desktop_bg     = 0x001B2A38u;
static uint32_t g_accent_color   = 0x002B6CB0u;
static uint32_t g_taskbar_bg     = 0x00141C26u;
static int      g_dirty          = 1;
static int      g_shift_down     = 0;

/* US Set-1 scancode to ASCII lookup */
static const char kScanBase[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0, '\\','z','x','c','v',
    'b','n','m',',','.','/', 0, '*', 0, ' ', 0
};
static const char kScanShift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b', '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0, '|','Z','X','C','V',
    'B','N','M','<','>','?', 0, '*', 0, ' ', 0
};

static uint32_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(0, &ts) == 0)
        return (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
    return 0;
}

/* Fast backbuffer primitives */
static inline void bb_fill_rect(int x, int y, int w, int h, uint32_t col) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > (int)g_fb_w ? (int)g_fb_w : x + w;
    int y1 = y + h > (int)g_fb_h ? (int)g_fb_h : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    size_t count = (size_t)(x1 - x0);
    uint64_t pair = ((uint64_t)col << 32) | (uint64_t)col;
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* row = g_back + (size_t)yy * g_fb_w + x0;
        size_t q = count >> 1;
        void* dst = row;
        if (q) __asm__ volatile("rep stosq" : "+D"(dst), "+c"(q) : "a"(pair) : "memory");
        if (count & 1u) *(uint32_t*)dst = col;
    }
}

static inline void bb_draw_border(int x, int y, int w, int h, uint32_t col) {
    bb_fill_rect(x, y, w, 1, col);
    bb_fill_rect(x, y + h - 1, w, 1, col);
    bb_fill_rect(x, y, 1, h, col);
    bb_fill_rect(x + w - 1, y, 1, h, col);
}

static void bb_draw_text(int x, int y, const char* s, uint32_t col) {
    if (!s) return;
    for (; *s; ++s, x += 8) {
        unsigned char ch = (unsigned char)*s;
        if (ch >= 128) continue;
        for (int r = 0; r < 8; ++r) {
            int py = y + r;
            if (py < 0 || py >= (int)g_fb_h) continue;
            unsigned char bits = font8x8_basic[ch][r];
            uint32_t* row = g_back + (size_t)py * g_fb_w;
            for (int c = 0; c < 8; ++c) {
                int px = x + c;
                if (px < 0 || px >= (int)g_fb_w) continue;
                if (bits & (1u << c)) row[px] = col;
            }
        }
    }
}

/* Draw into a client window's backing store */
static void win_fill_rect(XWin* w, int x, int y, int rw, int rh, uint32_t col) {
    if (!w || !w->pixels || rw <= 0 || rh <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + rw > w->w ? w->w : x + rw;
    int y1 = y + rh > w->h ? w->h : y + rh;
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* row = w->pixels + (size_t)yy * (size_t)w->w + x0;
        for (int xx = x0; xx < x1; ++xx) *row++ = col;
    }
}

static void win_draw_text(XWin* w, int x, int y, const char* s, size_t len, uint32_t fg, uint32_t bg) {
    if (!w || !w->pixels || !s) return;
    for (size_t i = 0; i < len; ++i, x += 8) {
        unsigned char ch = (unsigned char)s[i];
        if (ch >= 128) ch = '?';
        for (int r = 0; r < 8; ++r) {
            int py = y + r;
            if (py < 0 || py >= w->h) continue;
            unsigned char bits = font8x8_basic[ch][r];
            uint32_t* row = w->pixels + (size_t)py * (size_t)w->w;
            for (int c = 0; c < 8; ++c) {
                int px = x + c;
                if (px < 0 || px >= w->w) continue;
                if (bits & (1u << c)) row[px] = fg;
                else if (bg != 0xFFFFFFFFu) row[px] = bg;
            }
        }
    }
}

static void draw_cursor(int cx, int cy) {
    static const char* const kCursor[14] = {
        "X             ",
        "XX            ",
        "X.X           ",
        "X..X          ",
        "X...X         ",
        "X....X        ",
        "X.....X       ",
        "X......X      ",
        "X.......X     ",
        "X.....XXXX    ",
        "X..X..X       ",
        "X.X X..X      ",
        "XX   X..X     ",
        "     XXXX     "
    };
    for (int r = 0; r < 14; ++r) {
        int py = cy + r;
        if (py < 0 || py >= (int)g_fb_h) continue;
        uint32_t* row = g_back + (size_t)py * g_fb_w;
        for (int c = 0; c < 14; ++c) {
            int px = cx + c;
            if (px < 0 || px >= (int)g_fb_w) continue;
            char p = kCursor[r][c];
            if (p == 'X') row[px] = 0x00000000u;
            else if (p == '.') row[px] = 0x00FFFFFFu;
        }
    }
}

static void swap_buffers(void) {
    if (!g_fb || !g_back) return;
    if (g_fb_stride == g_fb_w) {
        size_t qwords = ((size_t)g_fb_w * (size_t)g_fb_h) / 2u;
        void* dst = g_fb;
        const void* src = g_back;
        __asm__ volatile("rep movsq" : "+D"(dst), "+S"(src), "+c"(qwords) :: "memory");
    } else {
        for (uint32_t y = 0; y < g_fb_h; ++y) {
            size_t qwords = (size_t)g_fb_w / 2u;
            void* dst = g_fb + (size_t)y * g_fb_stride;
            const void* src = g_back + (size_t)y * g_fb_w;
            __asm__ volatile("rep movsq" : "+D"(dst), "+S"(src), "+c"(qwords) :: "memory");
        }
    }
}

/* Z-order management */
static void zorder_remove(int idx) {
    int out = 0;
    for (int i = 0; i < g_zcount; ++i) {
        if (g_zorder[i] != idx) g_zorder[out++] = g_zorder[i];
    }
    g_zcount = out;
}

static void zorder_raise(int idx) {
    zorder_remove(idx);
    if (g_zcount < MAX_WINDOWS) g_zorder[g_zcount++] = idx;
    g_focused_win = idx;
    g_dirty = 1;
}

static int find_win_by_id(uint32_t id) {
    for (int i = 0; i < MAX_WINDOWS; ++i)
        if (g_wins[i].used && g_wins[i].id == id) return i;
    return -1;
}

static int find_gc_by_id(uint32_t id) {
    for (int i = 0; i < MAX_GCS; ++i)
        if (g_gcs[i].used && g_gcs[i].id == id) return i;
    return -1;
}

static void send_event_to_client(int cidx, uint8_t type, uint8_t detail,
                                 uint32_t win_id, int16_t rx, int16_t ry,
                                 int16_t ex, int16_t ey, uint16_t state, uint8_t ascii) {
    if (cidx < 0 || cidx >= MAX_CLIENTS || !g_clients[cidx].used) return;
    uint8_t ev[32];
    memset(ev, 0, sizeof(ev));
    ev[0] = type;
    ev[1] = detail;
    x11_put16(ev, 2, g_clients[cidx].seq);
    x11_put32(ev, 4, now_ms());
    x11_put32(ev, 8, 1u); /* root */
    x11_put32(ev, 12, win_id);
    x11_put16(ev, 20, (uint16_t)rx);
    x11_put16(ev, 22, (uint16_t)ry);
    x11_put16(ev, 24, (uint16_t)ex);
    x11_put16(ev, 26, (uint16_t)ey);
    x11_put16(ev, 28, state);
    ev[30] = ascii;
    (void)write(g_clients[cidx].fd, ev, sizeof(ev));
}

static void destroy_window_idx(int idx) {
    if (idx < 0 || idx >= MAX_WINDOWS || !g_wins[idx].used) return;
    if (g_wins[idx].pixels) {
        free(g_wins[idx].pixels);
        g_wins[idx].pixels = NULL;
    }
    g_wins[idx].used = 0;
    g_wins[idx].mapped = 0;
    zorder_remove(idx);
    if (g_focused_win == idx) {
        g_focused_win = (g_zcount > 0) ? g_zorder[g_zcount - 1] : -1;
    }
    if (g_drag_win == idx) g_drag_win = -1;
    g_dirty = 1;
}

static void close_client(int cidx) {
    if (cidx < 0 || cidx >= MAX_CLIENTS || !g_clients[cidx].used) return;
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        if (g_wins[i].used && g_wins[i].client_idx == cidx)
            destroy_window_idx(i);
    }
    close(g_clients[cidx].fd);
    memset(&g_clients[cidx], 0, sizeof(g_clients[cidx]));
    g_clients[cidx].fd = -1;
    g_dirty = 1;
}

static long sys_vfork(void) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(92L) : "rcx", "r11", "memory", "cc");
    return r;
}

static void spawn_x11_app(const char* path) {
    /* Use SYS_vfork (92) so spawning from the X server never clones the 4 MB
     * framebuffer or 4 MB backbuffer pages. */
    long pid = sys_vfork();
    if (pid == 0) {
        for (int fd = 3; fd < 32; ++fd) close(fd);
        char* const argv[] = { (char*)path, NULL };
        char* const envp[] = { "DISPLAY=:0", NULL };
        execve(path, argv, envp);
        _exit(127);
    }
}

/* Taskbar button layout: Settings | Files | Terminal */
static void handle_taskbar_click(int mx) {
    if (mx >= 8 && mx < 104) {
        spawn_x11_app("/bin/x11_settings");
    } else if (mx >= 112 && mx < 192) {
        spawn_x11_app("/bin/x11_files");
    } else if (mx >= 200 && mx < 296) {
        spawn_x11_app("/bin/x11_terminal");
    }
}

static void compose_frame(void) {
    /* 1. Desktop background */
    bb_fill_rect(0, 0, (int)g_fb_w, (int)g_fb_h - TASKBAR_H, g_desktop_bg);

    /* Subtle desktop watermark */
    bb_draw_text(16, 14, "BlockOS X11 Session (:0)", 0x00526D82u);

    /* 2. Composite mapped X11 client windows in Z-order */
    for (int zi = 0; zi < g_zcount; ++zi) {
        int idx = g_zorder[zi];
        XWin* w = &g_wins[idx];
        if (!w->used || !w->mapped) continue;

        int fx = w->x;
        int fy = w->y;
        int fw = w->w + 2;
        int fh = w->h + TITLEBAR_H + 2;
        int focused = (idx == g_focused_win);
        uint32_t tb_col = focused ? g_accent_color : 0x002C3744u;
        uint32_t bd_col = focused ? 0x0063B3EDu : 0x004A5568u;

        /* Drop shadow */
        bb_fill_rect(fx + 4, fy + 4, fw, fh, 0x000D131Au);

        /* Titlebar & frame */
        bb_fill_rect(fx, fy, fw, TITLEBAR_H + 1, tb_col);
        bb_draw_border(fx, fy, fw, fh, bd_col);
        bb_draw_text(fx + 8, fy + 8, w->title[0] ? w->title : "X11 Window", 0x00FFFFFFu);

        /* Close [X] button */
        int cx = fx + fw - 22;
        int cy = fy + 4;
        bb_fill_rect(cx, cy, 18, 16, 0x00C53030u);
        bb_draw_text(cx + 5, cy + 4, "X", 0x00FFFFFFu);

        /* Blit client window pixel buffer */
        if (w->pixels) {
            int dst_x0 = fx + 1;
            int dst_y0 = fy + TITLEBAR_H + 1;
            for (int r = 0; r < w->h; ++r) {
                int py = dst_y0 + r;
                if (py < 0 || py >= (int)g_fb_h - TASKBAR_H) continue;
                int c0 = dst_x0 < 0 ? -dst_x0 : 0;
                int c1 = (dst_x0 + w->w > (int)g_fb_w) ? ((int)g_fb_w - dst_x0) : w->w;
                if (c1 > c0) {
                    memcpy(g_back + (size_t)py * g_fb_w + (dst_x0 + c0),
                           w->pixels + (size_t)r * (size_t)w->w + c0,
                           (size_t)(c1 - c0) * 4u);
                }
            }
        }
    }

    /* 3. Bottom 28px taskbar */
    int ty = (int)g_fb_h - TASKBAR_H;
    bb_fill_rect(0, ty, (int)g_fb_w, TASKBAR_H, g_taskbar_bg);
    bb_fill_rect(0, ty, (int)g_fb_w, 1, g_accent_color);

    /* Three launcher buttons: Settings, Files, Terminal */
    bb_fill_rect(8,   ty + 4, 96, 20, g_accent_color);
    bb_draw_text(24,  ty + 10, "Settings", 0x00FFFFFFu);

    bb_fill_rect(112, ty + 4, 80, 20, g_accent_color);
    bb_draw_text(132, ty + 10, "Files", 0x00FFFFFFu);

    bb_fill_rect(200, ty + 4, 96, 20, g_accent_color);
    bb_draw_text(216, ty + 10, "Terminal", 0x00FFFFFFu);

    /* Uptime / status indicator on the right */
    uint32_t sec = now_ms() / 1000u;
    uint32_t mins = sec / 60u;
    uint32_t rem = sec % 60u;
    char status[64];
    snprintf(status, sizeof(status), "X11 :0 | Up %u:%02u", mins, rem);
    int sx = (int)g_fb_w - (int)strlen(status) * 8 - 12;
    bb_draw_text(sx, ty + 10, status, 0x00CBD5E0u);

    /* 4. Software cursor drawn last */
    draw_cursor(g_mouse_x, g_mouse_y);

    swap_buffers();
    g_dirty = 0;
}

/* Process X11 client setup handshake */
static void handle_client_handshake(int cidx) {
    XClient* cl = &g_clients[cidx];
    if (cl->rx_len < 12) return;
    uint16_t auth_name_len = x11_get16(cl->rx, 6);
    uint16_t auth_data_len = x11_get16(cl->rx, 8);
    size_t need = 12u + ((auth_name_len + 3u) & ~3u) + ((auth_data_len + 3u) & ~3u);
    if (cl->rx_len < need) return;

    /* Consume handshake bytes */
    memmove(cl->rx, cl->rx + need, cl->rx_len - need);
    cl->rx_len -= need;
    cl->handshaked = 1;

    /* Build standard X11 Connection Setup Success reply */
    const char* vendor = "BlockOS-X11";
    size_t vlen = strlen(vendor);
    size_t vpad = (vlen + 3u) & ~3u;
    size_t body_len = 32u + vpad + 8u + 40u + 8u + 24u;
    uint8_t reply[256];
    memset(reply, 0, 8u + body_len);

    reply[0] = 1; /* Success */
    x11_put16(reply, 2, 11); /* protocol major */
    x11_put16(reply, 4, 0);  /* protocol minor */
    x11_put16(reply, 6, (uint16_t)(body_len / 4u));

    uint8_t* b = reply + 8;
    x11_put32(b, 0, 11000000u);        /* release */
    x11_put32(b, 4, cl->rid_base);     /* resource-id-base */
    x11_put32(b, 8, 0x000FFFFFu);      /* resource-id-mask */
    x11_put32(b, 12, 256u);            /* motion-buffer-size */
    x11_put16(b, 16, (uint16_t)vlen);  /* vendor length */
    x11_put16(b, 18, 65535u);          /* max-request-length */
    b[20] = 1;                         /* num screens */
    b[21] = 1;                         /* num pixmap formats */
    b[22] = 0;                         /* image-byte-order LSBFirst */
    b[23] = 0;                         /* bitmap-bit-order */
    b[24] = 32;                        /* scanline unit */
    b[25] = 32;                        /* scanline pad */
    b[26] = 8;                         /* min-keycode */
    b[27] = 255;                       /* max-keycode */
    memcpy(b + 32, vendor, vlen);

    /* Format 0 (8 bytes): 32bpp TrueColor */
    uint8_t* fmt = b + 32u + vpad;
    fmt[0] = 24; fmt[1] = 32; fmt[2] = 32;

    /* Screen 0 (40 bytes) */
    uint8_t* scr = fmt + 8u;
    x11_put32(scr, 0, 1u);             /* root window id */
    x11_put32(scr, 4, 0x20u);          /* default colormap */
    x11_put32(scr, 8, 0x00FFFFFFu);    /* white pixel */
    x11_put32(scr, 12, 0x00000000u);   /* black pixel */
    x11_put16(scr, 20, (uint16_t)g_fb_w);
    x11_put16(scr, 22, (uint16_t)g_fb_h);
    x11_put16(scr, 24, 340);
    x11_put16(scr, 26, 210);
    x11_put16(scr, 28, 1);
    x11_put16(scr, 30, 1);
    x11_put32(scr, 32, 0x21u);         /* root visual */
    scr[38] = 24;                      /* root depth */
    scr[39] = 1;                       /* allowed depths */

    /* Depth 0 (8 bytes) + VisualType (24 bytes) */
    uint8_t* dep = scr + 40u;
    dep[0] = 24;
    x11_put16(dep, 2, 1);
    uint8_t* vis = dep + 8u;
    x11_put32(vis, 0, 0x21u);
    vis[4] = 4; /* TrueColor */
    vis[5] = 8;
    x11_put16(vis, 6, 256);
    x11_put32(vis, 8,  0x00FF0000u);
    x11_put32(vis, 12, 0x0000FF00u);
    x11_put32(vis, 16, 0x000000FFu);

    (void)x11_send_all(cl->fd, reply, 8u + body_len);
}

/* Dispatch one complete X11 wire request */
static void handle_client_request(int cidx, const uint8_t* pkt, size_t len) {
    XClient* cl = &g_clients[cidx];
    cl->seq++;
    uint8_t op = pkt[0];

    switch (op) {
    case X_CreateWindow: {
        if (len < 32) break;
        uint32_t wid = x11_get32(pkt, 4);
        int16_t  wx  = (int16_t)x11_get16(pkt, 12);
        int16_t  wy  = (int16_t)x11_get16(pkt, 14);
        uint16_t ww  = x11_get16(pkt, 16);
        uint16_t wh  = x11_get16(pkt, 18);
        uint32_t mask = x11_get32(pkt, 28);
        uint32_t bg = 0x001E293Bu, ev_mask = 0;
        size_t off = 32;
        for (int bit = 0; bit < 16 && off + 4 <= len; ++bit) {
            if (!(mask & (1u << bit))) continue;
            uint32_t val = x11_get32(pkt, off);
            off += 4;
            if ((1u << bit) == X_CWBackPixel) bg = val;
            if ((1u << bit) == X_CWEventMask) ev_mask = val;
        }
        if (ww == 0) ww = 320;
        if (wh == 0) wh = 200;
        if (ww > g_fb_w) ww = (uint16_t)g_fb_w;
        if (wh > g_fb_h) wh = (uint16_t)g_fb_h;
        for (int i = 0; i < MAX_WINDOWS; ++i) {
            if (!g_wins[i].used) {
                memset(&g_wins[i], 0, sizeof(g_wins[i]));
                g_wins[i].used = 1;
                g_wins[i].id = wid;
                g_wins[i].client_idx = cidx;
                g_wins[i].x = wx;
                g_wins[i].y = wy;
                g_wins[i].w = ww;
                g_wins[i].h = wh;
                g_wins[i].bg = bg;
                g_wins[i].event_mask = ev_mask;
                g_wins[i].pixels = (uint32_t*)malloc((size_t)ww * (size_t)wh * 4u);
                if (g_wins[i].pixels) win_fill_rect(&g_wins[i], 0, 0, ww, wh, bg);
                break;
            }
        }
        break;
    }
    case X_ChangeWindowAttr: {
        /* Custom BlockOS root theme/mouse update message */
        if (len >= 24 && x11_get32(pkt, 4) == 1u && x11_get32(pkt, 8) == 0x424F5301u) {
            g_desktop_bg   = x11_get32(pkt, 12);
            g_accent_color = x11_get32(pkt, 16);
            uint32_t spd   = x11_get32(pkt, 20);
            if (spd >= 1 && spd <= 3) g_mouse_speed = spd;
            g_dirty = 1;
        }
        break;
    }
    case X_DestroyWindow: {
        if (len >= 8) destroy_window_idx(find_win_by_id(x11_get32(pkt, 4)));
        break;
    }
    case X_MapWindow: {
        if (len < 8) break;
        int idx = find_win_by_id(x11_get32(pkt, 4));
        if (idx >= 0) {
            g_wins[idx].mapped = 1;
            zorder_raise(idx);
            send_event_to_client(cidx, X_EV_Expose, 0, g_wins[idx].id, 0, 0, 0, 0, 0, 0);
        }
        break;
    }
    case X_UnmapWindow: {
        if (len < 8) break;
        int idx = find_win_by_id(x11_get32(pkt, 4));
        if (idx >= 0) { g_wins[idx].mapped = 0; g_dirty = 1; }
        break;
    }
    case X_ChangeProperty: {
        if (len < 24) break;
        int idx = find_win_by_id(x11_get32(pkt, 4));
        uint32_t prop = x11_get32(pkt, 8);
        uint32_t nitems = x11_get32(pkt, 20);
        if (idx >= 0 && prop == X_XA_WM_NAME && 24u + nitems <= len) {
            size_t cp = nitems < sizeof(g_wins[idx].title) - 1 ? nitems : sizeof(g_wins[idx].title) - 1;
            memcpy(g_wins[idx].title, pkt + 24, cp);
            g_wins[idx].title[cp] = '\0';
            g_dirty = 1;
        }
        break;
    }
    case X_CreateGC: {
        if (len < 16) break;
        uint32_t gid = x11_get32(pkt, 4);
        uint32_t drw = x11_get32(pkt, 8);
        uint32_t mask = x11_get32(pkt, 12);
        uint32_t fg = 0x00FFFFFFu, bg = 0xFFFFFFFFu;
        size_t off = 16;
        for (int bit = 0; bit < 23 && off + 4 <= len; ++bit) {
            if (!(mask & (1u << bit))) continue;
            uint32_t val = x11_get32(pkt, off);
            off += 4;
            if ((1u << bit) == X_GCForeground) fg = val;
            if ((1u << bit) == X_GCBackground) bg = val;
        }
        for (int i = 0; i < MAX_GCS; ++i) {
            if (!g_gcs[i].used) {
                g_gcs[i].used = 1; g_gcs[i].id = gid; g_gcs[i].drawable = drw;
                g_gcs[i].fg = fg; g_gcs[i].bg = bg;
                break;
            }
        }
        break;
    }
    case X_ChangeGC: {
        if (len < 12) break;
        int gi = find_gc_by_id(x11_get32(pkt, 4));
        if (gi < 0) break;
        uint32_t mask = x11_get32(pkt, 8);
        size_t off = 12;
        for (int bit = 0; bit < 23 && off + 4 <= len; ++bit) {
            if (!(mask & (1u << bit))) continue;
            uint32_t val = x11_get32(pkt, off);
            off += 4;
            if ((1u << bit) == X_GCForeground) g_gcs[gi].fg = val;
            if ((1u << bit) == X_GCBackground) g_gcs[gi].bg = val;
        }
        break;
    }
    case X_PolyFillRectangle: {
        if (len < 20) break;
        int wi = find_win_by_id(x11_get32(pkt, 4));
        int gi = find_gc_by_id(x11_get32(pkt, 8));
        if (wi < 0 || gi < 0) break;
        for (size_t off = 12; off + 8 <= len; off += 8) {
            int16_t rx = (int16_t)x11_get16(pkt, off);
            int16_t ry = (int16_t)x11_get16(pkt, off + 2);
            uint16_t rw = x11_get16(pkt, off + 4);
            uint16_t rh = x11_get16(pkt, off + 6);
            win_fill_rect(&g_wins[wi], rx, ry, rw, rh, g_gcs[gi].fg);
        }
        g_dirty = 1;
        break;
    }
    case X_ImageText8: {
        if (len < 16) break;
        uint8_t slen = pkt[1];
        int wi = find_win_by_id(x11_get32(pkt, 4));
        int gi = find_gc_by_id(x11_get32(pkt, 8));
        int16_t tx = (int16_t)x11_get16(pkt, 12);
        int16_t ty = (int16_t)x11_get16(pkt, 14);
        if (wi >= 0 && gi >= 0 && 16u + slen <= len) {
            win_draw_text(&g_wins[wi], tx, ty, (const char*)(pkt + 16), slen,
                          g_gcs[gi].fg, g_gcs[gi].bg);
            g_dirty = 1;
        }
        break;
    }
    default:
        break;
    }
}

static void poll_clients(int listen_fd) {
    /* Accept any new X11 client connections */
    for (;;) {
        int cfd = accept4(listen_fd, NULL, NULL, SOCK_NONBLOCK);
        if (cfd < 0) break;
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (!g_clients[i].used) { slot = i; break; }
        }
        if (slot < 0) { close(cfd); break; }
        memset(&g_clients[slot], 0, sizeof(g_clients[slot]));
        g_clients[slot].used = 1;
        g_clients[slot].fd = cfd;
        g_clients[slot].rid_base = (uint32_t)(slot + 1) << 20;
    }

    /* Drain pending X11 requests from connected clients */
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!g_clients[i].used) continue;
        XClient* cl = &g_clients[i];
        for (int iter = 0; iter < 16; ++iter) {
            size_t avail = sizeof(cl->rx) - cl->rx_len;
            if (avail == 0) break;
            ssize_t r = read(cl->fd, cl->rx + cl->rx_len, avail);
            if (r < 0 && errno == EAGAIN) break;
            if (r <= 0) { close_client(i); goto next_client; }
            cl->rx_len += (size_t)r;
        }
        if (!cl->handshaked) handle_client_handshake(i);
        while (cl->handshaked && cl->rx_len >= 4) {
            uint16_t words = x11_get16(cl->rx, 2);
            size_t req_len = (size_t)words * 4u;
            if (req_len < 4 || req_len > sizeof(cl->rx)) { close_client(i); goto next_client; }
            if (cl->rx_len < req_len) break;
            handle_client_request(i, cl->rx, req_len);
            memmove(cl->rx, cl->rx + req_len, cl->rx_len - req_len);
            cl->rx_len -= req_len;
        }
next_client:;
    }
}

static void handle_mouse_button(int mx, int my, uint32_t prev_btn, uint32_t cur_btn) {
    int left_down = (!(prev_btn & 1u) && (cur_btn & 1u));
    int left_up   = ((prev_btn & 1u) && !(cur_btn & 1u));

    if (left_up) {
        g_drag_win = -1;
        return;
    }
    if (!left_down) return;

    /* Check taskbar click first */
    if (my >= (int)g_fb_h - TASKBAR_H) {
        handle_taskbar_click(mx);
        return;
    }

    /* Hit-test windows top-to-bottom */
    for (int zi = g_zcount - 1; zi >= 0; --zi) {
        int idx = g_zorder[zi];
        XWin* w = &g_wins[idx];
        if (!w->used || !w->mapped) continue;

        int fx = w->x, fy = w->y;
        int fw = w->w + 2, fh = w->h + TITLEBAR_H + 2;
        if (mx < fx || mx >= fx + fw || my < fy || my >= fy + fh) continue;

        zorder_raise(idx);

        /* Close [X] button hit-test */
        int cx = fx + fw - 22;
        int cy = fy + 4;
        if (mx >= cx && mx < cx + 18 && my >= cy && my < cy + 16) {
            send_event_to_client(w->client_idx, X_EV_DestroyNotify, 0, w->id, 0, 0, 0, 0, 0, 0);
            destroy_window_idx(idx);
            return;
        }

        /* Titlebar drag */
        if (my < fy + TITLEBAR_H + 1) {
            g_drag_win = idx;
            g_drag_ox = mx - w->x;
            g_drag_oy = my - w->y;
            return;
        }

        /* Client area click -> forward X11 ButtonPress event */
        int ex = mx - (fx + 1);
        int ey = my - (fy + TITLEBAR_H + 1);
        send_event_to_client(w->client_idx, X_EV_ButtonPress, 1, w->id,
                             (int16_t)mx, (int16_t)my, (int16_t)ex, (int16_t)ey, 0, 0);
        return;
    }
}

static void poll_input(int input_fd) {
    struct BlockOSInputEvent evs[64];
    ssize_t nbytes = read(input_fd, evs, sizeof(evs));
    if (nbytes <= 0) return;
    size_t count = (size_t)nbytes / sizeof(evs[0]);

    for (size_t i = 0; i < count; ++i) {
        if (evs[i].type == 1u || evs[i].type == 3u) { /* Relative (1) or Absolute Tablet (3) Mouse */
            int nx, ny;
            if (evs[i].type == 3u) {
                /* Absolute coordinates in 0..32767 -> exact 1:1 pixel sync with host cursor */
                int32_t ax = evs[i].dx;
                int32_t ay = evs[i].dy;
                if (ax < 0) ax = 0;
                if (ay < 0) ay = 0;
                if (ax > 32767) ax = 32767;
                if (ay > 32767) ay = 32767;
                nx = (int)(((int64_t)ax * (int64_t)(g_fb_w - 1)) / 32767);
                ny = (int)(((int64_t)ay * (int64_t)(g_fb_h - 1)) / 32767);
            } else {
                int mult = (int)g_mouse_speed;
                nx = g_mouse_x + evs[i].dx * mult;
                ny = g_mouse_y + evs[i].dy * mult;
            }
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx >= (int)g_fb_w) nx = (int)g_fb_w - 1;
            if (ny >= (int)g_fb_h) ny = (int)g_fb_h - 1;

            if (nx != g_mouse_x || ny != g_mouse_y) {
                g_mouse_x = nx;
                g_mouse_y = ny;
                if (g_drag_win >= 0 && g_wins[g_drag_win].used) {
                    g_wins[g_drag_win].x = g_mouse_x - g_drag_ox;
                    g_wins[g_drag_win].y = g_mouse_y - g_drag_oy;
                    if (g_wins[g_drag_win].y < 0) g_wins[g_drag_win].y = 0;
                    if (g_wins[g_drag_win].y > (int)g_fb_h - TASKBAR_H - TITLEBAR_H)
                        g_wins[g_drag_win].y = (int)g_fb_h - TASKBAR_H - TITLEBAR_H;
                }
                g_dirty = 1;
            }
            if (evs[i].buttons != g_mouse_buttons) {
                uint32_t prev = g_mouse_buttons;
                g_mouse_buttons = evs[i].buttons;
                handle_mouse_button(g_mouse_x, g_mouse_y, prev, g_mouse_buttons);
                g_dirty = 1;
            }
        } else if (evs[i].type == 2u) { /* Keyboard */
            uint8_t sc = (uint8_t)(evs[i].scancode & 0x7Fu);
            if (sc == 0x2A || sc == 0x36) {
                g_shift_down = evs[i].pressed ? 1 : 0;
                continue;
            }
            if (!evs[i].pressed) continue;

            /* ESC closes X11 session if no window focused? Keep Alt+F4 or Ctrl+Q?
             * Forward all key presses to the focused X11 window. */
            if (g_focused_win >= 0 && g_wins[g_focused_win].used) {
                XWin* fw = &g_wins[g_focused_win];
                uint8_t ch = (uint8_t)(g_shift_down ? kScanShift[sc] : kScanBase[sc]);
                send_event_to_client(fw->client_idx, X_EV_KeyPress, sc, fw->id,
                                     (int16_t)g_mouse_x, (int16_t)g_mouse_y, 0, 0,
                                     (uint16_t)(g_shift_down ? 1u : 0u), ch);
            }
        }
    }
}

int main(void) {
    /* 1. Load GOP display geometry */
    struct BlockOSDisplayInfo dinfo;
    memset(&dinfo, 0, sizeof(dinfo));
    int info_fd = open("/system/display.info", O_RDONLY);
    if (info_fd < 0 || read(info_fd, &dinfo, sizeof(dinfo)) != (ssize_t)sizeof(dinfo)) {
        printf("X: cannot read /system/display.info\n");
        return 1;
    }
    close(info_fd);

    g_fb_w = dinfo.width;
    g_fb_h = dinfo.height;
    g_fb_stride = dinfo.stride;

    int disp_fd = open("/devices/display", O_RDWR);
    if (disp_fd < 0) {
        fprintf(stderr, "X: cannot open /devices/display\n");
        return 1;
    }
    int input_fd = open("/devices/x11-input", O_RDONLY | O_NONBLOCK);
    if (input_fd < 0) {
        fprintf(stderr, "X: cannot open /devices/x11-input\n");
        return 1;
    }

    /* 2. Bind standard X11 Unix socket /tmp/.X11-unix/X0 BEFORE mapping large
     * framebuffer / backbuffer so fork() only copies the tiny X server image! */
    mkdir("/tmp", 0777);
    mkdir("/tmp/.X11-unix", 0777);
    int listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd < 0) {
        fprintf(stderr, "X: socket failed\n");
        return 1;
    }
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "/tmp/.X11-unix/X0");
    if (bind(listen_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0 || listen(listen_fd, 16) < 0) {
        fprintf(stderr, "X: bind/listen /tmp/.X11-unix/X0 failed\n");
        return 1;
    }

    /* Spawn default X11 apps BEFORE mapping the 4 MB framebuffer + 4 MB backbuffer
     * so fork() does not have to clone 8 MB of display buffers per child. */
    spawn_x11_app("/bin/x11_settings");
    spawn_x11_app("/bin/x11_files");
    spawn_x11_app("/bin/x11_terminal");

    void* mapped = mmap(NULL, (size_t)dinfo.framebuffer_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, disp_fd, 0);
    if (mapped == MAP_FAILED || !mapped) {
        fprintf(stderr, "X: mmap /devices/display failed\n");
        return 1;
    }
    g_fb = (uint32_t*)mapped;
    g_back = (uint32_t*)malloc((size_t)g_fb_w * (size_t)g_fb_h * 4u);
    if (!g_back) {
        fprintf(stderr, "X: backbuffer allocation failed\n");
        return 1;
    }

    g_mouse_x = (int)(g_fb_w / 2);
    g_mouse_y = (int)(g_fb_h / 2);
    compose_frame();

    uint32_t last_clock_sec = 0;
    for (;;) {
        poll_clients(listen_fd);
        poll_input(input_fd);

        uint32_t cur_sec = now_ms() / 1000u;
        if (cur_sec != last_clock_sec) {
            last_clock_sec = cur_sec;
            g_dirty = 1;
        }
        if (g_dirty) compose_frame();
        x11_sleep_ms(4);
    }
    return 0;
}
