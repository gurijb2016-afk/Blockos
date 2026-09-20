#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$ROOT/versions-11-13.sh"
SRC=${GNOME42_SRC:-$ROOT/src}
STAGE=${GNOME42_STAGE:-$ROOT/stage}
CROSS=${GNOME42_CROSS_FILE:-$ROOT/blockos-x86_64.txt}
mkdir -p "$SRC" "$STAGE"
command -v meson >/dev/null 2>&1 || { echo "meson is required" >&2; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo "ninja is required" >&2; exit 1; }
command -v tar >/dev/null 2>&1 || exit 1

unpack() {
  archive=$1
  dir=$2
  [ -d "$SRC/$dir" ] || tar -xf "$SRC/$archive" -C "$SRC"
}

unpack "dconf-$DconfVersion.tar.xz" "dconf-$DconfVersion"
unpack "gvfs-$GVfsVersion.tar.xz" "gvfs-$GVfsVersion"
unpack "gjs-$GjsVersion.tar.xz" "gjs-$GjsVersion"

build_meson() {
  name=$1
  srcdir=$2
  shift 2
  b="$ROOT/build-$name"
  rm -rf "$b"
  meson setup "$b" "$srcdir" --cross-file "$CROSS" \
    --prefix=/System --libdir=lib --libexecdir=libexec \
    --buildtype=release "$@"
  meson compile -C "$b"
  DESTDIR="$STAGE" meson install -C "$b"
}

# 11: dconf/GSettings backend. X11/desktop integration does not need editor/docs.
build_meson dconf "$SRC/dconf-$DconfVersion" \
  -Dman=false -Dvapi=false -Dgtk=disabled -Deditor=true

# 12: GVfs. Keep the base daemon and local file backend; disable FUSE if unsupported.
build_meson gvfs "$SRC/gvfs-$GVfsVersion" \
  -Dman=false -Dsystemduserunitdir= \
  -Dfuse=false -Dgoogle=false -Dafp=false -Dgdu=false -Dgphoto2=false \
  -Ddnssd=false -Dsmb=false -Dmtp=false -Dcdio=false -Dudisks2=false

# 13: GJS. Requires a matching mozjs pkg-config module in the BlockOS sysroot.
# Set GJS_MOZJS_PKG when your SpiderMonkey port exposes a different module name.
MOZ=${GJS_MOZJS_PKG:-mozjs-91}
build_meson gjs "$SRC/gjs-$GjsVersion" \
  -Dinstalled_tests=false -Dprofiler=disabled -Dbsymbolic_functions=false \
  -Dbuildtype=release \
  -Dmozjs_rpm_name="$MOZ"

printf '%s\n' "11/12/13 staged under $STAGE/System"
