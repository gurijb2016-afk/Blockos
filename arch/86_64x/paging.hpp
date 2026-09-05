#pragma once
#include <stdint.h>
#include <stddef.h>

namespace paging {
bool init_paging();
void* alloc_page();
uint64_t create_pml4();
uint64_t clone_current_pml4();
bool map_4k(uint64_t pml4_phys, uint64_t vaddr, uint64_t paddr, uint64_t flags);
bool unmap_4k(uint64_t pml4_phys, uint64_t vaddr);
void switch_pml4(uint64_t pml4_phys);
uint64_t read_cr3();
bool is_user_range(uint64_t addr, size_t len, bool write);
bool map_user_range(uint64_t pml4_phys, uint64_t base, size_t len, uint64_t flags);
}
