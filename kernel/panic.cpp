
#include <stdint.h>
#include <stddef.h>

extern "C" void blockos_terminal_write(const char* str);
extern "C" void blockos_terminal_write_hex(uint64_t val);

static void panic_write(const char* s)
{
    if (s)
        blockos_terminal_write(s);
}

extern "C" [[noreturn]]
void handle_kernel_panic_real_reason(uint64_t rip, uint64_t rsp)
{
    panic_write("\n\n*** BlockOS KERNEL PANIC ***\n");

    panic_write("RIP = 0x");
    blockos_terminal_write_hex(rip);

    panic_write("\nRSP = 0x");
    blockos_terminal_write_hex(rsp);

    panic_write("\nCPU halted.\n");

    __asm__ __volatile__("cli" ::: "memory");

    for (;;) {
        __asm__ __volatile__("hlt" ::: "memory");
    }
}
