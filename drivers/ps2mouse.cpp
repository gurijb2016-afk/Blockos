#include "ps2mouse.hpp"

inline uint8_t PS2Mouse::inb(uint16_t port)
{
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
inline void PS2Mouse::outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void PS2Mouse::ps2_wait_input_empty()
{
    int i = 100000;
    while (i--)
    {
        uint8_t st = inb(0x64);
        if (!(st & 0x2)) return; // input buffer empty
    }
}
void PS2Mouse::ps2_wait_output_full()
{
    int i = 100000;
    while (i--)
    {
        uint8_t st = inb(0x64);
        if (st & 0x1) return; // output buffer full
    }
}

void PS2Mouse::ps2_write_cmd(uint8_t cmd)
{
    ps2_wait_input_empty();
    outb(0x64, cmd);
}
void PS2Mouse::ps2_write_data(uint8_t data)
{
    ps2_wait_input_empty();
    outb(0x60, data);
}

void PS2Mouse::ps2_send_mouse(uint8_t cmd)
{
    ps2_write_cmd(0xD4);
    ps2_write_data(cmd);
}

void PS2Mouse::init()
{
    // Enable auxiliary device
    ps2_write_cmd(0xA8);
    // Clear output buffer
    for (int i = 0; i < 100; i++)
    {
        if (inb(0x64) & 1)
        {
            (void) inb(0x60);
        }
    }
    // Send reset/enable sequence to mouse and consume ACK/BAT/device-ID bytes
    ps2_send_mouse(0xFF); // reset
    for (int n = 0; n < 3; ++n)
    {
        ps2_wait_output_full();
        if (inb(0x64) & 1) (void) inb(0x60);
    }
    ps2_send_mouse(0xF6); // set default settings
    ps2_wait_output_full();
    if (inb(0x64) & 1) (void) inb(0x60);
    ps2_send_mouse(0xF3); // set sample rate
    ps2_wait_output_full();
    if (inb(0x64) & 1) (void) inb(0x60);
    ps2_send_mouse(200);  // 200 samples/sec for smooth pointer tracking
    ps2_wait_output_full();
    if (inb(0x64) & 1) (void) inb(0x60);
    ps2_send_mouse(0xF4); // enable data reporting
    ps2_wait_output_full();
    if (inb(0x64) & 1) (void) inb(0x60); // consume 0xFA ACK so it doesn't stall keyboard/mouse reads
}

// Return type int16_t, as INT8_MIN was a valid value for keyboard and mouse scancodes
// Widening to 16-bit retains the same value while allowing us to return -1 as a sentinel
int16_t PS2Mouse::read_byte_nonblocking()
{
    uint8_t st = inb(0x64);
    if (st & 0x1 && (st & 0x20)) // output buffer full and mouse data
    {
        uint8_t d = inb(0x60);
        return d;
    }
    return -1; // -1 is the sentinel value
}
