#pragma once
#include <stdint.h>
#include <stddef.h>
#include "task_context.hpp"
namespace process {
enum class State:uint8_t{EMPTY=0,READY,RUNNING,BLOCKED,TERMINATED};
struct Process{uint64_t pid,pml4,entry,stack,task_id,brk_base,brk_current,mmap_next;State state;TaskContext context;};
void init();
Process* create(const void* elf,size_t size);
int run(Process* proc);
bool terminate(Process* proc);
Process* current(); Process* get(uint64_t pid); size_t count();
}
