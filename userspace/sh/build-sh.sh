#!/bin/sh
# Builds the minimal static /bin/sh that the kernel loads from the initramfs.
# Requires a host gcc (x86-64 ELF output); no libc is used.
set -eu
CC=${CC:-gcc}
"$CC" -ffreestanding -fno-pie -fno-stack-protector -nostdlib -static -no-pie -O2 \
  -Wall -Wextra -Wno-unused-parameter \
  userspace/crt/crt0.S userspace/sh/sh.c -Wl,-e,_start -o rootfs/bin/sh
echo "built rootfs/bin/sh"
