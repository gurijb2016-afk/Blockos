#pragma once
#include <stdint.h>
#include <stddef.h>
#include "task_context.hpp"
#include "preempt.hpp"

namespace process {

enum class State : uint8_t { EMPTY = 0, READY, RUNNING, BLOCKED, TERMINATED };

struct Process {
    uint64_t pid, pml4, entry, stack, task_id, brk_base, brk_current, mmap_next;
    State state;
    TaskContext context;
    bool has_interp;
    uint64_t real_entry;

    /* Full ring3 state while this task is not the running one. Seeded at
     * creation time so a never-yet-run task can be switched into exactly
     * like one that was preempted mid-execution - the scheduler needs no
     * special "first run" case. */
    TrapFrame saved_frame;
    bool frame_valid;
};

void init();

/* Creates a process in READY state (loads the ELF, sets up its address
 * space and initial frame) but does NOT start it. */
Process* spawn(const void* elf, size_t size, const char* path = nullptr);

/* Backwards-compatible: spawn + run immediately, blocking until it (and
 * everything else runnable) has finished. Existing kernel.cpp calls keep
 * working unchanged. */
Process* create(const void* elf, size_t size, const char* path = nullptr);
int run(Process* proc);

/* Starts the scheduler: enters the first READY task in ring3 and returns
 * only once every task has exited. */
void run_scheduler();

bool terminate(Process* proc);
Process* current();
void set_current(Process* p);
Process* get(uint64_t pid);
size_t count();

/* Raw slot access for the scheduler's round-robin walk. slot_at() may
 * return a slot in any state, including EMPTY. */
size_t slot_count();
Process* slot_at(size_t i);

}
