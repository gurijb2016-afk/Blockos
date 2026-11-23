/* Replacement for __blockos_tls_clone_current (libc tls.c).
 * The old version memcpy'd the parent thread's live TLS block, so a new thread
 * inherited the parent's thread-local values (e.g. Qt's QThreadData key).
 * A new thread must start from the PT_TLS initialisation image instead. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include "blockos_tls.h"

extern const unsigned char __ehdr_start[] __attribute__((weak));

struct tls_phdr {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
};

static const struct tls_phdr *find_tls(void) {
    if (!__ehdr_start) return 0;
    const unsigned char *base = __ehdr_start;
    uint64_t phoff = *(const uint64_t *)(base + 0x20);
    uint16_t phentsize = *(const uint16_t *)(base + 0x36);
    uint16_t phnum = *(const uint16_t *)(base + 0x38);
    for (uint16_t i = 0; i < phnum; i++) {
        const struct tls_phdr *ph = (const struct tls_phdr *)(base + phoff + (uint64_t)i * phentsize);
        if (ph->p_type == 7) return ph;
    }
    return 0;
}

unsigned long __blockos_tls_clone_current(void) {
    unsigned long tp;
    __asm__ volatile("mov %%fs:0,%0" : "=r"(tp));
    if (!tp) return 0;
    struct blockos_tls_tcb *old = (struct blockos_tls_tcb *)tp;
    if (!old->region_base || !old->region_size || old->region_size > (64ULL << 20)) return 0;
    void *mem = mmap(0, (size_t)old->region_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return 0;
    /* Start from a zeroed region, then copy the TCB page from the parent
       (tpoff table etc.) and rebuild the TLS image from the template. */
    unsigned long old_base = old->region_base;
    unsigned long delta = (unsigned long)mem - old_base;
    unsigned long new_tp = tp + delta;

    const struct tls_phdr *tls = find_tls();
    uint64_t align = (tls && tls->p_align) ? tls->p_align : 16;
    if (align > 4096) align = 4096;
    uint64_t memsz = tls ? tls->p_memsz : 0;
    uint64_t tls_size = (memsz + align - 1) & ~(align - 1);

    /* TCB page: everything from tp onwards is copied from the parent. */
    memcpy((void *)new_tp, (const void *)tp, (size_t)(old_base + old->region_size - tp));
    /* TLS block: fresh image from the template (zero-filled tbss). */
    unsigned long tls_start = new_tp - tls_size;
    memset((void *)tls_start, 0, (size_t)tls_size);
    if (tls && tls->p_filesz)
        memcpy((void *)tls_start, (const void *)(uintptr_t)tls->p_vaddr, (size_t)tls->p_filesz);

    struct blockos_tls_tcb *t = (struct blockos_tls_tcb *)new_tp;
    t->self = new_tp;
    t->region_base = (unsigned long)mem;
    return new_tp;
}
