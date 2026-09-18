#include "input_bridge.hpp"
#include <string.h>
#include "ps2keyboard.hpp"
#include "ps2mouse.hpp"

namespace blockos::input {
namespace {
constexpr size_t CAP = 256;
Event queue[CAP];
size_t head = 0;
size_t tail = 0;
PS2Keyboard keyboard;
PS2Mouse mouse;
uint8_t mouse_packet[3] = {0,0,0};
uint8_t mouse_packet_pos = 0;
bool hardware_ready = false;
}

void init() {
    head = tail = 0;
    memset(queue, 0, sizeof(queue));
    mouse_packet_pos = 0;
    keyboard.init();
    mouse.init();
    hardware_ready = true;
}

void poll_hardware() {
    if (!hardware_ready) return;
    for (;;) {
        KeyEvent e;
        if (!keyboard.poll(e)) break;
        push_keyboard(e.scancode, e.is_pressed, e.is_extended);
    }
    for (;;) {
        int16_t b = mouse.read_byte_nonblocking();
        if (b < 0) break;
        uint8_t byte = (uint8_t)b;
        if (mouse_packet_pos == 0 && !(byte & 0x08u)) continue;
        mouse_packet[mouse_packet_pos++] = byte;
        if (mouse_packet_pos == 3) {
            int32_t dx = (mouse_packet[0] & 0x10u) ? (int32_t)(int8_t)mouse_packet[1] : (int32_t)mouse_packet[1];
            int32_t dy = (mouse_packet[0] & 0x20u) ? (int32_t)(int8_t)mouse_packet[2] : (int32_t)mouse_packet[2];
            uint32_t buttons = mouse_packet[0] & 0x07u;
            push_mouse(dx, -dy, 0, buttons);
            mouse_packet_pos = 0;
        }
    }
}

static void push(const Event& e) {
    const size_t next = (tail + 1) % CAP;
    if (next == head) {
        /* Drop oldest input rather than blocking kernel input producers. */
        head = (head + 1) % CAP;
    }
    queue[tail] = e;
    tail = next;
}

void push_keyboard(uint32_t scancode, bool pressed, bool extended) {
    Event e{};
    e.magic = MAGIC;
    e.version = VERSION;
    e.type = EVENT_KEYBOARD;
    e.scancode = scancode;
    e.pressed = pressed ? 1u : 0u;
    e.extended = extended ? 1u : 0u;
    push(e);
}

void push_mouse(int32_t dx, int32_t dy, int32_t wheel, uint32_t buttons) {
    Event e{};
    e.magic = MAGIC;
    e.version = VERSION;
    e.type = EVENT_MOUSE;
    e.dx = dx;
    e.dy = dy;
    e.wheel = wheel;
    e.buttons = buttons;
    push(e);
}

size_t read(Event* out, size_t max_events) {
    if (!out || max_events == 0) return 0;
    size_t n = 0;
    while (head != tail && n < max_events) {
        out[n++] = queue[head];
        head = (head + 1) % CAP;
    }
    return n;
}

size_t pending() {
    return tail >= head ? tail - head : CAP - head + tail;
}

}

extern "C" void blockos_input_push_keyboard(uint32_t scancode, int pressed, int extended) {
    blockos::input::push_keyboard(scancode, pressed != 0, extended != 0);
}

extern "C" void blockos_input_push_mouse(int32_t dx, int32_t dy, int32_t wheel, uint32_t buttons) {
    blockos::input::push_mouse(dx, dy, wheel, buttons);
}
