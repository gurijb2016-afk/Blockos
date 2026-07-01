#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ramfile { const char* name; const uint8_t* data; uint32_t size; };

extern const struct ramfile __ramfs_start[];
extern const struct ramfile __ramfs_end[];

#ifdef __cplusplus
}
#endif

// C++ wrapper functions
#ifdef __cplusplus
#include <cstdint>
#include <cstddef>

namespace ramfs {
    size_t count();
    const char* name_at(size_t idx);
    const uint8_t* get(const char* name, uint32_t* size_out);
}
#endif
