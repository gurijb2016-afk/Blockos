#!/bin/sh
# Builds the BlockOS installer X11 client against userspace/libc (static, like the other rootfs tools).
# Usage (from the scaffold root): LIBC_DIR=rootfs/system/lib OUT_DIR=rootfs/bin sh userspace/installer/build-installer.sh
set -eu
CC=${CC:-gcc}
LIBC_DIR=${LIBC_DIR:-rootfs/system/lib}
OUT_DIR=${OUT_DIR:-rootfs/bin}
GCC_INC=$("$CC" -print-file-name=include)
LIBGCC=$("$CC" -print-libgcc-file-name)
"$CC" -std=gnu11 -O2 -Wall -ffreestanding -fno-stack-protector -fno-pie -no-pie -nostdinc \
  -isystem userspace/libc/include -isystem "$GCC_INC" -Iuserspace/installer \
  -static -nostdlib "$LIBC_DIR/crt1.o" userspace/installer/blockos_installer.c userspace/installer/disk_install.c \
  "$LIBC_DIR/libc.a" "$LIBGCC" -o "$OUT_DIR/blockos-installer"
echo "built $OUT_DIR/blockos-installer"
