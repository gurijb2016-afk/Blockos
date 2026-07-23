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
