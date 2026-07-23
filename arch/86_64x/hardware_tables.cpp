#include "hardware_tables.hpp"

__attribute__((aligned(4096))) static GDTEntry gdt[5];
static GDTDescriptor gdt_desc;
__attribute__((aligned(16))) static IDTEntry idt[256];

void HardwareTablesManager::setup_cpu_security() {
    gdt[0] = {0,0,0,0,0,0}; // Null
    gdt[1] = {0, 0, 0, 0x9A, 0x20, 0}; // Kernel Code
    gdt[2] = {0, 0, 0, 0x92, 0x00, 0}; // Kernel Data
    
    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base = (uint64_t)&gdt;
    asm volatile("lgdt %0" : : "m"(gdt_desc));
}

void HardwareTablesManager::register_interrupt_handler(uint8_t vector, void* handler, uint8_t flags) {
    uint64_t addr = (uint64_t)handler;
    idt[vector].isr_low = addr & 0xFFFF;
    idt[vector].kernel_cs = 0x08;
    idt[vector].ist = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

void HardwareTablesManager::load_idt() {
    IDTR idtr = { sizeof(idt) - 1, (uint64_t)&idt };
    asm volatile("lidt %0" : : "m"(idtr));
}
HardwareTablesManager cpu_tables;
