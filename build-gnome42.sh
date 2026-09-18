#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
: "${CC:=x86_64-elf-gcc}"
: "${AS:=x86_64-elf-as}"
: "${AR:=x86_64-elf-ar}"
export CC AS AR

echo '[1/6] libc + pthread + TLS'
sh "$ROOT/userspace/libc/build-libc.sh"

echo '[2/6] ld.so'
sh "$ROOT/userspace/ldso/build-ldso.sh"

echo '[3/6] D-Bus service'
sh "$ROOT/userspace/services/build-services.sh"
echo '[4/6] dynamic TLS/pthread test binary'
sh "$ROOT/userspace/tests/build-dynamic-tls.sh"
echo '[5/6] X11 KDrive'
sh "$ROOT/ports/x11/build-blockos-x11.sh"
echo '[6/6] GNOME 42 dependency stack'
sh "$ROOT/ports/gnome42/build-stack.sh"
