#include "drivers/ps2keyboard.hpp"
#include "drivers/ps2mouse.hpp"

void example_ps2_input()
{
    PS2Keyboard keyboard;
    PS2Mouse mouse;

    keyboard.init();
    mouse.init();

    KeyEvent key{};
    if (keyboard.poll(key)) {
        (void)PS2Keyboard::scancode_to_ascii(key.scancode);
    }

    (void)mouse.read_byte_nonblocking();
}
