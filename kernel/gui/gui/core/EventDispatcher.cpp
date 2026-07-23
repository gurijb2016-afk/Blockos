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
