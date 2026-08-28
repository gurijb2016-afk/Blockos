#include "hardware_tables.hpp"

#include "irq.hpp"
#include "isr_stubs.hpp"
#include "libc/include/stdio.h"
#include "libc/include/string.h"

extern "C" void reload_segments();

__attribute__((aligned(4096))) static GDTEntry gdt[5];
static GDTDescriptor gdt_desc;
__attribute__((aligned(16))) static IDTEntry idt[256];
__attribute__((aligned(16))) static TSS tss;
__attribute__((aligned(16))) static uint8_t df_stack[8192];

HardwareTablesManager cpu_tables;

void HardwareTablesManager::register_stubs()
{
    for (int i = 0; i < 256; i++)
    {
        register_interrupt_handler(i, (void*) default_stubs[i], 0x8E);
    }
}
void HardwareTablesManager::init()
{
    setup_cpu_security();
    register_stubs();
    idt[8].ist = 1;
    load_idt(); // Load the IDT with the new IDT entries
    pic_remap(); // Remap the PIC to avoid conflicts with CPU exceptions
    pit_init(100); // Initialize the PIT to 100 Hz

    __asm__ volatile("sti"); // Enable interrupts
}

void HardwareTablesManager::setup_cpu_security()
{
    __asm__ volatile("cli"); // Set IF of RFLAGS to 0 to disable interrupts

    gdt[0] = {0, 0, 0, 0, 0, 0}; // Null
    gdt[1] = {0, 0, 0, 0x9A, 0x20, 0}; // Kernel Code
    gdt[2] = {0, 0, 0, 0x92, 0x00, 0}; // Kernel Data
    setup_tss();

    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base = (uint64_t) &gdt;

    // Transition to the new GDT/IDT
    __asm__ volatile("lgdt %0" : : "m"(gdt_desc));
    reload_segments(); // Overwrite segment registers with new GDT values
    __asm__ volatile("ltr %w0" : : "r"((uint16_t) 0x18));
}

void HardwareTablesManager::setup_tss()
{
    memset(&tss, 0, sizeof(tss));
    tss.ist1 = (uint64_t) df_stack + sizeof(df_stack);
    tss.iomap_base = sizeof(tss);

    uint64_t base = (uint64_t) &tss;
    uint32_t limit = sizeof(tss) - 1;

    gdt[3] = {(uint16_t) (limit & 0xFFFF),
              (uint16_t) (base & 0xFFFF),
              (uint8_t) ((base >> 16) & 0xFF),
              0x89,
              (uint8_t) ((limit >> 16) & 0x0F),
              (uint8_t) ((base >> 24) & 0xFF)};

    uint32_t base_upper = (uint32_t) (base >> 32);
    gdt[4] = {(uint16_t) (base_upper & 0xFFFF), (uint16_t) ((base_upper >> 16) & 0xFFFF), 0, 0, 0, 0};
}

struct InterruptFrame
{
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t vector, error_code;
    uint64_t rip, cs, rflags, rsp, ss; // pushed by CPU automatically
} __attribute__((packed));

extern "C" void interrupt_handler_c(InterruptFrame* frame)
{
    if (frame->vector < 32)
    {
        printf("Interrupt: vector=%llu, error_code=%llu, rip=0x%llx\n", frame->vector, frame->error_code, frame->rip);
        for (;;) __asm__ volatile("cli; hlt"); // Halt the CPU for now
    }
    else if (frame->vector < 48)
    {
        if (frame->vector == 32)
        {
            pit_handler_c(); // Call the timer handler
        }
        pic_send_eoi((uint8_t) frame->vector);
    }
    else
    {
        // Handle other interrupts if needed
    }
}

void HardwareTablesManager::register_interrupt_handler(uint8_t vector, void* handler, uint8_t flags)
{
    uint64_t addr = (uint64_t) handler;
    idt[vector].isr_low = addr & 0xFFFF;
    idt[vector].cs_selector = 0x08;
    idt[vector].ist = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

void HardwareTablesManager::load_idt()
{
    IDTR idtr = {sizeof(idt) - 1, (uint64_t) &idt};
    asm volatile("lidt %0" : : "m"(idtr));
}