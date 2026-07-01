#include "fb.h"
#include <string.h>

static inline uint32_t to_pixel(uint8_t r, uint8_t g, uint8_t b) {
    // Compose as 0x00RRGGBB (GOP may expect BGR or RGB depending on mode/format).
    // This simple implementation writes the 32-bit value directly.
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void fb_put_pixel(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb->Width || y >= fb->Height) return;
    uint8_t *base = fb->Base + (y * fb->PixelsPerScanLine + x) * fb->PixelsPerPixel;
    // assume 32-bit little-endian pixels (B,G,R,0)
    uint32_t val = color;
    memcpy(base, &val, 4);
}

void fb_draw_rect(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (x >= fb->Width || y >= fb->Height) return;
    uint32_t maxx = (x + w > fb->Width) ? fb->Width : x + w;
    uint32_t maxy = (y + h > fb->Height) ? fb->Height : y + h;
    for (uint32_t yy = y; yy < maxy; ++yy) {
        for (uint32_t xx = x; xx < maxx; ++xx) {
            fb_put_pixel(fb, xx, yy, color);
        }
    }
}

void fb_draw_clear(Framebuffer* fb, uint32_t color) {
    for (uint32_t y = 0; y < fb->Height; ++y) {
        for (uint32_t x = 0; x < fb->Width; ++x) {
            fb_put_pixel(fb, x, y, color);
        }
    }
}
