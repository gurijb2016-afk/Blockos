#pragma once
#include <stdint.h>
#include <stddef.h>
namespace elf_loader {
bool load_elf64_from_mem(const void* elf_buf,size_t elf_size,uint64_t* entry_out,uint64_t* pml4_out,uint64_t* stack_out=nullptr);
}
