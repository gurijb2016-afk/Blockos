#pragma once
#include <stdint.h>

// Minimal GUI header placeholder so sources can include "gui.hpp".
// The real implementation should provide definitions (methods) in a .cpp file.

// Color macro: pack ARGB into 32-bit
#ifndef COLOR_ARGB
#define COLOR_ARGB(a,r,g,b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#endif

class GuiEngine {
public:
    GuiEngine() = default;
    // Render the current UI state to the framebuffer
    void render();

    // Draw a filled rectangle (x,y,w,h) with ARGB color
    void draw_rect(int x, int y, int w, int h, uint32_t color);

    // Draw text at (x,y) using the kernel's text renderer (optional)
    void draw_text(int x, int y, const char* text, uint32_t color);

    // Draw a single pixel
    void draw_pixel(int x, int y, uint32_t color);
};

extern GuiEngine desktop;
