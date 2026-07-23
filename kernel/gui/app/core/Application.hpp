#pragma once
#include "AppWindow.hpp"

class Application
{
public:
AppWindow* window = nullptr;

```
virtual ~Application() = default;
virtual void start();

void createWindow(int x, int y, int w, int h, const char* title);
```

};
