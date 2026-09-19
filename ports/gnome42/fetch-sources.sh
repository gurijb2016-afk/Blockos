#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
. "$BASE/ports/gnome42/sources.env"
OUT="$BASE/sources"
mkdir -p "$OUT"

fetch() {
  url=$1
  file=$2
  if [ -f "$OUT/$file" ]; then
    echo "exists: $file"
    return
  fi
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$OUT/$file" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$OUT/$file" "$url"
  else
    echo "error: curl or wget required" >&2
    exit 1
  fi
}

fetch "$GLIB_URL"   "glib-$GLIB_VERSION.tar.xz"
fetch "$LIBFFI_URL" "libffi-$LIBFFI_VERSION.tar.gz"
fetch "$CAIRO_URL"  "cairo-$CAIRO_VERSION.tar.xz"

echo "Downloaded sources to $OUT"
