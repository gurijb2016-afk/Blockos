#pragma once
#include <stdint.h>
#include <stddef.h>

struct blockos_tls_tcb {
    uint64_t self;
    int64_t  tpoff_table_offset;
    uint64_t region_base;
    uint64_t region_size;
    uint64_t generation;
    uint64_t reserved0, reserved1, reserved2;
};

struct blockos_tls_index { size_t module; size_t offset; };

void* __tls_get_addr(struct blockos_tls_index* index);
unsigned long __blockos_tls_get_fs(void);
unsigned long __blockos_tls_set_fs(unsigned long fs);
unsigned long __blockos_tls_clone_current(void);
