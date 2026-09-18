#!/bin/sh
set -eu
CC=${CC:-x86_64-elf-gcc}
OUT=${OUT:-userspace/ldso/ld.so}

mkdir -p "$(dirname "$OUT")"

"$CC" \
    -std=c11 -O2 \
    -ffreestanding -fPIE -fno-plt -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-unwind-tables \
    -fno-builtin -nostdlib -nostartfiles \
    -static-pie \
    -Wl,-e,_start \
    -Wl,-z,norelro \
    -Iuserspace/ldso \
    userspace/ldso/start.S userspace/ldso/ldso.c \
    -o "$OUT"

printf '%s\n' "$OUT"
