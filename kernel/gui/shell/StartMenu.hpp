#pragma once
#include <cstdint>

class StartMenu
{
public:
bool opened;

```
StartMenu();
void toggle();
void draw(uint32_t* framebuffer, int width);
```

};
