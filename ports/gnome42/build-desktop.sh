#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SRC=${SRC:-$ROOT/ports/gnome42/src}
BUILD=${BUILD:-$ROOT/ports/gnome42/build-desktop}
SYSROOT=${SYSROOT:-$ROOT/build/sysroot}
MESON=${MESON:-meson}
NINJA=${NINJA:-ninja}
CROSS=${CROSS:-$ROOT/ports/gnome42/blockos-x86_64.txt}

mkdir -p "$BUILD" "$SYSROOT"
export PKG_CONFIG_SYSROOT_DIR="/"
export PKG_CONFIG_LIBDIR="$SYSROOT/system/lib/pkgconfig:$SYSROOT/system/share/pkgconfig:$SYSROOT/lib/pkgconfig:$SYSROOT/share/pkgconfig"

meson_pkg() {
    name=$1
    shift
    [ -d "$SRC/$name" ] || { echo "missing source: $SRC/$name" >&2; exit 2; }
    rm -rf "$BUILD/$name"
    "$MESON" setup "$BUILD/$name" "$SRC/$name" \
        --cross-file "$CROSS" \
        --prefix=/system \
        --libdir=lib \
        --buildtype=release \
        "$@"
    "$NINJA" -C "$BUILD/$name"
    DESTDIR="$SYSROOT" "$NINJA" -C "$BUILD/$name" install
}

# GSettings schema backend first.
meson_pkg dconf

# GVfs provides GIO virtual filesystem daemons used by the GNOME desktop.
meson_pkg gvfs

# GJS is required by GNOME Shell because Shell is largely JavaScript on GJS.
meson_pkg gjs

# Mutter: build the X11 backend for the BlockOS X11 server.
meson_pkg mutter -Dxwayland=false -Dx11=true

# GNOME Shell: the X11 session is selected at runtime.
meson_pkg gnome-shell

# Session manager and desktop schemas.
meson_pkg gnome-session
meson_pkg gsettings-desktop-schemas
meson_pkg gnome-desktop

echo "Desktop stack installed under $SYSROOT"
