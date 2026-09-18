#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SYSROOT=${SYSROOT:-$ROOT/build/sysroot/system}
fail=0
for x in \
  lib/libglib-2.0.so \
  lib/libcairo.so \
  lib/libpango-1.0.so \
  lib/libgdk_pixbuf-2.0.so \
  lib/libgtk-4.so; do
  if [ -e "$SYSROOT/$x" ]; then echo "OK   $x"; else echo "MISS $x"; fail=1; fi
done
exit "$fail"
