#pragma once
#include <stddef.h>
#include <stdint.h>

namespace blockos::input {

constexpr uint32_t MAGIC = 0x424F5349u;
constexpr uint32_t VERSION = 1u;
constexpr uint32_t EVENT_MOUSE = 1u;
constexpr uint32_t EVENT_KEYBOARD = 2u;

struct Event {
    uint32_t magic;
    uint32_t version;
    uint32_t type;
    int32_t dx;
    int32_t dy;
    int32_t wheel;
    uint32_t buttons;
    uint32_t scancode;
    uint32_t pressed;
    uint32_t extended;
};

void init();
void poll_hardware();
void push_keyboard(uint32_t scancode, bool pressed, bool extended);
void push_mouse(int32_t dx, int32_t dy, int32_t wheel, uint32_t buttons);
size_t read(Event* out, size_t max_events);
size_t pending();

}
