#pragma once

#include <stdint.h>
#include <stddef.h>

namespace scheduler_preempt {

    void preempt_init(uint32_t timer_hz);

    int preempt_create_task(
        void (*entry)(void*),
        void* arg
    );

    int preempt_create_process_from_elf(
        const void* elf_buf,
        size_t elf_size
    );

    void preempt_yield();

    void preempt_scheduler_tick();

    void schedule_next();

    void task_exit();

    void timer_irq_cb();

}