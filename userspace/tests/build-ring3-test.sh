#!/bin/sh
set -eu
CC=${CC:-x86_64-elf-gcc}
OUT=${OUT:-userspace/tests/ring3-test}
"$CC" -ffreestanding -fno-pie -fno-stack-protector -nostdlib -static -no-pie -Iuserspace/include userspace/crt/crt0.S userspace/tests/ring3_test.c -Wl,-e,_start -o "$OUT"
echo "$OUT"
