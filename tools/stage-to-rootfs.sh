#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
. "$ROOT/config/environment.sh"
TARGET="${1:-$GNOME_OUT}"
mkdir -p "$TARGET/System"
cp -a "$GNOME_OUT/System/." "$TARGET/System/"
printf '%s\n' "staged GNOME tree into $TARGET/System"
