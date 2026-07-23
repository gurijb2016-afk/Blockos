#include "Panel.hpp"

Panel::Panel(int x, int y, int w, int h)
: Widget(x, y, w, h)
{
}

void Panel::add(Widget* widget)
{
children.push_back(widget);
}

void Panel::draw(uint32_t* framebuffer, int screenWidth)
{
for (int iy = 0; iy < height; iy++)
{
for (int ix = 0; ix < width; ix++)
{
framebuffer[(y + iy) * screenWidth + (x + ix)] = 0x252525;
}
}

```
for (auto child : children)
    child->draw(framebuffer, screenWidth);
```

}
