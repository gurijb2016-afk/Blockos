#!/bin/sh
# Update build_and_run.sh: persist files from persistent/ directory into disk image before boot
# build_and_run.sh - builds the EFI app, creates a FAT disk image and boots it in QEMU with OVMF

set -e

MAKE=make
BUILD_DIR=build
EFI_FILE=${BUILD_DIR}/BOOTX64.EFI
DISK_IMG=disk.img
DATA_IMG=data.img
IMG_SIZE_MB=16
PERSIST_DIR=persistent

if [ ! -f "$EFI_FILE" ]; then
  echo "Building..."
  $MAKE
fi

echo "Creating ${DISK_IMG} (${IMG_SIZE_MB}MB) ..."
rm -f ${DISK_IMG}
dd if=/dev/zero of=${DISK_IMG} bs=1M count=${IMG_SIZE_MB}
mkfs.vfat -n UEFI ${DISK_IMG}

# create EFI dir and copy
mmd -i ${DISK_IMG} ::/EFI || true
mmd -i ${DISK_IMG} ::/EFI/BOOT || true
mcopy -i ${DISK_IMG} ${EFI_FILE} ::/EFI/BOOT/BOOTX64.EFI

# If persistent directory exists, copy its files into the disk image root
if [ -d "$PERSIST_DIR" ]; then
  for f in "$PERSIST_DIR"/*; do
    if [ -f "$f" ]; then
      echo "Copying persistent file: $f"
      mcopy -i ${DISK_IMG} "$f" ::/
    fi
  done
fi

# Write the data image once for persistent storage
if [ ! -f "${DATA_IMG}" ]; then
  echo "Creating "${DATA_IMG}" of size "${IMG_SIZE}"..."
  dd if=/dev/zero of=${DATA_IMG} bs=1M count=${IMG_SIZE_MB}
fi

# QEMU firmware. OVMF is the UEFI implementation QEMU needs to boot an EFI
# application at all; the default SeaBIOS is legacy-BIOS only. Distributions
# disagree on where it lives, and newer ones split it into separate code/vars
# images, so probe common locations rather than hardcoding one. Combined
# images come first because this script uses -bios; the split CODE halves are
# really meant for the pflash pair, but work as a fallback.
if [ -z "$OVMF" ]; then
  for candidate in \
    /usr/share/ovmf/OVMF.fd \
    /usr/share/qemu/OVMF.fd \
    /usr/share/OVMF/OVMF.fd \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/ovmf/OVMF_CODE.fd \
    /usr/share/edk2/x64/OVMF_CODE.fd \
    /usr/share/edk2-ovmf/x64/OVMF_CODE.fd
  do
    if [ -f "$candidate" ]; then OVMF="$candidate"; break; fi
  done
fi

if [ -z "$OVMF" ] || [ ! -f "$OVMF" ]; then
  echo "OVMF firmware not found. Install it (Debian/Ubuntu: apt install ovmf)," >&2
  echo "or run with OVMF=/path/to/OVMF.fd $0" >&2
  exit 1
fi
echo "Using OVMF firmware: $OVMF"

# If first argument is 'usb', add USB tablet/mouse device options (experimental)
USB_OPTS=""
if [ "$1" = "usb" ]; then
  USB_OPTS="-device usb-ehci,id=ehci -device usb-tablet"
fi

qemu-system-x86_64 -bios "$OVMF" -drive file=${DISK_IMG},format=raw -m 1024 -device isa-debug-exit -boot order=d ${USB_OPTS}
