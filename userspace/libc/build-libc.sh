#!/bin/sh
# BlockOS libc build.
# Static first: libc.a (+ crt1.o) is the supported output for BlockOS userland.
# libc.so / libpthread.so are best-effort: several sources still define the same
# symbols (pwd/pwdgrp, stat/stat_extra, uname, ...), so the shared link may fail.
set -eu
CC=${CC:-${BLOCKOS_CC:-x86_64-elf-gcc}}
AS=${AS:-${BLOCKOS_AS:-x86_64-elf-as}}
AR=${AR:-${BLOCKOS_AR:-x86_64-elf-ar}}
OUTDIR=${OUTDIR:-userspace/libc/build}
SRC=userspace/libc/src
INC=userspace/libc/include
mkdir -p "$OUTDIR"
CFLAGS="-std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fPIC -fno-plt -I$INC -Iuserspace/ldso ${BLOCKOS_CFLAGS:-}"
rm -f "$OUTDIR"/*.o "$OUTDIR/libc.a" "$OUTDIR/libc.so" "$OUTDIR/libpthread.so"
"$AS" ${BLOCKOS_ASFLAGS:-} --64 -o "$OUTDIR/crt1.o" "$SRC/crt1.S"
"$AS" ${BLOCKOS_ASFLAGS:-} --64 -o "$OUTDIR/pthread_clone.o" "$SRC/pthread_clone.S"

LIBC_OBJS=""
for f in "$SRC"/*.c; do
    base=$(basename "$f" .c)
    case "$base" in
        pthread|tls) continue ;;
    esac
    "$CC" $CFLAGS -c "$f" -o "$OUTDIR/$base.o"
    LIBC_OBJS="$LIBC_OBJS $OUTDIR/$base.o"
done
"$CC" $CFLAGS -c "$SRC/tls.c" -o "$OUTDIR/tls.o"
"$CC" $CFLAGS -c "$SRC/pthread.c" -o "$OUTDIR/pthread.o"
LIBC_OBJS="$LIBC_OBJS $OUTDIR/tls.o $OUTDIR/pthread.o $OUTDIR/pthread_clone.o"

"$AR" rcs "$OUTDIR/libc.a" $LIBC_OBJS
echo "built $OUTDIR/libc.a"

if "$CC" -shared -nostdlib -nodefaultlibs -fPIC -Wl,-soname,libc.so -Wl,-z,norelro \
        -o "$OUTDIR/libc.so" $LIBC_OBJS 2>/dev/null; then
    echo "built $OUTDIR/libc.so"
else
    echo "note: libc.so skipped (duplicate symbols; static libc.a is complete)" >&2
    rm -f "$OUTDIR/libc.so"
fi
