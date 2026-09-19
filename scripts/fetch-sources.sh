#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$BASE/configs/versions.env"
mkdir -p "$BASE/sources"

fetch_one() {
    url="$1"; file="$2"; sum="$3"
    if [ ! -f "$BASE/sources/$file" ]; then
        curl -fL --retry 3 --retry-delay 1 -o "$BASE/sources/$file" "$url"
    fi
    printf '%s  %s\n' "$sum" "$BASE/sources/$file" | sha256sum -c -
}

fetch_one "$PANGO_URL" "pango-${PANGO_VERSION}.tar.xz" "$PANGO_SHA256"
fetch_one "$HARFBUZZ_URL" "harfbuzz-${HARFBUZZ_VERSION}.tar.xz" "$HARFBUZZ_SHA256"
fetch_one "$FREETYPE_URL" "freetype-${FREETYPE_VERSION}.tar.xz" "$FREETYPE_SHA256"

echo "All three release archives verified."
