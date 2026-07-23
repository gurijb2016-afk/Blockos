#include "process.hpp"
#include "elf_loader.hpp"
#include "allocator.hpp"
#include "scheduler.hpp"

namespace process {

#define MAX_PROCESS 128

static Process processes[MAX_PROCESS];

static uint64_t next_pid = 1;

static Process* current_process = nullptr;


void init()
{
    for (int i = 0; i < MAX_PROCESS; i++)
    {
        processes[i].state = State::EMPTY;
    }
}


Process* create(
    const void* elf,
    size_t size
)
{
    Process* proc = nullptr;


    for (int i = 0; i < MAX_PROCESS; i++)
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


    if (!elf_loader::load_elf64_from_mem(
            elf,
            size,
            &entry,
            &pml4))
    {
        return nullptr;
    }


    proc->pid = next_pid++;

    proc->pml4 = pml4;

    proc->entry = entry;


    // ide később saját user stack allocator jön
    proc->stack = 0x00007FFFFFF00000;


    proc->state = State::READY;


    task::init_context(
        &proc->context,
        entry
    );


    scheduler::add_task(proc);


    return proc;
}


void terminate(Process* proc)
{
    if (!proc)
        return;


    proc->state = State::TERMINATED;
}


Process* current()
{
    return current_process;
}


}
