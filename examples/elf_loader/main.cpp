#include "kernel/elf_loader.hpp"
#include <stdint.h>

void example_elf_loader(const void* image, size_t image_size)
{
    uint64_t entry = 0;
    uint64_t pml4 = 0;

    if (!image || image_size == 0)
        return;

    if (elf_loader::load_elf64_from_mem(image, image_size, &entry, &pml4)) {
        // A real process manager would install the address space and transfer
        // execution to `entry` in a controlled process context.
        (void)entry;
        (void)pml4;
    }
}
