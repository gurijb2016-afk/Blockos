#pragma once

#include <stddef.h>

#include "../kernel/process.hpp"

namespace blockos::proc
{

/* Kernel-side proc reader used by the shell/debug commands. */
size_t read(const char* name, char* buffer, size_t max_size);
bool exists(const char* name);
size_t count();
const char* name_at(size_t index);

/* Materialise the Linux-compatible /proc view in the current simple VFS. */
void init();
bool refresh(process::Process* current = nullptr);

/* Helpers used by the userspace syscall layer before VFS lookup. */
bool is_proc_path(const char* path);
bool is_directory_path(const char* path, process::Process* current = nullptr);
size_t directory_entry_count(const char* path, process::Process* current = nullptr);
const char* directory_entry_name(const char* path, size_t index, process::Process* current = nullptr);

bool test();

} // namespace blockos::proc

void init_proc_fs();
