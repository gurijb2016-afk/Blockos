#include "elf_loader.hpp"
#include "paging.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

// Minimal ELF definitions
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint64_t Elf64_Xword;

struct Elf64_Ehdr {
    unsigned char e_ident[16];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off e_phoff;
    Elf64_Off e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
};

struct Elf64_Phdr {
    Elf64_Word p_type;
    Elf64_Word p_flags;
    Elf64_Off p_offset;
    Elf64_Addr p_vaddr;
    Elf64_Addr p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
};

enum { PT_NULL = 0, PT_LOAD = 1 };

bool elf_loader::load_elf64_from_mem(const void* elf_buf, size_t elf_size, uint64_t* entry_out, uint64_t* pml4_out) {
    if (!elf_buf || elf_size < sizeof(Elf64_Ehdr)) return false;
    const Elf64_Ehdr* eh = (const Elf64_Ehdr*)elf_buf;
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') {
        Print(L"elf_loader: invalid ELF magic\n");
        return false;
    }
    if (eh->e_ident[4] != 2) { // ELFCLASS64
        Print(L"elf_loader: not ELF64\n");
        return false;
    }

    // Create a new PML4 for the process
    uint64_t pml4 = paging::create_pml4();
    if (!pml4) return false;

    // Load program headers
    const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)elf_buf + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint64_t file_off = ph[i].p_offset;
        uint64_t file_sz = ph[i].p_filesz;
        uint64_t mem_sz = ph[i].p_memsz;
        uint64_t vaddr = ph[i].p_vaddr;
        // Round addresses to 4KB
        uint64_t page_base = vaddr & ~0xFFFULL;
        uint64_t page_end = (vaddr + mem_sz + 0xFFFULL) & ~0xFFFULL;
        for (uint64_t a = page_base; a < page_end; a += 0x1000) {
            void* page = paging::alloc_page();
            if (!page) return false;
            uint64_t paddr = (uint64_t)(UINTN)page;
            // For simplicity map the page at the same physical address (identity mapping in new pml4)
            if (!paging::map_4k(pml4, a, paddr, (1ULL<<1) | 1ULL)) return false; // present|writable
            // Zero the page
            memset((void*)(UINTN)paddr, 0, 0x1000);
        }
        // Copy file contents into mapped pages
        memcpy((void*)(UINTN)vaddr, (const uint8_t*)elf_buf + file_off, file_sz);
    }

    if (entry_out) *entry_out = eh->e_entry;
    if (pml4_out) *pml4_out = pml4;
    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"elf_loader: loaded ELF entry=0x%016lx pml4=0x%016lx\n", eh->e_entry, pml4);
    Print(buf);
    return true;
}
