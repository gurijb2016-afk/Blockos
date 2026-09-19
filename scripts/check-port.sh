#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
fail=0
for x in \
  "$BASE/ports/gnome42/sources.env" \
  "$BASE/ports/gnome42/fetch-sources.sh" \
  "$BASE/ports/gnome42/build-glib.sh" \
  "$BASE/ports/gnome42/build-libffi.sh" \
  "$BASE/ports/gnome42/build-cairo.sh" \
  "$BASE/ports/gnome42/build-1-2-3.sh"; do
  [ -f "$x" ] || { echo "missing: $x"; fail=1; }
done
sh -n "$BASE/ports/gnome42/fetch-sources.sh"
sh -n "$BASE/ports/gnome42/make-cross-file.sh"
sh -n "$BASE/ports/gnome42/build-glib.sh"
sh -n "$BASE/ports/gnome42/build-libffi.sh"
sh -n "$BASE/ports/gnome42/build-cairo.sh"
sh -n "$BASE/ports/gnome42/build-1-2-3.sh"
[ "$fail" -eq 0 ] || exit 1
echo 'port scripts: syntax OK'
