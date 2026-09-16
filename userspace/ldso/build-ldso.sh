#!/bin/sh
set -eu
CC=${CC:-x86_64-elf-gcc}
OUT=${OUT:-userspace/ldso/ld.so}

"$CC" \
    -std=c11 -O1 -fpie -fno-stack-protector -fno-builtin \
    -ffreestanding -nostdlib -static-pie \
    -Wl,-e,_start \
    -Iuserspace/ldso \
    userspace/ldso/start.S userspace/ldso/ldso.c \
    -o "$OUT"

echo "$OUT"
