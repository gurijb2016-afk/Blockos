#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
CC=${CC:-x86_64-elf-gcc}
CXX=${CXX:-x86_64-elf-g++}
SYSROOT=${SYSROOT:-$ROOT/build/sysroot}
SRC=${SRC:-$ROOT/ports/gnome42/src}
BUILD=${BUILD:-$ROOT/ports/gnome42/build}
MESON=${MESON:-meson}
NINJA=${NINJA:-ninja}
export CC CXX
mkdir -p "$SRC" "$BUILD" "$SYSROOT/lib" "$SYSROOT/include"
cat <<MSG
BlockOS GNOME 42 cross-build driver
Sources are expected under: $SRC
Install destinations: $SYSROOT
Order: glib -> cairo -> pango -> gdk-pixbuf -> gtk -> mutter -> gjs -> gsettings-desktop-schemas -> dbus -> gnome-shell
This script deliberately stops on a failed package; it does not pretend a
host-built Linux library is a BlockOS library.
MSG
for p in glib cairo pango gdk-pixbuf gtk mutter gjs gsettings-desktop-schemas; do
  if [ ! -d "$SRC/$p" ]; then echo "[GNOME42] missing source: $SRC/$p" >&2; exit 2; fi
  rm -rf "$BUILD/$p"
  "$MESON" setup "$BUILD/$p" "$SRC/$p" \
    --cross-file "$ROOT/ports/gnome42/blockos-x86_64.txt" \
    --prefix=/system \
    -Dtests=false -Ddocumentation=false -Dexamples=false
  "$NINJA" -C "$BUILD/$p" install
 done
for p in dbus gnome-shell; do
  if [ -d "$SRC/$p" ]; then
    rm -rf "$BUILD/$p"
    "$MESON" setup "$BUILD/$p" "$SRC/$p" --cross-file "$ROOT/ports/gnome42/blockos-x86_64.txt" --prefix=/system
    "$NINJA" -C "$BUILD/$p" install
  fi
done
