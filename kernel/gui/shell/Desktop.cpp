#include "Desktop.hpp"

Desktop::Desktop()
{
background = 0x101820;
}

void Desktop::draw(uint32_t* framebuffer, int width, int height)
{
for (int y = 0; y < height; y++)
{
for (int x = 0; x < width; x++)
{
framebuffer[y * width + x] = background;
}
}
}
