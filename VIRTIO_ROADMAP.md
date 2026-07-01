# Virtio drivers roadmap and notes

This file documents the current scaffold for virtio-input and virtio-blk and the planned next steps.

Current status
- virtio_input.cpp/.hpp: stub implementation. Kernel falls back to PS/2 input when virtio input is unavailable.
- No functional virtio-blk driver yet. Persistence currently in-memory only.

Planned work (next iterations)
1) Implement virtio-pci probe and configuration space helper (scan PCI bus for vendor 0x1AF4). Create virtio_pci.* that: 
   - maps PCI BARs
   - reads virtio device features
   - initializes device (ACK/DRIVER/OK)
   - sets up virtqueues in DMA-capable memory
2) Implement virtio-input driver using virtio-1.0 event protocol (or legacy if needed) to receive input events from QEMU.
3) Implement virtio-blk driver to send requests (read/write) to the block device and provide a block read/write API for the kernel's VFS persistence.
4) Integrate virtio-blk persistence in vfs::write_file so saved files are persisted to the block device (FAT or raw simple container); optionally implement simple FAT write or use a known sector layout.

Notes
- Implementing virtio properly requires careful memory layout for virtqueues and use of PCI DMAable memory. For a freestanding kernel we will allocate physically contiguous memory from UEFI pools and use that as the descriptor/ring area.
- Testing: QEMU supports virtio-pci for both input and block. For virtio-blk use: -device virtio-blk-pci,drive=... and for input use: -device virtio-input-pci or provide virtio-serial + virtio-input in QEMU command line.

