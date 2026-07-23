#pragma once
#include "../core/Widget.hpp"
#include <vector>

class VerticalLayout
{
public:
int x;
int y;
int spacing;

```
VerticalLayout(int x, int y);
void add(Widget* widget);
```

private:
std::vector<Widget*> children;
};
