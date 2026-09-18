#!/bin/sh
set -eu

BASE_GNOME=${BASE_GNOME:-https://download.gnome.org/sources}
BASE_FT=${BASE_FT:-https://download.savannah.gnu.org/releases/freetype}
BASE_FC=${BASE_FC:-https://www.freedesktop.org/software/fontconfig/release}
BASE_HB=${BASE_HB:-https://github.com/harfbuzz/harfbuzz/releases/download}
BASE_FFI=${BASE_FFI:-https://github.com/libffi/libffi/releases/download}
BASE_ZLIB=${BASE_ZLIB:-https://zlib.net/fossils}
BASE_PIXMAN=${BASE_PIXMAN:-https://www.cairographics.org/releases}
OUT=${OUT:-$(CDPATH= cd -- "$(dirname "$0")" && pwd)/src}
mkdir -p "$OUT"

fetch_gnome() {
    name=$1 version=$2
    major=$(printf '%s' "$version" | cut -d. -f1,2)
    url="$BASE_GNOME/$name/$major/$name-$version.tar.xz"
    file="$OUT/$name-$version.tar.xz"
    echo "[GNOME42] $url"
    curl -L --fail --retry 3 "$url" -o "$file"
    tar -xf "$file" -C "$OUT"
    rm -rf "$OUT/$name"
    mv "$OUT/$name-$version" "$OUT/$name"
}

fetch_url() {
    name=$1 url=$2 archive=$3 dir=$4
    echo "[GNOME42] $url"
    curl -L --fail --retry 3 "$url" -o "$OUT/$archive"
    tar -xf "$OUT/$archive" -C "$OUT"
    rm -rf "$OUT/$dir"
    found=$(tar -tf "$OUT/$archive" | head -1 | cut -d/ -f1)
    mv "$OUT/$found" "$OUT/$dir"
}

# Versions are pinned to the GNOME 42-era stack.
fetch_gnome glib 2.72.4
fetch_gnome cairo 1.17.6
fetch_gnome pango 1.50.6
fetch_gnome gdk-pixbuf 2.42.10
fetch_gnome gtk 4.6.9

fetch_url libffi "$BASE_FFI/v3.4.4/libffi-3.4.4.tar.gz" libffi-3.4.4.tar.gz libffi
fetch_url zlib "$BASE_ZLIB/zlib-1.2.13.tar.gz" zlib-1.2.13.tar.gz zlib
fetch_url pixman "$BASE_PIXMAN/pixman-0.40.0.tar.gz" pixman-0.40.0.tar.gz pixman
fetch_url freetype "$BASE_FT/freetype-2.12.1.tar.xz" freetype-2.12.1.tar.xz freetype
fetch_url fontconfig "$BASE_FC/fontconfig-2.14.1.tar.xz" fontconfig-2.14.1.tar.xz fontconfig
fetch_url harfbuzz "$BASE_HB/5.3.1/harfbuzz-5.3.1.tar.xz" harfbuzz-5.3.1.tar.xz harfbuzz
