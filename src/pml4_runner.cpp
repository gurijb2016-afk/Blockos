#include "pml4_runner.hpp"
#include "elf_loader.hpp"
#include "paging.hpp"
#include <efi.h>
#include <efilib.h>

// Note: This runner switches CR3 to the given PML4 and calls the provided entry
// point while remaining in the current privilege level. It is intended as a
// safe stepping stone before implementing full ring-3 user-mode entry.

void run_in_address_space(uint64_t pml4_phys, void (*entry)(void)) {
    if (!pml4_phys || !entry) return;
    CHAR16 buf[128];
    uint64_t old_cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r" (old_cr3));
    UnicodeSPrint(buf, sizeof(buf), L"pml4_runner: switching CR3 0x%016lx -> 0x%016lx\n", old_cr3, pml4_phys);
    Print(buf);

    // Switch CR3
    paging::switch_pml4(pml4_phys);

    // Call entry
    entry();

    // Restore original CR3
    paging::switch_pml4(old_cr3);
    __asm__ volatile ("mov %%cr3, %0" : "=r" (old_cr3));
    UnicodeSPrint(buf, sizeof(buf), L"pml4_runner: restored CR3 to 0x%016lx\n", old_cr3);
    Print(buf);
}

bool run_elf_in_address_space(const void* elf_buf, size_t elf_size) {
    if (!elf_buf || elf_size == 0) return false;
    uint64_t entry = 0;
    uint64_t pml4 = 0;
    if (!elf_loader::load_elf64_from_mem(elf_buf, elf_size, &entry, &pml4)) {
        Print(L"pml4_runner: elf_loader failed\n");
        return false;
    }

    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"pml4_runner: running ELF entry 0x%016lx pml4 0x%016lx\n", entry, pml4);
    Print(buf);

    // Cast entry to a function pointer and run in the new address space.
    void (*entry_fn)(void) = (void(*)(void))(UINTN)entry;
    run_in_address_space(pml4, entry_fn);
    return true;
}
