#include "vfs.hpp"
#include "ramfs.hpp"
#include "allocator.hpp"
#include <cstring>

// Simple in-memory VFS that starts by importing embedded ramfs entries,
// and allows creating/writing files at runtime using allocator.

struct vfs_entry {
    const char* name;
    uint8_t* data;
    uint32_t size;
    vfs_entry* next;
};

static vfs_entry* vfs_root = nullptr;

static vfs_entry* find_entry(const char* name) {
    for (vfs_entry* e = vfs_root; e; e = e->next) {
        if (std::strcmp(e->name, name) == 0) return e;
    }
    return nullptr;
}

size_t vfs::count_files() {
    size_t cnt = 0;
    for (vfs_entry* e = vfs_root; e; e = e->next) ++cnt;
    return cnt;
}

const char* vfs::name_at(size_t idx) {
    size_t i = 0;
    for (vfs_entry* e = vfs_root; e; e = e->next) {
        if (i == idx) return e->name;
        ++i;
    }
    return nullptr;
}

const uint8_t* vfs::read_file(const char* name, uint32_t* out_size) {
    vfs_entry* e = find_entry(name);
    if (!e) return nullptr;
    if (out_size) *out_size = e->size;
    return e->data;
}

bool vfs::create_file(const char* name, const uint8_t* data, uint32_t size) {
    if (find_entry(name)) return false; // exists
    // allocate entry and copy name/data using allocator
    size_t name_len = std::strlen(name) + 1;
    vfs_entry* e = (vfs_entry*)allocator::alloc(sizeof(vfs_entry));
    if (!e) return false;
    char* n = (char*)allocator::alloc(name_len);
    if (!n) return false;
    memcpy(n, name, name_len);
    uint8_t* d = (uint8_t*)allocator::alloc(size);
    if (!d && size>0) return false;
    if (size>0) memcpy(d, data, size);
    e->name = n; e->data = d; e->size = size; e->next = vfs_root; vfs_root = e;
    return true;
}

bool vfs::write_file(const char* name, const uint8_t* data, uint32_t size) {
    vfs_entry* e = find_entry(name);
    if (!e) return create_file(name, data, size);
    // allocate new buffer and copy (no free)
    uint8_t* d = (uint8_t*)allocator::alloc(size);
    if (!d && size>0) return false;
    if (size>0) memcpy(d, data, size);
    e->data = d;
    e->size = size;
    return true;
}

// Initialize VFS from embedded ramfs (call this after allocator initialized)
void vfs_init_from_ramfs() {
    const struct ramfile* it = __ramfs_start;
    while (it < __ramfs_end) {
        vfs::create_file(it->name, it->data, it->size);
        ++it;
    }
}
