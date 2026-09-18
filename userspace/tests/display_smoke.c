#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define BLOCKOS_FB_MAGIC 0x424F5346u
#define BLOCKOS_PROTOCOL_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t framebuffer_phys;
    uint64_t framebuffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t bpp;
    uint32_t depth;
} BlockOSDisplayInfo;

int main(void) {
    int fd = open("/system/display.info", O_RDONLY);
    if (fd < 0) {
        printf("display_smoke: open failed errno=%d\n", errno);
        return 1;
    }
    BlockOSDisplayInfo d;
    memset(&d, 0, sizeof(d));
    ssize_t n = read(fd, &d, sizeof(d));
    close(fd);
    if (n != (ssize_t)sizeof(d)) {
        printf("display_smoke: short read (%ld)\n", n);
        return 2;
    }
    if (d.magic != BLOCKOS_FB_MAGIC || d.version != BLOCKOS_PROTOCOL_VERSION ||
        d.width == 0 || d.height == 0 || d.bpp != 32) {
        printf("display_smoke: invalid display ABI\n");
        return 3;
    }
    printf("display_smoke: %ux%u stride=%u bpp=%u fb=%llx size=%llu\n",
           (unsigned)d.width, (unsigned)d.height, (unsigned)d.stride,
           (unsigned)d.bpp,
           (unsigned long long)d.framebuffer_phys,
           (unsigned long long)d.framebuffer_size);
    return 0;
}
