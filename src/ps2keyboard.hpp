#pragma once
#include <cstdint>

class PS2Keyboard {
public:
    PS2Keyboard() = default;
    void init();
    int8_t read_byte_nonblocking();
    // Convert set1 scancode to ASCII (very basic US layout)
    static char scancode_to_ascii(uint8_t sc);
};
