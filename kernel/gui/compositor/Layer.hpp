#pragma once
#include <cstdint>

class Layer
{
public:
uint32_t* buffer;
int x;
int y;
int width;
int height;
uint8_t opacity;

```
Layer(uint32_t* buf, int x, int y, int w, int h);
```

};
