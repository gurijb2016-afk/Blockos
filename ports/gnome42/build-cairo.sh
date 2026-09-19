#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
. "$BASE/ports/gnome42/sources.env"
: "${BLOCKOS_SYSROOT:?set BLOCKOS_SYSROOT}"
: "${BLOCKOS_PREFIX:=$BLOCKOS_SYSROOT/System}"
: "${MESON:=meson}"
: "${NINJA:=ninja}"
SRC="$BASE/sources/cairo-$CAIRO_VERSION"
TAR="$BASE/sources/cairo-$CAIRO_VERSION.tar.xz"
[ -d "$SRC" ] || tar -xf "$TAR" -C "$BASE/sources"

"$BASE/ports/gnome42/make-cross-file.sh" >/dev/null
mkdir -p "$BASE/build/gnome42/cairo"
"$MESON" setup "$BASE/build/gnome42/cairo" "$SRC" \
  --cross-file "$BASE/ports/gnome42/blockos-x86_64.ini" \
  --prefix "$BLOCKOS_PREFIX" \
  -Dtests=disabled \
  -Dgtk_doc=false \
  -Dzlib=enabled \
  -Dpng=enabled
"$NINJA" -C "$BASE/build/gnome42/cairo"
"$NINJA" -C "$BASE/build/gnome42/cairo" install
