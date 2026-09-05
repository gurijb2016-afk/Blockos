# Lua on BlockOS

This directory is the real Lua integration point. The kernel side now supports ELF64 user processes, ring-3 entry, `int 0x80`, user memory validation, anonymous `mmap`, `brk`, VFS `openat/read/lseek/fstat/close`, tty read/write, `getpid`, `clock_gettime` and `exit`.

The Lua source is fetched from the official Lua release tarball by `build-lua.sh`; it is not a fake interpreter.

The remaining userspace C-library layer must be supplied by the selected freestanding libc/toolchain. The script intentionally fails instead of silently producing a Linux/glibc binary.
