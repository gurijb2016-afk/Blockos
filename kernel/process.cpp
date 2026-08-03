#include "process.hpp"
#include "elf_loader.hpp"
#include "allocator.hpp"
#include "scheduler.hpp"

#include <stdint.h>
#include <stddef.h>

namespace process {

#define MAX_PROCESS 128

static Process processes[MAX_PROCESS];
static uint64_t next_pid = 1;
static Process* current_process = nullptr;

/*
 * Scheduler wrapper.
 *
 * A scheduler void (*)(void*) entry-pointot vár.
 * A process objektumot arg-ként adjuk át.
 */
static void process_entry(void* arg)
{
    Process* proc = static_cast<Process*>(arg);

    if (!proc)
        return;

    current_process = proc;
    proc->state = State::RUNNING;

    /*
     * Az ELF entry címét itt tároljuk.
     *
     * FONTOS:
     * Ez még nem jelent valódi user-mode context switch-et.
     * Ehhez később szükség lesz GDT/IDT/TSS + CR3 + ring3
     * context switch implementációra.
     */
    using EntryPoint = void (*)();

    EntryPoint entry =
        reinterpret_cast<EntryPoint>(
            static_cast<uintptr_t>(proc->entry)
        );

    if (entry)
        entry();

    proc->state = State::TERMINATED;
    current_process = nullptr;
}


/*
 * Initialize process table.
 */
void init()
{
    for (int i = 0; i < MAX_PROCESS; ++i)
    {
        processes[i].pid = 0;
        processes[i].pml4 = 0;
        processes[i].entry = 0;
        processes[i].stack = 0;
        processes[i].state = State::EMPTY;
    }

    next_pid = 1;
    current_process = nullptr;
}


/*
 * Create process from an ELF64 image.
 */
Process* create(
    const void* elf,
    size_t size
)
{
    if (!elf || size == 0)
        return nullptr;

    Process* proc = nullptr;

    /*
     * Find free process slot.
     */
    for (int i = 0; i < MAX_PROCESS; ++i)
    {
        if (processes[i].state == State::EMPTY)
        {
            proc = &processes[i];
            break;
        }
    }

    if (!proc)
        return nullptr;


    uint64_t entry = 0;
    uint64_t pml4 = 0;


    /*
     * Load ELF64.
     */
    if (!elf_loader::load_elf64_from_mem(
            elf,
            size,
            &entry,
            &pml4))
    {
        proc->state = State::EMPTY;
        return nullptr;
    }


    /*
     * Process information.
     */
    proc->pid = next_pid++;
    proc->pml4 = pml4;
    proc->entry = entry;

    /*
     * Temporary user stack address.
     *
     * This is only an address reservation for now.
     * A real page allocation/mapping must be implemented
     * before entering ring 3.
     */
    proc->stack = 0x00007FFFFFF00000ULL;


    /*
     * Initial CPU context.
     */
    task::init_context(
        &proc->context,
        entry
    );


    proc->state = State::READY;


    /*
     * Register the process with the current scheduler API.
     *
     * scheduler.hpp provides create_task(), not add_task().
     */
    int task_id = scheduler::create_task(
        process_entry,
        proc
    );

    if (task_id < 0)
    {
        proc->state = State::EMPTY;
        return nullptr;
    }


    return proc;
}


/*
 * Terminate process.
 */
void terminate(Process* proc)
{
    if (!proc)
        return;

    proc->state = State::TERMINATED;

    if (current_process == proc)
        current_process = nullptr;
}


/*
 * Return currently running process.
 */
Process* current()
{
    return current_process;
}

} // namespace process