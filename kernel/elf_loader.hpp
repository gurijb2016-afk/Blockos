#pragma once
#include <stdint.h>
#include <stddef.h>

// Minimal ELF64 loader interface (userland process loader). The loader will parse ELF headers from
// a memory buffer and allocate/load segments into newly created address space (using paging helpers).

namespace elf_loader {
    // Load an ELF64 image from memory. On success, returns true and sets entry_out to the entry point.
    // The loader will create a new PML4 for the process; pml4_out will be set to the new PML4 physical address.
    bool load_elf64_from_mem(const void* elf_buf, size_t elf_size, uint64_t* entry_out, uint64_t* pml4_out);
}
