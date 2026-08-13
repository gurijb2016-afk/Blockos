# BlockOS Strong POSIX (C++)

A C++17/freestanding-friendly POSIX foundation designed around a **BlockOS-owned syscall ABI**, not Linux syscalls.

## Design

`POSIX API -> BlockOS C++ runtime -> BlockOS syscall ABI -> BlockOS kernel`

The syscall numbers and semantics in `include/internal/syscall.h` are BlockOS-owned examples and must be kept in sync with the real BlockOS kernel.

## Included

- file descriptors: open/read/write/close/dup/dup2/fcntl
- filesystem: stat/fstat/mkdir/unlink/rename/chdir/getcwd
- processes: fork/execve/waitpid/getpid/getppid/_exit
- pipes and sleep
- signal API and kill
- mmap/munmap/brk-style memory bridge
- environment API (`environ`, `getenv`, `setenv`, `unsetenv`)
- C++ string/memory helpers kept out of the kernel ABI
- kernel-side syscall dispatch declarations
- x86-64 BlockOS syscall entry stub using a **BlockOS-owned register ABI**
- errno translation
- status macros

This is intentionally **not claimed to be POSIX-certified**. It is a strong architectural base that can be expanded toward broad source compatibility.
