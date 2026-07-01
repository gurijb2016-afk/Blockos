#include "ramfs.h"
#include <string.h>

// Simple embedded ramfs: list of name/ptr/size
struct ramfile { const char* name; const uint8_t* data; uint32_t size; };

extern const struct ramfile __ramfs_start[];
extern const struct ramfile __ramfs_end[];

const uint8_t* ramfs_get(const char* name, uint32_t* size_out) {
    const struct ramfile* it = __ramfs_start;
    while (it < __ramfs_end) {
        if (strcmp(it->name, name) == 0) {
            if (size_out) *size_out = it->size;
            return it->data;
        }
        ++it;
    }
    return NULL;
}
