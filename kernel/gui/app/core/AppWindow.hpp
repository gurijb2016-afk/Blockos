#pragma once
#include <string>

class AppWindow
{
public:
int x;
int y;
int width;
int height;
std::string title;

```
AppWindow(int x, int y, int w, int h, const char* title);
void show();
```

};
