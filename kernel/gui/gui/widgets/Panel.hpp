#pragma once
#include "../core/Widget.hpp"
#include <vector>

class Panel : public Widget
{
public:
std::vector<Widget*> children;

```
Panel(int x, int y, int w, int h);

void add(Widget* widget);
void draw(uint32_t* framebuffer, int screenWidth) override;
```

};
