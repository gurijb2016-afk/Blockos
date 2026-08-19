#include "scheduler_preempt.hpp"

extern "C" {
#include <efi.h>
}
extern "C" {
#include <efilib.h>
}
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "irq.hpp"
#include "elf_loader.hpp"

/*
 * Assembly context switch.
 *
 * old_sp_ptr:
 *     ide kerül az előző task RSP-je
 *
 * new_sp:
 *     az új task RSP-je
 *
 * new_cr3:
 *     az új task address space CR3 értéke
 */
extern "C" void context_switch(
    uint64_t* old_sp_ptr,
    uint64_t new_sp,
    uint64_t new_cr3
);

extern "C" void task_start_trampoline();


namespace {

struct Task
{
    int id;

    uint64_t* sp_ptr;
    uint64_t sp;

    void* stack_mem;

    /*
     * 0 = READY
     * 1 = RUNNING
     * 2 = FINISHED
     */
    int state;

    Task* next;

    void (*entry)(void*);
    void* arg;

    uint64_t pml4;
};


static Task* runqueue_head = nullptr;
static Task* current = nullptr;

static int next_task_id = 1;

static volatile uint64_t ticks = 0;

static const size_t TASK_STACK_SIZE = 16 * 1024;


/*
 * Read current CR3.
 */
static uint64_t read_cr3()
{
    uint64_t cr3;

    __asm__ volatile(
        "mov %%cr3, %0"
        : "=r"(cr3)
    );

    return cr3;
}


/*
 * Add task to circular runqueue.
 */
static void enqueue_task(Task* task)
{
    if (!task)
        return;

    task->next = nullptr;

    if (!runqueue_head)
    {
        runqueue_head = task;
        task->next = task;
        return;
    }

    task->next = runqueue_head->next;
    runqueue_head->next = task;
}


/*
 * Allocate and initialize task stack.
 */
static bool initialize_task_stack(
    Task* task,
    void (*entry)(void*),
    void* arg
)
{
    if (!task || !entry)
        return false;

    void* stack = AllocatePool(TASK_STACK_SIZE);

    if (!stack)
        return false;

    memset(stack, 0, TASK_STACK_SIZE);

    task->stack_mem = stack;

    uint8_t* stack_top =
        (uint8_t*)stack + TASK_STACK_SIZE;

    uint64_t* p =
        (uint64_t*)stack_top;


    /*
     * Stack layout expected by context_switch.
     *
     * arg
     * entry
     * trampoline
     * r15
     * r14
     * r13
     * r12
     * rbx
     * rbp
     */

    *(--p) = (uint64_t)arg;
    *(--p) = (uint64_t)entry;
    *(--p) = (uint64_t)task_start_trampoline;

    *(--p) = 0; /* r15 */
    *(--p) = 0; /* r14 */
    *(--p) = 0; /* r13 */
    *(--p) = 0; /* r12 */
    *(--p) = 0; /* rbx */
    *(--p) = 0; /* rbp */


    task->sp = (uint64_t)(UINTN)p;

    task->sp_ptr =
        (uint64_t*)AllocatePool(sizeof(uint64_t));

    if (!task->sp_ptr)
    {
        FreePool(task->stack_mem);
        task->stack_mem = nullptr;
        return false;
    }

    *(task->sp_ptr) = task->sp;

    return true;
}


} // anonymous namespace


namespace scheduler_preempt {


void preempt_init(uint32_t timer_hz)
{
    /*
     * Reset scheduler state.
     */
    runqueue_head = nullptr;
    current = nullptr;
    next_task_id = 1;
    ticks = 0;


    /*
     * PIC + PIT.
     */
    pic_remap();

    pit_init(timer_hz);


    /*
     * The current IRQ implementation appears to initialize
     * the interrupt subsystem itself.
     *
     * Do NOT call a nonexistent idt_init() here.
     */


    irq_register_timer_handler(
        timer_irq_cb
    );


    /*
     * Enable interrupts.
     *
     * We don't call a nonexistent enable_interrupts()
     * function. Use the x86 STI instruction directly.
     */
    __asm__ volatile("sti");


    CHAR16 buf[128];

    const CHAR16 format[] = {
        'p','r','e','e','m','p','t',
        ':',' ',
        'i','n','i','t','i','a','l','i','z','e','d',
        ' ',
        't','i','m','e','r',
        ' ',
        '%','u',
        ' ',
        'H','z',
        '\n',
        0
    };

    UnicodeSPrint(
        buf,
        sizeof(buf),
        format,
        timer_hz
    );

    Print(buf);
}


int preempt_create_task(
    void (*entry)(void*),
    void* arg
)
{
    if (!entry)
        return -1;


    Task* task =
        (Task*)AllocatePool(sizeof(Task));

    if (!task)
        return -1;


    memset(
        task,
        0,
        sizeof(Task)
    );


    task->id = next_task_id++;
    task->entry = entry;
    task->arg = arg;
    task->state = 0;
    task->pml4 = read_cr3();


    if (!initialize_task_stack(
            task,
            entry,
            arg))
    {
        FreePool(task);
        return -1;
    }


    enqueue_task(task);


    CHAR16 buf[128];

    const CHAR16 format[] = {
        'p','r','e','e','m','p','t',
        ':',' ',
        'c','r','e','a','t','e','d',
        ' ',
        't','a','s','k',
        ' ',
        '%','d',
        ' ',
        's','p','=',
        '0','x',
        '%','l','x',
        '\n',
        0
    };

    UnicodeSPrint(
        buf,
        sizeof(buf),
        format,
        task->id,
        task->sp
    );

    Print(buf);

    return task->id;
}


int preempt_create_process_from_elf(
    const void* elf_buf,
    size_t elf_size
)
{
    if (!elf_buf || elf_size == 0)
        return -1;


    uint64_t entry = 0;
    uint64_t pml4 = 0;


    if (!elf_loader::load_elf64_from_mem(
            elf_buf,
            elf_size,
            &entry,
            &pml4))
    {
        const CHAR16 message[] = {
            'p','r','e','e','m','p','t',
            ':',' ',
            'e','l','f','_','l','o','a','d','e','r',
            ' ',
            'f','a','i','l','e','d',
            '\n',
            0
        };

        Print((CHAR16*)message);

        return -1;
    }


    Task* task =
        (Task*)AllocatePool(sizeof(Task));

    if (!task)
        return -1;


    memset(
        task,
        0,
        sizeof(Task)
    );


    task->id = next_task_id++;
    task->state = 0;

    task->entry =
        (void (*)(void*))(UINTN)entry;

    task->arg = nullptr;

    task->pml4 = pml4;


    if (!initialize_task_stack(
            task,
            task->entry,
            nullptr))
    {
        FreePool(task);
        return -1;
    }


    enqueue_task(task);


    CHAR16 buf[160];

    const CHAR16 format[] = {
        'p','r','e','e','m','p','t',
        ':',' ',
        'c','r','e','a','t','e','d',
        ' ',
        'p','r','o','c','e','s','s',
        ' ',
        '%','d',
        ' ',
        'e','n','t','r','y','=',
        '0','x','%','l','x',
        ' ',
        'p','m','l','4','=',
        '0','x','%','l','x',
        '\n',
        0
    };

    UnicodeSPrint(
        buf,
        sizeof(buf),
        format,
        task->id,
        entry,
        pml4
    );

    Print(buf);

    return task->id;
}


void schedule_next()
{
    if (!runqueue_head)
        return;


    Task* previous = current;


    /*
     * First scheduler invocation.
     */
    if (!current)
    {
        current = runqueue_head;
    }
    else
    {
        current = current->next;
    }


    /*
     * Search for READY task.
     */
    Task* start = current;

    do
    {
        if (current->state == 0)
            break;

        current = current->next;

    } while (current != start);


    /*
     * Nothing runnable.
     */
    if (!current ||
        current->state != 0)
    {
        return;
    }


    current->state = 1;


    CHAR16 buf[128];

    const CHAR16 format[] = {
        'p','r','e','e','m','p','t',
        ':',' ',
        's','w','i','t','c','h','i','n','g',
        ' ',
        't','o',
        ' ',
        't','a','s','k',
        ' ',
        '%','d',
        '\n',
        0
    };

    UnicodeSPrint(
        buf,
        sizeof(buf),
        format,
        current->id
    );

    Print(buf);


    /*
     * First task.
     */
    if (!previous)
    {
        uint64_t dummy_old_sp = 0;

        context_switch(
            &dummy_old_sp,
            current->sp,
            current->pml4
        );

        return;
    }


    /*
     * Normal context switch.
     */
    context_switch(
        previous->sp_ptr,
        current->sp,
        current->pml4
    );
}


void preempt_yield()
{
    if (!current)
        return;

    /*
     * Current task becomes READY again.
     */
    if (current->state == 1)
        current->state = 0;

    schedule_next();
}


void preempt_scheduler_tick()
{
    ++ticks;


    /*
     * Every 10 timer ticks perform scheduling.
     */
    if ((ticks % 10) == 0)
    {
        if (current &&
            current->state == 1)
        {
            current->state = 0;
        }

        schedule_next();
    }
}


void task_exit()
{
    if (!current)
        return;


    Task* exiting = current;


    CHAR16 buf[128];

    const CHAR16 format[] = {
        'p','r','e','e','m','p','t',
        ':',' ',
        't','a','s','k',
        ' ',
        '%','d',
        ' ',
        'e','x','i','t','i','n','g',
        '\n',
        0
    };

    UnicodeSPrint(
        buf,
        sizeof(buf),
        format,
        exiting->id
    );

    Print(buf);


    exiting->state = 2;


    /*
     * Remove from circular runqueue.
     */
    if (exiting->next == exiting)
    {
        runqueue_head = nullptr;
        current = nullptr;
        return;
    }


    Task* previous = runqueue_head;

    while (previous->next != exiting)
    {
        previous = previous->next;

        if (previous == runqueue_head)
        {
            return;
        }
    }


    previous->next = exiting->next;


    if (runqueue_head == exiting)
    {
        runqueue_head = exiting->next;
    }


    current = exiting->next;


    /*
     * Resource cleanup.
     */
    if (exiting->sp_ptr)
    {
        FreePool(exiting->sp_ptr);
        exiting->sp_ptr = nullptr;
    }


    if (exiting->stack_mem)
    {
        FreePool(exiting->stack_mem);
        exiting->stack_mem = nullptr;
    }


    FreePool(exiting);


    schedule_next();
}


void timer_irq_cb()
{
    preempt_scheduler_tick();
}


} // namespace scheduler_preempt
