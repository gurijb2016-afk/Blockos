#!/bin/sh
# build_and_run_iso.sh - Create a UEFI-bootable ISO with the BlockOS EFI app and run in QEMU

set -e

BUILD_DIR=build
EFI_FILE=${BUILD_DIR}/BOOTX64.EFI
ISO_IMG=blockos.iso
ISO_ROOT=iso_root

echo "=== Building BlockOS ==="
make -j$(nproc)

echo ""
echo "=== Preparing ISO root ==="
rm -rf ${ISO_ROOT}
mkdir -p ${ISO_ROOT}/EFI/BOOT
cp ${EFI_FILE} ${ISO_ROOT}/EFI/BOOT/BOOTX64.EFI

echo ""
echo "=== Creating UEFI-bootable ISO ==="
rm -f ${ISO_IMG}

xorriso -as mkisofs \
  -R -r -V "BLOCKOS" \
  -o ${ISO_IMG} \
  -b EFI/BOOT/BOOTX64.EFI \
  -no-emul-boot \
  -eltorito-alt-boot \
  -e EFI/BOOT/BOOTX64.EFI \
  -no-emul-boot \
  -isohybrid-gpt-basdat \
  ${ISO_ROOT}

rm -rf ${ISO_ROOT}

echo ""
echo "=== ISO created: ${ISO_IMG} ==="
ls -lh ${ISO_IMG}

echo ""
echo "=== Booting in QEMU ==="

# Find OVMF firmware
OVMF=""
for candidate in \
  /usr/share/edk2/x64/OVMF.4m.fd \
  /usr/share/ovmf/OVMF.fd \
  /usr/share/qemu/OVMF.fd \
  /usr/share/OVMF/OVMF.fd \
  /usr/share/OVMF/OVMF_CODE_4M.fd \
  /usr/share/OVMF/OVMF_CODE.fd \
  /usr/share/edk2/x64/OVMF_CODE.4m.fd \
  /usr/share/edk2/x64/OVMF_CODE.fd
do
  if [ -f "$candidate" ]; then OVMF="$candidate"; break; fi
done

if [ -z "$OVMF" ] || [ ! -f "$OVMF" ]; then
  echo "OVMF firmware not found. Install it or set OVMF=/path/to/OVMF.fd" >&2
  exit 1
fi
echo "Using OVMF: $OVMF"

qemu-system-x86_64 \
  -bios "$OVMF" \
  -cdrom ${ISO_IMG} \
  -m 1024 \
  -device isa-debug-exit \
  -display gtk \
  -boot d
