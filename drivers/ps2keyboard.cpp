#include "ps2keyboard.hpp"

#include <cstdint>

namespace {

constexpr uint16_t PS2_DATA = 0x60;
constexpr uint16_t PS2_STATUS = 0x64;
constexpr uint16_t PS2_CMD = 0x64;

inline uint8_t inb_k(uint16_t port)
{
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

inline void outb_k(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

bool wait_input_empty()
{
    for (int i = 0; i < 100000; ++i)
    {
        if ((inb_k(PS2_STATUS) & 0x02u) == 0) return true;
        __asm__ volatile("pause");
    }
    return false;
}

bool wait_output_full()
{
    for (int i = 0; i < 100000; ++i)
    {
        if (inb_k(PS2_STATUS) & 0x01u) return true;
        __asm__ volatile("pause");
    }
    return false;
}

void flush_output_buffer()
{
    for (int i = 0; i < 64; ++i)
    {
        if ((inb_k(PS2_STATUS) & 0x01u) == 0) break;
        (void) inb_k(PS2_DATA);
    }
}

} // namespace

void PS2Keyboard::init()
{
    prefix_ = 0;
    skip_ = 0;

    // Drain any stale scancodes/ACKs left by firmware/UEFI so the first keypress isn't lost.
    flush_output_buffer();

    // Read controller configuration byte (0x20), enable Port 1 clock and Set 1 translation.
    if (wait_input_empty())
    {
        outb_k(PS2_CMD, 0x20);
        if (wait_output_full())
        {
            uint8_t cfg = inb_k(PS2_DATA);
            cfg &= static_cast<uint8_t>(~(1u << 4)); // enable Port 1 clock
            cfg |= static_cast<uint8_t>(1u << 6);    // enable Set 1 translation
            if (wait_input_empty())
            {
                outb_k(PS2_CMD, 0x60);
                if (wait_input_empty()) outb_k(PS2_DATA, cfg);
            }
        }
    }

    // Enable first PS/2 port (keyboard).
    if (wait_input_empty()) outb_k(PS2_CMD, 0xAE);

    // Enable keyboard scanning (0xF4) and consume its 0xFA ACK byte.
    if (wait_input_empty())
    {
        outb_k(PS2_DATA, 0xF4);
        if (wait_output_full()) (void) inb_k(PS2_DATA);
    }

    flush_output_buffer();
}

// Return type int16_t, as INT8_MIN was a valid value for keyboard and mouse scancodes
// Widening to 16-bit retains the same value while allowing us to return -1 as a sentinel
int16_t PS2Keyboard::read_byte_nonblocking()
{
    uint8_t st = inb_k(PS2_STATUS);
    if ((st & 0x01u) && !(st & 0x20u)) // output buffer full and keyboard data
    {
        return inb_k(PS2_DATA);
    }
    return -1;
}

bool PS2Keyboard::poll(KeyEvent& out)
{
    for (;;)
    {
        int16_t scan = read_byte_nonblocking();

        if (scan == -1) return false;
        if (scan == 0x00 || scan == 0xFF || scan == 0xFA || scan == 0xFE)
            continue; // Error/overrun/ACK/RESEND protocol bytes aren't keys
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
        out.scancode = static_cast<uint8_t>(scan & 0x7F);
        out.is_extended = (prefix_ == 0xE0);

        prefix_ = 0;

        return true;
    }
}

char PS2Keyboard::scancode_to_ascii(uint8_t sc, bool shift)
{
    // PS/2 Set 1 US QWERTY scancode to ASCII table (0x00..0x39)
    static constexpr char kNormal[0x3A] = {
        0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00..0x07
        '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t', // 0x08..0x0F
        'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  // 0x10..0x17
        'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',  // 0x18..0x1F
        'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  // 0x20..0x27
        '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  // 0x28..0x2F
        'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  // 0x30..0x37
        0,    ' '                                       // 0x38..0x39
    };
    static constexpr char kShift[0x3A] = {
        0,    0x1B, '!',  '@',  '#',  '$',  '%',  '^',  // 0x00..0x07
        '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t', // 0x08..0x0F
        'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10..0x17
        'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',  // 0x18..0x1F
        'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20..0x27
        '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  // 0x28..0x2F
        'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  // 0x30..0x37
        0,    ' '                                       // 0x38..0x39
    };

    if (sc >= 0x3A) return 0;
    return shift ? kShift[sc] : kNormal[sc];
}

char PS2Keyboard::scancode_to_ascii(uint8_t sc)
{
    return scancode_to_ascii(sc, false);
}
