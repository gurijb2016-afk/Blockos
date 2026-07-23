#include "elf_loader_dynamic.hpp"
#include "paging.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

// Dinamikus ELF64 betöltő valódi implementációja.
//
// FONTOS: ez a fájl a betöltést + relokációt oldja meg, semmi mást.
// Nem ad syscall-t, nem ad libc-t, nem futtat le Chromiumot. Lásd a
// header tetején lévő megjegyzést.
//
// A meglévő kernel/elf_loader.cpp mintáját követi (identity-mapped
// fizikai memóriát feltételez a betöltés pillanatában — ugyanaz a
// feltétel, amit a statikus loader is használ), csak kiegészíti
// PT_DYNAMIC feldolgozással és relokációval.

namespace {

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

struct Elf64_Ehdr {
    unsigned char e_ident[16];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off  e_phoff;
    Elf64_Off  e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
};

struct Elf64_Phdr {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr;
    Elf64_Addr  p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
};

struct Elf64_Dyn {
    Elf64_Sxword d_tag;
    union { Elf64_Xword d_val; Elf64_Addr d_ptr; } d_un;
};

struct Elf64_Sym {
    Elf64_Word    st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half    st_shndx;
    Elf64_Addr    st_value;
    Elf64_Xword   st_size;
};

struct Elf64_Rela { Elf64_Addr r_offset; Elf64_Xword r_info; Elf64_Sxword r_addend; };
struct Elf64_Rel  { Elf64_Addr r_offset; Elf64_Xword r_info; };

#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffffULL)

enum { PT_NULL = 0, PT_LOAD = 1, PT_DYNAMIC = 2 };
enum { PF_X = 1, PF_W = 2, PF_R = 4 };

enum {
    DT_NULL = 0, DT_NEEDED = 1, DT_PLTRELSZ = 2, DT_PLTGOT = 3, DT_HASH = 4,
    DT_STRTAB = 5, DT_SYMTAB = 6, DT_RELA = 7, DT_RELASZ = 8, DT_RELAENT = 9,
    DT_STRSZ = 10, DT_SYMENT = 11, DT_REL = 17, DT_RELSZ = 18, DT_RELENT = 19,
    DT_PLTREL = 20, DT_JMPREL = 23
};

enum {
    R_X86_64_NONE = 0, R_X86_64_64 = 1, R_X86_64_GLOB_DAT = 6,
    R_X86_64_JUMP_SLOT = 7, R_X86_64_RELATIVE = 8
};

struct DynInfo {
    Elf64_Sym  *symtab = nullptr;
    const char *strtab = nullptr;
    Elf64_Rela *rela = nullptr;   uint64_t rela_count = 0;
    Elf64_Rel  *rel  = nullptr;   uint64_t rel_count  = 0;
    Elf64_Rela *jmprel_a = nullptr;
    Elf64_Rel  *jmprel   = nullptr;
    uint64_t    jmprel_count = 0;
    bool        jmprel_is_rela = false;
};

// Egyetlen PT_LOAD szegmens leképezése az új PML4-be, ugyanúgy, ahogy
// kernel/elf_loader.cpp csinálja: fizikai lapok foglalása, bemappelés
// load_base+vaddr-re, majd a fájltartalom bemásolása.
bool map_segment(uint64_t pml4, uint64_t load_base, uint64_t vaddr,
                  uint64_t memsz, uint64_t filesz,
                  const uint8_t *file_data, uint64_t file_off) {
    uint64_t target = load_base + vaddr;
    uint64_t page_base = target & ~0xFFFULL;
    uint64_t page_end = (target + memsz + 0xFFFULL) & ~0xFFFULL;

    for (uint64_t a = page_base; a < page_end; a += 0x1000) {
        void *page = paging::alloc_page();
        if (!page) return false;
        uint64_t paddr = (uint64_t)(UINTN)page;
        if (!paging::map_4k(pml4, a, paddr, (1ULL << 1) | 1ULL)) return false; // present|writable
        memset((void *)(UINTN)paddr, 0, 0x1000);
    }

    if (filesz > 0) {
        // Megjegyzés: ugyanazt a feltevést követi, mint a meglévő
        // statikus loader — a cél virtuális címet közvetlenül írható-
        // nak tekinti a jelenlegi (nem az új) CR3 alatt. Ha ez nem áll
        // fenn a te boot-környezetedben, ezt fizikai lapon keresztüli
        // másolásra kell cserélni (paddr-en át, nem target-en át).
        memcpy((void *)(UINTN)target, file_data + file_off, filesz);
    }
    return true;
}

uint64_t resolve_sym(uint64_t load_base, const DynInfo &di, uint64_t symidx,
                      elf_loader::symbol_resolver_fn resolver) {
    if (symidx == 0 || !di.symtab || !di.strtab) return 0;
    const Elf64_Sym &sym = di.symtab[symidx];
    const char *name = di.strtab + sym.st_name;

    if (sym.st_shndx != 0) {
        // Lokálisan definiált szimbólum: saját érték + load_base.
        return load_base + sym.st_value;
    }
    if (!resolver || name[0] == '\0') return 0;
    return resolver(name);
}

void apply_rela(uint64_t load_base, const DynInfo &di, Elf64_Rela *r,
                uint64_t count, elf_loader::symbol_resolver_fn resolver) {
    for (uint64_t i = 0; i < count; i++) {
        Elf64_Rela &rel = r[i];
        uint64_t type = ELF64_R_TYPE(rel.r_info);
        uint64_t symidx = ELF64_R_SYM(rel.r_info);
        uint64_t *target = (uint64_t *)(UINTN)(load_base + rel.r_offset);

        switch (type) {
            case R_X86_64_RELATIVE:
                *target = load_base + (uint64_t)rel.r_addend;
                break;
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                *target = resolve_sym(load_base, di, symidx, resolver);
                break;
            case R_X86_64_64:
                *target = resolve_sym(load_base, di, symidx, resolver) + (uint64_t)rel.r_addend;
                break;
            case R_X86_64_NONE:
            default:
                break;
        }
    }
}

void apply_rel(uint64_t load_base, const DynInfo &di, Elf64_Rel *r,
               uint64_t count, elf_loader::symbol_resolver_fn resolver) {
    for (uint64_t i = 0; i < count; i++) {
        Elf64_Rel &rel = r[i];
        uint64_t type = ELF64_R_TYPE(rel.r_info);
        uint64_t symidx = ELF64_R_SYM(rel.r_info);
        uint64_t *target = (uint64_t *)(UINTN)(load_base + rel.r_offset);

        switch (type) {
            case R_X86_64_RELATIVE:
                *target = load_base + *target;
                break;
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                *target = resolve_sym(load_base, di, symidx, resolver);
                break;
            case R_X86_64_NONE:
            default:
                break;
        }
    }
}

} // namespace

bool elf_loader::load_elf64_dynamic_from_mem(const void *elf_buf, size_t elf_size,
                                              uint64_t *entry_out, uint64_t *pml4_out,
                                              symbol_resolver_fn resolver) {
    if (!elf_buf || elf_size < sizeof(Elf64_Ehdr)) return false;
    const uint8_t *file_data = (const uint8_t *)elf_buf;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)elf_buf;

    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') {
        Print((CHAR16 *)L"elf_loader_dynamic: invalid ELF magic\n");
        return false;
    }
    if (eh->e_ident[4] != 2) {
        Print((CHAR16 *)L"elf_loader_dynamic: not ELF64\n");
        return false;
    }
    if (eh->e_type != 2 /* ET_EXEC */ && eh->e_type != 3 /* ET_DYN */) {
        Print((CHAR16 *)L"elf_loader_dynamic: unsupported e_type\n");
        return false;
    }
    if (eh->e_phnum == 0 || (uint64_t)eh->e_phoff + (uint64_t)eh->e_phnum * eh->e_phentsize > elf_size) {
        Print((CHAR16 *)L"elf_loader_dynamic: bad program header table\n");
        return false;
    }

    uint64_t pml4 = paging::create_pml4();
    if (!pml4) return false;

    // ET_EXEC: load_base 0 (abszolút vaddr-ek). ET_DYN (PIE): egy
    // választott bázis kell — itt fix placeholder, ASLR/valódi
    // címtér-kiosztás nélkül.
    uint64_t load_base = (eh->e_type == 3) ? 0x0000000000400000ULL : 0;

    const Elf64_Phdr *phdrs = (const Elf64_Phdr *)(file_data + eh->e_phoff);
    const Elf64_Dyn *dyn = nullptr;
    uint64_t dyn_count = 0;

    for (int i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr &ph = phdrs[i];
        if (ph.p_type == PT_LOAD) {
            if (ph.p_offset + ph.p_filesz > elf_size) {
                Print((CHAR16 *)L"elf_loader_dynamic: PT_LOAD out of file bounds\n");
                return false;
            }
            if (!map_segment(pml4, load_base, ph.p_vaddr, ph.p_memsz, ph.p_filesz,
                              file_data, ph.p_offset)) {
                Print((CHAR16 *)L"elf_loader_dynamic: failed to map PT_LOAD segment\n");
                return false;
            }
        } else if (ph.p_type == PT_DYNAMIC) {
            dyn = (const Elf64_Dyn *)(file_data + ph.p_offset);
            dyn_count = ph.p_memsz / sizeof(Elf64_Dyn);
        }
    }

    if (dyn) {
        DynInfo di;
        uint64_t rela_addr = 0, rel_addr = 0, jmprel_addr = 0;
        uint64_t symtab_addr = 0, strtab_addr = 0, pltrel_type = 0;

        for (uint64_t i = 0; i < dyn_count; i++) {
            const Elf64_Dyn &d = dyn[i];
            switch (d.d_tag) {
                case DT_NULL: i = dyn_count; break;
                case DT_SYMTAB: symtab_addr = d.d_un.d_ptr; break;
                case DT_STRTAB: strtab_addr = d.d_un.d_ptr; break;
                case DT_RELA:   rela_addr = d.d_un.d_ptr; break;
                case DT_RELASZ: di.rela_count = d.d_un.d_val / sizeof(Elf64_Rela); break;
                case DT_REL:    rel_addr = d.d_un.d_ptr; break;
                case DT_RELSZ:  di.rel_count = d.d_un.d_val / sizeof(Elf64_Rel); break;
                case DT_JMPREL: jmprel_addr = d.d_un.d_ptr; break;
                case DT_PLTRELSZ: di.jmprel_count = d.d_un.d_val; break;
                case DT_PLTREL: pltrel_type = d.d_un.d_val; break;
                default: break;
            }
        }

        di.strtab = (const char *)(UINTN)(load_base + strtab_addr);
        di.symtab = (Elf64_Sym *)(UINTN)(load_base + symtab_addr);
        if (rela_addr) di.rela = (Elf64_Rela *)(UINTN)(load_base + rela_addr);
        if (rel_addr)  di.rel  = (Elf64_Rel  *)(UINTN)(load_base + rel_addr);

        di.jmprel_is_rela = (pltrel_type == DT_RELA);
        if (jmprel_addr) {
            if (di.jmprel_is_rela) {
                di.jmprel_a = (Elf64_Rela *)(UINTN)(load_base + jmprel_addr);
                di.jmprel_count /= sizeof(Elf64_Rela);
            } else {
                di.jmprel = (Elf64_Rel *)(UINTN)(load_base + jmprel_addr);
                di.jmprel_count /= sizeof(Elf64_Rel);
            }
        } else {
            di.jmprel_count = 0;
        }

        if (di.rela && di.rela_count) apply_rela(load_base, di, di.rela, di.rela_count, resolver);
        if (di.rel && di.rel_count)   apply_rel(load_base, di, di.rel, di.rel_count, resolver);
        if (di.jmprel_count) {
            if (di.jmprel_is_rela) apply_rela(load_base, di, di.jmprel_a, di.jmprel_count, resolver);
            else                   apply_rel(load_base, di, di.jmprel, di.jmprel_count, resolver);
        }
    }

    if (entry_out) *entry_out = load_base + eh->e_entry;
    if (pml4_out) *pml4_out = pml4;

    CHAR16 buf[160];
    UnicodeSPrint(buf, sizeof(buf),
        (CHAR16 *)L"elf_loader_dynamic: loaded entry=0x%016lx pml4=0x%016lx dynamic=%s\n",
        load_base + eh->e_entry, pml4, dyn ? (CHAR16*)L"yes" : (CHAR16*)L"no");
    Print(buf);
    return true;
}
