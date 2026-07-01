# Blockos — UEFI -> ExitBootServices kernel scaffold

This branch contains a minimal x86_64 UEFI-based kernel scaffold that:

- Locates the UEFI Graphics Output Protocol (GOP)
- Obtains the memory map
- Calls ExitBootServices
- Draws directly to the framebuffer (simple rectangles)

Purpose: a small starting point for moving from UEFI boot services to a freestanding kernel body and a rudimentary framebuffer GUI.

Build notes:
- The Makefile is written for a typical Debian/Ubuntu system with gnu-efi installed (sudo apt install gnu-efi build-essential qemu-system-x86_64 ovmf dosfstools).
- You may need to adjust include/ld paths depending on your distribution.

Usage (quick):
- make
- Copy build/BOOTX64.EFI into a FAT image and boot with QEMU+OVMF (see Makefile comments)


---
