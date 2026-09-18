#!/bin/sh
set -eu
BASE=${BASE:-https://download.gnome.org/sources}
OUT=${OUT:-$(CDPATH= cd -- "$(dirname "$0")" && pwd)/src}
mkdir -p "$OUT"
fetch(){ n=$1; v=$2; major=$(printf '%s' "$v" | cut -d. -f1,2); url="$BASE/$n/$major/$n-$v.tar.xz"; echo "[GNOME42] $url"; curl -L --fail --retry 3 "$url" -o "$OUT/$n-$v.tar.xz"; tar -xf "$OUT/$n-$v.tar.xz" -C "$OUT"; rm -rf "$OUT/$n"; mv "$OUT/$n-$v" "$OUT/$n"; }
fetch glib 2.72.4
fetch cairo 1.17.6
fetch pango 1.50.6
fetch gdk-pixbuf 2.42.10
fetch gtk 4.6.9
fetch mutter 42.7
fetch gjs 1.72.3
fetch gsettings-desktop-schemas 42.0
fetch gnome-shell 42.9
