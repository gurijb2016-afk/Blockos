#!/bin/bash
# scripts/migrate_structure.sh
# Ez a szkript átmozgatja a régi zip-ből származó src/ fájlokat az új moduláris struktúrába.

echo "[BlockOS] Új könyvtárszerkezet létrehozása..."
mkdir -p arch/86_64x
mkdir -p arch/fs/targets/ext4
mkdir -p kernel
mkdir -p drivers
mkdir -p fs
mkdir -p filesystem
mkdir -p examples

echo "[BlockOS] Fájlok átmozgatása a feladatuk szerint..."

# 1. ARCHITEKTÚRA (arch/)
mv src/context_switch.S arch/86_64x/ 2>/dev/null
mv src/irq_stubs.S arch/86_64x/ 2>/dev/null
mv src/idt.cpp src/idt.hpp arch/86_64x/ 2>/dev/null
mv src/irq.cpp src/irq.hpp arch/86_64x/ 2>/dev/null
mv src/paging.cpp src/paging.hpp arch/86_64x/ 2>/dev/null
mv src/hardware_tables.cpp src/hardware_tables.hpp arch/86_64x/ 2>/dev/null
mv src/pml4_runner.cpp src/pml4_runner.hpp arch/86_64x/ 2>/dev/null

# 2. KERNEL MAG (kernel/)
mv src/kernel.cpp src/kernel.export kernel/ 2>/dev/null
mv src/scheduler.cpp src/scheduler.hpp kernel/ 2>/dev/null
mv src/scheduler_preempt.cpp src/scheduler_preempt.hpp kernel/ 2>/dev/null
mv src/allocator.cpp src/allocator.hpp kernel/ 2>/dev/null
mv src/elf_loader.cpp src/elf_loader.hpp kernel/ 2>/dev/null
mv src/systemd_graph.cpp src/systemd_graph.hpp kernel/ 2>/dev/null
mv src/systemd_parser.cpp src/systemd_parser.hpp kernel/ 2>/dev/null
mv src/events.cpp src/events.hpp kernel/ 2>/dev/null

# 3. HARDVERMEGHAJTÓK (drivers/)
mv src/virtio_* drivers/ 2>/dev/null
mv src/virtqueue_ops.cpp src/virtqueue_ops.hpp drivers/ 2>/dev/null
mv src/pci.cpp src/pci.hpp drivers/ 2>/dev/null
mv src/pci_config.cpp src/pci_config.hpp drivers/ 2>/dev/null
mv src/pci_msix.cpp src/pci_msix.hpp drivers/ 2>/dev/null
mv src/pci_subsystem.cpp src/pci_subsystem.hpp drivers/ 2>/dev/null
mv src/ps2keyboard.cpp src/ps2keyboard.hpp drivers/ 2>/dev/null
mv src/ps2mouse.cpp src/ps2mouse.h src/ps2mouse.hpp drivers/ 2>/dev/null
mv src/device_manager.cpp src/device_manager.hpp drivers/ 2>/dev/null
mv src/dma.cpp src/dma.hpp drivers/ 2>/dev/null
mv src/fb.cpp src/fb.h drivers/ 2>/dev/null
mv src/backbuffer.cpp src/backbuffer.h drivers/ 2>/dev/null

# 4. ÁLTALÁNOS VIRTUÁLIS FÁJLRENDSZER (fs/)
mv src/vfs.cpp src/vfs.hpp fs/ 2>/dev/null
mv src/vfs_blk_adapter.cpp src/vfs_blk_adapter.hpp fs/ 2>/dev/null
mv src/files.c src/files.cpp fs/ 2>/dev/null
mv src/mount.cpp src/mount.hpp fs/ 2>/dev/null
mv src/fat32.cpp src/fat32.hpp fs/ 2>/dev/null
mv src/ramfs.cpp src/ramfs.h src/ramfs.hpp fs/ 2>/dev/null
mv src/filesystem.export fs/ 2>/dev/null

# 5. AZ ÚJ EXT4 TARGET (arch/fs/targets/ext4/)
mv src/ext4.cpp src/ext4.hpp arch/fs/targets/ext4/ 2>/dev/null

# 6. APPLIKÁCIÓK / PÉLDÁK (examples/)
mv src/shell.cpp src/shell.hpp examples/ 2>/dev/null
mv src/browser.cpp src/browser.hpp examples/ 2>/dev/null
mv src/settings.cpp src/settings.hpp examples/ 2>/dev/null
mv src/snake.cpp src/snake.hpp examples/ 2>/dev/null

# 7. HÁLÓZATI STACK KIEGÉSZÍTÉS (drivers/)
mv src/network.cpp src/network.hpp drivers/ 2>/dev/null
mv src/dhcp_dns_stack.cpp src/dhcp_dns_stack.hpp drivers/ 2>/dev/null
mv src/lwip_adapter.cpp src/lwip_adapter.hpp drivers/ 2>/dev/null

# Maradék takarítása
rm -rf src/

echo "[SUCCESS] A BlockOS struktúra sikeresen frissítve!"
