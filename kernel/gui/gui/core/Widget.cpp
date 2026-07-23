#include "Widget.hpp"

Widget::Widget(int x, int y, int w, int h)
{
this->x = x;
this->y = y;
width = w;
height = h;
visible = true;
}

bool Widget::contains(int mx, int my)
{
return mx >= x && mx <= x + width && my >= y && my <= y + height;
}

void Widget::draw(uint32_t* framebuffer, int screenWidth)
{
for (int iy = 0; iy < height; iy++)
{
for (int ix = 0; ix < width; ix++)
{
framebuffer[(y + iy) * screenWidth + (x + ix)] = 0x303030;
}
}
}
