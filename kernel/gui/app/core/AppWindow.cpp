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
