#include "blockos_tls.h"
#include "sys/arch_prctl.h"
#include "sys/mman.h"
#include "string.h"

static unsigned long read_fs(void) {
    unsigned long v;
    __asm__ volatile("mov %%fs:0,%0" : "=r"(v));
    return v;
}

unsigned long __blockos_tls_get_fs(void) {
    unsigned long fs = 0;
    if (arch_prctl(ARCH_GET_FS, (unsigned long)&fs) < 0) return 0;
    return fs;
}

unsigned long __blockos_tls_set_fs(unsigned long fs) {
    return arch_prctl(ARCH_SET_FS, fs) < 0 ? (unsigned long)-1 : fs;
}

void* __tls_get_addr(struct blockos_tls_index* index) {
    if (!index || !index->module) return 0;
    unsigned long tp = __blockos_tls_get_fs();
    if (!tp) return 0;
    struct blockos_tls_tcb* tcb = (struct blockos_tls_tcb*)tp;
    if (!tcb->tpoff_table_offset) return 0;
    int64_t* table = (int64_t*)(tp + tcb->tpoff_table_offset);
    if (index->module > 4096) return 0;
    int64_t tpoff = table[index->module];
    return (void*)(tp + tpoff + index->offset);
}

unsigned long __blockos_tls_clone_current(void) {
    unsigned long tp = __blockos_tls_get_fs();
    if (!tp) return 0;
    struct blockos_tls_tcb* old = (struct blockos_tls_tcb*)tp;
    if (!old->region_base || !old->region_size || old->region_size > (64ULL<<20)) return 0;
    void* mem = mmap(0, (size_t)old->region_size, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return 0;
    memcpy(mem, (void*)(unsigned long)old->region_base, (size_t)old->region_size);
    unsigned long delta = (unsigned long)mem - old->region_base;
    unsigned long new_tp = tp + delta;
    struct blockos_tls_tcb* t = (struct blockos_tls_tcb*)new_tp;
    t->self = new_tp;
    t->region_base += delta;
    return new_tp;
}

/* Static executables have no ld.so to build the thread block, so crt1 calls
 * this before main(). It builds a variant II TLS block (PT_TLS image ending at
 * the thread pointer, TCB self pointer at fs:0) and sets FS to it. Returns at
 * once when FS is already set, which is the case for dynamic binaries. */
extern const unsigned char __ehdr_start[] __attribute__((weak));

struct blockos_static_phdr {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
};

void __blockos_static_tls_setup(void) {
    if (__blockos_tls_get_fs()) return;
    if (!__ehdr_start) return;
    const unsigned char* base = __ehdr_start;
    uint64_t phoff = *(const uint64_t*)(base + 0x20);
    uint16_t phentsize = *(const uint16_t*)(base + 0x36);
    uint16_t phnum = *(const uint16_t*)(base + 0x38);
    const struct blockos_static_phdr* tls = 0;
    for (uint16_t i = 0; i < phnum; i++) {
        const struct blockos_static_phdr* ph =
            (const struct blockos_static_phdr*)(base + phoff + (uint64_t)i * phentsize);
        if (ph->p_type == 7) { tls = ph; break; }  /* PT_TLS */
    }
    uint64_t align = (tls && tls->p_align) ? tls->p_align : 16;
    if (align > 4096) align = 4096;
    uint64_t memsz = tls ? tls->p_memsz : 0;
    uint64_t tls_size = (memsz + align - 1) & ~(align - 1);
    uint64_t total = tls_size + 4096;  /* block + one page for the TCB */
    void* mem = mmap(0, (size_t)total, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return;
    unsigned long tp = (unsigned long)mem + tls_size;
    if (tls && tls->p_filesz) {
        memcpy((void*)(tp - tls_size), (const void*)(uintptr_t)tls->p_vaddr,
               (size_t)tls->p_filesz);
    }
    struct blockos_tls_tcb* t = (struct blockos_tls_tcb*)tp;
    t->self = tp;
    t->region_base = (unsigned long)mem;
    t->region_size = total;
    __blockos_tls_set_fs(tp);
}
