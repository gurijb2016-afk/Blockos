#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ramfile { const char* name; const uint8_t* data; uint32_t size; };

extern const struct ramfile __ramfs_start[];
extern const struct ramfile __ramfs_end[];

const uint8_t* ramfs_get(const char* name, uint32_t* size_out);

#ifdef __cplusplus
}
#endif
