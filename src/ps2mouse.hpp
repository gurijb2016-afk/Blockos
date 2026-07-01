#pragma once
#include <cstdint>

class PS2Mouse {
public:
    PS2Mouse() = default;
    void init();
    // Non-blocking read: returns INT8_MIN if no byte available
    int8_t read_byte_nonblocking();

private:
    static inline uint8_t inb(uint16_t port);
    static inline void outb(uint16_t port, uint8_t val);
    static void ps2_wait_input_empty();
    static void ps2_wait_output_full();
    static void ps2_write_cmd(uint8_t cmd);
    static void ps2_write_data(uint8_t data);
    static void ps2_send_mouse(uint8_t cmd);
};
