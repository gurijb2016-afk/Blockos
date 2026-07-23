#include "Application.hpp"

void Application::createWindow(int x, int y, int w, int h, const char* title)
{
window = new AppWindow(x, y, w, h, title);
}

void Application::start()
{
}
