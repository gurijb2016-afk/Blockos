#include "ramfs.hpp"
#include <cstring>

size_t ramfs::count() {
    const struct ramfile* it = __ramfs_start;
    size_t cnt = 0;
    while (it < __ramfs_end) { ++cnt; ++it; }
    return cnt;
}

const char* ramfs::name_at(size_t idx) {
    const struct ramfile* it = __ramfs_start;
    size_t i = 0;
    while (it < __ramfs_end) {
        if (i == idx) return it->name;
        ++i; ++it;
    }
    return nullptr;
}

const uint8_t* ramfs::get(const char* name, uint32_t* size_out) {
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
