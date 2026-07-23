#include "WindowManager.hpp"
#include <algorithm>

void WindowManager::addWindow(Window* window)
{
windows.push_back(window);
}

void WindowManager::removeWindow(Window* window)
{
windows.erase(std::remove(windows.begin(), windows.end(), window), windows.end());
}

void WindowManager::focusWindow(Window* window)
{
for (auto w : windows)
w->focused = false;

```
window->focused = true;

windows.erase(std::remove(windows.begin(), windows.end(), window), windows.end());
windows.push_back(window);
```

}

void WindowManager::mouseDown(int x, int y)
{
for (auto it = windows.rbegin(); it != windows.rend(); ++it)
{
if ((*it)->inside(x, y))
{
focusWindow(*it);
(*it)->mouseDown(x, y);
break;
}
}
}

void WindowManager::mouseMove(int x, int y)
{
for (auto w : windows)
w->mouseMove(x, y);
}

void WindowManager::mouseUp()
{
for (auto w : windows)
w->mouseUp();
}

void WindowManager::drawAll(uint32_t* framebuffer, int screenWidth)
{
for (auto w : windows)
w->draw(framebuffer, screenWidth);
}
