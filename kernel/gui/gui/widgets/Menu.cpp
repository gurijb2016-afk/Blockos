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
