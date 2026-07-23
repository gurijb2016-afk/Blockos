#pragma once
#include <cstdint>
#include <string>

class Window
{
public:
int x;
int y;
int width;
int height;
bool focused;
bool dragging;
bool closed;
int dragOffsetX;
int dragOffsetY;
std::string title;

```
Window(int x, int y, int width, int height, std::string title);

void draw(uint32_t* framebuffer, int screenWidth);
bool inside(int mx, int my);
void mouseDown(int mx, int my);
void mouseMove(int mx, int my);
void mouseUp();
```

};
