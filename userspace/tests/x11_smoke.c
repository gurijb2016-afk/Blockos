#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int main(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { printf("x11_smoke: socket failed errno=%d\n", errno); return 1; }

    struct sockaddr_un a;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strcpy(a.sun_path, "/tmp/.X11-unix/X0");

    if (connect(fd, (const struct sockaddr*)&a,
                (socklen_t)(2 + strlen(a.sun_path) + 1)) < 0) {
        printf("x11_smoke: connect failed errno=%d\n", errno);
        close(fd);
        return 2;
    }

    /* X11 Setup request, no authentication. */
    uint8_t req[12] = {
        'l', 0,
        11, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    };
    if (write(fd, req, sizeof(req)) != (ssize_t)sizeof(req)) {
        printf("x11_smoke: setup write failed errno=%d\n", errno);
        close(fd);
        return 3;
    }

    uint8_t hdr[8];
    ssize_t n = read(fd, hdr, sizeof(hdr));
    if (n != (ssize_t)sizeof(hdr)) {
        printf("x11_smoke: short setup reply (%ld) errno=%d\n", n, errno);
        close(fd);
        return 4;
    }

    printf("x11_smoke: status=%u protocol=%u.%u extra_units=%u\n",
           (unsigned)hdr[0], (unsigned)le16(hdr + 2),
           (unsigned)le16(hdr + 4), (unsigned)le16(hdr + 6));

    if (hdr[0] != 1) {
        printf("x11_smoke: X server rejected connection\n");
        close(fd);
        return 5;
    }

    close(fd);
    printf("x11_smoke: X11 socket + setup handshake OK\n");
    return 0;
}
