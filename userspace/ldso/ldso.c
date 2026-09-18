#include "elf_abi.h"
#include <stdint.h>
#include <stddef.h>

/*
 * BlockOS x86-64 userspace syscall ABI.
 * int $0x80, rax = syscall number, rdi/rsi/rdx/r10/r8/r9 = arguments.
 */
enum {
    SYS_READ = 0,
    SYS_WRITE = 1,
    SYS_OPENAT = 2,
    SYS_CLOSE = 3,
    SYS_FSTAT = 4,
    SYS_MMAP = 5,
    SYS_BRK = 8,
    SYS_EXIT = 44,
    SYS_CLOCK_GETTIME = 51,
    SYS_ARCH_PRCTL = 83
};

static long sc3(long nr, long a0, long a1, long a2) {
    long r;
    __asm__ volatile("int $0x80"
        : "=a"(r)
        : "a"(nr), "D"(a0), "S"(a1), "d"(a2)
        : "rcx", "r11", "memory", "cc");
    return r;
}

static long sc6(long nr, long a0, long a1, long a2,
                long a3, long a4, long a5) {
    register long r10 __asm__("r10") = a3;
    register long r8  __asm__("r8")  = a4;
    register long r9  __asm__("r9")  = a5;
    long r;
    __asm__ volatile("int $0x80"
        : "=a"(r)
        : "a"(nr), "D"(a0), "S"(a1), "d"(a2),
          "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory", "cc");
    return r;
}

#define MAP_ANONYMOUS 0x20
#define PROT_READ     0x1
#define PROT_WRITE    0x2
#define PROT_EXEC     0x4

#define PAGE 0x1000ULL
#define USER_MIN 0x0000000000010000ULL
#define USER_MAX 0x00007ffff0000000ULL
#define LIB_BASE 0x0000600000000000ULL
#define LIB_GAP  0x0000000010000000ULL
#define MAX_OBJS 96
#define MAX_NEEDED 32
#define MAX_PATH 256
#define MAX_DYN 4096
#define MAX_SYMBOL_NAME 256

static long sc_mmap(uint64_t addr, uint64_t len, uint64_t prot,
                    uint64_t flags, int64_t fd, uint64_t offset) {
    return sc6(SYS_MMAP, (long)addr, (long)len, (long)prot,
               (long)flags, (long)fd, (long)offset);
}

static void ld_write(const char* s) {
    size_t n = 0;
    if (!s) return;
    while (s[n]) ++n;
    (void)sc3(SYS_WRITE, 1, (long)(uintptr_t)s, (long)n);
}

static void ld_die(const char* why) {
    ld_write("ld.so: ");
    ld_write(why ? why : "fatal");
    ld_write("\n");
    (void)sc3(SYS_EXIT, 127, 0, 0);
    for (;;) __asm__ volatile("hlt");
}

static size_t sstrlen(const char* s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

static void smemcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
}

static void smemset(void* dst, uint8_t v, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < n; ++i) d[i] = v;
}

static void scopy(char* dst, const char* src, size_t cap) {
    if (!cap) return;
    size_t i = 0;
    if (src) while (i + 1 < cap && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
}

static void scat(char* dst, const char* src, size_t cap) {
    size_t n = sstrlen(dst);
    size_t i = 0;
    if (!cap) return;
    while (n + 1 < cap && src && src[i]) { dst[n++] = src[i++]; }
    dst[n] = 0;
}

static int seq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static int prefix(const char* s, const char* p) {
    while (*p) {
        if (*s++ != *p++) return 0;
    }
    return 1;
}

static uint64_t page_down(uint64_t x) { return x & ~(PAGE - 1); }
static uint64_t page_up(uint64_t x) {
    if (x > UINT64_MAX - (PAGE - 1)) return 0;
    return (x + PAGE - 1) & ~(PAGE - 1);
}

static int range_ok(uint64_t base, uint64_t size) {
    if (!size || base + size < base) return 0;
    return base >= USER_MIN && base + size <= USER_MAX;
}

struct obj {
    char name[64];
    char path[MAX_PATH];
    uint64_t base;
    uint64_t min_vaddr;
    uint64_t max_vaddr;
    Elf64_Ehdr* ehdr;
    Elf64_Phdr* phdr;
    uint16_t phnum;

    Elf64_Dyn* dyn;
    Elf64_Sym* symtab;
    const char* strtab;
    uint64_t strsz;
    uint64_t nsyms;
    uint64_t syment;

    const uint32_t* hash;
    const uint32_t* gnu_hash;

    Elf64_Rela* rela;
    uint64_t rela_count;
    Elf64_Rela* jmprel;
    uint64_t jmprel_count;

    uint64_t init;
    uint64_t fini;
    uint64_t preinit_array;
    uint64_t preinit_array_sz;
    uint64_t init_array;
    uint64_t init_array_sz;
    uint64_t fini_array;
    uint64_t fini_array_sz;

    uint64_t flags;
    uint64_t flags_1;
    uint64_t versym;

    /* ELF TLS template information. The dynamic linker assigns each loaded
     * object a module id and a negative TP-relative base (x86-64 Variant II).
     */
    uint64_t tls_vaddr;
    uint64_t tls_filesz;
    uint64_t tls_memsz;
    uint64_t tls_align;
    uint64_t tls_tpoff;
    uint64_t tls_modid;
    const uint8_t* tls_source;

    char needed[MAX_NEEDED][64];
    int nneeded;
    char soname[64];

    int is_main;
    int mapped;
    int relocated;
    int initialized;
};

static struct obj g_objs[MAX_OBJS];
static int g_nobjs;
static uint64_t g_next_base = LIB_BASE;

#define BLOCKOS_TLS_TCB_BYTES 64ULL
#define BLOCKOS_TLS_MAX_BYTES (4ULL * 1024ULL * 1024ULL)
struct BlockOSTlsTcb {
    uint64_t self;
    int64_t  tpoff_table_offset;
    uint64_t region_base;
    uint64_t region_size;
    uint64_t generation;
    uint64_t reserved0;
    uint64_t reserved1;
    uint64_t reserved2;
};

static int validate_ehdr(const Elf64_Ehdr* e, size_t size) {
    if (!e || size < sizeof(*e)) return 0;
    if (e->e_ident[0] != 0x7f || e->e_ident[1] != 'E' ||
        e->e_ident[2] != 'L' || e->e_ident[3] != 'F') return 0;
    if (e->e_ident[4] != ELFCLASS64 || e->e_ident[5] != ELFDATA2LSB) return 0;
    if (e->e_machine != EM_X86_64 || e->e_version != EV_CURRENT) return 0;
    if (e->e_type != ET_EXEC && e->e_type != ET_DYN) return 0;
    if (e->e_ehsize < sizeof(*e) || e->e_phentsize != sizeof(Elf64_Phdr) || !e->e_phnum) return 0;
    uint64_t ph_end = e->e_phoff + (uint64_t)e->e_phnum * sizeof(Elf64_Phdr);
    if (ph_end < e->e_phoff || ph_end > size || e->e_phnum > 128) return 0;
    return 1;
}

static uint64_t runtime_addr(const struct obj* o, uint64_t link_addr) {
    return o->base + link_addr;
}

static int obj_contains(const struct obj* o, uint64_t addr, uint64_t size) {
    if (!o || addr + size < addr) return 0;
    return addr >= o->min_vaddr + o->base && addr + size <= o->max_vaddr + o->base;
}

static int str_valid(const struct obj* o, uint64_t off) {
    if (!o || !o->strtab || off >= o->strsz) return 0;
    const char* s = o->strtab + off;
    size_t left = (size_t)(o->strsz - off);
    for (size_t i = 0; i < left; ++i) if (!s[i]) return 1;
    return 0;
}

static uint32_t elf_hash(const char* name) {
    uint32_t h = 0, g;
    while (*name) {
        h = (h << 4) + (unsigned char)*name++;
        g = h & 0xf0000000U;
        if (g) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

static uint32_t gnu_hash(const char* name) {
    uint32_t h = 5381U;
    while (*name) h = (h << 5) + h + (unsigned char)*name++;
    return h;
}

static void count_symbols(struct obj* o) {
    o->nsyms = 0;
    if (o->hash) {
        o->nsyms = o->hash[1];
        return;
    }
    if (o->gnu_hash) {
        const uint32_t nbuckets = o->gnu_hash[0];
        const uint32_t symoffset = o->gnu_hash[1];
        const uint32_t bloom_size = o->gnu_hash[2];
        const uint64_t* bloom = (const uint64_t*)&o->gnu_hash[4];
        (void)bloom;
        const uint32_t* buckets = (const uint32_t*)(uintptr_t)(
            (uint64_t)(uintptr_t)(&o->gnu_hash[4]) + (uint64_t)bloom_size * sizeof(uint64_t));
        const uint32_t* chains = buckets + nbuckets;
        uint64_t max_sym = symoffset;
        for (uint32_t b = 0; b < nbuckets; ++b) {
            uint32_t s = buckets[b];
            if (s < symoffset) continue;
            uint64_t cidx = (uint64_t)s - symoffset;
            for (;;) {
                if ((uint64_t)s > max_sym) max_sym = s;
                uint32_t c = chains[cidx++];
                if (c & 1U) break;
                ++s;
            }
        }
        o->nsyms = max_sym ? max_sym + 1 : 0;
    }
}

static int parse_dynamic(struct obj* o, Elf64_Dyn* dyn) {
    if (!o || !dyn) return 0;
    o->dyn = dyn;
    uint64_t dt_strtab = 0, dt_symtab = 0, dt_rela = 0, dt_relasz = 0;
    uint64_t dt_jmprel = 0, dt_pltrelsz = 0;
    uint64_t dt_hash = 0, dt_gnuhash = 0;
    uint64_t dt_syment = sizeof(Elf64_Sym);
    uint64_t dt_relaent = sizeof(Elf64_Rela);
    uint64_t needed_offsets[MAX_NEEDED];
    int nneeded = 0;

    for (uint64_t n = 0; n < MAX_DYN; ++n) {
        Elf64_Dyn* d = &dyn[n];
        if (d->d_tag == DT_NULL) break;
        switch (d->d_tag) {
            case DT_STRTAB: dt_strtab = d->d_un.d_ptr; break;
            case DT_STRSZ: o->strsz = d->d_un.d_val; break;
            case DT_SYMTAB: dt_symtab = d->d_un.d_ptr; break;
            case DT_SYMENT: dt_syment = d->d_un.d_val; break;
            case DT_RELA: dt_rela = d->d_un.d_ptr; break;
            case DT_RELASZ: dt_relasz = d->d_un.d_val; break;
            case DT_RELAENT: dt_relaent = d->d_un.d_val; break;
            case DT_JMPREL: dt_jmprel = d->d_un.d_ptr; break;
            case DT_PLTRELSZ: dt_pltrelsz = d->d_un.d_val; break;
            case DT_HASH: dt_hash = d->d_un.d_ptr; break;
            case DT_GNU_HASH: dt_gnuhash = d->d_un.d_ptr; break;
            case DT_INIT: o->init = runtime_addr(o, d->d_un.d_ptr); break;
            case DT_FINI: o->fini = runtime_addr(o, d->d_un.d_ptr); break;
            case DT_PREINIT_ARRAY: o->preinit_array = runtime_addr(o, d->d_un.d_ptr); break;
            case DT_PREINIT_ARRAYSZ: o->preinit_array_sz = d->d_un.d_val; break;
            case DT_INIT_ARRAY: o->init_array = runtime_addr(o, d->d_un.d_ptr); break;
            case DT_INIT_ARRAYSZ: o->init_array_sz = d->d_un.d_val; break;
            case DT_FINI_ARRAY: o->fini_array = runtime_addr(o, d->d_un.d_ptr); break;
            case DT_FINI_ARRAYSZ: o->fini_array_sz = d->d_un.d_val; break;
            case DT_FLAGS: o->flags = d->d_un.d_val; break;
            case DT_FLAGS_1: o->flags_1 = d->d_un.d_val; break;
            case DT_VERSYM: o->versym = runtime_addr(o, d->d_un.d_ptr); break;
            case DT_SONAME:
                /* Parsed after DT_STRTAB/DT_STRSZ are known. */
                break;
            case DT_NEEDED:
                if (nneeded < MAX_NEEDED) needed_offsets[nneeded++] = d->d_un.d_val;
                break;
            case DT_REL:
            case DT_RELSZ:
            case DT_RELENT:
                /* BlockOS x86-64 userspace currently uses RELA. */
                break;
            default:
                break;
        }
    }

    if (dt_syment != sizeof(Elf64_Sym) || dt_relaent != sizeof(Elf64_Rela)) return 0;
    if (!dt_strtab || !o->strsz || !dt_symtab) return 0;

    o->strtab = (const char*)(uintptr_t)runtime_addr(o, dt_strtab);
    if (obj_contains(o, (uint64_t)(uintptr_t)o->strtab, o->strsz)) {
        for (uint64_t n = 0; n < MAX_DYN; ++n) {
            Elf64_Dyn* d = &dyn[n];
            if (d->d_tag == DT_NULL) break;
            if (d->d_tag == DT_SONAME && d->d_un.d_val < o->strsz && str_valid(o, d->d_un.d_val)) {
                scopy(o->soname, o->strtab + d->d_un.d_val, sizeof(o->soname));
                break;
            }
        }
    }
    o->symtab = (Elf64_Sym*)(uintptr_t)runtime_addr(o, dt_symtab);
    o->rela = dt_rela ? (Elf64_Rela*)(uintptr_t)runtime_addr(o, dt_rela) : 0;
    o->rela_count = dt_rela ? dt_relasz / sizeof(Elf64_Rela) : 0;
    o->jmprel = dt_jmprel ? (Elf64_Rela*)(uintptr_t)runtime_addr(o, dt_jmprel) : 0;
    o->jmprel_count = dt_jmprel ? dt_pltrelsz / sizeof(Elf64_Rela) : 0;
    o->hash = dt_hash ? (const uint32_t*)(uintptr_t)runtime_addr(o, dt_hash) : 0;
    o->gnu_hash = dt_gnuhash ? (const uint32_t*)(uintptr_t)runtime_addr(o, dt_gnuhash) : 0;

    if (!obj_contains(o, (uint64_t)(uintptr_t)o->strtab, o->strsz) ||
        !obj_contains(o, (uint64_t)(uintptr_t)o->symtab, sizeof(Elf64_Sym))) return 0;
    if (o->rela && !obj_contains(o, (uint64_t)(uintptr_t)o->rela,
                                  o->rela_count * sizeof(Elf64_Rela))) return 0;
    if (o->jmprel && !obj_contains(o, (uint64_t)(uintptr_t)o->jmprel,
                                   o->jmprel_count * sizeof(Elf64_Rela))) return 0;

    count_symbols(o);
    if (!o->nsyms) return 0;

    o->nneeded = nneeded;
    for (int i = 0; i < nneeded; ++i) {
        if (!str_valid(o, needed_offsets[i])) return 0;
        scopy(o->needed[i], o->strtab + needed_offsets[i], sizeof(o->needed[i]));
    }

    return 1;
}

static int validate_symbol(const struct obj* o, uint32_t idx) {
    if (!o || !o->symtab || idx >= o->nsyms) return 0;
    const Elf64_Sym* s = &o->symtab[idx];
    if (s->st_name >= o->strsz) return 0;
    if (s->st_name && !str_valid(o, s->st_name)) return 0;
    return 1;
}

struct sym_result {
    uint64_t value;
    uint64_t size;
    uint32_t shndx;
    unsigned char bind;
    unsigned char type;
    struct obj* owner;
    const Elf64_Sym* sym;
};

static int symbol_candidate(const Elf64_Sym* s) {
    unsigned char bind = ELF64_ST_BIND(s->st_info);
    return bind == STB_GLOBAL || bind == STB_WEAK;
}

static int lookup_in_obj(struct obj* o, const char* name, struct sym_result* out) {
    if (!o || !o->symtab || !o->strtab || !name) return 0;

    uint32_t idx = UINT32_MAX;

    if (o->gnu_hash) {
        const uint32_t nbuckets = o->gnu_hash[0];
        const uint32_t symoffset = o->gnu_hash[1];
        const uint32_t bloom_size = o->gnu_hash[2];
        const uint32_t bloom_shift = o->gnu_hash[3];
        if (nbuckets && bloom_size) {
            const uint64_t* bloom = (const uint64_t*)&o->gnu_hash[4];
            uint32_t h = gnu_hash(name);
            uint64_t word = bloom[(h / 64U) % bloom_size];
            uint64_t mask = (UINT64_C(1) << (h % 64U)) |
                            (UINT64_C(1) << ((h >> bloom_shift) % 64U));
            if ((word & mask) == mask) {
                const uint32_t* buckets = (const uint32_t*)(uintptr_t)(
                    (uint64_t)(uintptr_t)(&o->gnu_hash[4]) +
                    (uint64_t)bloom_size * sizeof(uint64_t));
                const uint32_t* chains = buckets + nbuckets;
                uint32_t b = buckets[h % nbuckets];
                if (b >= symoffset) {
                    for (uint32_t s = b;; ++s) {
                        uint32_t c = chains[(uint64_t)s - symoffset];
                        if (((c | 1U) == (h | 1U)) && validate_symbol(o, s)) {
                            const Elf64_Sym* es = &o->symtab[s];
                            if (symbol_candidate(es) && seq(o->strtab + es->st_name, name)) {
                                idx = s;
                                break;
                            }
                        }
                        if (c & 1U) break;
                    }
                }
            }
        }
    }

    if (idx == UINT32_MAX && o->hash) {
        uint32_t nbucket = o->hash[0];
        uint32_t nchain = o->hash[1];
        const uint32_t* buckets = &o->hash[2];
        const uint32_t* chains = buckets + nbucket;
        if (nbucket) {
            uint32_t h = elf_hash(name);
            for (uint32_t s = buckets[h % nbucket]; s != 0 && s < nchain; s = chains[s]) {
                if (!validate_symbol(o, s)) break;
                const Elf64_Sym* es = &o->symtab[s];
                if (symbol_candidate(es) && seq(o->strtab + es->st_name, name)) {
                    idx = s;
                    break;
                }
            }
        }
    }

    if (idx == UINT32_MAX) return 0;
    const Elf64_Sym* s = &o->symtab[idx];
    if (s->st_shndx == SHN_UNDEF) return 0;
    out->value = s->st_shndx == SHN_ABS ? s->st_value : runtime_addr(o, s->st_value);
    out->size = s->st_size;
    out->shndx = s->st_shndx;
    out->bind = ELF64_ST_BIND(s->st_info);
    out->type = ELF64_ST_TYPE(s->st_info);
    out->owner = o;
    out->sym = s;
    return 1;
}

static int resolve_symbol_from(int start, const char* name, struct sym_result* out) {
    struct sym_result weak = {0,0,SHN_UNDEF,STB_WEAK,STT_NOTYPE,0,0};
    int have_weak = 0;
    for (int i = start; i < g_nobjs; ++i) {
        if (lookup_in_obj(&g_objs[i], name, out)) {
            if (out->bind == STB_GLOBAL) return 1;
            if (!have_weak) { weak = *out; have_weak = 1; }
        }
    }
    if (have_weak) { *out = weak; return 1; }
    return 0;
}

static int resolve_symbol_for_obj(const struct obj* requester,
                                  const char* name, struct sym_result* out) {
    (void)requester;
    /* Main executable first, then DSOs in load order. */
    return resolve_symbol_from(0, name, out);
}

static int map_image(const uint8_t* buf, uint64_t size, const char* name,
                     const char* path, int is_main, uint64_t requested_base,
                     struct obj* o) {
    if (!buf || !o || size < sizeof(Elf64_Ehdr)) return 0;
    const Elf64_Ehdr* e = (const Elf64_Ehdr*)buf;
    if (!validate_ehdr(e, (size_t)size)) return 0;
    if (e->e_type == ET_EXEC && !is_main) return 0;

    const Elf64_Phdr* ph = (const Elf64_Phdr*)(buf + e->e_phoff);
    uint64_t min_v = UINT64_MAX, max_v = 0;
    const Elf64_Phdr* dynp = 0;
    const Elf64_Phdr* tlsp = 0;

    for (uint16_t i = 0; i < e->e_phnum; ++i) {
        const Elf64_Phdr* p = &ph[i];
        if (p->p_type == PT_DYNAMIC) dynp = p;
        if (p->p_type == PT_TLS) tlsp = p;
        if (p->p_type != PT_LOAD) continue;
        if (p->p_memsz < p->p_filesz) return 0;
        if (p->p_offset + p->p_filesz < p->p_offset || p->p_offset + p->p_filesz > size) return 0;
        if (p->p_vaddr + p->p_memsz < p->p_vaddr) return 0;
        if (p->p_align && (p->p_align & (p->p_align - 1))) return 0;
        if (p->p_vaddr < min_v) min_v = p->p_vaddr;
        if (p->p_vaddr + p->p_memsz > max_v) max_v = p->p_vaddr + p->p_memsz;
    }
    if (min_v == UINT64_MAX || max_v <= min_v) return 0;

    uint64_t vbase = page_down(min_v);
    uint64_t vend = page_up(max_v);
    if (!vend || vend < vbase) return 0;
    uint64_t span = vend - vbase;
    if (!span) return 0;

    uint64_t chosen;
    if (e->e_type == ET_EXEC) {
        chosen = vbase;
    } else {
        chosen = requested_base ? page_down(requested_base) : page_down(g_next_base);
        if (chosen < USER_MIN || chosen + span < chosen || chosen + span > USER_MAX) return 0;
    }

    uint64_t reserve_addr = chosen;
    if (e->e_type == ET_EXEC) {
        /* BlockOS mmap accepts a requested address and maps the exact range. */
        reserve_addr = vbase;
    }

    long mapped = sc_mmap(reserve_addr, span,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_ANONYMOUS, -1, 0);
    if (mapped < 0 || (uint64_t)mapped != reserve_addr) return 0;
    if (!range_ok((uint64_t)mapped, span)) return 0;
    smemset((void*)(uintptr_t)mapped, 0, (size_t)span);

    uint64_t load_bias = e->e_type == ET_DYN ? chosen - vbase : 0;

    for (uint16_t i = 0; i < e->e_phnum; ++i) {
        const Elf64_Phdr* p = &ph[i];
        if (p->p_type != PT_LOAD) continue;
        uint64_t dst = load_bias + p->p_vaddr;
        if (!obj_contains(&(struct obj){ .base = load_bias, .min_vaddr = vbase, .max_vaddr = vend }, dst, p->p_memsz)) {
            /* A local range check without relying on initialized object fields. */
            if (dst < chosen || dst + p->p_memsz < dst || dst + p->p_memsz > chosen + span) return 0;
        }
        smemcpy((void*)(uintptr_t)dst, buf + p->p_offset, (size_t)p->p_filesz);
    }

    smemset(o, 0, sizeof(*o));
    scopy(o->name, name ? name : "<main>", sizeof(o->name));
    scopy(o->path, path ? path : "", sizeof(o->path));
    o->base = load_bias;
    o->min_vaddr = vbase;
    o->max_vaddr = vend;
    o->ehdr = (Elf64_Ehdr*)(uintptr_t)(load_bias + vbase);
    o->phdr = (Elf64_Phdr*)(uintptr_t)0;
    o->phnum = e->e_phnum;
    o->is_main = is_main;
    o->mapped = 1;
    if (tlsp) {
        o->tls_vaddr = tlsp->p_vaddr;
        o->tls_filesz = tlsp->p_filesz;
        o->tls_memsz = tlsp->p_memsz;
        o->tls_align = tlsp->p_align ? tlsp->p_align : 1;
        if (o->tls_filesz > o->tls_memsz ||
            o->tls_vaddr + o->tls_memsz < o->tls_vaddr ||
            !o->tls_align || (o->tls_align & (o->tls_align - 1))) return 0;
        o->tls_source = (const uint8_t*)(uintptr_t)(load_bias + o->tls_vaddr);
    }

    /* Find the runtime PT_DYNAMIC and runtime program-header table. */
    uint64_t dyn_addr = 0;
    uint64_t phdr_addr = 0;
    for (uint16_t i = 0; i < e->e_phnum; ++i) {
        const Elf64_Phdr* p = &ph[i];
        if (p->p_type == PT_DYNAMIC) dyn_addr = load_bias + p->p_vaddr;
        if (p->p_type == PT_PHDR) phdr_addr = load_bias + p->p_vaddr;
        if (!phdr_addr && p->p_type == PT_LOAD && e->e_phoff >= p->p_offset && e->e_phoff < p->p_offset + p->p_filesz)
            phdr_addr = load_bias + p->p_vaddr + (e->e_phoff - p->p_offset);
    }
    o->ehdr = (Elf64_Ehdr*)(uintptr_t)(load_bias + e->e_entry - e->e_entry + vbase);
    o->phdr = (Elf64_Phdr*)(uintptr_t)phdr_addr;
    if (dynp && dyn_addr) {
        o->dyn = (Elf64_Dyn*)(uintptr_t)dyn_addr;
        if (!parse_dynamic(o, o->dyn)) return 0;
    }

    if (e->e_type == ET_DYN) {
        uint64_t next = page_up(chosen + span + LIB_GAP);
        if (next > g_next_base) g_next_base = next;
    }
    return 1;
}

static uint8_t* read_whole_file(const char* path, uint64_t* size_out) {
    long fd = sc3(SYS_OPENAT, 0, (long)(uintptr_t)path, 0);
    if (fd < 0) return 0;

    uint64_t st[16];
    smemset(st, 0, sizeof(st));
    if (sc3(SYS_FSTAT, fd, (long)(uintptr_t)st, 0) < 0) {
        (void)sc3(SYS_CLOSE, fd, 0, 0);
        return 0;
    }
    uint64_t size = st[7];
    if (!size || size > (USER_MAX - USER_MIN) / 2) {
        (void)sc3(SYS_CLOSE, fd, 0, 0);
        return 0;
    }
    uint64_t map_len = page_up(size);
    long addr = sc_mmap(0, map_len, PROT_READ | PROT_WRITE,
                        0, fd, 0);
    (void)sc3(SYS_CLOSE, fd, 0, 0);
    if (addr < 0) return 0;
    if (size_out) *size_out = size;
    return (uint8_t*)(uintptr_t)addr;
}

static int find_obj_name(const char* name) {
    for (int i = 0; i < g_nobjs; ++i) {
        if (seq(g_objs[i].name, name) || seq(g_objs[i].soname, name)) return i;
    }
    return -1;
}

static int load_needed(const char* name);

static int load_one_library_path(const char* name, const char* path) {
    if (g_nobjs >= MAX_OBJS) ld_die("too many loaded ELF objects");
    if (find_obj_name(name) >= 0) return find_obj_name(name);

    uint64_t size = 0;
    uint8_t* file = read_whole_file(path, &size);
    if (!file) return -1;

    struct obj* o = &g_objs[g_nobjs];
    int slot = g_nobjs;
    if (!map_image(file, size, name, path, 0, 0, o)) return -1;
    ++g_nobjs;

    for (int i = 0; i < o->nneeded; ++i) {
        if (load_needed(o->needed[i]) < 0) {
            ld_write("ld.so: missing dependency: ");
            ld_write(o->needed[i]);
            ld_write(" (for ");
            ld_write(o->name);
            ld_write(")\n");
            ld_die("dependency load failed");
        }
    }
    return slot;
}

static int load_needed(const char* name) {
    int existing = find_obj_name(name);
    if (existing >= 0) return existing;

    char path[MAX_PATH];
    const char* dirs[] = {
        "/system/lib/",
        "/System/lib/",
        "/lib/",
        "/System/usr/lib/"
    };

    if (!name || !*name) return -1;
    if (name[0] == '/') {
        scopy(path, name, sizeof(path));
        return load_one_library_path(name, path);
    }

    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); ++i) {
        scopy(path, dirs[i], sizeof(path));
        scat(path, name, sizeof(path));
        int r = load_one_library_path(name, path);
        if (r >= 0) return r;
    }
    return -1;
}

static uint64_t resolve_irelative(const struct obj* o, uint64_t addend) {
    uint64_t (*resolver)(void) = (uint64_t (*)(void))(uintptr_t)(o->base + addend);
    return resolver ? resolver() : 0;
}

static uint64_t find_got_slot(struct obj* o, uint32_t symidx) {
    if (!o || !symidx) return 0;
    if (o->rela) {
        for (uint64_t i = 0; i < o->rela_count; ++i) {
            const Elf64_Rela* rr = &o->rela[i];
            uint32_t t = ELF64_R_TYPE(rr->r_info);
            if ((t == R_X86_64_GLOB_DAT || t == R_X86_64_JUMP_SLOT) &&
                ELF64_R_SYM(rr->r_info) == symidx)
                return rr->r_offset;
        }
    }
    if (o->jmprel) {
        for (uint64_t i = 0; i < o->jmprel_count; ++i) {
            const Elf64_Rela* rr = &o->jmprel[i];
            uint32_t t = ELF64_R_TYPE(rr->r_info);
            if ((t == R_X86_64_GLOB_DAT || t == R_X86_64_JUMP_SLOT) &&
                ELF64_R_SYM(rr->r_info) == symidx)
                return rr->r_offset;
        }
    }
    return 0;
}

static int apply_rela_one(struct obj* o, const Elf64_Rela* r, int allow_copy) {
    uint32_t type = ELF64_R_TYPE(r->r_info);
    uint32_t symidx = ELF64_R_SYM(r->r_info);
    uint64_t P = o->base + r->r_offset;
    if (!obj_contains(o, P, sizeof(uint64_t))) return 0;
    uint64_t* where = (uint64_t*)(uintptr_t)P;

    switch (type) {
        case R_X86_64_NONE:
            return 1;
        case R_X86_64_RELATIVE:
            *where = o->base + (uint64_t)r->r_addend;
            return 1;
        case R_X86_64_IRELATIVE:
            *where = resolve_irelative(o, (uint64_t)r->r_addend);
            return 1;
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
        case R_X86_64_64:
        case R_X86_64_PC32:
        case R_X86_64_PLT32:
        case R_X86_64_GOTPCREL:
        case R_X86_64_GOTPCRELX:
        case R_X86_64_REX_GOTPCRELX:
        case R_X86_64_32:
        case R_X86_64_32S: {
            if (!symidx || symidx >= o->nsyms || !validate_symbol(o, symidx)) return 0;
            const Elf64_Sym* s = &o->symtab[symidx];
            if (!s->st_name || !str_valid(o, s->st_name)) return 0;
            const char* name = o->strtab + s->st_name;
            struct sym_result sr;
            int found = resolve_symbol_for_obj(o, name, &sr);
            if (!found) {
                if (ELF64_ST_BIND(s->st_info) == STB_WEAK) {
                    sr.value = 0;
                } else {
                    ld_write("ld.so: unresolved symbol: ");
                    ld_write(name);
                    ld_write("\n");
                    return 0;
                }
            }
            uint64_t S = sr.value;
            if (type == R_X86_64_GOTPCREL || type == R_X86_64_GOTPCRELX || type == R_X86_64_REX_GOTPCRELX) {
                uint64_t G = find_got_slot(o, symidx);
                if (G) {
                    *(uint32_t*)where = (uint32_t)(int32_t)((int64_t)G + r->r_addend - (int64_t)P);
                } else {
                    *(uint32_t*)where = (uint32_t)(int32_t)((int64_t)S + r->r_addend - (int64_t)P);
                }
            } else if (type == R_X86_64_PC32 || type == R_X86_64_PLT32) {
                int64_t v = (int64_t)S + r->r_addend - (int64_t)P;
                if (v < INT32_MIN || v > INT32_MAX) return 0;
                uint32_t x = (uint32_t)(int32_t)v;
                *(uint32_t*)where = x;
            } else if (type == R_X86_64_32) {
                uint64_t v = S + (uint64_t)r->r_addend;
                if (v > UINT32_MAX) return 0;
                *(uint32_t*)where = (uint32_t)v;
            } else if (type == R_X86_64_32S) {
                int64_t v = (int64_t)S + r->r_addend;
                if (v < INT32_MIN || v > INT32_MAX) return 0;
                *(uint32_t*)where = (uint32_t)(int32_t)v;
            } else if (type == R_X86_64_64) {
                *where = S + (uint64_t)r->r_addend;
            } else {
                *where = S + (type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT ? 0 : (uint64_t)r->r_addend);
            }
            return 1;
        }
        case R_X86_64_COPY: {
            if (!allow_copy || !symidx || symidx >= o->nsyms || !validate_symbol(o, symidx)) return 0;
            const Elf64_Sym* s = &o->symtab[symidx];
            if (!s->st_name || !str_valid(o, s->st_name)) return 0;
            struct sym_result sr;
            /* COPY's source comes from DSOs, not from the executable itself. */
            int found = resolve_symbol_from(1, o->strtab + s->st_name, &sr);
            if (!found) return 0;
            uint64_t n = s->st_size < sr.size ? s->st_size : sr.size;
            if (!obj_contains(o, P, n)) return 0;
            smemcpy((void*)(uintptr_t)P, (const void*)(uintptr_t)sr.value, (size_t)n);
            return 1;
        }
        case R_X86_64_DTPMOD64:
            if (!symidx || symidx >= o->nsyms || !validate_symbol(o, symidx)) return 0;
            {
                const Elf64_Sym* s = &o->symtab[symidx];
                uint64_t module = 0;
                if (s->st_shndx != SHN_UNDEF) module = o->tls_modid;
                else {
                    struct sym_result sr;
                    const char* name = (s->st_name && str_valid(o, s->st_name)) ? o->strtab + s->st_name : 0;
                    if (name && resolve_symbol_for_obj(o, name, &sr) && sr.owner) module = sr.owner->tls_modid;
                }
                if (!module) return 0;
                *where = module;
            }
            return 1;
        case R_X86_64_DTPOFF64:
            if (!symidx || symidx >= o->nsyms || !validate_symbol(o, symidx)) return 0;
            {
                const Elf64_Sym* s = &o->symtab[symidx];
                if (s->st_shndx == SHN_UNDEF) {
                    struct sym_result sr;
                    if (!s->st_name || !str_valid(o, s->st_name) || !resolve_symbol_for_obj(o, o->strtab + s->st_name, &sr)) return 0;
                    *where = sr.sym ? sr.sym->st_value + (uint64_t)r->r_addend : 0;
                } else {
                    *where = s->st_value + (uint64_t)r->r_addend;
                }
            }
            return 1;
        case R_X86_64_TPOFF64:
            if (!symidx || symidx >= o->nsyms || !validate_symbol(o, symidx)) return 0;
            {
                const Elf64_Sym* s = &o->symtab[symidx];
                uint64_t tpoff = o->tls_tpoff;
                if (s->st_shndx == SHN_UNDEF) {
                    struct sym_result sr;
                    if (!s->st_name || !str_valid(o, s->st_name) || !resolve_symbol_for_obj(o, o->strtab + s->st_name, &sr) || !sr.owner) return 0;
                    tpoff = sr.owner->tls_tpoff + (sr.sym ? sr.sym->st_value : 0);
                } else {
                    tpoff = o->tls_tpoff + s->st_value;
                }
                *where = tpoff + (uint64_t)r->r_addend;
            }
            return 1;
        case R_X86_64_TLSDESC:
            /* BlockOS uses the GCC TLSGD/TLSLD relocations and __tls_get_addr;
             * TLSDESC requires a resolver ABI we do not need for the initial
             * GNOME 42 bootstrap. */
            return 0;
        default:
            ld_write("ld.so: unsupported relocation type\n");
            return 0;
    }
}

static int align_down_tls(uint64_t value, uint64_t align, uint64_t* out) {
    if (!align || (align & (align - 1))) return 0;
    *out = value & ~(align - 1);
    return 1;
}

static int setup_initial_tls(void) {
    uint64_t cursor = 0;
    int have_tls = 0;

    for (int i = 0; i < g_nobjs; ++i) {
        struct obj* o = &g_objs[i];
        if (!o->tls_memsz) continue;
        have_tls = 1;
    }
    if (!have_tls) {
        /* Still install a TCB. This makes pthread-created non-TLS threads and
         * libraries that query FS_BASE behave predictably. */
        long mem = sc_mmap(0, BLOCKOS_TLS_TCB_BYTES, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS, -1, 0);
        if (mem < 0) return 0;
        struct BlockOSTlsTcb* tcb = (struct BlockOSTlsTcb*)(uintptr_t)mem;
        tcb->self = (uint64_t)mem;
        tcb->tpoff_table_offset = 0;
        tcb->region_base = (uint64_t)mem;
        tcb->region_size = BLOCKOS_TLS_TCB_BYTES;
        tcb->generation = 1;
        return sc6(SYS_ARCH_PRCTL, 0x1002, (long)mem, 0, 0, 0, 0) == 0;
    }

    /* TLS blocks occupy the negative range below TP. Each object gets a
     * module id matching its 1-based load order. */
    for (int i = 0; i < g_nobjs; ++i) {
        struct obj* o = &g_objs[i];
        if (!o->tls_memsz) continue;
        uint64_t size = o->tls_memsz;
        uint64_t a = o->tls_align > 0 ? o->tls_align : 1;
        uint64_t next = cursor + size;
        if (next < cursor || next > UINT64_MAX - (a - 1)) return 0;
        cursor = (next + a - 1) & ~(a - 1);
        o->tls_tpoff = -(int64_t)cursor;
        o->tls_modid = (uint64_t)(i + 1);
    }

    uint64_t table_bytes = (uint64_t)(g_nobjs + 1) * sizeof(int64_t);
    uint64_t table_offset = 0;
    uint64_t tls_bytes = cursor + table_bytes;
    if (tls_bytes > BLOCKOS_TLS_MAX_BYTES) return 0;
    uint64_t total = tls_bytes + BLOCKOS_TLS_TCB_BYTES;
    uint64_t alloc = page_up(total);
    if (!alloc) return 0;
    long mem = sc_mmap(0, alloc, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    if (mem < 0) return 0;

    uint64_t base = (uint64_t)mem;
    uint64_t tp = base + tls_bytes;
    struct BlockOSTlsTcb* tcb = (struct BlockOSTlsTcb*)(uintptr_t)tp;
    smemset((void*)(uintptr_t)base, 0, (size_t)alloc);
    tcb->self = tp;
    tcb->tpoff_table_offset = -(int64_t)tls_bytes;
    tcb->region_base = base;
    tcb->region_size = alloc;
    tcb->generation = 1;

    int64_t* tpoffs = (int64_t*)(uintptr_t)(base + table_offset);
    for (int i = 0; i < g_nobjs; ++i) {
        struct obj* o = &g_objs[i];
        tpoffs[i + 1] = o->tls_tpoff;
        if (!o->tls_memsz) continue;
        void* dst = (void*)(uintptr_t)(tp + o->tls_tpoff);
        if (o->tls_filesz && o->tls_source)
            smemcpy(dst, o->tls_source, (size_t)o->tls_filesz);
    }

    return sc6(SYS_ARCH_PRCTL, 0x1002, (long)tp, 0, 0, 0, 0) == 0;
}

static int apply_relocations(struct obj* o) {
    if (!o || !o->relocated) {
        if (!o) return 0;
        if (o->rela) {
            for (uint64_t i = 0; i < o->rela_count; ++i)
                if (!apply_rela_one(o, &o->rela[i], o->is_main)) return 0;
        }
        if (o->jmprel) {
            for (uint64_t i = 0; i < o->jmprel_count; ++i)
                if (!apply_rela_one(o, &o->jmprel[i], 0)) return 0;
        }
        o->relocated = 1;
    }
    return 1;
}

static void call_array(uint64_t array, uint64_t bytes, struct obj* o) {
    if (!array || !bytes) return;
    if (!obj_contains(o, array, bytes)) ld_die("invalid dynamic constructor array");
    uint64_t count = bytes / sizeof(uint64_t);
    uint64_t* fns = (uint64_t*)(uintptr_t)array;
    for (uint64_t i = 0; i < count; ++i) {
        if (!fns[i] || fns[i] == UINT64_MAX) continue;
        void (*fn)(void) = (void (*)(void))(uintptr_t)fns[i];
        fn();
    }
}

static void call_init_recursive(int index) {
    if (index < 0 || index >= g_nobjs) return;
    struct obj* o = &g_objs[index];
    if (o->initialized) return;

    /* Dependencies must be initialized before the object that needs them. */
    for (int i = 0; i < o->nneeded; ++i) {
        int dep = find_obj_name(o->needed[i]);
        if (dep >= 0) call_init_recursive(dep);
    }

    o->initialized = 1;
    if (o->init) {
        void (*fn)(void) = (void (*)(void))(uintptr_t)o->init;
        fn();
    }
    call_array(o->init_array, o->init_array_sz, o);
}

static void call_main_preinit(struct obj* main_o) {
    if (!main_o || !main_o->preinit_array || !main_o->preinit_array_sz) return;
    call_array(main_o->preinit_array, main_o->preinit_array_sz, main_o);
}

static uint64_t derive_main_base(Elf64_Phdr* ph, uint64_t phnum,
                                  uint64_t at_phdr, uint64_t e_phoff) {
    for (uint64_t i = 0; i < phnum; ++i) {
        if (ph[i].p_type == PT_PHDR) {
            return at_phdr - ph[i].p_vaddr;
        }
    }

    /* Linux/ELF also guarantees AT_PHDR points inside a PT_LOAD when there is
     * no explicit PT_PHDR.  The kernel's loader computes that exact address
     * from p_vaddr + (e_phoff - p_offset), so reconstruct the same bias here. */
    for (uint64_t i = 0; i < phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (e_phoff < ph[i].p_offset) continue;
        uint64_t delta = e_phoff - ph[i].p_offset;
        if (delta >= ph[i].p_filesz) continue;
        uint64_t link_phdr = ph[i].p_vaddr + delta;
        uint64_t candidate = at_phdr - link_phdr;
        if (candidate + ph[i].p_vaddr <= at_phdr &&
            at_phdr < candidate + ph[i].p_vaddr + ph[i].p_memsz) {
            return candidate;
        }
    }
    return 0;
}
/* The standard ELF entry symbol supplied by GNU ld for the running image. */
extern const Elf64_Ehdr __ehdr_start;

static void self_relocate(void) {
    const Elf64_Ehdr* e = &__ehdr_start;
    if (e->e_ident[0] != 0x7f || e->e_ident[1] != 'E' ||
        e->e_ident[2] != 'L' || e->e_ident[3] != 'F') return;

    const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)e + e->e_phoff);
    uint64_t base = 0;
    const Elf64_Phdr* dynp = 0;
    const Elf64_Phdr* tlsp = 0;
    for (uint16_t i = 0; i < e->e_phnum; ++i) {
        if (ph[i].p_type == PT_DYNAMIC) dynp = &ph[i];
        if (ph[i].p_type == PT_LOAD &&
            e->e_phoff >= ph[i].p_offset &&
            e->e_phoff < ph[i].p_offset + ph[i].p_filesz) {
            uint64_t runtime_phoff = (uint64_t)(uintptr_t)e + (e->e_phoff - ph[i].p_offset);
            base = runtime_phoff - ph[i].p_vaddr;
        }
    }
    if (!dynp || !base) return;
    Elf64_Dyn* dyn = (Elf64_Dyn*)(uintptr_t)(base + dynp->p_vaddr);
    uint64_t rela = 0, relasz = 0, relaent = sizeof(Elf64_Rela);
    for (uint64_t n = 0; n < MAX_DYN; ++n) {
        if (dyn[n].d_tag == DT_NULL) break;
        if (dyn[n].d_tag == DT_RELA) rela = base + dyn[n].d_un.d_ptr;
        else if (dyn[n].d_tag == DT_RELASZ) relasz = dyn[n].d_un.d_val;
        else if (dyn[n].d_tag == DT_RELAENT) relaent = dyn[n].d_un.d_val;
    }
    if (!rela || !relasz) return;
    if (relaent != sizeof(Elf64_Rela)) ld_die("invalid self-relocation entry size");
    Elf64_Rela* rs = (Elf64_Rela*)(uintptr_t)rela;
    for (uint64_t i = 0; i < relasz / sizeof(Elf64_Rela); ++i) {
        uint32_t type = ELF64_R_TYPE(rs[i].r_info);
        uint64_t P = base + rs[i].r_offset;
        if (type == R_X86_64_RELATIVE) *(uint64_t*)(uintptr_t)P = base + (uint64_t)rs[i].r_addend;
        else if (type == R_X86_64_IRELATIVE) {
            uint64_t (*fn)(void) = (uint64_t (*)(void))(uintptr_t)(base + rs[i].r_addend);
            *(uint64_t*)(uintptr_t)P = fn();
        } else if (type != R_X86_64_NONE) {
            ld_die("unsupported self-relocation type");
        }
    }
}

static void run_main_and_lib_initializers(int main_index) {
    if (main_index < 0 || main_index >= g_nobjs) return;
    call_main_preinit(&g_objs[main_index]);
    for (int i = 1; i < g_nobjs; ++i) call_init_recursive(i);
    call_init_recursive(main_index);
}

extern void ldso_jump_to_entry(void* original_sp, uint64_t entry) __attribute__((noreturn));

void ld_main(void* raw_sp) {
    self_relocate();
    g_nobjs = 0;
    g_next_base = LIB_BASE;
    smemset(g_objs, 0, sizeof(g_objs));

    uint64_t* sp = (uint64_t*)raw_sp;
    uint64_t argc = sp[0];
    if (argc > 4096) ld_die("invalid argc");
    char** argv = (char**)&sp[1];
    char** envp = (char**)&sp[argc + 2];

    uint64_t envc = 0;
    while (envp[envc]) {
        if (++envc > 4096) ld_die("invalid envp");
    }
    uint64_t* auxv = (uint64_t*)&envp[envc + 1];

    uint64_t at_phdr = 0, at_phent = 0, at_phnum = 0, at_entry = 0, at_base = 0;
    for (uint64_t* a = auxv; ; a += 2) {
        if (a[0] == AT_NULL) break;
        switch (a[0]) {
            case AT_PHDR: at_phdr = a[1]; break;
            case AT_PHENT: at_phent = a[1]; break;
            case AT_PHNUM: at_phnum = a[1]; break;
            case AT_ENTRY: at_entry = a[1]; break;
            case AT_BASE: at_base = a[1]; break;
            default: break;
        }
    }

    if (!at_phdr || at_phent != sizeof(Elf64_Phdr) || !at_phnum || !at_entry) {
        ld_die("invalid process auxv");
    }

    Elf64_Phdr* main_ph = (Elf64_Phdr*)(uintptr_t)at_phdr;

    /* Recover the executable's own ELF header. In BlockOS the first PT_LOAD
     * contains the file header, exactly as the normal ELF loader requires.
     * Scan page-aligned backwards from AT_PHDR so ET_EXEC (e.g. 0x400000)
     * and ET_DYN (randomized/fixed PIE base) both work without a new auxv ABI. */
    const Elf64_Ehdr* main_ehdr = 0;
    uint64_t scan = page_down(at_phdr);
    for (uint32_t pages = 0; pages < 16384 && scan >= USER_MIN; ++pages) {
        const Elf64_Ehdr* cand = (const Elf64_Ehdr*)(uintptr_t)scan;
        if (cand->e_ident[0] == 0x7f && cand->e_ident[1] == 'E' &&
            cand->e_ident[2] == 'L' && cand->e_ident[3] == 'F' &&
            cand->e_ident[4] == ELFCLASS64 && cand->e_ident[5] == ELFDATA2LSB &&
            cand->e_machine == EM_X86_64 && cand->e_phnum == at_phnum &&
            cand->e_phentsize == sizeof(Elf64_Phdr)) {
            main_ehdr = cand;
            break;
        }
        if (scan < PAGE) break;
        scan -= PAGE;
    }
    if (!main_ehdr) ld_die("cannot locate main ELF header");

    uint64_t main_base = derive_main_base(main_ph, at_phnum, at_phdr, main_ehdr->e_phoff);
    /* ET_EXEC has a zero load bias. For ET_DYN this is the actual runtime bias. */

    struct obj* main_o = &g_objs[g_nobjs];
    smemset(main_o, 0, sizeof(*main_o));
    scopy(main_o->name, "(main)", sizeof(main_o->name));
    main_o->base = main_base;
    main_o->min_vaddr = 0;
    main_o->max_vaddr = USER_MAX - main_base;
    main_o->phdr = main_ph;
    main_o->phnum = (uint16_t)at_phnum;
    main_o->is_main = 1;
    main_o->mapped = 1;
    ++g_nobjs;

    Elf64_Dyn* main_dyn = 0;
    uint64_t main_min = UINT64_MAX, main_max = 0;
    for (uint64_t i = 0; i < at_phnum; ++i) {
        if (main_ph[i].p_type == PT_TLS) {
            main_o->tls_vaddr = main_base + main_ph[i].p_vaddr;
            main_o->tls_filesz = main_ph[i].p_filesz;
            main_o->tls_memsz = main_ph[i].p_memsz;
            main_o->tls_align = main_ph[i].p_align ? main_ph[i].p_align : 1;
            main_o->tls_source = (const uint8_t*)(uintptr_t)main_o->tls_vaddr;
        }
        if (main_ph[i].p_type == PT_DYNAMIC)
            main_dyn = (Elf64_Dyn*)(uintptr_t)(main_base + main_ph[i].p_vaddr);
        if (main_ph[i].p_type == PT_LOAD) {
            if (main_ph[i].p_vaddr < main_min) main_min = main_ph[i].p_vaddr;
            uint64_t e = main_ph[i].p_vaddr + main_ph[i].p_memsz;
            if (e > main_max) main_max = e;
        }
    }
    if (main_min != UINT64_MAX) {
        main_o->min_vaddr = page_down(main_min);
        main_o->max_vaddr = page_up(main_max);
    }
    if (!main_dyn) ld_die("main program has no PT_DYNAMIC");
    if (!parse_dynamic(main_o, main_dyn)) ld_die("cannot parse main PT_DYNAMIC");

    /* Main executable dependencies recursively load before relocation. */
    for (int i = 0; i < main_o->nneeded; ++i)
        if (load_needed(main_o->needed[i]) < 0) ld_die("main dependency load failed");

    if (!setup_initial_tls()) ld_die("TLS setup failed");

    /* Relocate every DSO first, then the main executable. */
    for (int i = 1; i < g_nobjs; ++i)
        if (!apply_relocations(&g_objs[i])) ld_die("DSO relocation failed");
    if (!apply_relocations(main_o)) ld_die("main relocation failed");

    /* Run constructors after all symbols are resolved. */
    run_main_and_lib_initializers(0);

    (void)argv;
    (void)at_base;
    ldso_jump_to_entry(raw_sp, at_entry);
}
