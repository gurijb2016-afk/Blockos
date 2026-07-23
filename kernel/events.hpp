#pragma once
#include <cstdint>

enum class EventType : uint8_t { MouseMove, MouseButton, KeyPress };

struct Event {
    EventType type;
    union {
        struct { int32_t x, y; uint8_t buttons; } mouse;
        struct { uint8_t scancode; } key;
    } data;
};

class EventQueue {
public:
    EventQueue() = default;
    bool push(const Event& ev);
    bool pop(Event& out);
private:
    static const int CAP = 256;
    Event buf[CAP];
    int head = 0;
    int tail = 0;
};

extern EventQueue g_event_queue;
