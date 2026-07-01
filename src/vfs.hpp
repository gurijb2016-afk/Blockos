#pragma once
#include <cstddef>
#include <cstdint>

namespace vfs {
    size_t count_files();
    const char* name_at(size_t idx);
    const uint8_t* read_file(const char* name, uint32_t* out_size);
}
