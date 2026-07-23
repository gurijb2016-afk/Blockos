#pragma once
#include <vector>
#include "Window.hpp"

class WindowManager
{
private:
std::vector<Window*> windows;

public:
void addWindow(Window* window);
void removeWindow(Window* window);
void focusWindow(Window* window);

```
void mouseDown(int x, int y);
void mouseMove(int x, int y);
void mouseUp();

void drawAll(uint32_t* framebuffer, int screenWidth);
```

};
