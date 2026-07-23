#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:-kuroko}"
mkdir -p "$ROOT"

echo "[1/7] Creating directory tree..."

mkdir -p "$ROOT"/{wm,compositor,graphics,input,gui/{core,widgets},shell,app/{core,runtime,apps/HelloKuro},loader/package,kpm}

echo "[2/7] Writing WM files..."

cat > "$ROOT/wm/Window.hpp" <<'EOF'
#pragma once
#include <cstdint>
#include <string>

class Window
{
public:
int x;
int y;
int width;
int height;
bool focused;
bool dragging;
bool closed;
int dragOffsetX;
int dragOffsetY;
std::string title;

```
Window(int x, int y, int width, int height, std::string title);

void draw(uint32_t* framebuffer, int screenWidth);
bool inside(int mx, int my);
void mouseDown(int mx, int my);
void mouseMove(int mx, int my);
void mouseUp();
```

};
EOF

cat > "$ROOT/wm/Window.cpp" <<'EOF'
#include "Window.hpp"

Window::Window(int x, int y, int width, int height, std::string title)
{
this->x = x;
this->y = y;
this->width = width;
this->height = height;
this->title = title;
focused = false;
dragging = false;
closed = false;
dragOffsetX = 0;
dragOffsetY = 0;
}

bool Window::inside(int mx, int my)
{
return mx >= x && mx <= x + width && my >= y && my <= y + height;
}

void Window::mouseDown(int mx, int my)
{
if (inside(mx, my))
{
focused = true;
dragging = true;
dragOffsetX = mx - x;
dragOffsetY = my - y;
}
}

void Window::mouseMove(int mx, int my)
{
if (dragging)
{
x = mx - dragOffsetX;
y = my - dragOffsetY;
}
}

void Window::mouseUp()
{
dragging = false;
}

void Window::draw(uint32_t* framebuffer, int screenWidth)
{
uint32_t color = focused ? 0x303060 : 0x202020;

```
for (int iy = 0; iy < height; iy++)
{
    for (int ix = 0; ix < width; ix++)
    {
        framebuffer[(y + iy) * screenWidth + (x + ix)] = color;
    }
}

for (int i = 0; i < width; i++)
{
    framebuffer[y * screenWidth + x + i] = 0x505050;
}
```

}
EOF

cat > "$ROOT/wm/WindowManager.hpp" <<'EOF'
#pragma once
#include <vector>
#include "Window.hpp"

class WindowManager
{
private:
std::vector<Window*> windows;

public:
void addWindow(Window* window);
void removeWindow(Window* window);
void focusWindow(Window* window);

```
void mouseDown(int x, int y);
void mouseMove(int x, int y);
void mouseUp();

void drawAll(uint32_t* framebuffer, int screenWidth);
```

};
EOF

cat > "$ROOT/wm/WindowManager.cpp" <<'EOF'
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
EOF

echo "[3/7] Writing compositor files..."

cat > "$ROOT/compositor/Layer.hpp" <<'EOF'
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
EOF

cat > "$ROOT/compositor/Layer.cpp" <<'EOF'
#include "Layer.hpp"

Layer::Layer(uint32_t* buf, int x, int y, int w, int h)
{
buffer = buf;
this->x = x;
this->y = y;
width = w;
height = h;
opacity = 255;
}
EOF

cat > "$ROOT/compositor/Compositor.hpp" <<'EOF'
#pragma once
#include <vector>
#include "Layer.hpp"

class Compositor
{
private:
std::vector<Layer*> layers;

public:
void addLayer(Layer* layer);
void render(uint32_t* screen, int screenWidth, int screenHeight);
};
EOF

cat > "$ROOT/compositor/Compositor.cpp" <<'EOF'
#include "Compositor.hpp"

void Compositor::addLayer(Layer* layer)
{
layers.push_back(layer);
}

void Compositor::render(uint32_t* screen, int screenWidth, int screenHeight)
{
for (auto layer : layers)
{
for (int y = 0; y < layer->height; y++)
{
for (int x = 0; x < layer->width; x++)
{
int sx = layer->x + x;
int sy = layer->y + y;

```
            if (sx < 0 || sy < 0 || sx >= screenWidth || sy >= screenHeight)
                continue;

            screen[sy * screenWidth + sx] = layer->buffer[y * layer->width + x];
        }
    }
}
```

}
EOF

echo "[4/7] Writing GUI toolkit files..."

cat > "$ROOT/gui/core/Widget.hpp" <<'EOF'
#pragma once
#include <cstdint>

class Widget
{
public:
int x;
int y;
int width;
int height;
bool visible;

```
Widget(int x, int y, int w, int h);
virtual ~Widget() = default;

virtual void draw(uint32_t* framebuffer, int screenWidth);
virtual void onClick() {}
virtual void onKey(char key) {}

bool contains(int mx, int my);
```

};
EOF

cat > "$ROOT/gui/core/Widget.cpp" <<'EOF'
#include "Widget.hpp"

Widget::Widget(int x, int y, int w, int h)
{
this->x = x;
this->y = y;
width = w;
height = h;
visible = true;
}

bool Widget::contains(int mx, int my)
{
return mx >= x && mx <= x + width && my >= y && my <= y + height;
}

void Widget::draw(uint32_t* framebuffer, int screenWidth)
{
for (int iy = 0; iy < height; iy++)
{
for (int ix = 0; ix < width; ix++)
{
framebuffer[(y + iy) * screenWidth + (x + ix)] = 0x303030;
}
}
}
EOF

cat > "$ROOT/gui/widgets/Button.hpp" <<'EOF'
#pragma once
#include "../core/Widget.hpp"
#include <string>

class Button : public Widget
{
public:
std::string text;
bool pressed;

```
Button(int x, int y, int w, int h, std::string text);

void draw(uint32_t* framebuffer, int screenWidth) override;
void click();
```

};
EOF

cat > "$ROOT/gui/widgets/Button.cpp" <<'EOF'
#include "Button.hpp"

Button::Button(int x, int y, int w, int h, std::string text)
: Widget(x, y, w, h)
{
this->text = text;
pressed = false;
}

void Button::click()
{
pressed = true;
}

void Button::draw(uint32_t* framebuffer, int screenWidth)
{
uint32_t color = pressed ? 0x5050ff : 0x404040;

```
for (int iy = 0; iy < height; iy++)
{
    for (int ix = 0; ix < width; ix++)
    {
        framebuffer[(y + iy) * screenWidth + (x + ix)] = color;
    }
}
```

}
EOF

cat > "$ROOT/gui/widgets/Label.hpp" <<'EOF'
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
EOF

cat > "$ROOT/gui/widgets/Label.cpp" <<'EOF'
#include "Label.hpp"

Label::Label(int x, int y, std::string text)
: Widget(x, y, 120, 24)
{
this->text = text;
}

void Label::draw(uint32_t* framebuffer, int screenWidth)
{
for (int iy = 0; iy < height; iy++)
{
for (int ix = 0; ix < width; ix++)
{
framebuffer[(y + iy) * screenWidth + (x + ix)] = 0x202020;
}
}
}
EOF

cat > "$ROOT/gui/widgets/TextBox.hpp" <<'EOF'
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
EOF

cat > "$ROOT/gui/widgets/TextBox.cpp" <<'EOF'
#include "TextBox.hpp"

TextBox::TextBox(int x, int y, int w, int h)
: Widget(x, y, w, h)
{
active = false;
}

void TextBox::keyInput(char c)
{
if (active)
value.push_back(c);
}

void TextBox::draw(uint32_t* framebuffer, int screenWidth)
{
for (int iy = 0; iy < height; iy++)
{
for (int ix = 0; ix < width; ix++)
{
framebuffer[(y + iy) * screenWidth + (x + ix)] = 0x101010;
}
}
}
EOF

cat > "$ROOT/gui/widgets/Panel.hpp" <<'EOF'
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
EOF

cat > "$ROOT/gui/widgets/Panel.cpp" <<'EOF'
#include "Panel.hpp"

Panel::Panel(int x, int y, int w, int h)
: Widget(x, y, w, h)
{
}

void Panel::add(Widget* widget)
{
children.push_back(widget);
}

void Panel::draw(uint32_t* framebuffer, int screenWidth)
{
for (int iy = 0; iy < height; iy++)
{
for (int ix = 0; ix < width; ix++)
{
framebuffer[(y + iy) * screenWidth + (x + ix)] = 0x252525;
}
}

```
for (auto child : children)
    child->draw(framebuffer, screenWidth);
```

}
EOF

cat > "$ROOT/gui/widgets/Menu.hpp" <<'EOF'
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
EOF

cat > "$ROOT/gui/widgets/Menu.cpp" <<'EOF'
#include "Menu.hpp"

Menu::Menu(int x, int y, int w, int h)
: Panel(x, y, w, h)
{
opened = false;
}

void Menu::toggle()
{
opened = !opened;
}
EOF

cat > "$ROOT/gui/core/Event.hpp" <<'EOF'
#pragma once

enum class EventType
{
MouseDown,
MouseUp,
MouseMove,
KeyPress
};

class Event
{
public:
EventType type;
int x;
int y;
char key;

```
Event(EventType t)
{
    type = t;
    x = 0;
    y = 0;
    key = 0;
}
```

};
EOF

cat > "$ROOT/gui/core/EventDispatcher.hpp" <<'EOF'
#pragma once
#include "Event.hpp"
#include "../core/Widget.hpp"
#include <vector>

class EventDispatcher
{
private:
std::vector<Widget*> widgets;

public:
void addWidget(Widget* widget);
void sendEvent(Event event);
};
EOF

cat > "$ROOT/gui/core/EventDispatcher.cpp" <<'EOF'
#include "EventDispatcher.hpp"

void EventDispatcher::addWidget(Widget* widget)
{
widgets.push_back(widget);
}

void EventDispatcher::sendEvent(Event event)
{
for (auto widget : widgets)
{
if (event.type == EventType::MouseDown && widget->contains(event.x, event.y))
widget->onClick();

```
    if (event.type == EventType::KeyPress)
        widget->onKey(event.key);
}
```

}
EOF

cat > "$ROOT/gui/core/Layout.hpp" <<'EOF'
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
EOF

cat > "$ROOT/gui/core/Layout.cpp" <<'EOF'
#include "Layout.hpp"

VerticalLayout::VerticalLayout(int x, int y)
{
this->x = x;
this->y = y;
spacing = 10;
}

void VerticalLayout::add(Widget* widget)
{
widget->x = x;
widget->y = y + (int)children.size() * (widget->height + spacing);
children.push_back(widget);
}
EOF

echo "[5/7] Writing desktop shell files..."

cat > "$ROOT/shell/Desktop.hpp" <<'EOF'
#pragma once
#include <cstdint>

class Desktop
{
public:
uint32_t background;

```
Desktop();
void draw(uint32_t* framebuffer, int width, int height);
```

};
EOF

cat > "$ROOT/shell/Desktop.cpp" <<'EOF'
#include "Desktop.hpp"

Desktop::Desktop()
{
background = 0x101820;
}

void Desktop::draw(uint32_t* framebuffer, int width, int height)
{
for (int y = 0; y < height; y++)
{
for (int x = 0; x < width; x++)
{
framebuffer[y * width + x] = background;
}
}
}
EOF

cat > "$ROOT/shell/Taskbar.hpp" <<'EOF'
#pragma once
#include <cstdint>

class Taskbar
{
public:
int height;

```
Taskbar();
void draw(uint32_t* framebuffer, int width, int screenHeight);
```

};
EOF

cat > "$ROOT/shell/Taskbar.cpp" <<'EOF'
#include "Taskbar.hpp"

Taskbar::Taskbar()
{
height = 45;
}

void Taskbar::draw(uint32_t* framebuffer, int width, int screenHeight)
{
for (int y = screenHeight - height; y < screenHeight; y++)
{
for (int x = 0; x < width; x++)
{
framebuffer[y * width + x] = 0x202020;
}
}
}
EOF

cat > "$ROOT/shell/StartMenu.hpp" <<'EOF'
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
EOF

cat > "$ROOT/shell/StartMenu.cpp" <<'EOF'
#include "StartMenu.hpp"

StartMenu::StartMenu()
{
opened = false;
}

void StartMenu::toggle()
{
opened = !opened;
}

void StartMenu::draw(uint32_t* framebuffer, int width)
{
if (!opened) return;

```
for (int y = 300; y < 650; y++)
{
    for (int x = 50; x < 350; x++)
    {
        framebuffer[y * width + x] = 0x303030;
    }
}
```

}
EOF

cat > "$ROOT/shell/Icon.hpp" <<'EOF'
#pragma once
#include <string>

class Icon
{
public:
int x;
int y;
std::string name;

```
Icon(int x, int y, std::string name);
void launch();
```

};
EOF

cat > "$ROOT/shell/Icon.cpp" <<'EOF'
#include "Icon.hpp"
#include <iostream>

Icon::Icon(int x, int y, std::string name)
{
this->x = x;
this->y = y;
this->name = name;
}

void Icon::launch()
{
std::cout << "Launching " << name << std::endl;
}
EOF

echo "[6/7] Writing app framework + loader..."

cat > "$ROOT/app/core/AppWindow.hpp" <<'EOF'
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
EOF

cat > "$ROOT/app/core/AppWindow.cpp" <<'EOF'
#include "AppWindow.hpp"
#include <iostream>

AppWindow::AppWindow(int x, int y, int w, int h, const char* title)
{
this->x = x;
this->y = y;
width = w;
height = h;
this->title = title;
}

void AppWindow::show()
{
std::cout << "Window: " << title << " opened" << std::endl;
}
EOF

cat > "$ROOT/app/core/Application.hpp" <<'EOF'
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
EOF

cat > "$ROOT/app/core/Application.cpp" <<'EOF'
#include "Application.hpp"

void Application::createWindow(int x, int y, int w, int h, const char* title)
{
window = new AppWindow(x, y, w, h, title);
}

void Application::start()
{
}
EOF

cat > "$ROOT/app/apps/HelloKuroko/main.cpp" <<'EOF'
#include "../../core/Application.hpp"

class HelloApp : public Application
{
public:
void start() override
{
createWindow(100, 100, 400, 300, "Hello Kuroko");
window->show();
}
};

int main()
{
HelloApp app;
app.start();

```
while (true)
{
}

return 0;
```

}
EOF

cat > "$ROOT/loader/package/Manifest.hpp" <<'EOF'
#pragma once
#include <string>

class Manifest
{
public:
std::string name;
std::string version;
std::string executable;

```
bool load(const std::string& file);
```

};
EOF

cat > "$ROOT/loader/package/Manifest.cpp" <<'EOF'
#include "Manifest.hpp"
#include <fstream>

bool Manifest::load(const std::string& file)
{
std::ifstream input(file);
if (!input) return false;

```
std::getline(input, name);
std::getline(input, version);
std::getline(input, executable);
return true;
```

}
EOF

cat > "$ROOT/loader/loader/AppLoader.hpp" <<'EOF'
#pragma once
#include "../package/Manifest.hpp"

class AppLoader
{
public:
bool loadApp(const std::string& path);
void startApp();

private:
Manifest manifest;
};
EOF

cat > "$ROOT/loader/AppLoader.cpp" <<'EOF'
#include "loader/AppLoader.hpp"
#include <iostream>
#include <cstdlib>

bool AppLoader::loadApp(const std::string& path)
{
std::string manifestFile = path + "/manifest.kuro";

```
if (!manifest.load(manifestFile))
{
    std::cout << "Manifest error\n";
    return false;
}

std::cout << "Loaded: " << manifest.name << "\n";
return true;
```

}

void AppLoader::startApp()
{
std::cout << "Starting " << manifest.executable << "\n";
system(manifest.executable.c_str());
}
EOF

cat > "$ROOT/loader/main.cpp" <<'EOF'
#include "loader/AppLoader.hpp"

int main()
{
AppLoader loader;

```
if (loader.loadApp("Calculator.kapp"))
{
    loader.startApp();
}

return 0;
```

}
EOF

echo "[7/7] Writing kpm files..."

cat > "$ROOT/kpm/Package.hpp" <<'EOF'
#pragma once
#include <string>

class Package
{
public:
std::string name;
std::string version;
std::string path;

```
Package(std::string name, std::string version, std::string path);
```

};
EOF

cat > "$ROOT/kpm/Package.cpp" <<'EOF'
#include "Package.hpp"

Package::Package(std::string name, std::string version, std::string path)
{
this->name = name;
this->version = version;
this->path = path;
}
EOF

cat > "$ROOT/kpm/PackageManager.hpp" <<'EOF'
#pragma once
#include "Package.hpp"
#include <vector>

class PackageManager
{
private:
std::vector<Package> packages;

public:
void install(Package pkg);
void remove(std::string name);
void list();
};
EOF

cat > "$ROOT/kpm/PackageManager.cpp" <<'EOF'
#include "PackageManager.hpp"
#include <iostream>
#include <algorithm>

void PackageManager::install(Package pkg)
{
packages.push_back(pkg);
std::cout << "Installed: " << pkg.name << "\n";
}

void PackageManager::remove(std::string name)
{
for (auto i = packages.begin(); i != packages.end(); i++)
{
if (i->name == name)
{
packages.erase(i);
std::cout << "Removed: " << name << "\n";
return;
}
}
}

void PackageManager::list()
{
std::cout << "Kuroko packages:\n";
for (auto &p : packages)
{
std::cout << p.name << " " << p.version << "\n";
}
}
EOF

cat > "$ROOT/kpm/main.cpp" <<'EOF'
#include "PackageManager.hpp"

int main()
{
PackageManager kpm;

```
Package calculator("Calculator", "1.0", "/apps/calculator");
Package terminal("Terminal", "1.0", "/apps/terminal");

kpm.install(calculator);
kpm.install(terminal);

kpm.list();

kpm.remove("Calculator");

kpm.list();

return 0;
```

}
EOF

echo "Done."
echo "Project scaffold created at: $ROOT"
