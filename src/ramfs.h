#pragma once
#include <stdint.h>

const uint8_t* ramfs_get(const char* name, uint32_t* size_out);
