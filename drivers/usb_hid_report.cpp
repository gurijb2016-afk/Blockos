#include "usb_hid_report.hpp"

namespace usb { namespace hid {
namespace {

static bool contains(const uint8_t keys[6], uint8_t usage)
{
    for (unsigned i = 0; i < 6; ++i) {
        if (keys[i] == usage)
            return true;
    }
    return false;
}

static bool append(InputEvent* list, size_t& count, EventType type,
                   uint16_t code, int16_t dx, int16_t dy, int8_t wheel,
                   uint8_t modifiers)
{
    if (count >= 20)
        return false;
    list[count++] = {type, code, dx, dy, wheel, modifiers};
    return true;
}

static bool shift_down(uint8_t modifiers)
{
    return (modifiers & ((1U << 1) | (1U << 5))) != 0;
}

} // namespace

bool decode_boot_keyboard(const uint8_t* report, size_t report_len,
                          KeyboardState* state, InputEvent* events,
                          size_t event_capacity, size_t* out_count)
{
    if (out_count)
        *out_count = 0;
    if (!report || report_len < 8 || !state || !out_count)
        return false;

    KeyboardState next{};
    next.modifiers = report[0];
    for (unsigned i = 0; i < 6; ++i) {
        next.keys[i] = report[2 + i];
        // HID usages 1..3 mean ErrorRollOver, POSTFail or ErrorUndefined.
        // Do not emit key releases from a transient rollover report.
        if (next.keys[i] >= 1 && next.keys[i] <= 3)
            return false;
    }

    // Duplicate nonzero usages are malformed. Reject before generating any
    // transition so that the caller's previous state remains authoritative.
    for (unsigned i = 0; i < 6; ++i) {
        if (!next.keys[i])
            continue;
        for (unsigned j = i + 1; j < 6; ++j) {
            if (next.keys[i] == next.keys[j])
                return false;
        }
    }

    InputEvent pending[20]{};
    size_t count = 0;

    // Releases first, so modifier and key state transitions are deterministic.
    for (unsigned bit = 0; bit < 8; ++bit) {
        const uint8_t mask = static_cast<uint8_t>(1U << bit);
        if ((state->modifiers & mask) && !(next.modifiers & mask)) {
            if (!append(pending, count, EventType::KeyUp,
                        static_cast<uint16_t>(0xE0 + bit), 0, 0, 0,
                        next.modifiers))
                return false;
        }
    }

    for (unsigned i = 0; i < 6; ++i) {
        const uint8_t old_key = state->keys[i];
        if (old_key && !contains(next.keys, old_key)) {
            if (!append(pending, count, EventType::KeyUp, old_key,
                        0, 0, 0, next.modifiers))
                return false;
        }
    }

    for (unsigned bit = 0; bit < 8; ++bit) {
        const uint8_t mask = static_cast<uint8_t>(1U << bit);
        if (!(state->modifiers & mask) && (next.modifiers & mask)) {
            if (!append(pending, count, EventType::KeyDown,
                        static_cast<uint16_t>(0xE0 + bit), 0, 0, 0,
                        next.modifiers))
                return false;
        }
    }

    for (unsigned i = 0; i < 6; ++i) {
        const uint8_t new_key = next.keys[i];
        if (new_key && !contains(state->keys, new_key)) {
            if (!append(pending, count, EventType::KeyDown, new_key,
                        0, 0, 0, next.modifiers))
                return false;
        }
    }

    *out_count = count;
    if (count > event_capacity || (count != 0 && !events))
        return false;
    for (size_t i = 0; i < count; ++i)
        events[i] = pending[i];
    *state = next;
    return true;
}

bool decode_boot_mouse(const uint8_t* report, size_t report_len,
                       MouseState* state, InputEvent* events,
                       size_t event_capacity, size_t* out_count)
{
    if (out_count)
        *out_count = 0;
    if (!report || report_len < 3 || !state || !out_count)
        return false;

    const uint8_t buttons = report[0] & 0x1FU;
    const int16_t dx = static_cast<int8_t>(report[1]);
    const int16_t dy = static_cast<int8_t>(report[2]);
    const int8_t wheel = report_len >= 4 ? static_cast<int8_t>(report[3]) : 0;

    InputEvent pending[8]{};
    size_t count = 0;
    for (unsigned bit = 0; bit < 5; ++bit) {
        const uint8_t mask = static_cast<uint8_t>(1U << bit);
        if ((state->buttons & mask) != (buttons & mask)) {
            const EventType type = (buttons & mask)
                ? EventType::ButtonDown : EventType::ButtonUp;
            pending[count++] = {type, static_cast<uint16_t>(bit + 1),
                                0, 0, 0, 0};
        }
    }
    if (dx || dy)
        pending[count++] = {EventType::PointerMove, 0, dx, dy, 0, 0};
    if (wheel)
        pending[count++] = {EventType::Wheel, 0, 0, 0, wheel, 0};

    *out_count = count;
    if (count > event_capacity || (count != 0 && !events))
        return false;
    for (size_t i = 0; i < count; ++i)
        events[i] = pending[i];
    state->buttons = buttons;
    return true;
}

bool usage_to_ascii(uint8_t usage, uint8_t modifiers, char* out_character)
{
    if (!out_character)
        return false;
    const bool shift = shift_down(modifiers);

    if (usage >= 0x04 && usage <= 0x1D) {
        char c = static_cast<char>('a' + (usage - 0x04));
        *out_character = shift ? static_cast<char>(c - 'a' + 'A') : c;
        return true;
    }

    if (usage >= 0x1E && usage <= 0x27) {
        static const char normal[] = "1234567890";
        static const char shifted[] = "!@#$%^&*()";
        const unsigned idx = usage - 0x1E;
        *out_character = shift ? shifted[idx] : normal[idx];
        return true;
    }

    switch (usage) {
        case 0x28: *out_character = '\n'; return true;
        case 0x2A: *out_character = '\b'; return true;
        case 0x2B: *out_character = '\t'; return true;
        case 0x2C: *out_character = ' '; return true;
        case 0x2D: *out_character = shift ? '_' : '-'; return true;
        case 0x2E: *out_character = shift ? '+' : '='; return true;
        case 0x2F: *out_character = shift ? '{' : '['; return true;
        case 0x30: *out_character = shift ? '}' : ']'; return true;
        case 0x31: *out_character = shift ? '|' : '\\'; return true;
        case 0x33: *out_character = shift ? ':' : ';'; return true;
        case 0x34: *out_character = shift ? '"' : '\''; return true;
        case 0x35: *out_character = shift ? '~' : '`'; return true;
        case 0x36: *out_character = shift ? '<' : ','; return true;
        case 0x37: *out_character = shift ? '>' : '.'; return true;
        case 0x38: *out_character = shift ? '?' : '/'; return true;
        default: return false;
    }
}

} } // namespace usb::hid
