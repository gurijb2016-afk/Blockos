/*
 * BlockOS Userspace X11 Wire Protocol Client Library (X11R6 / X.Org subset)
 * Communicates over AF_UNIX /tmp/.X11-unix/X0 using standard X11 wire packets.
 * Strictly C only — zero external dependencies beyond BlockOS userspace libc.
 */
#pragma once

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

/* Standard X11 core opcodes */
#define X_CreateWindow       1
#define X_ChangeWindowAttr   2
#define X_DestroyWindow      4
#define X_MapWindow          8
#define X_UnmapWindow       10
#define X_ConfigureWindow   12
#define X_ChangeProperty    18
#define X_CreateGC          55
#define X_ChangeGC          56
#define X_FreeGC            60
#define X_ClearArea         61
#define X_PolyFillRectangle 70
#define X_ImageText8        76

/* Standard X11 core event types */
#define X_EV_KeyPress        2
#define X_EV_KeyRelease      3
#define X_EV_ButtonPress     4
#define X_EV_ButtonRelease   5
#define X_EV_MotionNotify    6
#define X_EV_Expose         12
#define X_EV_DestroyNotify  17
#define X_EV_ConfigureNotify 22
#define X_EV_ClientMessage  33

/* Standard X11 event masks */
#define X_KeyPressMask        (1u << 0)
#define X_KeyReleaseMask      (1u << 1)
#define X_ButtonPressMask     (1u << 2)
#define X_ButtonReleaseMask   (1u << 3)
#define X_PointerMotionMask   (1u << 6)
#define X_ExposureMask        (1u << 15)
#define X_StructureNotifyMask (1u << 17)

/* Standard X11 CreateWindow value-mask bits */
#define X_CWBackPixel  (1u << 1)
#define X_CWEventMask  (1u << 11)

/* Standard X11 GC value-mask bits */
#define X_GCForeground (1u << 2)
#define X_GCBackground (1u << 3)

/* Standard X11 Atoms */
#define X_XA_STRING    31u
#define X_XA_WM_NAME   39u

typedef struct {
    int fd;
    uint32_t rid_base;
    uint32_t rid_next;
    uint32_t root;
    uint16_t screen_w;
    uint16_t screen_h;
} X11Conn;

typedef struct {
    uint8_t  type;
    uint8_t  detail;
    uint16_t seq;
    uint32_t time_ms;
    uint32_t root;
    uint32_t window;
    int16_t  root_x;
    int16_t  root_y;
    int16_t  event_x;
    int16_t  event_y;
    uint16_t state;
    uint8_t  ascii;
    uint8_t  pad;
} X11Event;

static inline void x11_put16(uint8_t* b, size_t o, uint16_t v) {
    b[o] = (uint8_t)(v & 0xFFu);
    b[o + 1] = (uint8_t)(v >> 8);
}
static inline void x11_put32(uint8_t* b, size_t o, uint32_t v) {
    b[o]     = (uint8_t)(v & 0xFFu);
    b[o + 1] = (uint8_t)((v >> 8) & 0xFFu);
    b[o + 2] = (uint8_t)((v >> 16) & 0xFFu);
    b[o + 3] = (uint8_t)((v >> 24) & 0xFFu);
}
static inline uint16_t x11_get16(const uint8_t* b, size_t o) {
    return (uint16_t)b[o] | ((uint16_t)b[o + 1] << 8);
}
static inline uint32_t x11_get32(const uint8_t* b, size_t o) {
    return (uint32_t)b[o] | ((uint32_t)b[o + 1] << 8) |
           ((uint32_t)b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24);
}

static inline void x11_sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, 0);
}

static inline int x11_send_all(int fd, const void* buf, size_t n) {
    const uint8_t* p = (const uint8_t*)buf;
    int retries = 0;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0 && errno == EAGAIN) {
            if (++retries > 2000) return -1;
            x11_sleep_ms(1);
            continue;
        }
        if (w <= 0) return -1;
        p += (size_t)w;
        n -= (size_t)w;
        retries = 0;
    }
    return 0;
}

static inline int x11_recv_all(int fd, void* buf, size_t n) {
    uint8_t* p = (uint8_t*)buf;
    int retries = 0;
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r < 0 && errno == EAGAIN) {
            if (++retries > 5000) return -1;
            x11_sleep_ms(1);
            continue;
        }
        if (r <= 0) return -1;
        p += (size_t)r;
        n -= (size_t)r;
        retries = 0;
    }
    return 0;
}

static inline int x11_connect(X11Conn* c, const char* sock_path) {
    memset(c, 0, sizeof(*c));
    c->fd = -1;
    const char* path = sock_path ? sock_path : "/tmp/.X11-unix/X0";

    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, path, sizeof(sa.sun_path) - 1);

    for (int attempt = 0; attempt < 50; ++attempt) {
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0) return -1;
        if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) == 0) {
            c->fd = fd;
            break;
        }
        close(fd);
        x11_sleep_ms(50);
    }
    if (c->fd < 0) return -1;

    uint8_t req[12];
    memset(req, 0, sizeof(req));
    req[0] = 'l';
    x11_put16(req, 2, 11);
    x11_put16(req, 4, 0);
    if (x11_send_all(c->fd, req, sizeof(req)) < 0) { close(c->fd); c->fd = -1; return -1; }

    uint8_t hdr[8];
    if (x11_recv_all(c->fd, hdr, sizeof(hdr)) < 0 || hdr[0] != 1) {
        close(c->fd); c->fd = -1; return -1;
    }
    uint16_t words = x11_get16(hdr, 6);
    uint8_t body[512];
    size_t blen = (size_t)words * 4u;
    if (blen > sizeof(body)) blen = sizeof(body);
    if (x11_recv_all(c->fd, body, blen) < 0) { close(c->fd); c->fd = -1; return -1; }

    c->rid_base = x11_get32(body, 4);
    c->rid_next = 1;
    uint16_t vlen = x11_get16(body, 16);
    uint8_t nfmt = body[21];
    size_t vpad = ((size_t)vlen + 3u) & ~3u;
    size_t scr = 32u + vpad + 8u * (size_t)nfmt;
    if (scr + 24u <= blen) {
        c->root = x11_get32(body, scr);
        c->screen_w = x11_get16(body, scr + 20);
        c->screen_h = x11_get16(body, scr + 22);
    }
    return 0;
}

static inline uint32_t x11_alloc_id(X11Conn* c) {
    return c->rid_base | (c->rid_next++);
}

static inline uint32_t x11_create_window(X11Conn* c, int16_t x, int16_t y,
                                         uint16_t w, uint16_t h,
                                         uint32_t bg, uint32_t ev_mask) {
    uint32_t wid = x11_alloc_id(c);
    uint8_t pkt[40];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = X_CreateWindow;
    x11_put16(pkt, 2, 10);
    x11_put32(pkt, 4, wid);
    x11_put32(pkt, 8, c->root);
    x11_put16(pkt, 12, (uint16_t)x);
    x11_put16(pkt, 14, (uint16_t)y);
    x11_put16(pkt, 16, w);
    x11_put16(pkt, 18, h);
    x11_put16(pkt, 22, 1); /* InputOutput */
    x11_put32(pkt, 28, X_CWBackPixel | X_CWEventMask);
    x11_put32(pkt, 32, bg);
    x11_put32(pkt, 36, ev_mask);
    if (x11_send_all(c->fd, pkt, sizeof(pkt)) < 0) return 0;
    return wid;
}

static inline void x11_set_title(X11Conn* c, uint32_t wid, const char* title) {
    size_t len = strlen(title);
    if (len > 120) len = 120;
    size_t pad = (4u - (len & 3u)) & 3u;
    size_t total = 24u + len + pad;
    uint8_t pkt[160];
    memset(pkt, 0, total);
    pkt[0] = X_ChangeProperty;
    pkt[1] = 0; /* Replace */
    x11_put16(pkt, 2, (uint16_t)(total / 4u));
    x11_put32(pkt, 4, wid);
    x11_put32(pkt, 8, X_XA_WM_NAME);
    x11_put32(pkt, 12, X_XA_STRING);
    pkt[16] = 8;
    x11_put32(pkt, 20, (uint32_t)len);
    memcpy(pkt + 24, title, len);
    (void)x11_send_all(c->fd, pkt, total);
}

static inline void x11_map_window(X11Conn* c, uint32_t wid) {
    uint8_t pkt[8];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = X_MapWindow;
    x11_put16(pkt, 2, 2);
    x11_put32(pkt, 4, wid);
    (void)x11_send_all(c->fd, pkt, sizeof(pkt));
}

static inline uint32_t x11_create_gc(X11Conn* c, uint32_t wid, uint32_t fg, uint32_t bg) {
    uint32_t gc = x11_alloc_id(c);
    uint8_t pkt[24];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = X_CreateGC;
    x11_put16(pkt, 2, 6);
    x11_put32(pkt, 4, gc);
    x11_put32(pkt, 8, wid);
    x11_put32(pkt, 12, X_GCForeground | X_GCBackground);
    x11_put32(pkt, 16, fg);
    x11_put32(pkt, 20, bg);
    if (x11_send_all(c->fd, pkt, sizeof(pkt)) < 0) return 0;
    return gc;
}

static inline void x11_set_gc_colors(X11Conn* c, uint32_t gc, uint32_t fg, uint32_t bg) {
    uint8_t pkt[20];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = X_ChangeGC;
    x11_put16(pkt, 2, 5);
    x11_put32(pkt, 4, gc);
    x11_put32(pkt, 8, X_GCForeground | X_GCBackground);
    x11_put32(pkt, 12, fg);
    x11_put32(pkt, 16, bg);
    (void)x11_send_all(c->fd, pkt, sizeof(pkt));
}

static inline void x11_fill_rect(X11Conn* c, uint32_t wid, uint32_t gc,
                                 int16_t x, int16_t y, uint16_t w, uint16_t h) {
    uint8_t pkt[20];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = X_PolyFillRectangle;
    x11_put16(pkt, 2, 5);
    x11_put32(pkt, 4, wid);
    x11_put32(pkt, 8, gc);
    x11_put16(pkt, 12, (uint16_t)x);
    x11_put16(pkt, 14, (uint16_t)y);
    x11_put16(pkt, 16, w);
    x11_put16(pkt, 18, h);
    (void)x11_send_all(c->fd, pkt, sizeof(pkt));
}

static inline void x11_draw_text(X11Conn* c, uint32_t wid, uint32_t gc,
                                 int16_t x, int16_t y, const char* text) {
    if (!text) return;
    size_t len = strlen(text);
    if (len == 0) return;
    if (len > 200) len = 200;
    size_t pad = (4u - (len & 3u)) & 3u;
    size_t total = 16u + len + pad;
    uint8_t pkt[224];
    memset(pkt, 0, total);
    pkt[0] = X_ImageText8;
    pkt[1] = (uint8_t)len;
    x11_put16(pkt, 2, (uint16_t)(total / 4u));
    x11_put32(pkt, 4, wid);
    x11_put32(pkt, 8, gc);
    x11_put16(pkt, 12, (uint16_t)x);
    x11_put16(pkt, 14, (uint16_t)y);
    memcpy(pkt + 16, text, len);
    (void)x11_send_all(c->fd, pkt, total);
}

/* Custom X11 client message to update server desktop theme or mouse speed */
static inline void x11_send_desktop_cfg(X11Conn* c, uint32_t bg_color, uint32_t accent_color, uint32_t mouse_speed) {
    uint8_t pkt[24];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = X_ChangeWindowAttr;
    x11_put16(pkt, 2, 6);
    x11_put32(pkt, 4, c->root);
    x11_put32(pkt, 8, 0x424F5301u); /* BlockOS X11 desktop theme config marker */
    x11_put32(pkt, 12, bg_color);
    x11_put32(pkt, 16, accent_color);
    x11_put32(pkt, 20, mouse_speed);
    (void)x11_send_all(c->fd, pkt, sizeof(pkt));
}

/* Non-blocking poll for one 32-byte X11 event. Returns 1 if event read, 0 if none, -1 on EOF. */
static inline int x11_poll_event(X11Conn* c, X11Event* ev) {
    uint8_t raw[32];
    ssize_t r = read(c->fd, raw, sizeof(raw));
    if (r < 0 && errno == EAGAIN) return 0;
    if (r <= 0) return -1;
    size_t got = (size_t)r;
    while (got < sizeof(raw)) {
        ssize_t m = read(c->fd, raw + got, sizeof(raw) - got);
        if (m < 0 && errno == EAGAIN) { x11_sleep_ms(1); continue; }
        if (m <= 0) return -1;
        got += (size_t)m;
    }
    memset(ev, 0, sizeof(*ev));
    ev->type    = raw[0] & 0x7Fu;
    ev->detail  = raw[1];
    ev->seq     = x11_get16(raw, 2);
    ev->time_ms = x11_get32(raw, 4);
    ev->root    = x11_get32(raw, 8);
    ev->window  = x11_get32(raw, 12);
    ev->root_x  = (int16_t)x11_get16(raw, 20);
    ev->root_y  = (int16_t)x11_get16(raw, 22);
    ev->event_x = (int16_t)x11_get16(raw, 24);
    ev->event_y = (int16_t)x11_get16(raw, 26);
    ev->state   = x11_get16(raw, 28);
    ev->ascii   = raw[30];
    return 1;
}
