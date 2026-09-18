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
