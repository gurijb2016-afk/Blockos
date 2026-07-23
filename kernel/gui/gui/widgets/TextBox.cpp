#include "TextBox.hpp"

TextBox::TextBox(int x, int y, int w, int h)
: Widget(x, y, w, h)
{
active = false;
}

void TextBox::keyInput(char c)
{
if (active)
value.push_back(c);
}

void TextBox::draw(uint32_t* framebuffer, int screenWidth)
{
for (int iy = 0; iy < height; iy++)
{
for (int ix = 0; ix < width; ix++)
{
framebuffer[(y + iy) * screenWidth + (x + ix)] = 0x101010;
}
}
}
