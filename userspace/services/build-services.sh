#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
CC=${CC:-x86_64-elf-gcc}
OUTDIR=${OUTDIR:-$ROOT/build/services}
INC=${INC:-$ROOT/userspace/libc/include}
mkdir -p "$OUTDIR"
CFLAGS="-std=c11 -O2 -ffreestanding -fPIC -fno-stack-protector -fno-builtin -I$INC"
"$CC" $CFLAGS "$ROOT/userspace/services/dbusd.c" -L"$ROOT/userspace/libc/build" -Wl,-rpath,/system/lib -Wl,-dynamic-linker,/system/lib/ld.so -lc -o "$OUTDIR/dbusd"
printf '%s\n' "$OUTDIR/dbusd"
