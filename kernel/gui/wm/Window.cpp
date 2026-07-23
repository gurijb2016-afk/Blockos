#include "Window.hpp"

Window::Window(int x, int y, int width, int height, std::string title)
{
this->x = x;
this->y = y;
this->width = width;
this->height = height;
this->title = title;
focused = false;
dragging = false;
closed = false;
dragOffsetX = 0;
dragOffsetY = 0;
}

bool Window::inside(int mx, int my)
{
return mx >= x && mx <= x + width && my >= y && my <= y + height;
}

void Window::mouseDown(int mx, int my)
{
if (inside(mx, my))
{
focused = true;
dragging = true;
dragOffsetX = mx - x;
dragOffsetY = my - y;
}
}

void Window::mouseMove(int mx, int my)
{
if (dragging)
{
x = mx - dragOffsetX;
y = my - dragOffsetY;
}
}

void Window::mouseUp()
{
dragging = false;
}

void Window::draw(uint32_t* framebuffer, int screenWidth)
{
uint32_t color = focused ? 0x303060 : 0x202020;

```
for (int iy = 0; iy < height; iy++)
{
    for (int ix = 0; ix < width; ix++)
    {
        framebuffer[(y + iy) * screenWidth + (x + ix)] = color;
    }
}

for (int i = 0; i < width; i++)
{
    framebuffer[y * screenWidth + x + i] = 0x505050;
}
```

}
