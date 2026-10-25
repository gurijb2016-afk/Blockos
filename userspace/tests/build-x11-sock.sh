#!/bin/sh
# Builds the static socket/fork/exec test pair against userspace/libc (libc.a):
#   x11_sock   - parent: socket, bind, listen, accept, poll, fork + exec
#   x11_child  - client, exec'd by x11_sock (installed as /bin/x11_child)
# Usage: LIBC_DIR=<dir with libc.a and crt1.o> OUT_DIR=rootfs/bin sh userspace/tests/build-x11-sock.sh
set -eu
CC=${CC:-gcc}
LIBC_DIR=${LIBC_DIR:-userspace/libc/build}
OUT_DIR=${OUT_DIR:-rootfs/bin}
GCC_INC=$("$CC" -print-file-name=include)
LIBGCC=$("$CC" -print-libgcc-file-name)
for name in x11_sock x11_child envtest xbindtest xclient epoll_sock rwm; do
  "$CC" -std=gnu11 -O2 -ffreestanding -fno-stack-protector -fno-pie -no-pie -nostdinc \
    -isystem userspace/libc/include -isystem "$GCC_INC" \
    -static -nostdlib "$LIBC_DIR/crt1.o" "userspace/tests/$name.c" \
    "$LIBC_DIR/libc.a" "$LIBGCC" -o "$OUT_DIR/$name"
  echo "built $OUT_DIR/$name"
done
