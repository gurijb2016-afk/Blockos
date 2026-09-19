#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
. "$ROOT/config/environment.sh"
fail=0
for p in \
  "$GNOME_OUT/System/lib/libglib-2.0.so" \
  "$GNOME_OUT/System/lib/libgobject-2.0.so" \
  "$GNOME_OUT/System/lib/libgio-2.0.so" \
  "$GNOME_OUT/System/lib/libcairo.so" \
  "$GNOME_OUT/System/lib/libpango-1.0.so" \
  "$GNOME_OUT/System/lib/libgdk_pixbuf-2.0.so" \
  "$GNOME_OUT/System/lib/libgtk-3.so" \
  "$GNOME_OUT/System/lib/libgtk-4.so"; do
  if [ ! -e "$p" ]; then echo "MISSING: $p"; fail=1; else echo "OK:      $p"; fi
done

if find "$GNOME_OUT/System" -type f -name '*.so*' -print | grep -q .; then
  echo "Shared libraries present."
else
  echo "No shared libraries found."; fail=1
fi

exit "$fail"
