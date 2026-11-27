#include "input_bridge.hpp"
#include <string.h>
#include "ps2keyboard.hpp"
#include "ps2mouse.hpp"
#include "virtio_input.hpp"
#include "usb_xhci.hpp"
#include "spinlock.hpp"

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
Blockos::Spinlock input_lock;

inline uint8_t inb_8042(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void push_locked(const Event& e) {
    const size_t next = (tail + 1) % CAP;
    if (next == head) {
        /* Drop oldest input rather than blocking kernel input producers. */
        head = (head + 1) % CAP;
    }
    queue[tail] = e;
    tail = next;
}

void push_keyboard_locked(uint32_t scancode, bool pressed, bool extended) {
    Event e{};
    e.magic = MAGIC;
    e.version = VERSION;
    e.type = EVENT_KEYBOARD;
    e.scancode = scancode;
    e.pressed = pressed ? 1u : 0u;
    e.extended = extended ? 1u : 0u;
    push_locked(e);
}

void push_mouse_locked(int32_t dx, int32_t dy, int32_t wheel, uint32_t buttons) {
    Event e{};
    e.magic = MAGIC;
    e.version = VERSION;
    e.type = EVENT_MOUSE;
    e.dx = dx;
    e.dy = dy;
    e.wheel = wheel;
    e.buttons = buttons;
    push_locked(e);
}

void push_mouse_abs_locked(int32_t abs_x, int32_t abs_y, int32_t wheel, uint32_t buttons) {
    Event e{};
    e.magic = MAGIC;
    e.version = VERSION;
    e.type = EVENT_MOUSE_ABS;
    e.dx = abs_x;
    e.dy = abs_y;
    e.wheel = wheel;
    e.buttons = buttons;
    push_locked(e);
}
} // namespace

void init() {
    {
        Blockos::ScopedLock guard(input_lock);
        head = tail = 0;
        memset(queue, 0, sizeof(queue));
        mouse_packet_pos = 0;
        keyboard.init();
        mouse.init();
        hardware_ready = true;
    }
    static usb::xhci::Controller s_xhci{};
    if (usb::xhci::probe(&s_xhci)) {
        (void)usb::xhci::initialize(&s_xhci);
    }
}

void poll_hardware() {
    {
        Blockos::ScopedLock guard(input_lock);
        if (!hardware_ready) return;

        // Drain the shared 8042 PS/2 controller output buffer in one interleaved
        // loop so mouse AUX bytes can never block keyboard scancodes (or vice versa).
        for (int budget = 0; budget < 64; ++budget) {
            uint8_t st = inb_8042(0x64);
            if ((st & 0x01u) == 0) break;
            if (st & 0x20u) {
                int16_t b = mouse.read_byte_nonblocking();
                if (b < 0) break;
                uint8_t byte = static_cast<uint8_t>(b);
                if (mouse_packet_pos == 0 && !(byte & 0x08u)) continue;
                mouse_packet[mouse_packet_pos++] = byte;
                if (mouse_packet_pos == 3) {
                    int32_t dx = (mouse_packet[0] & 0x10u) ? static_cast<int32_t>(static_cast<int8_t>(mouse_packet[1])) : static_cast<int32_t>(mouse_packet[1]);
                    int32_t dy = (mouse_packet[0] & 0x20u) ? static_cast<int32_t>(static_cast<int8_t>(mouse_packet[2])) : static_cast<int32_t>(mouse_packet[2]);
                    uint32_t buttons = mouse_packet[0] & 0x07u;
                    push_mouse_locked(dx, -dy, 0, buttons);
                    mouse_packet_pos = 0;
                }
            } else {
                KeyEvent e{};
                if (keyboard.poll(e)) {
                    push_keyboard_locked(e.scancode, e.is_pressed, e.is_extended);
                }
            }
        }

        int32_t v_dx = 0, v_dy = 0, v_wheel = 0;
        static int32_t v_abs_x = 0, v_abs_y = 0;
        static uint32_t v_buttons = 0;
        bool v_rel = false;
        bool v_abs = false;
        bool v_btn = false;
        VirtioInputEvent ve{};
        while (virtio_input::poll(&ve)) {
            if (ve.type == 1) { // EV_KEY
                if (ve.code >= 0x110 && ve.code <= 0x112) {
                    uint32_t bit = 1u << (ve.code - 0x110);
                    if (ve.value) v_buttons |= bit; else v_buttons &= ~bit;
                    v_btn = true;
                } else if (ve.code > 0 && ve.code < 0x80) {
                    push_keyboard_locked(ve.code, ve.value != 0, false);
                }
            } else if (ve.type == 2) { // EV_REL
                if (ve.code == 0) { v_dx += ve.value; v_rel = true; }
                else if (ve.code == 1) { v_dy += ve.value; v_rel = true; }
                else if (ve.code == 8) { v_wheel += ve.value; v_rel = true; }
            } else if (ve.type == 3) { // EV_ABS (virtio-tablet-pci: 0..32767)
                if (ve.code == 0) { v_abs_x = ve.value; v_abs = true; }
                else if (ve.code == 1) { v_abs_y = ve.value; v_abs = true; }
            } else if (ve.type == 0) { // EV_SYN
                if (v_abs) {
                    push_mouse_abs_locked(v_abs_x, v_abs_y, v_wheel, v_buttons);
                    v_wheel = 0; v_abs = false; v_btn = false;
                } else if (v_rel || v_btn) {
                    push_mouse_locked(v_dx, v_dy, v_wheel, v_buttons);
                    v_dx = v_dy = v_wheel = 0; v_rel = false; v_btn = false;
                }
            }
        }
        if (v_abs) {
            push_mouse_abs_locked(v_abs_x, v_abs_y, v_wheel, v_buttons);
        } else if (v_rel || v_btn) {
            push_mouse_locked(v_dx, v_dy, v_wheel, v_buttons);
        }
    }
    usb::xhci::poll_input();
}

void push_keyboard(uint32_t scancode, bool pressed, bool extended) {
    Blockos::ScopedLock guard(input_lock);
    push_keyboard_locked(scancode, pressed, extended);
}

void push_mouse(int32_t dx, int32_t dy, int32_t wheel, uint32_t buttons) {
    Blockos::ScopedLock guard(input_lock);
    push_mouse_locked(dx, dy, wheel, buttons);
}

void push_mouse_abs(int32_t abs_x, int32_t abs_y, int32_t wheel, uint32_t buttons) {
    Blockos::ScopedLock guard(input_lock);
    push_mouse_abs_locked(abs_x, abs_y, wheel, buttons);
}

size_t read(Event* out, size_t max_events) {
    if (!out || max_events == 0) return 0;
    Blockos::ScopedLock guard(input_lock);
    size_t n = 0;
    while (head != tail && n < max_events) {
        out[n++] = queue[head];
        head = (head + 1) % CAP;
    }
    return n;
}

size_t pending() {
    Blockos::ScopedLock guard(input_lock);
    return tail >= head ? tail - head : CAP - head + tail;
}

} // namespace blockos::input

extern "C" void blockos_input_push_keyboard(uint32_t scancode, int pressed, int extended) {
    blockos::input::push_keyboard(scancode, pressed != 0, extended != 0);
}

extern "C" void blockos_input_push_mouse(int32_t dx, int32_t dy, int32_t wheel, uint32_t buttons) {
    blockos::input::push_mouse(dx, dy, wheel, buttons);
}
