#include "StartMenu.hpp"

StartMenu::StartMenu()
{
opened = false;
}

void StartMenu::toggle()
{
opened = !opened;
}

void StartMenu::draw(uint32_t* framebuffer, int width)
{
if (!opened) return;

```
for (int y = 300; y < 650; y++)
{
    for (int x = 50; x < 350; x++)
    {
        framebuffer[y * width + x] = 0x303030;
    }
}
```

}
