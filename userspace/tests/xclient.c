/* Raw X11 protocol client (no Xlib). Connects to the X server on :0 over the
 * Unix socket /tmp/.X11-unix/X0, creates one window on the root, gives it a
 * GC, fills a rectangle with a colour and then reads events until the server
 * closes the connection. Used as the first client test for the BlockOS X port.
 *
 * Start the server first, then this client in the background:
 *   /system/bin/X :0 -extension Composite -extension DAMAGE &
 *   /bin/xclient
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int execve(const char *path, char *const argv[], char *const envp[]);

static int fd = -1;

static void die(const char *what) {
    printf("xclient: %s failed\n", what);
    _exit(1);
}

/* BlockOS unix sockets are non-blocking: an empty read or a full write
 * returns EAGAIN, so back off briefly and retry. */
static void wait_a_little(void) {
    struct timespec ts = { 0, 1000000 };   /* 1 ms */
    nanosleep(&ts, 0);
}

static void put_all(const void *buf, size_t n) {
    const unsigned char *p = buf;
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w < 0 && errno == EAGAIN) { wait_a_little(); continue; }
        if (w <= 0) die("write");
        p += w;
        n -= (size_t) w;
    }
}

static void get_all(void *buf, size_t n) {
    unsigned char *p = buf;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r < 0 && errno == EAGAIN) { wait_a_little(); continue; }
        if (r <= 0) die("read");
        p += r;
        n -= (size_t) r;
    }
}

static void put16(unsigned char *b, int off, uint16_t v) { b[off] = v & 0xff; b[off + 1] = v >> 8; }
static void put32(unsigned char *b, int off, uint32_t v) {
    b[off] = v & 0xff; b[off + 1] = (v >> 8) & 0xff; b[off + 2] = (v >> 16) & 0xff; b[off + 3] = v >> 24;
}
static uint32_t get32(const unsigned char *b, int off) {
    return (uint32_t) b[off] | ((uint32_t) b[off + 1] << 8) |
           ((uint32_t) b[off + 2] << 16) | ((uint32_t) b[off + 3] << 24);
}
static uint16_t get16(const unsigned char *b, int off) {
    return (uint16_t) (b[off] | (b[off + 1] << 8));
}

int main(void) {
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    struct sockaddr_un sa;
    memset(&sa, 0, sizeof sa);
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "/tmp/.X11-unix/X0");
    if (connect(fd, (struct sockaddr *) &sa, sizeof sa) < 0) {
        /* No server yet: start it ourselves (test convenience), then retry. */
        pid_t pid = fork();
        if (pid == 0) {
            char *const xargv[] = { "X", ":0", 0 };
            char *const xenv[] = { 0 };
            execve("/system/bin/X", xargv, xenv);
            _exit(127);
        }
        int tries = 0;
        for (;;) {
            sleep(1);
            close(fd);
            fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) die("socket");
            if (connect(fd, (struct sockaddr *) &sa, sizeof sa) == 0) break;
            if (++tries >= 30) die("connect (server did not come up)");
        }
        printf("xclient: started X and connected after %d s\n", tries + 1);
    }

    /* Setup request: little-endian, protocol 11.0, no authorization. */
    unsigned char req[12];
    memset(req, 0, sizeof req);
    req[0] = 'l';
    put16(req, 2, 11);
    put16(req, 4, 0);
    put_all(req, sizeof req);

    printf("xclient: setup request sent\n");
    /* Setup reply header (8 bytes), then the body whose length is in words. */
    unsigned char hdr[8];
    get_all(hdr, sizeof hdr);
    if (hdr[0] != 1) {
        printf("xclient: setup refused (status %d)\n", hdr[0]);
        return 2;
    }
    uint32_t words = get16(hdr, 6);
    unsigned char body[65536];
    if (words * 4u > sizeof body) die("setup too large");
    get_all(body, words * 4u);

    uint32_t rid_base = get32(body, 4);
    uint16_t vendor_len = get16(body, 16);
    uint8_t nformats = body[21];
    int vpad = (vendor_len + 3) & ~3;
    int screen = 32 + vpad + 8 * nformats;     /* body offset of screen 0 */
    uint32_t root = get32(body, screen);
    uint32_t root_visual = get32(body, screen + 32);
    uint8_t root_depth = body[screen + 38];
    uint16_t width = get16(body, screen + 20);
    uint16_t height = get16(body, screen + 22);
    printf("xclient: connected, root=0x%x %ux%u depth=%u visual=0x%x\n",
           root, width, height, root_depth, root_visual);

    uint32_t win = rid_base | 1;
    uint32_t gc = rid_base | 2;

    /* CreateWindow: 10 words, background + event mask (ExposureMask|KeyPress|ButtonPress). */
    unsigned char r[40];
    memset(r, 0, sizeof r);
    r[0] = 1;                          /* CreateWindow */
    r[1] = 0;                          /* depth: CopyFromParent */
    put16(r, 2, 10);
    put32(r, 4, win);
    put32(r, 8, root);
    put16(r, 12, 100);                 /* x */
    put16(r, 14, 100);                 /* y */
    put16(r, 16, 320);                 /* width */
    put16(r, 18, 200);                 /* height */
    put16(r, 20, 0);                   /* border */
    put16(r, 22, 1);                   /* InputOutput */
    put32(r, 24, 0);                   /* visual: CopyFromParent */
    put32(r, 28, 0x0802);              /* CWBackPixel | CWEventMask */
    put32(r, 32, 0x00204080);          /* background */
    put32(r, 36, 0x0000800d);          /* Exposure|ButtonPress|KeyPress|StructureNotify */
    put_all(r, sizeof r);

    /* MapWindow */
    unsigned char m[8] = {0};
    m[0] = 8;
    put16(m, 2, 2);
    put32(m, 4, win);
    put_all(m, sizeof m);

    /* CreateGC with foreground */
    unsigned char g[20];
    memset(g, 0, sizeof g);
    g[0] = 55;
    put16(g, 2, 5);
    put32(g, 4, gc);
    put32(g, 8, win);
    put32(g, 12, 0x4);                 /* GCForeground */
    put32(g, 16, 0x00ff8000);          /* orange (TrueColor 0xRRGGBB) */
    put_all(g, sizeof g);

    /* PolyFillRectangle: one rectangle covering most of the window. Redone on Expose. */
    unsigned char f[20];
    memset(f, 0, sizeof f);
    f[0] = 70;
    put16(f, 2, 5);
    put32(f, 4, win);
    put32(f, 8, gc);
    put16(f, 12, 20);
    put16(f, 14, 20);
    put16(f, 16, 280);
    put16(f, 18, 160);
    put_all(f, sizeof f);

    printf("xclient: window mapped and filled, waiting for events\n");

    /* Event loop: every event is 32 bytes. Print the type codes. */
    for (;;) {
        unsigned char ev[32];
        get_all(ev, sizeof ev);
        int type = ev[0] & 0x7f;
        if (type == 0) {               /* Error */
            printf("xclient: X error code %u opcode %u\n", ev[1], ev[10]);
            continue;
        }
        printf("xclient: event %d\n", type);
        if (type == 12)                /* Expose: the window content was lost, draw again */
            put_all(f, sizeof f);
    }
    return 0;
}
