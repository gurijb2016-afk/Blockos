#include "Label.hpp"

Label::Label(int x, int y, std::string text)
: Widget(x, y, 120, 24)
{
this->text = text;
}

void Label::draw(uint32_t* framebuffer, int screenWidth)
{
for (int iy = 0; iy < height; iy++)
{
for (int ix = 0; ix < width; ix++)
{
framebuffer[(y + iy) * screenWidth + (x + ix)] = 0x202020;
}
}
}
