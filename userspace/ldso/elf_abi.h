#pragma once
#include <stdint.h>

/* Minimal self-contained ELF64 ABI used by BlockOS ld.so. */
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

typedef struct {
    unsigned char e_ident[16];
    Elf64_Half e_type, e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off  e_phoff, e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word  p_type, p_flags;
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr, p_paddr;
    Elf64_Xword p_filesz, p_memsz, p_align;
} Elf64_Phdr;

typedef struct {
    Elf64_Sxword d_tag;
    union { Elf64_Xword d_val; Elf64_Addr d_ptr; } d_un;
} Elf64_Dyn;

typedef struct {
    Elf64_Word  st_name;
    unsigned char st_info, st_other;
    Elf64_Half  st_shndx;
    Elf64_Addr  st_value;
    Elf64_Xword st_size;
} Elf64_Sym;

typedef struct {
    Elf64_Addr  r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

#define ELF64_R_SYM(i)  ((uint32_t)((i) >> 32))
#define ELF64_R_TYPE(i) ((uint32_t)(i))

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EM_X86_64 62
#define ET_EXEC 2
#define ET_DYN 3
#define EV_CURRENT 1

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_PHDR    6
#define PT_TLS     7
#define PT_GNU_RELRO 0x6474e552

#define PF_X 1
#define PF_W 2
#define PF_R 4

#define DT_NULL       0
#define DT_NEEDED     1
#define DT_PLTRELSZ   2
#define DT_PLTGOT     3
#define DT_HASH       4
#define DT_STRTAB     5
#define DT_SYMTAB     6
#define DT_RELA       7
#define DT_RELASZ     8
#define DT_RELAENT    9
#define DT_STRSZ      10
#define DT_SYMENT     11
#define DT_INIT       12
#define DT_FINI       13
#define DT_SONAME     14
#define DT_RPATH      15
#define DT_SYMBOLIC   16
#define DT_REL        17
#define DT_RELSZ      18
#define DT_RELENT     19
#define DT_PLTREL     20
#define DT_DEBUG      21
#define DT_TEXTREL    22
#define DT_JMPREL    23
#define DT_BIND_NOW   24
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH    29
#define DT_FLAGS      30
#define DT_GNU_HASH   0x6ffffef5
#define DT_VERSYM     0x6ffffff0
#define DT_RELACOUNT  0x6ffffff9
#define DT_PREINIT_ARRAY 32
#define DT_PREINIT_ARRAYSZ 33
#define DT_FLAGS_1    0x6ffffffb
#define DT_VERDEF     0x6ffffffc
#define DT_VERDEFNUM  0x6ffffffd
#define DT_VERNEED    0x6ffffffe
#define DT_VERNEEDNUM 0x6fffffff

/* GNU ld/lld flags. */
#define DF_SYMBOLIC   0x2
#define DF_BIND_NOW   0x8
#define DF_1_NOW      0x00000001
#define DF_1_NODEFLIB 0x00000800

/* Symbol binding/type helpers. */
#define ELF64_ST_BIND(i) ((unsigned char)((i) >> 4))
#define ELF64_ST_TYPE(i) ((unsigned char)((i) & 0x0f))
#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2
#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC   2
#define STT_TLS    6

#define SHN_UNDEF 0
#define SHN_ABS   0xfff1

/* x86-64 dynamic relocations. */
#define R_X86_64_NONE       0
#define R_X86_64_64         1
#define R_X86_64_PC32       2
#define R_X86_64_PLT32      4
#define R_X86_64_GLOB_DAT   6
#define R_X86_64_JUMP_SLOT  7
#define R_X86_64_RELATIVE   8
#define R_X86_64_DTPMOD64   16
#define R_X86_64_DTPOFF64   17
#define R_X86_64_TPOFF64    18
#define R_X86_64_GOTPCREL   9
#define R_X86_64_32         10
#define R_X86_64_32S        11
#define R_X86_64_GOTPCRELX  41
#define R_X86_64_REX_GOTPCRELX 42
#define R_X86_64_IRELATIVE  37
#define R_X86_64_COPY       5
#define R_X86_64_TLSDESC     36

/* auxv types used by the BlockOS process loader. */
#define AT_NULL    0
#define AT_PHDR    3
#define AT_PHENT   4
#define AT_PHNUM   5
#define AT_PAGESZ  6
#define AT_BASE    7
#define AT_ENTRY   9
#define AT_RANDOM 25
#define AT_EXECFN 31
