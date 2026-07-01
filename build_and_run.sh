# Update build_and_run.sh: persist files from persistent/ directory into disk image before boot
#!/bin/sh
# build_and_run.sh - builds the EFI app, creates a FAT disk image and boots it in QEMU with OVMF

set -e

MAKE=make
BUILD_DIR=build
EFI_FILE=${BUILD_DIR}/BOOTX64.EFI
DISK_IMG=disk.img
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

# QEMU command
OVMF=/usr/share/ovmf/OVMF_CODE.fd
if [ ! -f "$OVMF" ]; then
  echo "OVMF firmware not found at $OVMF. Modify the script to point to your OVMF file." >&2
  exit 1
fi

# If first argument is 'usb', add USB tablet/mouse device options (experimental)
USB_OPTS=""
if [ "$1" = "usb" ]; then
  USB_OPTS="-device usb-ehci,id=ehci -device usb-tablet"
fi

qemu-system-x86_64 -bios "$OVMF" -drive file=${DISK_IMG},format=raw -m 1024 -device isa-debug-exit -boot order=d ${USB_OPTS}
