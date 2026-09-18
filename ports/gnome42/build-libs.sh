#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SRC=${SRC:-$ROOT/ports/gnome42/src}
BUILD=${BUILD:-$ROOT/ports/gnome42/build}
SYSROOT=${SYSROOT:-$ROOT/build/sysroot}
CC=${CC:-x86_64-elf-gcc}
CXX=${CXX:-x86_64-elf-g++}
MESON=${MESON:-meson}
NINJA=${NINJA:-ninja}
export CC CXX PKG_CONFIG_SYSROOT_DIR="/" PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig:$SYSROOT/share/pkgconfig"

mkdir -p "$BUILD" "$SYSROOT"/system/lib "$SYSROOT"/system/include "$SYSROOT"/system/share

meson_pkg() {
  name=$1
  shift
  [ -d "$SRC/$name" ] || { echo "missing source: $SRC/$name" >&2; exit 2; }
  rm -rf "$BUILD/$name"
  "$MESON" setup "$BUILD/$name" "$SRC/$name" --cross-file "$ROOT/ports/gnome42/blockos-x86_64.txt" --prefix=/system --libdir=lib --buildtype=release "$@"
  "$NINJA" -C "$BUILD/$name"
  "$NINJA" -C "$BUILD/$name" install
}

# Foundation
[ -d "$SRC/libffi" ] && {
  meson_pkg libffi -Dtests=false -Ddocs=false || true
}
[ -d "$SRC/zlib" ] && {
  cmake -S "$SRC/zlib" -B "$BUILD/zlib" -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_C_COMPILER="$CC" -DCMAKE_INSTALL_PREFIX=/system -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  cmake --build "$BUILD/zlib" -j
  DESTDIR="$SYSROOT" cmake --install "$BUILD/zlib"
}
[ -d "$SRC/pixman" ] && {
  meson_pkg pixman -Dtests=disabled || true
}
[ -d "$SRC/freetype" ] && {
  meson_pkg freetype -Dtests=disabled -Dharfbuzz=enabled || true
}
[ -d "$SRC/fontconfig" ] && {
  meson_pkg fontconfig -Dtests=disabled -Ddoc=disabled || true
}
[ -d "$SRC/harfbuzz" ] && {
  meson_pkg harfbuzz -Dtests=disabled -Ddocs=disabled || true
}

# GLib -> Cairo -> Pango -> GdkPixbuf -> GTK4.
meson_pkg glib -Dtests=false -Ddocumentation=false -Dman=false -Dsysprof=disabled -Dglib_debug=disabled
meson_pkg cairo -Dtests=disabled -Dxcb=disabled -Dxlib=disabled
meson_pkg pango -Dbuild-tests=false -Dbuild-documentation=false -Dxft=disabled
meson_pkg gdk-pixbuf -Dtests=false -Dman=false

# GTK4: keep the X11 backend, turn off Wayland and optional docs/tests.
meson_pkg gtk -Dbuild-tests=false -Dbuild-documentation=false -Dwayland-backend=false -Dx11-backend=true

echo "GNOME 42 graphics stack installed under $SYSROOT/system"
