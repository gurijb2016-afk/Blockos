#include "idt.hpp"
#include <efi.h>
#include <efilib.h>

struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct IDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static IDTEntry idt[256];

extern void isr_timer_entry();

static void set_idt_entry(int vec, void* handler, uint8_t type_attr) {
    uint64_t addr = (uint64_t)(UINTN)handler;
    idt[vec].offset_low = addr & 0xFFFF;
    idt[vec].selector = 0x08; // kernel CODE segment
    idt[vec].ist = 0;
    idt[vec].type_attr = type_attr;
    idt[vec].offset_mid = (addr >> 16) & 0xFFFF;
    idt[vec].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vec].zero = 0;
}

void idt_init() {
    // zero the table
    for (int i = 0; i < 256; ++i) memset(&idt[i], 0, sizeof(IDTEntry));
    // set timer IRQ (IRQ0 -> vector 0x20)
    set_idt_entry(0x20, (void*)isr_timer_entry, 0x8E); // interrupt gate, present
    IDTPtr p;
    p.limit = sizeof(idt) - 1;
    p.base = (uint64_t)(UINTN)idt;
    __asm__ volatile ("lidt %0" : : "m" (p));
}

void enable_interrupts() {
    __asm__ volatile ("sti");
}
void disable_interrupts() {
    __asm__ volatile ("cli");
}
