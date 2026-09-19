#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
. "$BASE/ports/gnome42/sources.env"
: "${BLOCKOS_SYSROOT:?set BLOCKOS_SYSROOT}"
: "${BLOCKOS_PREFIX:=$BLOCKOS_SYSROOT/System}"
: "${BLOCKOS_BUILD:=build/gnome42}"
: "${MESON:=meson}"
: "${NINJA:=ninja}"

SRC="$BASE/sources/glib-$GLIB_VERSION"
TAR="$BASE/sources/glib-$GLIB_VERSION.tar.xz"
[ -d "$SRC" ] || tar -xf "$TAR" -C "$BASE/sources"

"$BASE/ports/gnome42/make-cross-file.sh" >/dev/null
mkdir -p "$BASE/$BLOCKOS_BUILD/glib"

"$MESON" setup "$BASE/$BLOCKOS_BUILD/glib" "$SRC" \
  --cross-file "$BASE/ports/gnome42/blockos-x86_64.ini" \
  --prefix "$BLOCKOS_PREFIX" \
  -Dtests=false \
  -Dinstalled_tests=false \
  -Dgtk_doc=false \
  -Dman-pages=false \
  -Dsysprof=disabled \
  -Dlibmount=disabled \
  -Dselinux=disabled \
  -Dsystemtap=false

"$NINJA" -C "$BASE/$BLOCKOS_BUILD/glib"
"$NINJA" -C "$BASE/$BLOCKOS_BUILD/glib" install
