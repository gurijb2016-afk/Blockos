#include "elf_abi.h"
#include <stdint.h>
#include <stddef.h>

/* ---- BlockOS syscall ABI: rax=nr, rdi=a0, rsi=a1, rdx=a2 (see
 * userspace/tests/ring3_test.c and kernel/user_syscall.cpp) ---- */
enum {
    SYS_READ = 0, SYS_WRITE = 1, SYS_OPENAT = 2, SYS_CLOSE = 3,
    SYS_FSTAT = 4, SYS_MMAP = 5, SYS_BRK = 8, SYS_LSEEK = 63, SYS_EXIT = 44
};

static long sc(long n, long a0, long a1, long a2, long a3, long a4, long a5) {
    long r;
    __asm__ volatile("int $0x80"
        : "=a"(r)
        : "a"(n), "D"(a0), "S"(a1), "d"(a2), "r"(a3), "r"(a4), "r"(a5)
        : "rcx", "r11", "memory", "cc");
    return r;
}
#define SC3(n,a,b,c) sc((n),(long)(a),(long)(b),(long)(c),0,0,0)

/* mmap needs 6 real arguments (addr,len,prot,flags,fd,offset) and the
 * kernel now reads flags/fd/offset from r10/r8/r9 specifically (the same
 * registers real x86-64 syscalls use) - sc()'s generic "r" constraints
 * don't guarantee THOSE particular registers, so mmap gets its own
 * wrapper that pins them explicitly. */
#define MAP_ANONYMOUS 0x20
static long sc_mmap(long addr, long len, long prot, long flags, long fd, long offset) {
    register long r10 __asm__("r10") = flags;
    register long r8  __asm__("r8")  = fd;
    register long r9  __asm__("r9")  = offset;
    long r;
    __asm__ volatile("int $0x80"
        : "=a"(r)
        : "a"(SYS_MMAP), "D"(addr), "S"(len), "d"(prot), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory", "cc");
    return r;
}

static void ld_write(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    SC3(SYS_WRITE, 1, s, n);
}
static void ld_die(const char* msg) {
    ld_write("ld.so: ");
    ld_write(msg);
    ld_write("\n");
    SC3(SYS_EXIT, 1, 0, 0);
    for (;;) __asm__ volatile("hlt");
}

/* ---- tiny freestanding helpers (no libc yet) ---- */
static size_t sstrlen(const char* s){size_t n=0;while(s[n])n++;return n;}
static void scopy(char* d,const char* s,size_t cap){size_t i=0;for(;i+1<cap&&s[i];i++)d[i]=s[i];d[i]=0;}
static void scat(char* d,const char* s,size_t cap){size_t i=sstrlen(d);for(size_t j=0;i+1<cap&&s[j];i++,j++)d[i]=s[j];d[i]=0;}
static int seq(const char* a,const char* b){while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;}
static void* smemcpy(void* d,const void* s,size_t n){uint8_t* dd=(uint8_t*)d;const uint8_t* ss=(const uint8_t*)s;for(size_t i=0;i<n;i++)dd[i]=ss[i];return d;}
static void* smemset(void* d,int v,size_t n){uint8_t* dd=(uint8_t*)d;for(size_t i=0;i<n;i++)dd[i]=(uint8_t)v;return d;}

#define PAGE 0x1000ULL
static uint64_t page_down(uint64_t v){return v & ~(PAGE-1);}
static uint64_t page_up(uint64_t v){return (v+PAGE-1)&~(PAGE-1);}

/* ---- one loaded object (the main program counts as object 0) ---- */
#define MAX_OBJS 32
#define MAX_NEEDED 16

struct obj {
    char name[64];
    uint64_t base;          /* load bias: runtime_addr = base + link_time_vaddr */
    Elf64_Sym*  symtab;
    const char* strtab;
    uint64_t    nsyms;
    Elf64_Rela* rela;      uint64_t rela_count;
    Elf64_Rela* jmprel;    uint64_t jmprel_count;
    char needed[MAX_NEEDED][64];
    int  nneeded;
};

static struct obj g_objs[MAX_OBJS];
static int g_nobjs = 0;
static uint64_t g_next_base = 0x0000600000000000ULL;

/* Reads the whole content of `path` via the kernel's file-backed mmap (one
 * syscall maps AND populates the pages - no manual read() loop needed).
 * Returns the buffer and size, or 0 on failure. */
static uint8_t* read_whole_file(const char* path, uint64_t* size_out) {
    long fd = SC3(SYS_OPENAT, 0, path, 0);
    if (fd < 0) return 0;

    uint64_t stat_buf[16];
    if (SC3(SYS_FSTAT, fd, stat_buf, 0) < 0) { SC3(SYS_CLOSE, fd, 0, 0); return 0; }
    uint64_t size = stat_buf[7];
    if (size == 0) { SC3(SYS_CLOSE, fd, 0, 0); return 0; }

    /* flags=0 (MAP_ANONYMOUS clear) + a real fd => kernel copies the
     * file's content straight into the newly mapped pages for us. */
    long addr = sc_mmap(0, (long)page_up(size), 1 | 2 /* PROT_READ|PROT_WRITE */,
                        0 /* !MAP_ANONYMOUS */, fd, 0 /* offset */);
    SC3(SYS_CLOSE, fd, 0, 0); /* the mapping already has everything we need */
    if (addr < 0) return 0;

    if (size_out) *size_out = size;
    return (uint8_t*)(uintptr_t)addr;
}

/* Finds an already-loaded object by name, or -1. */
static int find_obj(const char* name) {
    for (int i = 0; i < g_nobjs; i++) if (seq(g_objs[i].name, name)) return i;
    return -1;
}

/* Applies R_X86_64_RELATIVE only, used both for self-relocation (ld.so
 * relocating itself) and for any object before its symbol table is
 * otherwise needed. */
static void apply_relative(Elf64_Rela* rela, uint64_t count, uint64_t base) {
    for (uint64_t i = 0; i < count; i++) {
        if (ELF64_R_TYPE(rela[i].r_info) == R_X86_64_RELATIVE) {
            uint64_t* target = (uint64_t*)(uintptr_t)(base + rela[i].r_offset);
            *target = base + (uint64_t)rela[i].r_addend;
        }
    }
}

/* Global symbol lookup across every object loaded so far - a simplified
 * approximation of real symbol scope rules (first definition found wins,
 * in load order). Good enough for a small, hand-picked set of libraries. */
static uint64_t resolve_symbol(const char* name) {
    for (int i = 0; i < g_nobjs; i++) {
        struct obj* o = &g_objs[i];
        if (!o->symtab || !o->strtab) continue;
        for (uint64_t s = 0; s < o->nsyms; s++) {
            if (o->symtab[s].st_shndx == SHN_UNDEF) continue;
            const char* sym_name = o->strtab + o->symtab[s].st_name;
            if (seq(sym_name, name)) return o->base + o->symtab[s].st_value;
        }
    }
    return 0;
}

static void apply_symbolic_relocs(Elf64_Rela* rela, uint64_t count, struct obj* o) {
    for (uint64_t i = 0; i < count; i++) {
        uint32_t type = ELF64_R_TYPE(rela[i].r_info);
        uint64_t* target = (uint64_t*)(uintptr_t)(o->base + rela[i].r_offset);
        if (type == R_X86_64_RELATIVE) {
            *target = o->base + (uint64_t)rela[i].r_addend;
            continue;
        }
        if (type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT || type == R_X86_64_64) {
            uint64_t sym_idx = ELF64_R_SYM(rela[i].r_info);
            const char* sym_name = o->strtab + o->symtab[sym_idx].st_name;
            uint64_t addr = resolve_symbol(sym_name);
            /* Undefined weak symbols resolve to 0 - tolerated, not fatal;
             * a real libc/Xlib port will need every mandatory symbol to
             * actually be found in one of the loaded objects. */
            *target = addr + (type == R_X86_64_64 ? (uint64_t)rela[i].r_addend : 0);
            continue;
        }
        /* R_X86_64_COPY and anything else: not handled by this MVP. */
    }
}

/* Parses the Elf64_Dyn array for one object once its segments are already
 * mapped and populates symtab/strtab/rela/jmprel/needed[] on `o`. `dyn` is
 * the RUNTIME address of the object's PT_DYNAMIC content. */
static void parse_dynamic(struct obj* o, Elf64_Dyn* dyn) {
    uint64_t strtab_off = 0, symtab_off = 0, rela_off = 0, jmprel_off = 0;
    uint64_t relasz = 0, pltrelsz = 0, syment = sizeof(Elf64_Sym);
    char needed_offsets[MAX_NEEDED]; (void)needed_offsets;
    uint64_t needed_off[MAX_NEEDED]; int nneeded = 0;

    for (Elf64_Dyn* d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
            case DT_STRTAB: strtab_off = d->d_un.d_val; break;
            case DT_SYMTAB: symtab_off = d->d_un.d_val; break;
            case DT_RELA:   rela_off   = d->d_un.d_val; break;
            case DT_RELASZ: relasz     = d->d_un.d_val; break;
            case DT_JMPREL: jmprel_off = d->d_un.d_val; break;
            case DT_PLTRELSZ: pltrelsz = d->d_un.d_val; break;
            case DT_SYMENT: syment     = d->d_un.d_val; break;
            case DT_NEEDED:
                if (nneeded < MAX_NEEDED) needed_off[nneeded++] = d->d_un.d_val;
                break;
            default: break;
        }
    }

    o->strtab = strtab_off ? (const char*)(uintptr_t)(o->base + strtab_off) : 0;
    o->symtab = symtab_off ? (Elf64_Sym*)(uintptr_t)(o->base + symtab_off) : 0;
    o->rela = rela_off ? (Elf64_Rela*)(uintptr_t)(o->base + rela_off) : 0;
    o->rela_count = syment ? relasz / sizeof(Elf64_Rela) : 0;
    o->jmprel = jmprel_off ? (Elf64_Rela*)(uintptr_t)(o->base + jmprel_off) : 0;
    o->jmprel_count = pltrelsz / sizeof(Elf64_Rela);

    /* Without a hash table this is an approximation: assume the symbol
     * table runs up to the lowest of (strtab start) when laid out normally
     * by the linker (.dynsym directly followed by .dynstr). This holds for
     * default GNU ld layouts; if a toolchain differs, nsyms comes out 0 and
     * every symbolic relocation below will fail to resolve - see README. */
    if (o->symtab && o->strtab && (uint8_t*)o->strtab > (uint8_t*)o->symtab)
        o->nsyms = ((uint8_t*)o->strtab - (uint8_t*)o->symtab) / sizeof(Elf64_Sym);
    else
        o->nsyms = 0;

    o->nneeded = nneeded;
    for (int i = 0; i < nneeded; i++)
        scopy(o->needed[i], o->strtab + needed_off[i], sizeof(o->needed[i]));
}

/* Loads one ELF64 image (shared object OR the main program's already-known
 * bytes) from a raw in-memory buffer into a freshly reserved region, fills
 * in `o->base`, and returns the object's own PT_DYNAMIC runtime pointer (or
 * 0 if it has none, e.g. a non-PIE static main program with no PT_DYNAMIC -
 * shouldn't happen here since we only get called for dynamic objects). */
static Elf64_Dyn* map_image(const uint8_t* buf, uint64_t size, struct obj* o) {
    Elf64_Ehdr* e = (Elf64_Ehdr*)buf;
    if (e->e_ident[0]!=0x7f||e->e_ident[1]!='E'||e->e_ident[2]!='L'||e->e_ident[3]!='F') return 0;
    Elf64_Phdr* ph = (Elf64_Phdr*)(buf + e->e_phoff);

    uint64_t min_v = ~0ULL, max_v = 0;
    Elf64_Phdr* dynp = 0;
    for (int i = 0; i < e->e_phnum; i++) {
        if (ph[i].p_type == PT_LOAD) {
            if (ph[i].p_vaddr < min_v) min_v = ph[i].p_vaddr;
            uint64_t v_end = ph[i].p_vaddr + ph[i].p_memsz;
            if (v_end > max_v) max_v = v_end;
        } else if (ph[i].p_type == PT_DYNAMIC) {
            dynp = &ph[i];
        }
    }
    if (min_v == ~0ULL) return 0;

    uint64_t vbase = page_down(min_v);
    uint64_t span  = page_up(max_v) - vbase;

    uint64_t chosen = g_next_base;
    g_next_base += page_up(span) + 0x10000000ULL; /* generous gap between objects */

    long mres = sc_mmap((long)chosen, (long)span, 1 | 2 | 4 /* R|W|X - see README */,
                         MAP_ANONYMOUS, -1, 0);
    if (mres < 0 || (uint64_t)mres != chosen) return 0;

    uint64_t base = chosen - vbase; /* runtime_addr = base + link_time_vaddr */
    smemset((void*)(uintptr_t)chosen, 0, span);

    for (int i = 0; i < e->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        void* dst = (void*)(uintptr_t)(base + ph[i].p_vaddr);
        smemcpy(dst, buf + ph[i].p_offset, ph[i].p_filesz);
    }

    o->base = base;
    if (!dynp) return 0;
    return (Elf64_Dyn*)(uintptr_t)(base + dynp->p_vaddr);
}

/* Loads (if not already loaded) the named library from /system/lib/<name>,
 * recursively loading its own DT_NEEDED entries first. */
static int load_needed(const char* name) {
    int existing = find_obj(name);
    if (existing >= 0) return existing;
    if (g_nobjs >= MAX_OBJS) ld_die("too many loaded objects");

    char path[192];
    scopy(path, "/system/lib/", sizeof(path));
    scat(path, name, sizeof(path));

    uint64_t size = 0;
    uint8_t* buf = read_whole_file(path, &size);
    if (!buf) { ld_write("ld.so: cannot open "); ld_write(path); ld_write("\n"); ld_die("missing dependency"); }

    struct obj* o = &g_objs[g_nobjs];
    smemset(o, 0, sizeof(*o));
    scopy(o->name, name, sizeof(o->name));
    int slot = g_nobjs++;

    Elf64_Dyn* dyn = map_image(buf, size, o);
    if (!dyn) ld_die("failed to map shared object");
    parse_dynamic(o, dyn);

    for (int i = 0; i < o->nneeded; i++) load_needed(o->needed[i]);

    return slot;
}

/* ---- entry point, called from start.S with the raw kernel stack ---- */
extern void ldso_jump_to_entry(void* original_sp, uint64_t entry) __attribute__((noreturn));

void ld_main(void* raw_sp) {
    uint64_t* sp = (uint64_t*)raw_sp;
    long argc = (long)sp[0];
    char** argv = (char**)&sp[1];
    char** envp = (char**)&sp[argc + 2];

    uint64_t i = 0;
    while (envp[i]) i++;
    uint64_t* auxv = (uint64_t*)&envp[i + 1];

    uint64_t at_phdr = 0, at_phent = 0, at_phnum = 0, at_entry = 0;
    for (uint64_t* a = auxv; a[0] != AT_NULL; a += 2) {
        switch (a[0]) {
            case AT_PHDR:  at_phdr  = a[1]; break;
            case AT_PHENT: at_phent = a[1]; break;
            case AT_PHNUM: at_phnum = a[1]; break;
            case AT_ENTRY: at_entry = a[1]; break;
            default: break;
        }
    }
    if (!at_phdr || !at_entry) ld_die("no AT_PHDR/AT_ENTRY - kernel stack malformed");

    /* Find the MAIN PROGRAM's PT_DYNAMIC. The main program is ET_EXEC
     * (non-PIE), so its vaddrs are already runtime addresses (load_bias 0) -
     * see README for the PIE-main limitation. */
    Elf64_Phdr* main_ph = (Elf64_Phdr*)(uintptr_t)at_phdr;
    Elf64_Dyn* main_dyn = 0;
    for (uint64_t i2 = 0; i2 < at_phnum; i2++) {
        if (main_ph[i2].p_type == PT_DYNAMIC) { main_dyn = (Elf64_Dyn*)(uintptr_t)main_ph[i2].p_vaddr; break; }
    }
    if (!main_dyn) ld_die("main program has no PT_DYNAMIC");

    struct obj* mo = &g_objs[g_nobjs++];
    smemset(mo, 0, sizeof(*mo));
    scopy(mo->name, "(main)", sizeof(mo->name));
    mo->base = 0; /* ET_EXEC: no bias */
    parse_dynamic(mo, main_dyn);

    for (int i2 = 0; i2 < mo->nneeded; i2++) load_needed(mo->needed[i2]);

    /* Now that every object's symbol table is known, apply relocations. */
    for (int i2 = 0; i2 < g_nobjs; i2++) {
        struct obj* o = &g_objs[i2];
        if (o->rela) apply_symbolic_relocs(o->rela, o->rela_count, o);
        if (o->jmprel) apply_symbolic_relocs(o->jmprel, o->jmprel_count, o);
    }

    (void)at_phent;
    (void)argv;
    ldso_jump_to_entry(raw_sp, at_entry);
}
