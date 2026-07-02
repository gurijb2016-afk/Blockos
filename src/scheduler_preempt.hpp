#pragma once
#include <stdint.h>

// Preemptive scheduler interfaces

void preempt_init(uint32_t timer_hz);
int preempt_create_task(void (*entry)(void*), void* arg);
void preempt_yield();
void preempt_scheduler_tick();
void task_exit();
