#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$BASE/configs/versions.env"
. "$BASE/ports/blockos/blockos-env.sh"
SRC="$BASE/src/freetype-${FREETYPE_VERSION}"
BUILD="$BASE/build/freetype"
PREFIX="$BLOCKOS_STAGING/System"
[ -d "$SRC" ] || { echo "Missing $SRC; run fetch-sources.sh and unpack-sources.sh" >&2; exit 2; }
rm -rf "$BUILD"
meson setup "$BUILD" "$SRC"   --cross-file "$BASE/ports/blockos/blockos-x86_64.ini"   --prefix "$PREFIX"   --libdir lib   -Dharfbuzz=enabled   -Dbrotli=disabled   -Dbzip2=disabled   -Dzlib=disabled   -Dpng=disabled   -Dtests=disabled   -Ddoc=disabled   -Dbuildtype=release
meson compile -C "$BUILD"
meson install -C "$BUILD"
