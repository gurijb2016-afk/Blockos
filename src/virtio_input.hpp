#pragma once

class VirtioInput {
public:
    VirtioInput() = default;
    void init();
    int8_t read_byte_nonblocking();
};
