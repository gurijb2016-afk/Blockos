
#include "vfs.hpp"

#include <stdint.h>
#include <stddef.h>

extern "C"
{
    extern const unsigned char __lua_elf_start[];
    extern const unsigned char __lua_elf_end[];
}

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
{
    const ramfile __ramfs_lua
        __attribute__((section(".ramfs"), used, aligned(8))) =
    {
        "/bin/lua",
        __lua_elf_start,
        static_cast<uint32_t>(
            __lua_elf_end - __lua_elf_start
        )
    };
}

extern "C" const ramfile __ramfs_end[];

asm(
    ".section .ramfs\n"
    ".balign 8\n"
    ".global __ramfs_end\n"
    "__ramfs_end:\n"
    ".previous\n"
);

