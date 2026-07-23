#pragma once

#include <stdint.h>
#include <stddef.h>
#include "task_context.hpp"

namespace process {

enum class State {
    EMPTY,
    READY,
    RUNNING,
    TERMINATED
};

struct Process {
    uint64_t pid;

    uint64_t pml4;
    uint64_t entry;
    uint64_t stack;

    State state;

    TaskContext context;
};


void init();

Process* create(
    const void* elf,
    size_t size
);

void terminate(Process* proc);

Process* current();

}
