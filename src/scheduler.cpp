#include "scheduler.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

// Minimal round-robin scheduler skeleton. This is a cooperative stub; preemption will be added
// after timer/interrupt integration.

struct Task {
    int id;
    void (*entry)(void*);
    void* arg;
    int state; // 0 = ready, 1 = running, 2 = blocked
    Task* next;
};

static Task* runqueue_head = nullptr;
static int next_task_id = 1;

void scheduler::scheduler_init() {
    runqueue_head = nullptr;
    next_task_id = 1;
}

int scheduler::create_task(void (*entry)(void*), void* arg) {
    Task* t = (Task*)AllocatePool(sizeof(Task));
    if (!t) return -1;
    memset(t, 0, sizeof(Task));
    t->id = next_task_id++;
    t->entry = entry;
    t->arg = arg;
    t->state = 0;
    // push to runqueue
    if (!runqueue_head) { runqueue_head = t; t->next = t; }
    else { t->next = runqueue_head->next; runqueue_head->next = t; }
    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"scheduler: created task %d\n", t->id);
    Print(buf);
    return t->id;
}

void scheduler::yield() {
    // move head pointer
    if (runqueue_head && runqueue_head->next) runqueue_head = runqueue_head->next;
}

void scheduler::run_scheduler_loop() {
    if (!runqueue_head) return;
    Task* start = runqueue_head;
    Task* cur = start;
    do {
        if (cur->entry && cur->state == 0) {
            cur->state = 1;
            // Run the task -- NOTE: this is cooperative and will block here until return
            cur->entry(cur->arg);
            cur->state = 2; // finished/blocked
        }
        cur = cur->next;
    } while (cur != start);
}
