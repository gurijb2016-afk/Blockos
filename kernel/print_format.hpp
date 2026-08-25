#pragma once

#include <stddef.h>
#include <stdint.h>

void hexdump(const uint8_t* data, size_t output_length, uint32_t offset = 0);
