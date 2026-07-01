#pragma once

// Simple IDT entry structure and loader.

void idt_init();
void enable_interrupts();
void disable_interrupts();
