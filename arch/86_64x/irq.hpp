#pragma once
#include <stdint.h>

// Basic IRQ/PIC/PIT helpers for timer-based preemption (early stage).

void pic_remap();
void pit_init(uint32_t frequency_hz);
void irq_register_timer_handler(void (*handler)(void));

// Send End Of Interrupt to PIC
static inline void pic_send_eoi();
