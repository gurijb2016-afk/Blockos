#!/bin/sh
# Update build_and_run.sh: persist files from persistent/ directory into disk image before boot
# build_and_run.sh - builds the EFI app, creates a FAT disk image and boots it in QEMU with OVMF

set -e

MAKE=make
BUILD_DIR=build
EFI_FILE=${BUILD_DIR}/BOOTX64.EFI
DISK_IMG=disk.img
DATA_IMG=data.img
FS_IMG=fat32_test.img
IMG_SIZE_MB=16
FS_IMG_SIZE_MB=64
PERSIST_DIR=persistent

echo "Building..."
$MAKE

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

# Write the data image once for persistent storage.
DATA_BYTES=$((IMG_SIZE_MB * 1024 * 1024))

if [ ! -f "${DATA_IMG}" ] || [ $(wc -c < "${DATA_IMG}") -ne ${DATA_BYTES} ]; then
  echo "Creating ${DATA_IMG} (${IMG_SIZE_MB}MB) ..."
  rm -f "${DATA_IMG}"
  dd if=/dev/zero of=${DATA_IMG} bs=1M count=${IMG_SIZE_MB}
fi

# Write the fat32 test image
if [ ! -f "${FS_IMG}" ]; then
  echo "Creating ${FS_IMG} (${FS_IMG_SIZE_MB}MB, FAT32) ..."
  dd if=/dev/zero of=${FS_IMG} bs=1M count=${FS_IMG_SIZE_MB}
  mkfs.fat -F 32 ${FS_IMG}
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

# index selects the IDE slot: 0 is primary master (the ESP we boot from), 1 is
# primary slave, which is what the ata-read/ata-write commands talk to.
qemu-system-x86_64 -bios "$OVMF" \
  -drive file=${DISK_IMG},format=raw,if=ide,index=0 \
  -drive file=${DATA_IMG},format=raw,if=ide,index=1 \
  -drive file=${FS_IMG},format=raw,if=ide,index=2 \
  -m 1024 -device isa-debug-exit -boot order=d ${USB_OPTS}
