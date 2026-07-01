#include "ps2mouse.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a" (val) : "Nd" (port));
    return val;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a" (val), "Nd" (port));
}

static void ps2_wait_input_empty() {
    int i = 100000;
    while (i--) {
        uint8_t st = inb(0x64);
        if (!(st & 0x2)) return; // input buffer empty
    }
}
static void ps2_wait_output_full() {
    int i = 100000;
    while (i--) {
        uint8_t st = inb(0x64);
        if (st & 0x1) return; // output buffer full
    }
}

void ps2_write_cmd(uint8_t cmd) {
    ps2_wait_input_empty();
    outb(0x64, cmd);
}
void ps2_write_data(uint8_t data) {
    ps2_wait_input_empty();
    outb(0x60, data);
}

void ps2_send_mouse(uint8_t cmd) {
    ps2_write_cmd(0xD4);
    ps2_write_data(cmd);
}

void ps2_init() {
    // Enable auxiliary device
    ps2_write_cmd(0xA8);
    // Enable the aux device IRQ and clocks via controller command - skip controller config for brevity
    // Clear output buffer
    for (int i=0;i<100;i++) { if (inb(0x64) & 1) { (void)inb(0x60); } }
    // Send reset/enable sequence to mouse
    ps2_send_mouse(0xFF); // reset
    // ack may appear but we won't strictly check
    for (int i=0;i<100;i++) { if (inb(0x64) & 1) { (void)inb(0x60); } }
    ps2_send_mouse(0xF4); // enable data reporting
}

// Non-blocking read: returns INT8_MIN (-128) if no byte available, otherwise the byte
int8_t ps2_read_byte_nonblocking() {
    uint8_t st = inb(0x64);
    if (st & 0x1) {
        uint8_t d = inb(0x60);
        return (int8_t)d;
    }
    return INT8_MIN;
}
