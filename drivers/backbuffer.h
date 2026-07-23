#pragma once
#include <cstdint>
#include "fb.h"

// Backbuffer API: allocate a buffer externally and pass pointer + stride equal to width*4
// These routines draw into the backbuffer memory. The memory layout is contiguous rows of width*4 bytes.

void bb_put_pixel(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, uint32_t color);
void bb_draw_rect(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void bb_clear(uint8_t* buf, uint32_t width, uint32_t height, uint32_t color);
void bb_draw_char(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, char c, uint32_t color);
void bb_draw_text(uint8_t* buf, uint32_t width, uint32_t x, uint32_t y, const char* text, uint32_t color);

// Blit backbuffer to framebuffer. backbuf is expected to be width*height*4 bytes (packed).
void bb_blit_to_fb(Framebuffer* fb, const uint8_t* backbuf);
// Blit region (x,y,w,h) from backbuffer to framebuffer
void bb_blit_region_to_fb(Framebuffer* fb, const uint8_t* backbuf, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
