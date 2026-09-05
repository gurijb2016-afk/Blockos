#include "ramfs.h"
extern "C" const uint8_t __lua_elf_start[];
extern "C" const uint8_t __lua_elf_end[];
asm(".section .rodata\n.global __lua_elf_start\n__lua_elf_start:\n.incbin \"ports/lua/build/lua\"\n.global __lua_elf_end\n__lua_elf_end:");
extern "C" const struct ramfile __ramfs_lua[] __attribute__((section(".ramfs"))) = { {"/bin/lua", __lua_elf_start, (uint32_t)(__lua_elf_end-__lua_elf_start)} };
extern "C" const struct ramfile __ramfs_end[] __attribute__((section(".ramfs"))) = { };
