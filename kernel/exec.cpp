#include "elf_loader.hpp"
#include "task_context.hpp"
#include <stdint.h>
#include <stddef.h>

namespace exec {

struct Process {
    uint64_t pid;
    uint64_t pml4;
    uint64_t entry;
    uint64_t stack;
    TaskContext context;
    bool running;
};

#define MAX_PROCESSES 64

static Process processes[MAX_PROCESSES];
static uint64_t next_pid = 1;
static uint64_t process_count = 0;


Process* create_process(
    const void* elf_data,
    size_t elf_size
)
{
    if (process_count >= MAX_PROCESSES)
        return nullptr;

    uint64_t entry = 0;
    uint64_t pml4 = 0;

    if (!elf_loader::load_elf64_from_mem(
            elf_data,
            elf_size,
            &entry,
            &pml4))
    {
        return nullptr;
    }


    Process* proc = &processes[process_count++];

    proc->pid = next_pid++;
    proc->pml4 = pml4;
    proc->entry = entry;
    proc->running = false;


    task::init_context(
        &proc->context,
        entry
    );


    return proc;
}


void start_process(Process* proc)
{
    if (!proc)
        return;

    proc->running = true;

    task::switch_to(
        &proc->context
    );
}


}
