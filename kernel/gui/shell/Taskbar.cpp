#include "Taskbar.hpp"

Taskbar::Taskbar()
{
height = 45;
}

void Taskbar::draw(uint32_t* framebuffer, int width, int screenHeight)
{
for (int y = screenHeight - height; y < screenHeight; y++)
{
for (int x = 0; x < width; x++)
{
framebuffer[y * width + x] = 0x202020;
}
}
}
