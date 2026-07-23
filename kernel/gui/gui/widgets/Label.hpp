#pragma once
#include "../core/Widget.hpp"
#include <string>

class Label : public Widget
{
public:
std::string text;

```
Label(int x, int y, std::string text);

void draw(uint32_t* framebuffer, int screenWidth) override;
```

};
