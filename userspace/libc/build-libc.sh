#!/bin/sh
set -eu
CC=${CC:-x86_64-elf-gcc}
AS=${AS:-x86_64-elf-as}
AR=${AR:-x86_64-elf-ar}
OUTDIR=${OUTDIR:-userspace/libc/build}
SRC=userspace/libc/src
INC=userspace/libc/include

mkdir -p "$OUTDIR"

CFLAGS="-std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fpic -I$INC"

"$AS" --64 -o "$OUTDIR/crt1.o" "$SRC/crt1.S"

for f in "$SRC"/*.c; do
    base=$(basename "$f" .c)
    "$CC" $CFLAGS -c "$f" -o "$OUTDIR/$base.o"
done

"$AR" rcs "$OUTDIR/libc.a" "$OUTDIR"/*.o
# crt1.o stays out of the archive - it's linked explicitly, first, like
# any normal crt1.o/crt0.o, not pulled in on demand like a library symbol.
"$AR" d "$OUTDIR/libc.a" crt1.o 2>/dev/null || true

echo "$OUTDIR/libc.a"
echo "$OUTDIR/crt1.o"
