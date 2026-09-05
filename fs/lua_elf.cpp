
#include "ramfs.h"

#include <stdint.h>

extern "C" const uint8_t __lua_elf_start[];
extern "C" const uint8_t __lua_elf_end[];

asm(
    ".section .rodata\n"
    ".balign 16\n"
    ".global __lua_elf_start\n"
    "__lua_elf_start:\n"
    ".incbin \"ports/lua/build/lua\"\n"
    ".global __lua_elf_end\n"
    "__lua_elf_end:\n"
    ".previous\n"
);

extern "C"
const struct ramfile __ramfs_lua[]
    __attribute__((section(".ramfs"), used, aligned(8))) =
{
    {
        "/bin/lua",
        __lua_elf_start,
        (uint32_t)(__lua_elf_end - __lua_elf_start)
    }
};
