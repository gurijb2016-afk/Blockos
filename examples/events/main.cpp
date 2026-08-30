#include "kernel/events.hpp"

void example_event_queue()
{
    Event event{};
    while (g_event_queue.pop(event)) {
        switch (event.type) {
        case EventType::MouseMove:
            (void)event.data.mouse.x;
            (void)event.data.mouse.y;
            break;
        case EventType::MouseButton:
            (void)event.data.mouse.buttons;
            break;
        case EventType::KeyPress:
            (void)event.data.key.scancode;
            break;
        }
    }
}
