#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
. "$ROOT/config/environment.sh"

NAME="$1"; TYPE="$2"; shift 2
SRC_ARCHIVE="$GNOME_SOURCES/$NAME.tar.xz"
[ -f "$SRC_ARCHIVE" ] || SRC_ARCHIVE="$GNOME_SOURCES/$NAME.tar.gz"
[ -f "$SRC_ARCHIVE" ] || { echo "missing source: $NAME" >&2; exit 1; }

WORK="$GNOME_BUILDS/$NAME"
SRC="$WORK/src"
BUILD="$WORK/build"
rm -rf "$SRC" "$BUILD"
mkdir -p "$SRC" "$BUILD"
tar -xf "$SRC_ARCHIVE" -C "$SRC" --strip-components=1

export PKG_CONFIG_SYSROOT_DIR="$BLOCKOS_SYSROOT"
export PKG_CONFIG_LIBDIR="$BLOCKOS_SYSROOT/System/lib/pkgconfig:$BLOCKOS_SYSROOT/System/share/pkgconfig"
export CC="$BLOCKOS_CC" CXX="$BLOCKOS_CXX" AR="$BLOCKOS_AR" RANLIB="$BLOCKOS_RANLIB" STRIP="$BLOCKOS_STRIP"
export CPPFLAGS="--sysroot=$BLOCKOS_SYSROOT -I$BLOCKOS_SYSROOT/System/include"
export CFLAGS="$CFLAGS --sysroot=$BLOCKOS_SYSROOT"
export CXXFLAGS="$CXXFLAGS --sysroot=$BLOCKOS_SYSROOT"
export LDFLAGS="$LDFLAGS --sysroot=$BLOCKOS_SYSROOT"

case "$TYPE" in
  meson)
    meson setup "$BUILD" "$SRC" --cross-file "$MESON_CROSS_FILE" --prefix=/system --buildtype=release "$@"
    meson compile -C "$BUILD"
    DESTDIR="$GNOME_OUT" meson install -C "$BUILD"
    ;;
  autotools)
    cd "$SRC"
    ./configure --host="$BLOCKOS_TRIPLET" --build="$(./config.guess 2>/dev/null || echo x86_64-pc-linux-gnu)" --prefix=/system --libdir=/system/lib --disable-static "$@"
    make -j"${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
    DESTDIR="$GNOME_OUT" make install
    ;;
  *) echo "unknown build type: $TYPE" >&2; exit 2;;
esac

echo "[ok] $NAME"
