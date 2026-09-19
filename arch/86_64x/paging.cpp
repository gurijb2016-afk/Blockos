#include "paging.hpp"
#include "dma.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static const uint64_t PTE_PRESENT  = 1ULL << 0;
static const uint64_t PTE_WRITABLE = 1ULL << 1;
static const uint64_t PTE_USER     = 1ULL << 2;
static const uint64_t PTE_PWT      = 1ULL << 3;
static const uint64_t PTE_PCD      = 1ULL << 4;
static const uint64_t PTE_NX       = 1ULL << 63;
static const uint64_t ADDR_MASK    = 0x000ffffffffff000ULL;
static const uint64_t PAGE_MASK    = ~0xFFFULL;

static inline uint64_t idx(uint64_t v, int level) {
    return (v >> (12 + (level - 1) * 9)) & 0x1ffULL;
}

void* paging::alloc_page() {
    void* p = dma::alloc(4096, 4096);
    if (p) memset(p, 0, 4096);
    return p;
}

uint64_t paging::create_pml4() {
    void* p = paging::alloc_page();
    return (uint64_t)(uintptr_t)p;
}

uint64_t paging::create_user_pml4() {
    // A user address space still needs the kernel mappings. Clone the
    // currently active PML4 so kernel code, interrupt/CPU state, and
    // shared kernel services remain mapped while user pages are added.
    return paging::clone_current_pml4();
}

uint64_t paging::clone_current_pml4() {
    uint64_t cur = paging::read_cr3() & ADDR_MASK;
    if (!cur) return 0;
    uint64_t dst = paging::create_pml4();
    if (!dst) return 0;
    memcpy((void*)(uintptr_t)dst, (const void*)(uintptr_t)cur, 4096);
    return dst;
}

static bool table(uint64_t phys, uint64_t i, uint64_t flags, uint64_t*& out) {
    uint64_t* t = (uint64_t*)(uintptr_t)phys;
    uint64_t e = t[i];
    if (!(e & PTE_PRESENT)) {
        void* p = dma::alloc(4096,4096);
        if (!p) return false;
        memset(p,0,4096);
        uint64_t f = PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
        t[i] = ((uint64_t)(uintptr_t)p & ADDR_MASK) | f;
        out=(uint64_t*)(uintptr_t)p;
        return true;
    }
    if (flags & PTE_USER) t[i] |= PTE_USER;
    if (flags & PTE_WRITABLE) t[i] |= PTE_WRITABLE;
    out=(uint64_t*)(uintptr_t)(e & ADDR_MASK);
    return true;
}

bool paging::map_4k(uint64_t pml4_phys,uint64_t vaddr,uint64_t paddr,uint64_t flags) {
    if (!pml4_phys || (vaddr & 0xfff) || (paddr & 0xfff)) return false;
    uint64_t* a=nullptr; uint64_t* b=nullptr; uint64_t* c=nullptr;
    if (!table(pml4_phys,idx(vaddr,4),flags,a)) return false;
    if (!table((uint64_t)(uintptr_t)a,idx(vaddr,3),flags,b)) return false;
    if (!table((uint64_t)(uintptr_t)b,idx(vaddr,2),flags,c)) return false;
    uint64_t* pt=c;
    pt[idx(vaddr,1)] = (paddr & ADDR_MASK) | (flags & (PTE_WRITABLE|PTE_USER|PTE_PWT|PTE_PCD|PTE_NX)) | PTE_PRESENT;
    return true;
}

bool paging::unmap_4k(uint64_t pml4_phys,uint64_t vaddr) {
    if (!pml4_phys || (vaddr & 0xfff)) return false;
    uint64_t* pml4=(uint64_t*)(uintptr_t)pml4_phys;
    uint64_t e=pml4[idx(vaddr,4)]; if (!(e&PTE_PRESENT)) return false;
    uint64_t* pdpt=(uint64_t*)(uintptr_t)(e&ADDR_MASK);
    e=pdpt[idx(vaddr,3)]; if (!(e&PTE_PRESENT)) return false;
    uint64_t* pd=(uint64_t*)(uintptr_t)(e&ADDR_MASK);
    e=pd[idx(vaddr,2)]; if (!(e&PTE_PRESENT)) return false;
    uint64_t* pt=(uint64_t*)(uintptr_t)(e&ADDR_MASK);
    pt[idx(vaddr,1)] = 0;
    asm volatile("invlpg (%0)"::"r"((void*)(uintptr_t)vaddr):"memory");
    return true;
}

uint64_t paging::read_cr3(){uint64_t x; asm volatile("mov %%cr3,%0":"=r"(x)); return x & ADDR_MASK;}
void paging::switch_pml4(uint64_t pml4_phys){asm volatile("mov %0,%%cr3"::"r"(pml4_phys&ADDR_MASK):"memory");}

bool paging::map_user_range(uint64_t pml4,uint64_t base,size_t len,uint64_t flags){
    if (!len || base+len<base) return false;
    uint64_t a=base&PAGE_MASK, end=(base+len+0xfffULL)&PAGE_MASK;
    if (end<a) return false;
    for(;a<end;a+=0x1000){void* p=alloc_page(); if(!p) return false; if(!map_4k(pml4,a,(uint64_t)(uintptr_t)p,flags|PTE_USER)) return false;}
    return true;
}

static bool walk_leaf(uint64_t pml4_phys, uint64_t vaddr, uint64_t*& out_pte) {
    if (!pml4_phys || (vaddr & ~0x00007fffffffffffULL)) return false;
    uint64_t* pml4 = (uint64_t*)(uintptr_t)(pml4_phys & ADDR_MASK);
    uint64_t e = pml4[idx(vaddr, 4)];
    if (!(e & PTE_PRESENT) || !(e & PTE_USER)) return false;
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(e & ADDR_MASK);
    e = pdpt[idx(vaddr, 3)];
    if (!(e & PTE_PRESENT) || !(e & PTE_USER)) return false;
    uint64_t* pd = (uint64_t*)(uintptr_t)(e & ADDR_MASK);
    e = pd[idx(vaddr, 2)];
    if (!(e & PTE_PRESENT) || !(e & PTE_USER)) return false;
    uint64_t* pt = (uint64_t*)(uintptr_t)(e & ADDR_MASK);
    e = pt[idx(vaddr, 1)];
    if (!(e & PTE_PRESENT) || !(e & PTE_USER)) return false;
    out_pte = pt + idx(vaddr, 1);
    return true;
}

bool paging::protect_user_range(uint64_t pml4, uint64_t base, size_t len, uint64_t flags) {
    if (!pml4 || !len) return false;
    if (base > 0x00007fffffffffffULL) return false;
    uint64_t end_addr = base + (uint64_t)len;
    if (end_addr < base || end_addr > 0x0000800000000000ULL) return false;

    uint64_t first = base & PAGE_MASK;
    uint64_t last = (end_addr - 1ULL) & PAGE_MASK;

    // Validate the complete range first so a failure cannot leave only part
    // of the requested region with changed permissions.
    for (uint64_t page = first;; page += 0x1000ULL) {
        uint64_t* pte = nullptr;
        if (!walk_leaf(pml4, page, pte)) return false;
        if (page == last) break;
        if (page > last - 0x1000ULL) return false;
    }

    const uint64_t leaf_flags = PTE_WRITABLE | PTE_USER | PTE_PWT | PTE_PCD | PTE_NX;
    for (uint64_t page = first;; page += 0x1000ULL) {
        uint64_t* pte = nullptr;
        if (!walk_leaf(pml4, page, pte)) return false;
        uint64_t old = *pte;
        uint64_t next = (old & ADDR_MASK) | PTE_PRESENT | (flags & leaf_flags);
        next |= PTE_USER;
        *pte = next;

        if (page == last) break;
        if (page > last - 0x1000ULL) return false;
    }

    if ((paging::read_cr3() & ADDR_MASK) == (pml4 & ADDR_MASK)) {
        for (uint64_t page = first;; page += 0x1000ULL) {
            asm volatile("invlpg (%0)" :: "r"((void*)(uintptr_t)page) : "memory");
            if (page == last) break;
            if (page > last - 0x1000ULL) break;
        }
    }
    return true;
}

static bool user_page(uint64_t v,bool write){
    uint64_t cr3=paging::read_cr3(); if(!cr3) return false;
    uint64_t* pml4=(uint64_t*)(uintptr_t)cr3; uint64_t e=pml4[idx(v,4)]; if(!(e&PTE_PRESENT)||!(e&PTE_USER)) return false;
    uint64_t* pdpt=(uint64_t*)(uintptr_t)(e&ADDR_MASK); e=pdpt[idx(v,3)]; if(!(e&PTE_PRESENT)||!(e&PTE_USER)) return false;
    uint64_t* pd=(uint64_t*)(uintptr_t)(e&ADDR_MASK); e=pd[idx(v,2)]; if(!(e&PTE_PRESENT)||!(e&PTE_USER)) return false;
    uint64_t* pt=(uint64_t*)(uintptr_t)(e&ADDR_MASK); e=pt[idx(v,1)]; if(!(e&PTE_PRESENT)||!(e&PTE_USER)) return false;
    return !write || (e&PTE_WRITABLE);
}

bool paging::is_user_range(uint64_t addr,size_t len,bool write){
    if (!len || addr+len<addr) return false;
    uint64_t end=addr+len-1;
    if (end>0x00007fffffffffffULL) return false;
    for(uint64_t p=addr&PAGE_MASK;;p+=0x1000){if(!user_page(p,write)) return false; if(p>end-((end)&0xfffULL)) break;}
    return user_page(end&PAGE_MASK,write);
}

bool paging::init_paging(){ return paging::read_cr3()!=0; }
