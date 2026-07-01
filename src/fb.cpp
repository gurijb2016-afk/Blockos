#include "fb.h"
#include "font8x8.h"
#include <string.h>

static inline uint32_t to_pixel(uint8_t r, uint8_t g, uint8_t b) {
    // Compose as 0x00RRGGBB (we write 32-bit directly)
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void fb_put_pixel(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb->Width || y >= fb->Height) return;
    uint8_t *base = fb->Base + (y * fb->PixelsPerScanLine + x) * fb->PixelsPerPixel;
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

void fb_draw_char(Framebuffer* fb, uint32_t x, uint32_t y, char c, uint32_t color) {
    const unsigned char *glyph = font8x8_basic[(unsigned char)c];
    for (int row=0; row<8; ++row) {
        unsigned char bits = glyph[row];
        for (int col=0; col<8; ++col) {
            if (bits & (1<<col)) fb_put_pixel(fb, x+col, y+row, color);
        }
    }
}

void fb_draw_text(Framebuffer* fb, uint32_t x, uint32_t y, const char* text, uint32_t color) {
    for (size_t i=0; text[i]; ++i) {
        fb_draw_char(fb, x + i*8, y, text[i], color);
    }
}
