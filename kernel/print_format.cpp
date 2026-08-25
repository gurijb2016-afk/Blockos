#include "print_format.hpp"

#include "libc/include/stdio.h"

void hexdump(const uint8_t* data, size_t output_length, uint32_t offset)
{
    const size_t hex_per_row = 16;

    for (size_t i = offset; i < output_length; i += hex_per_row)
    {
        printf("%08X", (unsigned int) i);

        for (size_t j = 0; j < hex_per_row && i + j < output_length; ++j)
        {
            printf(" %02X", (unsigned int) data[i + j]);
        }

        printf(" ");
        for (size_t j = 0; j < hex_per_row && i + j < output_length; ++j)
        {
            const char c = (char) data[i + j];
            (c >= 32 && c < 127) ? printf("%c", c) : printf(".");
        }

        printf("\n");
    }
}
