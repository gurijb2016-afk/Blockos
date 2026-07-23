#pragma once
#include "Panel.hpp"

class Menu : public Panel
{
public:
bool opened;

```
Menu(int x, int y, int w, int h);

void toggle();
```

};
