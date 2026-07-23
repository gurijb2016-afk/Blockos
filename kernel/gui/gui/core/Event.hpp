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
