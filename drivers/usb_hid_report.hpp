#pragma once

#include <stddef.h>
#include <stdint.h>

namespace usb { namespace hid {

enum class EventType : uint8_t {
    KeyDown,
    KeyUp,
    PointerMove,
    ButtonDown,
    ButtonUp,
    Wheel
};

struct InputEvent {
    EventType type;
    uint16_t code;       // HID usage for keys; 1..5 for mouse buttons
    int16_t dx;
    int16_t dy;
    int8_t wheel;
    uint8_t modifiers;   // HID modifier bitmap from the current report
};

struct KeyboardState {
    uint8_t modifiers;
    uint8_t keys[6];
};

struct MouseState {
    uint8_t buttons;
};

// Decodes the USB HID boot-protocol keyboard report (8 bytes minimum).
// On malformed rollover reports or insufficient output capacity, the state
// is left unchanged. out_count reports the required event capacity on failure.
bool decode_boot_keyboard(
    const uint8_t* report, size_t report_len,
    KeyboardState* state,
    InputEvent* events, size_t event_capacity, size_t* out_count);

// Decodes a boot-protocol mouse report (3-byte report, optional wheel byte).
bool decode_boot_mouse(
    const uint8_t* report, size_t report_len,
    MouseState* state,
    InputEvent* events, size_t event_capacity, size_t* out_count);

// Translate common keyboard usages using the US layout. Returns false for
// non-printable/unmapped keys. modifiers uses the standard HID modifier bits.
bool usage_to_ascii(uint8_t usage, uint8_t modifiers, char* out_character);

} } // namespace usb::hid
