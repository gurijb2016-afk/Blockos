#include "paging.hpp"
#include "dma.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

// Basic x86_64 4-level page table helpers. This is minimal and assumes
// the environment maps physical memory 1:1 in the initial stage (UEFI usually does).

static const uint64_t PTE_PRESENT = (1ULL << 0);
static const uint64_t PTE_WRITABLE = (1ULL << 1);
static const uint64_t PTE_USER = (1ULL << 2);
static const uint64_t PTE_PWT = (1ULL << 3);
static const uint64_t PTE_PCD = (1ULL << 4);
static const uint64_t PTE_ACCESSED = (1ULL << 5);
static const uint64_t PTE_DIRTY = (1ULL << 6);
static const uint64_t PTE_PSE = (1ULL << 7);
static const uint64_t PTE_GLOBAL = (1ULL << 8);

static inline uint64_t vaddr_index(uint64_t vaddr, int level) {
    // level: 4 -> PML4, 3 -> PDPT, 2 -> PD, 1 -> PT
    int shift = 12 + (level - 1) * 9;
    return (vaddr >> shift) & 0x1FFu;
}

void* paging::alloc_page() {
    void* p = dma::alloc(4096, 4096);
    if (p) memset(p, 0, 4096);
    return p;
}

uint64_t paging::create_pml4() {
    void* p = paging::alloc_page();
    if (!p) return 0;
    return (uint64_t)(UINTN)p;
}

bool ensure_table_entry(uint64_t table_phys, uint64_t idx, uint64_t flags, uint64_t*& out_table) {
    // table_phys is physical address of a 4KB table, and virtual access is assumed identity-mapped
    uint64_t* table = (uint64_t*)(UINTN)table_phys;
    uint64_t ent = table[idx];
    if ((ent & PTE_PRESENT) == 0) {
        void* newp = dma::alloc(4096, 4096);
        if (!newp) return false;
        memset(newp, 0, 4096);
        uint64_t newp_phys = (uint64_t)(UINTN)newp;
        table[idx] = (newp_phys & 0x000ffffffffff000ULL) | (flags & 0xFFFULL) | PTE_PRESENT;
        out_table = (uint64_t*)(UINTN)newp_phys;
        return true;
    } else {
        uint64_t next_phys = ent & 0x000ffffffffff000ULL;
        out_table = (uint64_t*)(UINTN)next_phys;
        return true;
    }
}

bool paging::map_4k(uint64_t pml4_phys, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (pml4_phys == 0) return false;
    // Walk levels 4 -> 1
    uint64_t* pml4 = (uint64_t*)(UINTN)pml4_phys;
    uint64_t idx4 = vaddr_index(vaddr, 4);
    uint64_t idx3 = vaddr_index(vaddr, 3);
    uint64_t idx2 = vaddr_index(vaddr, 2);
    uint64_t idx1 = vaddr_index(vaddr, 1);

    uint64_t* pdpt = nullptr;
    if (!ensure_table_entry(pml4_phys, idx4, PTE_WRITABLE, pdpt)) return false;
    uint64_t pdpt_phys = (uint64_t)(UINTN)pdpt;

    uint64_t* pd = nullptr;
    if (!ensure_table_entry(pdpt_phys, idx3, PTE_WRITABLE, pd)) return false;
    uint64_t pd_phys = (uint64_t)(UINTN)pd;

    uint64_t* pt = nullptr;
    if (!ensure_table_entry(pd_phys, idx2, PTE_WRITABLE, pt)) return false;
    uint64_t pt_phys = (uint64_t)(UINTN)pt;

    // Finally set PT entry
    uint64_t* pt_table = (uint64_t*)(UINTN)pt_phys;
    pt_table[idx1] = (paddr & 0x000ffffffffff000ULL) | (flags & 0xFFFULL) | PTE_PRESENT;
    return true;
}

uint64_t paging::read_cr3() {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r" (cr3));
    return cr3;
}

void paging::switch_pml4(uint64_t pml4_phys) {
    __asm__ volatile ("mov %0, %%cr3" : : "r" (pml4_phys));
}

bool paging::init_paging() {
    // Create a kernel PML4 and identity-map the first 4MB for initial use.
    uint64_t pml4 = paging::create_pml4();
    if (!pml4) return false;
    // Map low memory (0..4MB) identity to ensure simple accesses if needed
    for (uint64_t addr = 0; addr < 4 * 1024 * 1024; addr += 0x1000) {
        if (!paging::map_4k(pml4, addr, addr, PTE_WRITABLE)) {
            Print(L"paging: map_4k failed during init\n");
            return false;
        }
    }
    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"paging: created PML4 at phys 0x%016lx\n", pml4);
    Print(buf);
    // Do not switch CR3 automatically; caller decides when to enable paging
    return true;
}
