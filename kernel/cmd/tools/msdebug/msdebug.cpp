#include "msdebug.hpp"

namespace msdebug
{
    static Registers regs = {};

    void init()
    {
        regs = {};
    }

    void show_registers()
    {
        // BlockOS register output will be connected here.
    }

    void dump_memory(uint64_t address, uint64_t length)
    {
        volatile uint8_t* memory =
            (volatile uint8_t*)address;

        for (uint64_t i = 0; i < length; ++i)
        {
            volatile uint8_t value = memory[i];
            (void)value;
        }
    }

    bool set_breakpoint(uint64_t address)
    {
        volatile uint8_t* code =
            (volatile uint8_t*)address;

        *code = 0xCC;
        return true;
    }

    bool clear_breakpoint(uint64_t address)
    {
        (void)address;
        return true;
    }

    void step()
    {
