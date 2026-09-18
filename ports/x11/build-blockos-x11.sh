#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SRC=${SRC:-$ROOT/ports/x11/src}
BUILD=${BUILD:-$ROOT/ports/x11/build}
SYSROOT=${SYSROOT:-$ROOT/build/sysroot}
ARCHIVE=${ARCHIVE:-$ROOT/kernel/x11/xserver-master.zip}
CROSS=${CROSS:-$ROOT/ports/x11/blockos-x86_64.txt}
mkdir -p "$SRC" "$BUILD" "$SYSROOT"
if [ ! -d "$SRC/xserver-master" ]; then unzip -q "$ARCHIVE" -d "$SRC"; fi
rm -rf "$BUILD/xserver"
meson setup "$BUILD/xserver" "$SRC/xserver-master" \
  --cross-file "$CROSS" --prefix=/system \
  -Dxorg=false -Dblockos=true -Dxfbdev=false -Dxephyr=false \
  -Dxvfb=false -Dxnest=false -Dglamor=false -Dgbm=false -Dglx=false \
  -Dlisten_tcp=false -Dlisten_unix=true -Dlisten_local=true
ninja -C "$BUILD/xserver" install
