#pragma once

#include <stdint.h>
#include <stddef.h>
#include "task_context.hpp"

namespace process
{

enum class State : uint8_t
{
    EMPTY = 0,
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
};

struct Process
{
    uint64_t pid;

    uint64_t pml4;
    uint64_t entry;
    uint64_t stack;

    uint64_t task_id;

    State state;

    TaskContext context;
};

void init();

Process* create(
    const void* elf,
    size_t size
);

bool terminate(
    Process* proc
);

Process* current();

Process* get(
    uint64_t pid
);

size_t count();

}
