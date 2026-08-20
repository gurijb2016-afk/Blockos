#pragma once
#include <cstdint>

#include "ps2keyboard.hpp"

enum class NonCharacterKey : uint8_t
{
    None = 0,
    Backspace,
    Enter,
    Escape,
    Tab,
    Delete,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
};

struct KeyPress
{
    char ch; // 0 when this key produces no character
    NonCharacterKey key; // NonCharacterKey::None when this key produces a character
};


// Scancode to character tables, indexed by the set-1 make code.
// PS2Keyboard::poll() masks the scancode with 0x7F, so the index is always in the range 0-127.
static constexpr char scan_base[128] = {
    /* 0x00 */ 0,
    0,
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    /* 0x08 */ '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    0,
    0,
    /* 0x10 */ 'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    /* 0x18 */ 'o',
    'p',
    '[',
    ']',
    0,
    0,
    'a',
    's',
    /* 0x20 */ 'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    /* 0x28 */ '\'',
    '`',
    0,
    '\\',
    'z',
    'x',
    'c',
    'v',
    /* 0x30 */ 'b',
    'n',
    'm',
    ',',
    '.',
    '/',
    0,
    0,
    /* 0x38 */ 0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
};

static constexpr char scan_shift[128] = {
    /* 0x00 */ 0,
    0,
    '!',
    '@',
    '#',
    '$',
    '%',
    '^',
    /* 0x08 */ '&',
    '*',
    '(',
    ')',
    '_',
    '+',
    0,
    0,
    /* 0x10 */ 'Q',
    'W',
    'E',
    'R',
    'T',
    'Y',
    'U',
    'I',
    /* 0x18 */ 'O',
    'P',
    '{',
    '}',
    0,
    0,
    'A',
    'S',
    /* 0x20 */ 'D',
    'F',
    'G',
    'H',
    'J',
    'K',
    'L',
    ':',
    /* 0x28 */ '"',
    '~',
    0,
    '|',
    'Z',
    'X',
    'C',
    'V',
    /* 0x30 */ 'B',
    'N',
    'M',
    '<',
    '>',
    '?',
    0,
    0,
    /* 0x38 */ 0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
};

// guard against an off-by-one error
static_assert(scan_base[0x02] == '1', "digit row shifted");
static_assert(scan_base[0x11] == 'w', "qwerty row shifted");
static_assert(scan_base[0x1E] == 'a', "home row shifted");
static_assert(scan_base[0x2C] == 'z', "bottom row shifted");
static_assert(scan_base[0x35] == '/', "punctuation shifted");
static_assert(scan_base[0x39] == ' ', "space shifted");

static_assert(scan_shift[0x02] == '!', "shifted digit row shifted");
static_assert(scan_shift[0x11] == 'W', "shifted qwerty row shifted");
static_assert(scan_shift[0x1E] == 'A', "shifted home row shifted");
static_assert(scan_shift[0x2C] == 'Z', "shifted bottom row shifted");
static_assert(scan_shift[0x35] == '?', "shifted punctuation shifted");

class Keymap
{
   public:
    Keymap() = default;

    KeyPress translate(const KeyEvent& event)
    {
        KeyPress key_press{0};

        switch (event.scancode)
        {
            case 0x1D: // Ctrl
                ctrl_pressed_ = event.is_pressed;
                return key_press;
            case 0x2A: // Left Shift
                shift_pressed_ = event.is_pressed;
                return key_press;
            case 0x36: // Right Shift
                shift_pressed_ = event.is_pressed;
                return key_press;
            case 0x38: // Alt
                alt_pressed_ = event.is_pressed;
                return key_press;
            case 0x3A: // Caps Lock
                if (event.is_pressed)
                {
                    caps_lock_ = !caps_lock_;
                    caps_held = true;
                }
                else
                {
                    caps_held = false;
                }
                return key_press;
        }

        if (!event.is_pressed) return key_press; // don't emit non-modifier keys on release

        switch (event.scancode)
        {
            case 0x0E: // Backspace
                key_press.key = NonCharacterKey::Backspace;
                return key_press;
            case 0x1C: // Enter
                key_press.key = NonCharacterKey::Enter;
                return key_press;
            case 0x01: // Escape
                key_press.key = NonCharacterKey::Escape;
                return key_press;
            case 0x0F: // Tab
                key_press.key = NonCharacterKey::Tab;
                return key_press;
            case 0x53: // Delete
                key_press.key = NonCharacterKey::Delete;
                return key_press;
        }

        if (event.scancode != 0)
        {
            char base_char = scan_base[event.scancode];
            if (base_char == 0)
            {
                return key_press;
            }

            bool is_alpha = (base_char >= 'a' && base_char <= 'z');

            if (is_alpha && (caps_lock_ ^ caps_held ^ shift_pressed_))
            {
                key_press.ch = scan_shift[event.scancode];
            }
            else
            {
                key_press.ch = shift_pressed_ ? scan_shift[event.scancode] : base_char;
            }

            key_press.key = NonCharacterKey::None;
        }

        return key_press;
    }

   private:
    bool shift_pressed_ = false;
    bool ctrl_pressed_ = false;
    bool alt_pressed_ = false;
    bool caps_lock_ = false;
    bool caps_held = false;
};
