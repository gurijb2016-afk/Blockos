#!/bin/sh
set -eu
CC=${CC:-x86_64-elf-gcc}
OUT=${OUT:-userspace/tests/build/dynamic-hello}
mkdir -p "$(dirname "$OUT")"
"$CC" -O2 -fPIE -pie -nostdlib -nodefaultlibs \
  -Iuserspace/libc/include \
  userspace/libc/build/crt1.o userspace/tests/dynamic_hello.c \
  -Luserspace/libc/build -Wl,-rpath,/system/lib -lc -Wl,-dynamic-linker,/system/lib/ld.so \
  -o "$OUT"
printf '%s\\n' "$OUT"
