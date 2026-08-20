#include "ps2keyboard.hpp"

#include <cstdint>


static inline uint8_t inb_k(uint16_t port)
{
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outb_k(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void PS2Keyboard::init()
{
    // Usually nothing required here for QEMU; leave as placeholder.
}

// Return type int16_t, as INT8_MIN was a valid value for keyboard and mouse scancodes
// Widening to 16-bit retains the same value while allowing us to return -1 as a sentinel
int16_t PS2Keyboard::read_byte_nonblocking()
{
    uint8_t st = inb_k(0x64);
    if (st & 0x1 && !(st & 0x20)) // output buffer full and keyboard data
    {
        uint8_t d = inb_k(0x60);
        return d;
    }
    return -1;
}

bool PS2Keyboard::poll(KeyEvent& out)
{
    for (;;)
    {
        int16_t scan = read_byte_nonblocking();

        if (scan == -1) return false;
        if (scan == 0x00 || scan == 0xFF) continue; // Error/overrun indicators aren't keys
        if (skip_ > 0)
        {
            skip_--;
            continue;
        }

        if (scan == 0xE1)
        {
            // Discard pause and break for now
            skip_ = 5;
            continue;
        }

        if (scan == 0xE0)
        {
            prefix_ = 0xE0;
            continue;
        }

        out.is_pressed = !(scan & 0x80);
        out.scancode = scan & 0x7F;
        out.is_extended = (prefix_ == 0xE0);

        prefix_ = 0;

        return true;
    }
}

char PS2Keyboard::scancode_to_ascii(uint8_t sc)
{
    // PS/2 Set 1 scancode to ASCII (partial, common keys)
    switch (sc)
    {
        case 0x02:
            return '1';
        case 0x03:
            return '2';
        case 0x04:
            return '3';
        case 0x05:
            return '4';
        case 0x06:
            return '5';
        case 0x07:
            return '6';
        case 0x08:
            return '7';
        case 0x09:
            return '8';
        case 0x0A:
            return '9';
        case 0x0B:
            return '0';
        case 0x10:
            return 'q';
        case 0x11:
            return 'w';
        case 0x12:
            return 'e';
        case 0x13:
            return 'r';
        case 0x14:
            return 't';
        case 0x15:
            return 'y';
        case 0x16:
            return 'u';
        case 0x17:
            return 'i';
        case 0x18:
            return 'o';
        case 0x19:
            return 'p';
        case 0x1E:
            return 'a';
        case 0x1F:
            return 's';
        case 0x20:
            return 'd';
        case 0x21:
            return 'f';
        case 0x22:
            return 'g';
        case 0x23:
            return 'h';
        case 0x24:
            return 'j';
        case 0x25:
            return 'k';
        case 0x26:
            return 'l';
        case 0x2C:
            return 'z';
        case 0x2D:
            return 'x';
        case 0x2E:
            return 'c';
        case 0x2F:
            return 'v';
        case 0x30:
            return 'b';
        case 0x31:
            return 'n';
        case 0x32:
            return 'm';
        case 0x39:
            return ' ';
        case 0x1C:
            return '\n'; // Enter
        default:
            return 0;
    }
}
