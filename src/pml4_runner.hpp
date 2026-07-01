#pragma once
#include <stdint.h>

// Run code while CPU CR3 is switched to the provided PML4 physical address.
// This does not change privilege level (still runs in kernel mode). Use only for
// controlled demos and testing of address-space isolation for mapped pages.

// Save current CR3, switch to pml4_phys, call entry(), restore CR3.
void run_in_address_space(uint64_t pml4_phys, void (*entry)(void));

// Load ELF from memory and run its entry point inside a freshly created PML4.
// Returns true on success.
bool run_elf_in_address_space(const void* elf_buf, size_t elf_size);
