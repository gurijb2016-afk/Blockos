#include "process.hpp"
#include "elf_loader.hpp"
#include "allocator.hpp"
#include "scheduler.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace process
{

static constexpr size_t MAX_PROCESS = 128;

static Process processes[MAX_PROCESS];

static uint64_t next_pid = 1;

static Process* current_process = nullptr;

static Process* find_free()
{
    for (size_t i = 0; i < MAX_PROCESS; ++i)
    {
        if (processes[i].state == State::EMPTY)
            return &processes[i];
    }

    return nullptr;
}

static Process* find_by_pid(uint64_t pid)
{
    if (pid == 0)
        return nullptr;

    for (size_t i = 0; i < MAX_PROCESS; ++i)
    {
        if (processes[i].state != State::EMPTY &&
            processes[i].pid == pid)
        {
            return &processes[i];
        }
    }

    return nullptr;
}

static void process_stub(void* arg)
{
    Process* proc =
        static_cast<Process*>(arg);

    if (!proc)
        return;

    current_process = proc;

    if (proc->state != State::READY &&
        proc->state != State::RUNNING)
    {
        current_process = nullptr;
        return;
    }

    proc->state = State::RUNNING;

    if (proc->entry != 0)
    {

    }

    if (proc->state == State::RUNNING)
        proc->state = State::TERMINATED;

    if (current_process == proc)
        current_process = nullptr;
}

void init()
{
    memset(
        processes,
        0,
        sizeof(processes)
    );

    for (size_t i = 0; i < MAX_PROCESS; ++i)
    {
        processes[i].state = State::EMPTY;
    }

    next_pid = 1;
    current_process = nullptr;
}

Process* create(
    const void* elf,
    size_t size
)
{
    if (!elf || size == 0)
        return nullptr;

    Process* proc = find_free();

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
        proc->state = State::EMPTY;
        return nullptr;
    }

    memset(
        &proc->context,
        0,
        sizeof(proc->context)
    );

    proc->pid = next_pid++;
    proc->pml4 = pml4;
    proc->entry = entry;
    proc->stack = 0;

    task::init_context(
        &proc->context,
        reinterpret_cast<uint64_t>(
            process_stub
        )
    );

    proc->state = State::READY;

    int task_id =
        scheduler::create_task(
            process_stub,
            proc
        );

    if (task_id < 0)
    {
        proc->state = State::EMPTY;
        proc->pid = 0;
        proc->pml4 = 0;
        proc->entry = 0;
        proc->stack = 0;
        return nullptr;
    }

    proc->task_id =
        static_cast<uint64_t>(task_id);

    return proc;
}

bool terminate(
    Process* proc
)
{
    if (!proc)
        return false;

    if (proc->state == State::EMPTY)
        return false;

    if (proc->task_id != 0)
    {
        scheduler::set_finished(
            proc->task_id
        );
    }

    proc->state = State::TERMINATED;

    if (current_process == proc)
        current_process = nullptr;

    return true;
}

Process* current()
{
    return current_process;
}

Process* get(uint64_t pid)
{
    return find_by_pid(pid);
}

size_t count()
{
    size_t result = 0;

    for (size_t i = 0; i < MAX_PROCESS; ++i)
    {
        if (processes[i].state != State::EMPTY)
            ++result;
    }

    return result;
}

}
