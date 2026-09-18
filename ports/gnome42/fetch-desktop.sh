#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SRC=${SRC:-$ROOT/ports/gnome42/src}
BASE=https://download.gnome.org
mkdir -p "$SRC"

fetch() {
    name=$1
    url=$2
    archive=${url##*/}
    if [ -d "$SRC/$name" ]; then
        echo "[skip] $name"
        return
    fi
    mkdir -p "$SRC/.cache"
    echo "[fetch] $archive"
    curl -fL --retry 3 -o "$SRC/.cache/$archive" "$url"
    mkdir -p "$SRC/$name"
    tar -xf "$SRC/.cache/$archive" -C "$SRC/$name" --strip-components=1
}

# GNOME 42 stack used by this BlockOS port.
fetch dconf "$BASE/sources/dconf/0.40/dconf-0.40.0.tar.xz"
fetch gvfs "$BASE/core/42/42.3/sources/gvfs-1.50.2.tar.xz"
fetch mutter "$BASE/core/42/42.3/sources/mutter-42.3.tar.xz"
fetch gjs "$BASE/core/42/42.3/sources/gjs-1.72.1.tar.xz"
fetch gnome-shell "$BASE/core/42/42.3/sources/gnome-shell-42.3.1.tar.xz"
fetch gnome-session "$BASE/core/42/42.3/sources/gnome-session-42.0.tar.xz"
fetch gsettings-desktop-schemas "$BASE/core/42/42.3/sources/gsettings-desktop-schemas-42.0.tar.xz"
fetch gnome-desktop "$BASE/core/42/42.3/sources/gnome-desktop-42.3.tar.xz"

echo "GNOME 42 desktop sources are in $SRC"
