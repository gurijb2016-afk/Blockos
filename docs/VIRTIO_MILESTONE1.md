Virtio Infra — Milestone 1 Completed

What this commit includes (milestone 1)
- PCI config helpers: read/write 8/16/32, BAR reading, pci_device_exists.
- virtio_common: probe that detects virtio devices, determines BAR0 address and whether IO or MMIO, and basic device_init with legacy status handshake and guest page size setup.
- MMIO / IO register access helpers for virtio devices.
- dma allocator wrapper (dma::alloc) for drivers to allocate DMA-capable buffers.
- virtqueue structures and alloc helper (VirtqDesc, VirtqAvail, VirtqUsed, alloc_virtqueue).
- virtqueue_ops: view_from_mem, init_rings, set_descriptor, submit_descriptor, try_dequeue_used.
- virtio_input: legacy best-effort queue programming, submit receive buffers, poll used ring and deliver bytes via read_byte_nonblocking() FIFO.
- virtio_blk skeleton: allocates queue memory; read/write TODOs remain.

How to test (QEMU)
- Boot the kernel under QEMU and check UEFI console output for virtio probe messages and DMA allocations:
  qemu-system-x86_64 -bios /usr/share/ovmf/OVMF_CODE.fd -drive file=disk.img,format=raw -device virtio-input-pci -m 1024

Notes & next steps
- Next: implement full virtqueue programming (notify, features) for input, then implement virtio-blk request/response and integrate with VFS persistence (raw-sector container).
- Modern virtio (1.0) support and MSI-X notify will be addressed in later milestones; current code focuses on legacy-compatible init and a conservative, QEMU-friendly path.

Files added/updated in this milestone
- src/pci.hpp / src/pci.cpp
- src/virtio_common.hpp / src/virtio_common.cpp
- src/dma.hpp / src/dma.cpp
- src/virtqueue.hpp / src/virtqueue.cpp
- src/virtqueue_ops.hpp / src/virtqueue_ops.cpp
- src/virtio_input.cpp
- src/virtio_blk.cpp

