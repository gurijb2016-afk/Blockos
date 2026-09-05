#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
VER=5.4.9
URL="https://www.lua.org/ftp/lua-${VER}.tar.gz"
OUT="$ROOT/ports/lua/build"
mkdir -p "$OUT"
cd "$OUT"
[ -f "lua-${VER}.tar.gz" ] || curl -fL "$URL" -o "lua-${VER}.tar.gz"
rm -rf "lua-${VER}"
tar xf "lua-${VER}.tar.gz"
cd "lua-${VER}"
: "${CC:=x86_64-elf-gcc}"
: "${AR:=x86_64-elf-ar}"
: "${RANLIB:=x86_64-elf-ranlib}"
: "${USERLIBC:=}"
if [ -z "$USERLIBC" ]; then
  echo "ERROR: USERLIBC must point at a BlockOS freestanding libc sysroot" >&2
  exit 2
fi
CFLAGS="-ffreestanding -fno-pie -fno-stack-protector -fno-builtin -I$ROOT/userspace/include -I$ROOT/posix/include -I$USERLIBC/include ${CFLAGS:-}"
LDFLAGS="-nostdlib -static -no-pie ${LDFLAGS:-}"
SRC="lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c lutf8lib.c lvm.c lzio.c lua.c"
mkdir -p obj
$CC $CFLAGS -c $SRC
$CC $CFLAGS -c "$ROOT/userspace/crt/syscall.S" -o obj/blockos_syscall.o
$CC $LDFLAGS -o lua *.o obj/blockos_syscall.o "$USERLIBC/lib/libc.a" -lgcc
mkdir -p "$OUT/root"
cp lua "$OUT/root/lua"
chmod +x "$OUT/root/lua"
printf '%s\n' "Built real Lua ${VER}: $OUT/root/lua"
