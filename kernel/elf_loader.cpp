#include "elf_loader.hpp"
#include "arch/86_64x/paging.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace {
struct Ehdr{unsigned char id[16];uint16_t type,machine;uint32_t version;uint64_t entry,phoff,shoff;uint32_t flags;uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;};
struct Phdr{uint32_t type,flags;uint64_t offset,vaddr,paddr,filesz,memsz,align;};
constexpr uint32_t PT_LOAD=1,PT_INTERP=3; constexpr uint16_t ET_EXEC=2,ET_DYN=3,EM_X86_64=62;
constexpr uint32_t PF_X=1,PF_W=2,PF_R=4; constexpr uint64_t U=4,W=2,NX=1ULL<<63;
constexpr uint64_t PAGE=0x1000,USER_TOP=0x00007ffffffff000ULL,STACK_TOP=0x00007ffffff00000ULL,STACK_PAGES=32;
constexpr uint64_t DEFAULT_DYN_BASE=0x0000700000000000ULL; // used only if load_elf64_into is ever called with fixed_base==0 for an ET_DYN image outside the interpreter path
static bool add_ok(uint64_t a,uint64_t b,uint64_t* out){if(a+b<a)return false;*out=a+b;return true;}
}

bool elf_loader::load_elf64_into(
    uint64_t pml4,
    const void* buf,
    size_t size,
    uint64_t fixed_base,
    LoadResult* out)
{
    if (!buf || size < sizeof(Ehdr) || !out || !pml4) return false;
    memset(out, 0, sizeof(*out));

    const Ehdr* e = (const Ehdr*)buf;
    if (e->id[0]!=0x7f||e->id[1]!='E'||e->id[2]!='L'||e->id[3]!='F'||e->id[4]!=2||e->id[5]!=1) return false;
    if (e->machine!=EM_X86_64||(e->type!=ET_EXEC&&e->type!=ET_DYN)||e->version!=1) return false;
    if (e->ehsize<sizeof(Ehdr)||e->phentsize!=sizeof(Phdr)||e->phnum==0||e->phnum>128) return false;

    uint64_t phend=0;
    if(!add_ok(e->phoff,(uint64_t)e->phnum*sizeof(Phdr),&phend)||phend>size) return false;
    const Phdr* ph=(const Phdr*)((const uint8_t*)buf+e->phoff);

    uint64_t load_bias = 0;
    if (e->type == ET_DYN) load_bias = fixed_base ? fixed_base : DEFAULT_DYN_BASE;

    bool loaded = false;
    uint64_t phdr_vaddr = 0;

    for (uint16_t i = 0; i < e->phnum; i++) {
        const Phdr& p = ph[i];

        if (p.type == PT_INTERP) {
            uint64_t iend = 0;
            if (!add_ok(p.offset, p.filesz, &iend) || iend > size ||
                p.filesz == 0 || p.filesz >= sizeof(out->interp_path))
                return false;
            memcpy(out->interp_path, (const uint8_t*)buf + p.offset, p.filesz);
            out->interp_path[p.filesz] = 0;
            out->has_interp = true;
            continue;
        }

        if (p.type != PT_LOAD) continue;
        if (p.memsz < p.filesz || (p.align && (p.align & (p.align - 1)))) return false;

        uint64_t fend = 0;
        if (!add_ok(p.offset, p.filesz, &fend) || fend > size) return false;

        uint64_t seg_vaddr = 0;
        if (!add_ok(p.vaddr, load_bias, &seg_vaddr)) return false;
        uint64_t vend = 0;
        if (!add_ok(seg_vaddr, p.memsz, &vend) || vend > USER_TOP) return false;
        if (seg_vaddr < 0x1000 || vend <= seg_vaddr) return false;

        // If this segment's file range covers the program header table,
        // remember where the headers ended up in the mapped address space -
        // any real dynamic linker needs this via AT_PHDR to find PT_DYNAMIC.
        if (e->phoff >= p.offset && e->phoff < p.offset + p.filesz) {
            phdr_vaddr = seg_vaddr + (e->phoff - p.offset);
        }

        uint64_t base = seg_vaddr & ~(PAGE - 1);
        uint64_t end  = (vend + PAGE - 1) & ~(PAGE - 1);
        if (end < vend) return false;

        uint64_t flags = U;
        if (p.flags & PF_W) flags |= W;
        if (!(p.flags & PF_X)) flags |= NX;

        for (uint64_t va = base; va < end; va += PAGE) {
            void* page = paging::alloc_page();
            if (!page) return false;
            if (!paging::map_4k(pml4, va, (uint64_t)(uintptr_t)page, flags)) return false;

            uint64_t page_lo = va, page_hi = va + PAGE;
            uint64_t src_lo = seg_vaddr, src_hi = seg_vaddr + p.filesz;
            uint64_t lo = page_lo > src_lo ? page_lo : src_lo;
            uint64_t hi = page_hi < src_hi ? page_hi : src_hi;
            if (hi > lo) {
                size_t n = (size_t)(hi - lo);
                memcpy((uint8_t*)page + (lo - va),
                       (const uint8_t*)buf + p.offset + (lo - seg_vaddr), n);
            }
        }
        loaded = true;
    }

    if (!loaded) return false;

    uint64_t final_entry = e->entry + load_bias;
    if (final_entry < 0x1000 || final_entry >= USER_TOP) return false;

    out->entry      = final_entry;
    out->load_bias  = load_bias;
    out->phdr_vaddr = phdr_vaddr;
    out->phnum      = e->phnum;
    out->phentsize  = e->phentsize;
    return true;
}

bool elf_loader::load_elf64_from_mem(
    const void* buf,
    size_t size,
    uint64_t* entry_out,
    uint64_t* pml4_out,
    uint64_t* stack_out)
{
    if (!entry_out || !pml4_out) return false;

    uint64_t pml4 = paging::create_user_pml4();
    if (!pml4) return false;

    LoadResult r;
    if (!load_elf64_into(pml4, buf, size, 0, &r)) return false;

    // Preserve the old, already-working behavior for plain static binaries:
    // a dynamically-linked image (PT_INTERP present) must go through
    // process::create()'s interpreter-aware path instead, since a single
    // LoadResult/entry pair can't describe "run ld.so first" on its own.
    if (r.has_interp) return false;

    uint64_t stack_base = STACK_TOP - STACK_PAGES * PAGE;
    if (!paging::map_user_range(pml4, stack_base, STACK_PAGES * PAGE, U | W | NX))
        return false;

    *entry_out = r.entry;
    *pml4_out  = pml4;
    if (stack_out) *stack_out = STACK_TOP - 16;
    return true;
}

uint64_t elf_loader::build_initial_stack(
    uint64_t pml4,
    uint64_t stack_top,
    size_t stack_pages,
    const char* const* argv,
    int argc,
    const char* const* envp,
    int envc,
    const LoadResult& main_image,
    uint64_t interp_base)
{
    if (stack_pages < 1 || !pml4) return 0;

    uint64_t low_base = stack_top - (uint64_t)stack_pages * PAGE;

    // Bulk-map the lower stack pages the normal way (no kernel pointer
    // needed - the process just uses them as scratch stack space).
    if (stack_pages > 1) {
        if (!paging::map_user_range(pml4, low_base, (stack_pages - 1) * PAGE, U | W | NX))
            return 0;
    }

    // Map the top page by hand so we get a writable kernel-side pointer to
    // it *before* the process ever runs, to seed argv/envp/auxv.
    void* top_page = paging::alloc_page();
    if (!top_page) return 0;
    uint64_t top_vaddr = stack_top - PAGE;
    if (!paging::map_4k(pml4, top_vaddr, (uint64_t)(uintptr_t)top_page, U | W | NX))
        return 0;

    uint8_t* p = (uint8_t*)top_page;
    size_t off = 0;

    if (argc < 0) argc = 0;
    if (envc < 0) envc = 0;
    if (argc > 16) argc = 16; // generous for a bootstrap loader; raise if a real shell needs more
    if (envc > 16) envc = 16;

    uint64_t argv_vaddrs[16];
    uint64_t envp_vaddrs[16];

    for (int i = 0; i < argc; i++) {
        size_t len = 0;
        while (argv[i][len]) len++;
        len++;
        if (off + len > PAGE) return 0;
        memcpy(p + off, argv[i], len);
        argv_vaddrs[i] = top_vaddr + off;
        off += len;
    }
    for (int i = 0; i < envc; i++) {
        size_t len = 0;
        while (envp[i][len]) len++;
        len++;
        if (off + len > PAGE) return 0;
        memcpy(p + off, envp[i], len);
        envp_vaddrs[i] = top_vaddr + off;
        off += len;
    }

    off = (off + 7) & ~size_t(7);

    constexpr int AUXC = 7; // AT_PHDR, AT_PHENT, AT_PHNUM, AT_BASE, AT_ENTRY, AT_PAGESZ, AT_NULL
    size_t table_bytes =
        8 +
        (size_t)(argc + 1) * 8 +
        (size_t)(envc + 1) * 8 +
        (size_t)AUXC * 16;

    if (off + table_bytes > PAGE) return 0;

    size_t table_off = (PAGE - table_bytes) & ~size_t(15);
    if (table_off < off) return 0;

    uint64_t* tbl = (uint64_t*)(p + table_off);
    size_t idx = 0;
    tbl[idx++] = (uint64_t)argc;
    for (int i = 0; i < argc; i++) tbl[idx++] = argv_vaddrs[i];
    tbl[idx++] = 0;
    for (int i = 0; i < envc; i++) tbl[idx++] = envp_vaddrs[i];
    tbl[idx++] = 0;

    auto put_aux = [&](uint64_t type, uint64_t val) {
        tbl[idx++] = type;
        tbl[idx++] = val;
    };
    put_aux(3 /*AT_PHDR*/,   main_image.phdr_vaddr);
    put_aux(4 /*AT_PHENT*/,  main_image.phentsize);
    put_aux(5 /*AT_PHNUM*/,  main_image.phnum);
    put_aux(7 /*AT_BASE*/,   interp_base);
    put_aux(9 /*AT_ENTRY*/,  main_image.entry);
    put_aux(6 /*AT_PAGESZ*/, PAGE);
    put_aux(0 /*AT_NULL*/,   0);

    return top_vaddr + table_off;
}
