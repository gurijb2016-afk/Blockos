#include "ramfs.h"
#include <cstring>

const uint8_t* ramfs_get(const char* name, uint32_t* size_out) {
    const struct ramfile* it = __ramfs_start;
    while (it < __ramfs_end) {
        if (std::strcmp(it->name, name) == 0) {
            if (size_out) *size_out = it->size;
            return it->data;
        }
        ++it;
    }
    return nullptr;
}
