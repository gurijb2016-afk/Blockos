# BlockOS: real Lua + ELF64 + ring3 + syscall integration

This patch replaces the previous placeholder process path with a real x86-64 privilege transition:

* ELF64 loader validates headers/program-header bounds and rejects PT_INTERP.
* User pages are mapped with U/S permissions and segment W/NX permissions.
* A cloned kernel PML4 is used for the process address space.
* A user stack is allocated and mapped.
* GDT contains kernel/user code+data and a TSS with RSP0.
* IDT vector 0x80 is an interrupt gate with DPL3.
* `int 0x80` saves all GPRs, dispatches, restores them, and `iretq`s.
* `exit` edits the saved frame so control returns to the kernel continuation.
* `read`, `write`, `openat`, `close`, `lseek`, `fstat`, `mmap`, `brk`, `clock_gettime`, `getpid`, and `exit` have a real kernel path.
* The shell resolves `lua` to `/bin/lua` in the embedded VFS.
* `fs/lua_elf.cpp` embeds the actual Lua executable produced by `ports/lua/build-lua.sh`.

## Build

The kernel still requires GNU-EFI exactly as the original project does. The Lua executable additionally requires a freestanding BlockOS userspace libc sysroot and an x86_64-elf toolchain. Set `USERLIBC=/path/to/sysroot` before building Lua.

`ports/lua/build-lua.sh` downloads official Lua 5.4.9 and refuses to build a Linux/glibc executable. The resulting binary is embedded into the BlockOS ramfs.

## Important

The current assistant environment does not contain GNU-EFI, so the complete UEFI image cannot be truthfully certified here. The changed C++ and assembly files were syntax/assembly checked independently; final `make` and QEMU execution must be performed on the BlockOS build host.
