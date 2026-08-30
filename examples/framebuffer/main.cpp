#include "drivers/fb.h"

void example_framebuffer(Framebuffer* fb)
{
    if (!fb)
        return;

    fb_draw_clear(fb, 0x00101820);
    fb_draw_rect(fb, 16, 16, 320, 180, 0x00204060);
    fb_draw_text(fb, 32, 32, "BlockOS framebuffer", 0x00FFFFFF);
}
