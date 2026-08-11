#pragma once

#include <stdint.h>

namespace msdebug
{
    struct Registers
    {
        uint64_t rax;
        uint64_t rbx;
        uint64_t rcx;
        uint64_t rdx;
        uint64_t rsi;
        uint64_t rdi;
        uint64_t rbp;
        uint64_t rsp;
        uint64_t r8;
        uint64_t r9;
        uint64_t r10;
        uint64_t r11;
        uint64_t r12;
        uint64_t r13;
        uint64_t r14;
        uint64_t r15;
        uint64_t rip;
        uint64_t rflags;
    };

    void init();
    void shell();
    void show_registers();
    void dump_memory(uint64_t address, uint64_t length);
    bool set_breakpoint(uint64_t address);
    bool clear_breakpoint(uint64_t address);
    void step();
    void continue_execution();
}
