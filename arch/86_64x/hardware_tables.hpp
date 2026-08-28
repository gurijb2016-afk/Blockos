#pragma once
#include <stdint.h>

struct GDTEntry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct GDTDescriptor
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct IDTEntry
{
    uint16_t isr_low;
    uint16_t cs_selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed));

struct IDTR
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct TSS
{
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

class HardwareTablesManager
{
   public:
    void init();
    void setup_cpu_security();
    void load_idt();
    void register_interrupt_handler(uint8_t vector, void* handler, uint8_t flags);
    void register_stubs();
    void setup_tss();
};

extern HardwareTablesManager cpu_tables;