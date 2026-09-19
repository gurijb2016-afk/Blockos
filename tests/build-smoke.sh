#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$BASE/ports/blockos/blockos-env.sh"
mkdir -p "$BASE/build/tests"

"${CC}" $CPPFLAGS "$BASE/tests/test-freetype.c" -o "$BASE/build/tests/test-freetype"   -L"$BLOCKOS_STAGING/System/lib" -lfreetype

"${CC}" $CPPFLAGS "$BASE/tests/test-harfbuzz.c" -o "$BASE/build/tests/test-harfbuzz"   -L"$BLOCKOS_STAGING/System/lib" -lharfbuzz

"${CC}" $CPPFLAGS "$BASE/tests/test-pango.c" -o "$BASE/build/tests/test-pango"   -L"$BLOCKOS_STAGING/System/lib" -lpangocairo-1.0 -lpango-1.0 -lcairo -lharfbuzz -lfreetype

echo "Smoke test binaries built."
