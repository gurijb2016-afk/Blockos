#pragma once
#include <stdint.h>

struct GDTEntry { uint16_t limit_low; uint16_t base_low; uint8_t base_middle; uint8_t access; uint8_t granularity; uint8_t base_high; } __attribute__((packed));
struct GDTDescriptor { uint16_t limit; uint64_t base; } __attribute__((packed));
struct IDTEntry { uint16_t isr_low; uint16_t kernel_cs; uint8_t ist; uint8_t attributes; uint16_t isr_mid; uint32_t isr_high; uint32_t reserved; } __attribute__((packed));
struct IDTR { uint16_t limit; uint64_t base; } __attribute__((packed));

class HardwareTablesManager {
public:
    void setup_cpu_security();
    void load_idt();
    void register_interrupt_handler(uint8_t vector, void* handler, uint8_t flags);
};
