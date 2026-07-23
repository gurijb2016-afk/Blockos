#pragma once
#include <stdint.h>

namespace scheduler {
    // Initialize scheduler subsystem
    void scheduler_init();
    // Create a new task (stub) and add to runqueue
    int create_task(void (*entry)(void*), void* arg);
    // Trigger scheduling (voluntary yield)
    void yield();
    // Start the scheduling loop (for single-core cooperative test)
    void run_scheduler_loop();
}
