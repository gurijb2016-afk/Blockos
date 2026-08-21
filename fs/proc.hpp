#pragma once

#include <stddef.h>

namespace Blockos {
namespace proc {

size_t read(const char* name, char* buffer, size_t max_size);
bool exists(const char* name);
size_t count();
const char* name_at(size_t index);
void init();
bool test();

} // namespace proc
} // namespace Blockos

void init_proc_fs();
