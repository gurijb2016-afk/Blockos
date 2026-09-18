#pragma once
#include <stdint.h>

namespace tls {

constexpr uint32_t IA32_FS_BASE = 0xC0000100u;

uint64_t read_fs_base();
void write_fs_base(uint64_t value);
void activate_for_process(uint64_t fs_base);

}

