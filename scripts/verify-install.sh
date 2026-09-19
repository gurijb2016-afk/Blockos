#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$BASE/ports/blockos/blockos-env.sh"
ROOT="$BLOCKOS_STAGING/System"
fail=0
for f in   "$ROOT/lib/libfreetype.so" "$ROOT/lib/libharfbuzz.so" "$ROOT/lib/libpango-1.0.so"; do
  if [ ! -e "$f" ] && [ ! -e "${f}.0" ]; then
    echo "MISSING: $f"; fail=1
  else
    echo "OK: $f"
  fi
done
for pc in freetype2 harfbuzz pango; do
  if [ -f "$ROOT/lib/pkgconfig/$pc.pc" ] || [ -f "$ROOT/share/pkgconfig/$pc.pc" ]; then
    echo "OK pkg-config: $pc"
  else
    echo "MISSING pkg-config: $pc"; fail=1
  fi
done
exit "$fail"
