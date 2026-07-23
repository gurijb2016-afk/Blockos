#pragma once
#include <cstdint>

class Widget
{
public:
int x;
int y;
int width;
int height;
bool visible;

```
Widget(int x, int y, int w, int h);
virtual ~Widget() = default;

virtual void draw(uint32_t* framebuffer, int screenWidth);
virtual void onClick() {}
virtual void onKey(char key) {}

bool contains(int mx, int my);
```

};
