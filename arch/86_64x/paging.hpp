#pragma once
#include <stdint.h>

namespace paging {
    // Initialize paging subsystem (create initial kernel PML4 but do not switch automatically).
    // Returns true on success.
    bool init_paging();

    // Allocate a single 4KB physical page (uses dma::alloc under the hood).
    void* alloc_page();

    // Create an empty PML4 table and return its physical address (as uint64_t).
    uint64_t create_pml4();

    // Map a single 4KB page in the given PML4 (pml4_phys is physical address of PML4 table).
    // flags should be standard x86_64 PTE flags (present/write/user/etc).
    bool map_4k(uint64_t pml4_phys, uint64_t vaddr, uint64_t paddr, uint64_t flags);

    // Switch the CPU to use the given PML4 physical address (writes CR3).
    void switch_pml4(uint64_t pml4_phys);

    // Read current CR3 value (physical address of current PML4)
    uint64_t read_cr3();
}
