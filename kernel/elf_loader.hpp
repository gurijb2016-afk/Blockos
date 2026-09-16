#pragma once
#include <stdint.h>
#include <stddef.h>

namespace elf_loader {

// Result of loading one ELF image (main executable OR interpreter) into a
// process address space.
struct LoadResult {
    uint64_t entry;         // e_entry, adjusted by load_bias for ET_DYN
    uint64_t load_bias;     // 0 for ET_EXEC, chosen base for ET_DYN
    uint64_t phdr_vaddr;    // vaddr of the program header table once mapped
    uint16_t phnum;
    uint16_t phentsize;
    bool     has_interp;
    char     interp_path[256]; // valid only if has_interp
};

// Loads an ELF64 image into the given address space (pml4 must already
// exist - use paging::clone_current_pml4() or paging::create_pml4() by the
// caller). If fixed_base != 0, ET_DYN images are loaded at that base
// (used for the interpreter); if fixed_base == 0, ET_DYN images are loaded
// at a default base. PT_INTERP is no longer a hard failure: if found, its
// path is copied into out->interp_path and out->has_interp is set, and the
// caller is expected to load the interpreter separately and hand control to
// IT instead of out->entry.
bool load_elf64_into(
    uint64_t pml4,
    const void* elf_buf,
    size_t elf_size,
    uint64_t fixed_base,
    LoadResult* out);

// Convenience wrapper kept for existing static-binary callers
// (e.g. the current /bin/sh ring3 test): clones the current pml4, loads a
// single static (no PT_INTERP, no dynamic linker) ELF, and returns a plain
// entry/stack/pml4 triple exactly like the pre-existing API did. Fails if
// the image has a PT_INTERP - callers that need dynamic linking must use
// load_elf64_into + load_interpreter_and_build_stack below instead.
bool load_elf64_from_mem(
    const void* elf_buf,
    size_t elf_size,
    uint64_t* entry_out,
    uint64_t* pml4_out,
    uint64_t* stack_out = nullptr);

// Builds the initial SysV-ABI-style stack (argc, argv[], envp[], auxv[])
// at the top of an already-mapped user stack region and returns the rsp
// the process should start with. argv/envp are single-string-array style:
// argv_count strings pointed to by argv, NULL-terminated; same for envp.
// entry is the *real* program entry point (AT_ENTRY); if interp_base != 0
// the caller should still jump to the interpreter's own entry, not this
// one - the interpreter reads AT_ENTRY off this stack to find it.
uint64_t build_initial_stack(
    uint64_t pml4,
    uint64_t stack_top,
    size_t stack_pages,
    const char* const* argv,
    int argc,
    const char* const* envp,
    int envc,
    const LoadResult& main_image,
    uint64_t interp_base);

}
