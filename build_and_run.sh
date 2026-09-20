#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"

MAKE=make

BUILD_DIR="$ROOT/build"
EFI_FILE="$BUILD_DIR/BOOTX64.EFI"

DISK_IMG="$ROOT/disk.img"
DATA_IMG="$ROOT/data.img"
FS_IMG="$ROOT/fat32_test.img"

# 256 MiB: a ~19 MB-os BOOTX64.EFI és a további fájlok is elférnek.
IMG_SIZE_MB=256
FS_IMG_SIZE_MB=64

PERSIST_DIR="$ROOT/persistent"

echo "========================================"
echo " BlockOS boot from build/BOOTX64.EFI"
echo "========================================"

echo
echo "[1/7] Building..."

"$MAKE"

if [ ! -f "$EFI_FILE" ]; then
    echo "ERROR: missing:"
    echo "  $EFI_FILE"
    exit 1
fi

echo
echo "EFI:"
ls -lh "$EFI_FILE"

echo
echo "[2/7] Creating ${IMG_SIZE_MB}MB disk.img..."

rm -f "$DISK_IMG"

dd \
    if=/dev/zero \
    of="$DISK_IMG" \
    bs=1M \
    count="$IMG_SIZE_MB" \
    status=progress

mkfs.fat -F 32 -n BLOCKOS "$DISK_IMG"

echo
echo "[3/7] Creating EFI directory..."

mmd -i "$DISK_IMG" ::/EFI
mmd -i "$DISK_IMG" ::/EFI/BOOT

echo
echo "[4/7] Copying BOOTX64.EFI..."

mcopy \
    -i "$DISK_IMG" \
    "$EFI_FILE" \
    ::/EFI/BOOT/BOOTX64.EFI

echo
echo "Checking EFI file..."

mdir -i "$DISK_IMG" ::/EFI/BOOT

echo
echo "[5/7] Copying persistent files..."

if [ -d "$PERSIST_DIR" ]; then
    for f in "$PERSIST_DIR"/*; do
        if [ -f "$f" ]; then
            echo "Copying:"
            echo "  $f"

            mcopy \
                -i "$DISK_IMG" \
                "$f" \
                ::/
        fi
    done
fi

echo
echo "[6/7] Creating additional disks..."

DATA_BYTES=$((IMG_SIZE_MB * 1024 * 1024))

if [ ! -f "$DATA_IMG" ] || \
   [ "$(wc -c < "$DATA_IMG")" -ne "$DATA_BYTES" ]; then

    rm -f "$DATA_IMG"

    dd \
        if=/dev/zero \
        of="$DATA_IMG" \
        bs=1M \
        count="$IMG_SIZE_MB" \
        status=progress
fi

if [ ! -f "$FS_IMG" ]; then

    dd \
        if=/dev/zero \
        of="$FS_IMG" \
        bs=1M \
        count="$FS_IMG_SIZE_MB" \
        status=progress

    mkfs.fat -F 32 "$FS_IMG"
fi

echo
echo "Disk image:"
ls -lh "$DISK_IMG"

echo
echo "[7/7] Finding OVMF..."

OVMF_CODE=""
OVMF_VARS=""

for code in \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/edk2/ovmf/OVMF_CODE.fd \
    /usr/share/edk2/x64/OVMF_CODE.fd
do
    if [ -r "$code" ]; then
        OVMF_CODE="$code"
        break
    fi
done

if [ -z "$OVMF_CODE" ]; then
    echo "ERROR: OVMF CODE not found."
    echo "Install with:"
    echo "  sudo apt install ovmf"
    exit 1
fi

for vars in \
    /usr/share/OVMF/OVMF_VARS_4M.fd \
    /usr/share/OVMF/OVMF_VARS.fd \
    /usr/share/edk2/ovmf/OVMF_VARS.fd \
    /usr/share/edk2/x64/OVMF_VARS.fd
do
    if [ -r "$vars" ]; then
        OVMF_VARS="$vars"
        break
    fi
done

if [ -z "$OVMF_VARS" ]; then
    echo "ERROR: OVMF VARS template not found."
    echo "Install with:"
    echo "  sudo apt install ovmf"
    exit 1
fi

echo
echo "OVMF CODE:"
echo "  $OVMF_CODE"

echo "OVMF VARS:"
echo "  $OVMF_VARS"

# Saját írható VARS példány.
VARS_COPY="$BUILD_DIR/OVMF_VARS.fd"

mkdir -p "$BUILD_DIR"

rm -f "$VARS_COPY"

cp \
    "$OVMF_VARS" \
    "$VARS_COPY"

echo
echo "Writable OVMF VARS:"
echo "  $VARS_COPY"

echo
echo "========================================"
echo " Starting QEMU"
echo "========================================"

exec qemu-system-x86_64 \
    -machine q35 \
    -m 2048 \
    -cpu max \
    -drive "if=pflash,format=raw,unit=0,readonly=on,file=$OVMF_CODE" \
    -drive "if=pflash,format=raw,unit=1,file=$VARS_COPY" \
    -drive "file=$DISK_IMG,format=raw,if=ide,index=0" \
    -drive "file=$DATA_IMG,format=raw,if=ide,index=1" \
    -drive "file=$FS_IMG,format=raw,if=ide,index=2" \
    -device isa-debug-exit \
    -serial stdio \
    -boot order=c \
    -display gtk
