#pragma once
#include "../core/Widget.hpp"
#include <string>

class Button : public Widget
{
public:
std::string text;
bool pressed;

```
Button(int x, int y, int w, int h, std::string text);

void draw(uint32_t* framebuffer, int screenWidth) override;
void click();
```

};
