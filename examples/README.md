# BlockOS examples

This directory mirrors the spirit of the Linux kernel `samples/` tree, but every
example is written for the BlockOS interfaces present in this repository.

The examples are intentionally small. They demonstrate how BlockOS kernel and
userspace facilities are consumed without pretending that Linux kernel APIs are
available.

## Included areas

- `hello/` — minimal BlockOS POSIX userspace program
- `threads/` — pthreads example
- `sockets/` — BlockOS POSIX socket example
- `files/` — BlockOS file I/O example
- `device_manager/` — hardware registration and lookup
- `pci/` — PCI configuration-space discovery
- `virtio_blk/` — block-device initialization and sector I/O
- `virtio_net/` — packet TX/RX demonstration
- `virtio_input/` — VirtIO input event polling
- `ps2/` — PS/2 keyboard and mouse polling
- `framebuffer/` — framebuffer drawing API
- `events/` — BlockOS event queue consumption
- `vfs/` — VFS file listing/reading/writing
- `elf_loader/` — loading an ELF64 image from memory
- `io_uring/` — BlockOS io_uring submission/completion flow
- `drivers/` — kernel-driver integration notes and a minimal driver skeleton

These examples are references, not drop-in production drivers. Hardware access
must follow the BlockOS kernel's existing locking, allocator, IRQ, DMA and VFS
rules.
