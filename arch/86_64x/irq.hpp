#pragma once
#include <stdint.h>

// PIC ports
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

// Basic IRQ/PIC/PIT helpers for timer-based preemption (early stage).
void pic_remap();
void pit_init(uint32_t frequency_hz);
void irq_register_timer_handler(void (*handler)(void));
void pit_handler_c();

// Send End Of Interrupt to PIC
void pic_send_eoi(uint8_t vector);

extern volatile uint64_t timer_ticks;

uint64_t timer_uptime_ms();
uint16_t timer_divisor();
uint32_t timer_frequency_millihz();
