#include "scheduler_preempt.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>
#include "irq.hpp"
#include "elf_loader.hpp"

extern void context_switch(uint64_t* old_sp_ptr, uint64_t new_sp, uint64_t new_cr3);
extern void task_start_trampoline();

struct Task {
    int id;
    uint64_t* sp_ptr; // pointer to saved rsp (stored in task struct)
    uint64_t sp; // current stack pointer value
    void* stack_mem;
    int state; // 0 ready, 1 running, 2 finished
    Task* next;
    void (*entry)(void*);
    void* arg;
    uint64_t pml4; // CR3 value for this task's address space
};

static Task* runqueue_head = nullptr;
static Task* current = nullptr;
static int next_task_id = 1;
static volatile uint64_t ticks = 0;
static const size_t TASK_STACK_SIZE = 16 * 1024;

static uint64_t read_cr3() {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r" (cr3));
    return cr3;
}

int preempt_create_task(void (*entry)(void*), void* arg) {
    Task* t = (Task*)AllocatePool(sizeof(Task));
    if (!t) return -1;
    memset(t, 0, sizeof(Task));
    t->id = next_task_id++;
    t->entry = entry;
    t->arg = arg;
    t->state = 0;
    // allocate stack
    void* stack = AllocatePool(TASK_STACK_SIZE);
    if (!stack) return -1;
    memset(stack, 0, TASK_STACK_SIZE);
    t->stack_mem = stack;
    uint8_t* sp = (uint8_t*)stack + TASK_STACK_SIZE;

    // Build initial stack frame expected by context_switch and trampoline
    // Stack grows down. We'll place the callee-saved slots, then the return address (trampoline), then entry and arg.
    // Order of pushes (low addr -> high addr): rbp, rbx, r12, r13, r14, r15, return_addr, entry_addr, arg

    uint64_t* p = (uint64_t*)sp;
    // push arg
    *(--p) = (uint64_t)arg;
    // push entry
    *(--p) = (uint64_t)entry;
    // push return address = address of trampoline
    *(--p) = (uint64_t)task_start_trampoline;
    // push initial callee-saved registers (rbp, rbx, r12..r15)
    *(--p) = 0; // r15
    *(--p) = 0; // r14
    *(--p) = 0; // r13
    *(--p) = 0; // r12
    *(--p) = 0; // rbx
    *(--p) = 0; // rbp

    t->sp = (uint64_t)(UINTN)p;
    t->sp_ptr = (uint64_t*)AllocatePool(sizeof(uint64_t));
    if (!t->sp_ptr) return -1;
    *(t->sp_ptr) = t->sp;

    // default to current CR3 (kernel address space)
    t->pml4 = read_cr3();

    // push to runqueue
    if (!runqueue_head) { runqueue_head = t; t->next = t; }
    else { t->next = runqueue_head->next; runqueue_head->next = t; }

    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), (CHAR16*)L"preempt: created task %d sp=0x%016lx\n", t->id, t->sp);
    Print(buf);
    return t->id;
}

int preempt_create_process_from_elf(const void* elf_buf, size_t elf_size) {
    uint64_t entry = 0, pml4 = 0;
    if (!elf_loader::load_elf64_from_mem(elf_buf, elf_size, &entry, &pml4)) {
        Print((CHAR16*)L"preempt: elf_loader failed to create process\n");
        return -1;
    }

    Task* t = (Task*)AllocatePool(sizeof(Task));
    if (!t) return -1;
    memset(t, 0, sizeof(Task));
    t->id = next_task_id++;
    t->state = 0;
    // allocate stack
    void* stack = AllocatePool(TASK_STACK_SIZE);
    if (!stack) return -1;
    memset(stack, 0, TASK_STACK_SIZE);
    t->stack_mem = stack;
    uint8_t* sp = (uint8_t*)stack + TASK_STACK_SIZE;

    uint64_t* p = (uint64_t*)sp;
    // push arg (NULL)
    *(--p) = (uint64_t)0;
    // push entry (virtual address in new pml4)
    *(--p) = (uint64_t)entry;
    // push return address = trampoline
    *(--p) = (uint64_t)task_start_trampoline;
    // push callee-saved
    *(--p) = 0; *(--p) = 0; *(--p) = 0; *(--p) = 0; *(--p) = 0; *(--p) = 0;

    t->sp = (uint64_t)(UINTN)p;
    t->sp_ptr = (uint64_t*)AllocatePool(sizeof(uint64_t));
    if (!t->sp_ptr) return -1;
    *(t->sp_ptr) = t->sp;

    t->pml4 = pml4;

    // push to runqueue
    if (!runqueue_head) { runqueue_head = t; t->next = t; }
    else { t->next = runqueue_head->next; runqueue_head->next = t; }

    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), (CHAR16*)L"preempt: created process %d entry=0x%016lx pml4=0x%016lx\n", t->id, entry, pml4);
    Print(buf);
    return t->id;
}

void schedule_next() {
    if (!runqueue_head) return;
    Task* prev = current;
    // simple round-robin: move head to next
    if (!current) current = runqueue_head;
    else current = current->next;
    // find next runnable
    Task* start = current;
    while (current && current->state != 0) {
        current = current->next;
        if (current == start) break;
    }
    if (!current || current->state != 0) return; // nothing to run

    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), (CHAR16*)L"preempt: switching to task %d\n", current->id);
    Print(buf);

    // perform context switch
    if (prev) {
        context_switch(prev->sp_ptr, current->sp, current->pml4);
        // when we return here, we've been switched back
        return;
    } else {
        // no previous (initial), just set up current and jump via fake switch
        uint64_t dummy_old_sp = 0;
        context_switch(&dummy_old_sp, current->sp, current->pml4);
    }
}

void preempt_yield() {
    schedule_next();
}

void preempt_scheduler_tick() {
    ticks++;
    // simple timeslice: switch every 10 ticks
    if ((ticks % 10) == 0) {
        schedule_next();
    }
}

void task_exit() {
    // mark current finished and remove from runqueue
    if (!current) return;
    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), (CHAR16*)L"preempt: task %d exiting\n", current->id);
    Print(buf);
    current->state = 2;
    // remove from runqueue (simple removal)
    Task* t = runqueue_head;
    if (t == current && t->next == t) { // only task
        runqueue_head = nullptr;
        current = nullptr;
    } else {
        // find predecessor
        Task* prev = t;
        while (prev->next != current) prev = prev->next;
        prev->next = current->next;
        if (runqueue_head == current) runqueue_head = current->next;
        Task* next = current->next;
        current = next;
    }
    // free resources (not fully freeing for simplicity)
    // schedule next
    schedule_next();
}

void timer_irq_cb() {
    // Called from pit_handler_c -> timer_handler_cb
    preempt_scheduler_tick();
}

void preempt_init(uint32_t timer_hz) {
    pic_remap();
    pit_init(timer_hz);
    idt_init();
    irq_register_timer_handler(timer_irq_cb);
    enable_interrupts();
    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), (CHAR16*)L"preempt: initialized timer %u Hz\n", timer_hz);
    Print(buf);
}
