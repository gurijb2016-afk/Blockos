#include "irq.hpp"
#include <efi.h>
#include <efilib.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a" (val), "dN" (port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

// PIC ports
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
define PIC2_DATA 0xA1

// Remap PIC to avoid conflicts with CPU exceptions
void pic_remap() {
    // Initialization control words for PIC remap
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20); // IRQs 0-7 -> vectors 0x20-0x27
    outb(PIC2_DATA, 0x28); // IRQs 8-15 -> vectors 0x28-0x2F
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    // Mask nothing
    outb(PIC1_DATA, 0x0);
    outb(PIC2_DATA, 0x0);
}

static void (*timer_handler_cb)(void) = 0;

void irq_register_timer_handler(void (*handler)(void)) {
    timer_handler_cb = handler;
}

void pit_init(uint32_t frequency_hz) {
    // PIT freq base
    const uint32_t PIT_BASE = 1193182u;
    uint16_t divisor = (uint16_t)(PIT_BASE / frequency_hz);
    // Command: channel 0, lo/hi, rate generator
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// Called from assembly stub when timer IRQ occurs
void pit_handler_c() {
    // call registered handler
    if (timer_handler_cb) timer_handler_cb();
    // send EOI to PIC
    outb(PIC1_CMD, 0x20);
}

static inline void pic_send_eoi() {
    outb(PIC1_CMD, 0x20);
}
