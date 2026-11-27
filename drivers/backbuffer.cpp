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

static inline void fast_fill_dwords(void* dst, uint32_t color, size_t dwords) {
    uint64_t pair = (static_cast<uint64_t>(color) << 32) | static_cast<uint64_t>(color);
    size_t qwords = dwords >> 1;
    void* d = dst;
    if (qwords) {
        __asm__ volatile(
            "rep stosq"
            : "+D"(d), "+c"(qwords)
            : "a"(pair)
            : "memory"
        );
    }
    if (dwords & 1u) {
        *reinterpret_cast<uint32_t*>(d) = color;
    }
}

void bb_draw_rect(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (x >= width || w == 0 || h == 0) return;
    uint32_t cols = (x + w > width) ? (width - x) : w;
    for (uint32_t yy = y; yy < y + h; ++yy) {
        uint8_t* row_ptr = buf + (static_cast<size_t>(yy) * width + x) * 4;
        fast_fill_dwords(row_ptr, color, cols);
    }
}

void bb_clear(uint8_t* buf, uint32_t width, uint32_t height, uint32_t color) {
    fast_fill_dwords(buf, color, static_cast<size_t>(width) * static_cast<size_t>(height));
}

void bb_draw_char(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, char c, uint32_t color) {
    if ((unsigned char)c >= 128) return;
    const unsigned char *glyph = font8x8_basic[(unsigned char)c];
    uint32_t* dst = reinterpret_cast<uint32_t*>(buf) + static_cast<size_t>(y) * width + x;
    for (int row = 0; row < 8; ++row) {
        unsigned char bits = glyph[row];
        if (bits) {
            if (bits & 0x01u) dst[0] = color;
            if (bits & 0x02u) dst[1] = color;
            if (bits & 0x04u) dst[2] = color;
            if (bits & 0x08u) dst[3] = color;
            if (bits & 0x10u) dst[4] = color;
            if (bits & 0x20u) dst[5] = color;
            if (bits & 0x40u) dst[6] = color;
            if (bits & 0x80u) dst[7] = color;
        }
        dst += width;
    }
}

void bb_draw_text(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, const char* text, uint32_t color) {
    for (size_t i=0; text[i]; ++i) {
        bb_draw_char(buf, width, x + (int)i*8, y, text[i], color);
    }
}

static inline void fast_copy_dwords(void* dst, const void* src, size_t dwords) {
    size_t qwords = dwords >> 1;
    void* d = dst;
    const void* s = src;
    if (qwords) {
        __asm__ volatile(
            "rep movsq"
            : "+D"(d), "+S"(s), "+c"(qwords)
            :
            : "memory"
        );
    }
    if (dwords & 1u) {
        *reinterpret_cast<uint32_t*>(d) = *reinterpret_cast<const uint32_t*>(s);
    }
}

void bb_blit_to_fb(Framebuffer* fb, const uint8_t* backbuf) {
    if (fb->PixelsPerPixel == 4 && fb->PixelsPerScanLine == fb->Width) {
        fast_copy_dwords(fb->Base, backbuf, static_cast<size_t>(fb->Width) * fb->Height);
        return;
    }
    // fb->PixelsPerScanLine may be >= fb->Width. Copy row by row.
    for (uint32_t row = 0; row < fb->Height; ++row) {
        uint8_t *dest = fb->Base + (row * fb->PixelsPerScanLine) * fb->PixelsPerPixel;
        const uint8_t *src = backbuf + (row * fb->Width) * 4;
        if (fb->PixelsPerPixel == 4) {
            fast_copy_dwords(dest, src, fb->Width);
        } else {
            memcpy(dest, src, fb->Width * 4);
        }
    }
}

void bb_blit_region_to_fb(Framebuffer* fb, const uint8_t* backbuf, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (x >= fb->Width || y >= fb->Height) return;
    uint32_t maxx = (x + w > fb->Width) ? fb->Width : x + w;
    uint32_t maxy = (y + h > fb->Height) ? fb->Height : y + h;
    uint32_t cols = maxx - x;
    for (uint32_t row = y; row < maxy; ++row) {
        uint8_t* dest = fb->Base + (row * fb->PixelsPerScanLine) * fb->PixelsPerPixel + x * 4;
        const uint8_t* src = backbuf + (row * fb->Width + x) * 4;
        if (fb->PixelsPerPixel == 4) {
            fast_copy_dwords(dest, src, cols);
        } else {
            memcpy(dest, src, cols * 4);
        }
    }
}
