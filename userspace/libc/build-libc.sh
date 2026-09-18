#!/bin/sh
set -eu
CC=${CC:-x86_64-elf-gcc}
AS=${AS:-x86_64-elf-as}
AR=${AR:-x86_64-elf-ar}
OUTDIR=${OUTDIR:-userspace/libc/build}
SRC=userspace/libc/src
INC=userspace/libc/include
mkdir -p "$OUTDIR"
CFLAGS="-std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fPIC -fno-plt -I$INC -Iuserspace/ldso"
"$AS" --64 -o "$OUTDIR/crt1.o" "$SRC/crt1.S"
"$AS" --64 -o "$OUTDIR/pthread_clone.o" "$SRC/pthread_clone.S"
rm -f "$OUTDIR"/*.o "$OUTDIR/libc.a" "$OUTDIR/libc.so" "$OUTDIR/libpthread.so"
"$AS" --64 -o "$OUTDIR/crt1.o" "$SRC/crt1.S"
"$AS" --64 -o "$OUTDIR/pthread_clone.o" "$SRC/pthread_clone.S"

LIBC_OBJS=""
for f in "$SRC"/*.c; do
    base=$(basename "$f" .c)
    case "$base" in
        pthread|tls) continue ;;
    esac
    "$CC" $CFLAGS -c "$f" -o "$OUTDIR/$base.o"
    LIBC_OBJS="$LIBC_OBJS $OUTDIR/$base.o"
done

"$AR" rcs "$OUTDIR/libc.a" $LIBC_OBJS

"$CC" -shared -nostdlib -nodefaultlibs -fPIC \
    -Wl,-soname,libc.so -Wl,-z,norelro \
    -o "$OUTDIR/libc.so" $LIBC_OBJS

"$CC" $CFLAGS -c "$SRC/tls.c" -o "$OUTDIR/tls.o"
"$CC" $CFLAGS -c "$SRC/pthread.c" -o "$OUTDIR/pthread.o"
"$CC" -shared -nostdlib -nodefaultlibs -fPIC \
    -Wl,-soname,libpthread.so -Wl,-rpath,/system/lib -Wl,-z,norelro \
    -L"$OUTDIR" -o "$OUTDIR/libpthread.so" \
    "$OUTDIR/pthread.o" "$OUTDIR/pthread_clone.o" "$OUTDIR/tls.o" \
    -lc

printf '%s\n' "$OUTDIR/libc.a" "$OUTDIR/crt1.o" "$OUTDIR/libc.so" "$OUTDIR/libpthread.so"
