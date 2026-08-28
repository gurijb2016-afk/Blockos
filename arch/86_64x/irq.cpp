#include "irq.hpp"
extern "C"
{
#include <efi.h>
}
extern "C"
{
#include <efilib.h>
}
#include "drivers/io.hpp"
#include "irq.hpp"

using io::outb;

static void (*timer_handler_cb)(void) = 0;

// Remap PIC to avoid conflicts with CPU exceptions
void pic_remap()
{
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

void irq_register_timer_handler(void (*handler)(void))
{
    timer_handler_cb = handler;
}

// PIT freq base
static const uint32_t PIT_BASE = 1193182u;
static uint16_t pit_divisor = 0;
volatile uint64_t timer_ticks = 0;

void pit_init(uint32_t frequency_hz)
{
    // Divisor is 16-bit, so the achievable rate is 19 Hz to PIT_BASE
    if (frequency_hz < 19) frequency_hz = 19;
    if (frequency_hz > PIT_BASE) frequency_hz = PIT_BASE;

    uint16_t divisor = (uint16_t) (PIT_BASE / frequency_hz);
    pit_divisor = divisor;
    // Command: channel 0, lo/hi, rate generator
    outb(0x43, 0x36);
    outb(0x40, (uint8_t) (divisor & 0xFF));
    outb(0x40, (uint8_t) ((divisor >> 8) & 0xFF));
}

uint16_t timer_divisor()
{
    return pit_divisor;
}

uint32_t timer_frequency_millihz()
{
    return (uint32_t) ((uint64_t) PIT_BASE * 1000u / pit_divisor);
}

uint64_t timer_uptime_ms()
{
    return (uint64_t) timer_ticks * pit_divisor * 1000u / PIT_BASE;
}


// Called from assembly stub when timer IRQ occurs
void pit_handler_c()
{
    // call registered handler
    if (timer_handler_cb) timer_handler_cb();
    timer_ticks++;
    // Send EOI to PIC
    pic_send_eoi(32);
}

void pic_send_eoi(uint8_t vector)
{
    if (vector >= 40) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}
