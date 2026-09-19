#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$BASE/ports/blockos/blockos-env.sh"
ROOT="$BLOCKOS_STAGING/System/lib"
for f in "$ROOT"/*.so*; do
  [ -e "$f" ] || continue
  if command -v readelf >/dev/null 2>&1; then
    echo "===== $f ====="
    readelf -h "$f" | sed -n '/Class:/p;/Machine:/p;/Type:/p'
    readelf -d "$f" 2>/dev/null | sed -n '/NEEDED/p;/SONAME/p;/RPATH/p;/RUNPATH/p' || true
  fi
done
