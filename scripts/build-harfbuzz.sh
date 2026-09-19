#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$BASE/configs/versions.env"
. "$BASE/ports/blockos/blockos-env.sh"
SRC="$BASE/src/harfbuzz-${HARFBUZZ_VERSION}"
BUILD="$BASE/build/harfbuzz"
PREFIX="$BLOCKOS_STAGING/System"
[ -d "$SRC" ] || { echo "Missing $SRC; run fetch-sources.sh and unpack-sources.sh" >&2; exit 2; }
rm -rf "$BUILD"
meson setup "$BUILD" "$SRC"   --cross-file "$BASE/ports/blockos/blockos-x86_64.ini"   --prefix "$PREFIX"   --libdir lib   -Dglib=enabled   -Dgobject=disabled   -Dcairo=disabled   -Dfreetype=enabled   -Dgraphite2=disabled   -Dicu=disabled   -Dtests=disabled   -Ddocs=disabled   -Dutilities=disabled   -Dbenchmark=disabled   -Dintrospection=disabled   -Dbuildtype=release
meson compile -C "$BUILD"
meson install -C "$BUILD"
