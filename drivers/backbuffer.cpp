#include "backbuffer.h"
#include "font8x8.h"
#include <string.h>

static inline uint32_t to_pixel(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void bb_put_pixel(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, uint32_t color) {
    uint32_t idx = (y * width + x);
    uint8_t *p = buf + idx * 4;
    memcpy(p, &color, 4);
}

void bb_draw_rect(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t yy = y; yy < y + h; ++yy) {
        if (yy >= 0xFFFFFFFF) break; // defensive
        for (uint32_t xx = x; xx < x + w; ++xx) {
            if (xx >= width) break;
            bb_put_pixel(buf, width, xx, yy, color);
        }
    }
}

void bb_clear(uint8_t* buf, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            bb_put_pixel(buf, width, x, y, color);
        }
    }
}

void bb_draw_char(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, char c, uint32_t color) {
    if ((unsigned char)c >= 128) return;
    const unsigned char *glyph = font8x8_basic[(unsigned char)c];
    for (int row=0; row<8; ++row) {
        unsigned char bits = glyph[row];
        for (int col=0; col<8; ++col) {
            if (bits & (1<<col)) bb_put_pixel(buf, width, x+col, y+row, color);
        }
    }
}

void bb_draw_text(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, const char* text, uint32_t color) {
    for (size_t i=0; text[i]; ++i) {
        bb_draw_char(buf, width, x + (int)i*8, y, text[i], color);
    }
}

void bb_blit_to_fb(Framebuffer* fb, const uint8_t* backbuf) {
    // fb->PixelsPerScanLine may be >= fb->Width. Copy row by row.
    for (uint32_t row=0; row<fb->Height; ++row) {
        uint8_t *dest = fb->Base + (row * fb->PixelsPerScanLine) * fb->PixelsPerPixel;
        const uint8_t *src = backbuf + (row * fb->Width) * 4;
        memcpy(dest, src, fb->Width * 4);
    }
}

void bb_blit_region_to_fb(Framebuffer* fb, const uint8_t* backbuf, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (x >= fb->Width || y >= fb->Height) return;
    uint32_t maxx = (x + w > fb->Width) ? fb->Width : x + w;
    uint32_t maxy = (y + h > fb->Height) ? fb->Height : y + h;
    for (uint32_t row = y; row < maxy; ++row) {
        uint8_t* dest = fb->Base + (row * fb->PixelsPerScanLine) * fb->PixelsPerPixel + x * 4;
        const uint8_t* src = backbuf + (row * fb->Width + x) * 4;
        memcpy(dest, src, (maxx - x) * 4);
    }
}
