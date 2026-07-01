# Blockos — UEFI -> ExitBootServices kernel scaffold

This branch contains a minimal x86_64 UEFI-based kernel scaffold that:

- Locates the UEFI Graphics Output Protocol (GOP)
- Obtains the memory map
- Calls ExitBootServices
- Draws directly to the framebuffer (simple rectangles)
- Basic PS/2 mouse polling driver (polls IO ports after ExitBootServices)
- Simple in-memory ramfs with embedded files
- Movable window driven by mouse drag

Purpose: a small starting point for moving from UEFI boot services to a freestanding kernel body and
providing a crude GUI and filesystem-like access to embedded files.

Build notes:
- The Makefile is written for a typical Debian/Ubuntu system with gnu-efi installed (sudo apt install gnu-efi build-essential qemu-system-x86_64 ovmf dosfstools mtools).
- You may need to adjust include/ld paths depending on your distribution.

Usage (quick):
- make
- Copy build/BOOTX64.EFI into a FAT image and boot with QEMU+OVMF (see Makefile comments)

Notes & limitations:
- The PS/2 mouse code uses direct I/O port access (inb/outb). QEMU will emulate a PS/2 mouse, but on
  real modern hardware the mouse is often USB — a full USB/HID driver would be needed for real machines.
- The "filesystem" is a tiny embedded ramfs (files are compiled into files.c). This avoids needing
  block device drivers in the scaffold.
- Text rendering uses a small 8x8 bitmap font (font8x8.h).
- This is a demo scaffold, not a production-quality kernel.

