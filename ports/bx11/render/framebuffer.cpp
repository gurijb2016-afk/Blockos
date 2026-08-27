#include "framebuffer.hpp"

namespace blockos::bx11::render {

bool Surface::bind(Framebuffer fb)
{
    fb_ = fb;
    return fb_.pixels && fb_.width && fb_.height && fb_.stride >= fb_.width;
}

void Surface::clear(uint32_t argb)
{
    if (!fb_.pixels) return;
    for (uint32_t y = 0; y < fb_.height; ++y) {
        for (uint32_t x = 0; x < fb_.width; ++x)
            fb_.pixels[y * fb_.stride + x] = argb;
    }
}

void Surface::pixel(int x, int y, uint32_t argb)
{
    if (!fb_.pixels || x < 0 || y < 0 || x >= static_cast<int>(fb_.width) || y >= static_cast<int>(fb_.height)) return;
    fb_.pixels[y * fb_.stride + x] = argb;
}

void Surface::rect(int x, int y, int w, int h, uint32_t argb)
{
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx)
            pixel(x + xx, y + yy, argb);
}

void Surface::copy(int sx, int sy, int w, int h, int dx, int dy)
{
    if (!fb_.pixels || w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            int srcx = sx + xx, srcy = sy + yy;
            int dstx = dx + xx, dsty = dy + yy;
            if (srcx < 0 || srcy < 0 || dstx < 0 || dsty < 0 ||
                srcx >= static_cast<int>(fb_.width) || dstx >= static_cast<int>(fb_.width) ||
                srcy >= static_cast<int>(fb_.height) || dsty >= static_cast<int>(fb_.height)) continue;
            uint32_t p = fb_.pixels[srcy * fb_.stride + srcx];
            fb_.pixels[dsty * fb_.stride + dstx] = p;
        }
    }
}

}
