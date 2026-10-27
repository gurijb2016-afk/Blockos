#include "drivers/serial.hpp"

void example_serial()
{
    if (!serial::init(serial::COM1, 115200))
        return;

    serial::write_string("BlockOS 16550 UART Serial online\r\n", serial::COM1);

    if (serial::has_data(serial::COM1)) {
        int16_t c = serial::read_char_nonblocking(serial::COM1);
        (void)c;
    }
}
