#pragma once
#include <cstdint>

struct KeyEvent
{
    uint8_t scancode;
    bool is_pressed;
    bool is_extended;
};

class PS2Keyboard
{
   public:
    PS2Keyboard() = default;
    void init();
    int16_t read_byte_nonblocking();
    static char scancode_to_ascii(uint8_t sc);
    bool poll(KeyEvent& out);

   private:
    uint8_t prefix_ = 0x00; // 0xE0 for extended scancodes, 0x00 otherwise
    uint8_t skip_ = 0; // countdown for pause sequencing
};
