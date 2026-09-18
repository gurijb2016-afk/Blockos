#include <kdrive-config.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "kdrive.h"
#include "blockos.h"

static BlockOSDisplayInfo g_info;
static void *g_fb_map = NULL;
static void *g_fb_base = NULL;
static size_t g_fb_map_len = 0;
static int g_fb_fd = -1;

static Bool
read_display_info(BlockOSDisplayInfo *out)
{
    int fd;
    ssize_t n;

    if (!out)
        return FALSE;

    fd = open(BLOCKOS_DISPLAY_INFO_PATH, O_RDONLY);
    if (fd < 0)
        return FALSE;

    n = read(fd, out, sizeof(*out));
    close(fd);

    if (n != (ssize_t) sizeof(*out))
        return FALSE;

    if (out->magic != BLOCKOS_FB_MAGIC ||
        out->version != BLOCKOS_PROTOCOL_VERSION)
        return FALSE;

    if (!out->framebuffer_phys ||
        !out->framebuffer_size ||
        !out->width ||
        !out->height ||
        out->stride < out->width ||
        out->bpp != 32)
        return FALSE;

    if (out->height > UINT64_MAX / out->stride)
        return FALSE;

    if ((uint64_t) out->stride * out->height >
        UINT64_MAX / (out->bpp / 8u))
        return FALSE;

    return TRUE;
}

static Bool
map_framebuffer(void)
{
    const uint64_t page_size = 4096u;
    uint64_t base;
    uint64_t aligned;
    uint64_t delta;
    uint64_t map_len64;
    size_t len;
    void *mapped;

    base = g_info.framebuffer_phys;
    aligned = base & ~((uint64_t) page_size - 1);
    delta = base - aligned;

    if (g_info.framebuffer_size > UINT64_MAX - delta - (uint64_t) page_size + 1)
        return FALSE;

    map_len64 = (g_info.framebuffer_size + delta + (uint64_t) page_size - 1) &
                ~((uint64_t) page_size - 1);

    if (map_len64 == 0 || map_len64 > (uint64_t) SIZE_MAX)
        return FALSE;

    len = (size_t) map_len64;

    g_fb_fd = open(BLOCKOS_DISPLAY_DEVICE_PATH, O_RDWR);
    if (g_fb_fd < 0)
        return FALSE;

    mapped = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
    if (mapped == MAP_FAILED) {
        close(g_fb_fd);
        g_fb_fd = -1;
        return FALSE;
    }

    g_fb_map = mapped;
    g_fb_base = (uint8_t *) mapped + delta;
    g_fb_map_len = len;

    return TRUE;
}

static void
unmap_framebuffer(void)
{
    if (g_fb_map && g_fb_map != MAP_FAILED)
        munmap(g_fb_map, g_fb_map_len);

    if (g_fb_fd >= 0)
        close(g_fb_fd);

    g_fb_map = NULL;
    g_fb_base = NULL;
    g_fb_map_len = 0;
    g_fb_fd = -1;
}

Bool
blockosCardInit(KdCardInfo *card)
{
    (void) card;

    memset(&g_info, 0, sizeof(g_info));

    if (!read_display_info(&g_info)) {
        ErrorF("BlockOS KDrive: cannot read %s\n", BLOCKOS_DISPLAY_INFO_PATH);
        return FALSE;
    }

    if (!map_framebuffer()) {
        ErrorF("BlockOS KDrive: cannot mmap %s\n", BLOCKOS_DISPLAY_DEVICE_PATH);
        return FALSE;
    }

    return TRUE;
}

Bool
blockosScreenInit(KdScreenInfo *screen)
{
    if (!screen || !g_fb_base)
        return FALSE;

    screen->width = (int) g_info.width;
    screen->height = (int) g_info.height;
    screen->rate = 60;
    screen->fb.depth = (int) g_info.depth;
    screen->fb.bitsPerPixel = (int) g_info.bpp;
    screen->fb.pixelStride = (int) g_info.stride;
    screen->fb.byteStride = (int) (g_info.stride * (g_info.bpp / 8));
    screen->fb.shadow = FALSE;
    screen->fb.frameBuffer = (CARD8 *) g_fb_base;
    screen->fb.visuals = (1UL << TrueColor);
    screen->fb.redMask = 0x00FF0000;
    screen->fb.greenMask = 0x0000FF00;
    screen->fb.blueMask = 0x000000FF;
    screen->softCursor = TRUE;
    screen->dumb = TRUE;

    return TRUE;
}

Bool blockosInitScreen(ScreenPtr pScreen) { (void) pScreen; return TRUE; }
Bool blockosFinishInitScreen(ScreenPtr pScreen) { (void) pScreen; return TRUE; }
Bool blockosCreateResources(ScreenPtr pScreen) { (void) pScreen; return TRUE; }
void blockosPreserve(KdCardInfo *card) { (void) card; }
Bool blockosEnable(ScreenPtr pScreen) { (void) pScreen; return TRUE; }
Bool blockosDPMS(ScreenPtr pScreen, int mode) { (void) pScreen; (void) mode; return TRUE; }
void blockosDisable(ScreenPtr pScreen) { (void) pScreen; }
void blockosRestore(KdCardInfo *card) { (void) card; }
void blockosScreenFini(KdScreenInfo *screen) { (void) screen; }
void blockosCardFini(KdCardInfo *card) { (void) card; unmap_framebuffer(); }

KdCardFuncs blockosFuncs = {
    .cardinit = blockosCardInit,
    .scrinit = blockosScreenInit,
    .initScreen = blockosInitScreen,
    .finishInitScreen = blockosFinishInitScreen,
    .createRes = blockosCreateResources,
    .preserve = blockosPreserve,
    .enable = blockosEnable,
    .dpms = blockosDPMS,
    .disable = blockosDisable,
    .restore = blockosRestore,
    .scrfini = blockosScreenFini,
    .cardfini = blockosCardFini,
};
