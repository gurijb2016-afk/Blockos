#include "ps2keyboard.hpp"
#include <cstdint>

static inline uint8_t inb_k(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a" (val) : "Nd" (port));
    return val;
}
static inline void outb_k(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a" (val), "Nd" (port));
}

void PS2Keyboard::init() {
    // Usually nothing required here for QEMU; leave as placeholder.
    // Could send commands to controller if needed.
}

int8_t PS2Keyboard::read_byte_nonblocking() {
    uint8_t st = inb_k(0x64);
    if (st & 0x1) {
        uint8_t d = inb_k(0x60);
        return (int8_t)d;
    }
    return INT8_MIN;
}
