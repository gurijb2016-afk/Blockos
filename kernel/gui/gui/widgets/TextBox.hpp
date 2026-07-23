#pragma once
#include "../core/Widget.hpp"
#include <string>

class TextBox : public Widget
{
public:
std::string value;
bool active;

```
TextBox(int x, int y, int w, int h);

void keyInput(char c);
void draw(uint32_t* framebuffer, int screenWidth) override;
```

};
