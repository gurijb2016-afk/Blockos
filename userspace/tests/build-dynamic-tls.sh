#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
CC=${CC:-x86_64-elf-gcc}
OUT=${OUT:-$ROOT/userspace/tests/dynamic-tls}
LIB=$ROOT/userspace/libc/build
"$CC" -std=c11 -O2 -fPIE -pie -ffreestanding -fno-stack-protector \
  -I"$ROOT/userspace/libc/include" \
  "$ROOT/userspace/tests/dynamic_tls.c" "$LIB/crt1.o" \
  -L"$LIB" -lpthread -lc \
  -Wl,-dynamic-linker,/system/lib/ld.so -Wl,-rpath,/system/lib \
  -o "$OUT"
printf '%s\n' "$OUT"
