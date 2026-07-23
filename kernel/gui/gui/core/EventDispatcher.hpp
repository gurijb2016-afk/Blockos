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
