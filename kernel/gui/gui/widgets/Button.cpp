#include "Button.hpp"

Button::Button(int x, int y, int w, int h, std::string text)
: Widget(x, y, w, h)
{
this->text = text;
pressed = false;
}

void Button::click()
{
pressed = true;
}

void Button::draw(uint32_t* framebuffer, int screenWidth)
{
uint32_t color = pressed ? 0x5050ff : 0x404040;

```
for (int iy = 0; iy < height; iy++)
{
    for (int ix = 0; ix < width; ix++)
    {
        framebuffer[(y + iy) * screenWidth + (x + ix)] = color;
    }
}
```

}
