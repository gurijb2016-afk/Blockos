#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$BASE/configs/versions.env"
. "$BASE/ports/blockos/blockos-env.sh"

SRC="$BASE/src/pango-${PANGO_VERSION}"
BUILD="$BASE/build/pango"
PREFIX="$BLOCKOS_STAGING/System"
[ -d "$SRC" ] || { echo "Missing $SRC; run fetch-sources.sh and unpack-sources.sh" >&2; exit 2; }
rm -rf "$BUILD"
meson setup "$BUILD" "$SRC"   --cross-file "$BASE/ports/blockos/blockos-x86_64.ini"   --prefix "$PREFIX"   --libdir lib   -Dintrospection=disabled   -Dgtk_doc=false   -Dtests=false   -Dbuildtype=release   -Dsysprof=disabled   -Dcairo=enabled   -Dfontconfig=enabled   -Dfreetype=enabled   -Dharfbuzz=enabled
meson compile -C "$BUILD"
meson install -C "$BUILD"
