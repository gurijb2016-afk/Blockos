#pragma once
#include <stdint.h>

typedef struct Framebuffer {
    uint8_t* Base;
    uint32_t Width;
    uint32_t Height;
    uint32_t PixelsPerScanLine;
    uint32_t PixelsPerPixel;
} Framebuffer;

void fb_put_pixel(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t color);
void fb_draw_rect(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_clear(Framebuffer* fb, uint32_t color);
