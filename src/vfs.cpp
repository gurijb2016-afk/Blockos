#include "vfs.hpp"
#include "ramfs.hpp"

size_t vfs::count_files() { return ramfs::count(); }
const char* vfs::name_at(size_t idx) { return ramfs::name_at(idx); }
const uint8_t* vfs::read_file(const char* name, uint32_t* out_size) { return ramfs::get(name, out_size); }
