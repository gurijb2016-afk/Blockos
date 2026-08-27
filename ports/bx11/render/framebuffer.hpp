#pragma once
#include <cstdint>
#include <cstddef>

namespace blockos::bx11::render {

struct Framebuffer {
    uint32_t *pixels{};
    uint32_t width{};
    uint32_t height{};
    uint32_t stride{};
};

class Surface {
public:
    bool bind(Framebuffer fb);
    void clear(uint32_t argb);
    void pixel(int x, int y, uint32_t argb);
    void rect(int x, int y, int w, int h, uint32_t argb);
    void copy(int sx, int sy, int w, int h, int dx, int dy);
    const Framebuffer &framebuffer() const { return fb_; }
private:
    Framebuffer fb_{};
};

}
