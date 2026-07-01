#pragma once
#include <cstdint>

class PS2Keyboard {
public:
    PS2Keyboard() = default;
    void init();
    int8_t read_byte_nonblocking();
};
