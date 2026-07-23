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
