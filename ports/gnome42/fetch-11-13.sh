#!/bin/sh
set -eu
. "$(dirname "$0")/versions-11-13.sh"
ROOT=${GNOME42_SRC:-$PWD/ports/gnome42/src}
mkdir -p "$ROOT"
fetch() {
  name=$1
  url=$2
  out=$ROOT/$(basename "$url")
  if [ ! -f "$out" ]; then
    echo "FETCH $name: $url"
    command -v curl >/dev/null 2>&1 || { echo "curl is required" >&2; exit 1; }
    curl -L --fail --retry 3 -o "$out" "$url"
  else
    echo "HAVE  $out"
  fi
}
fetch dconf "https://download.gnome.org/sources/dconf/${DconfVersion%.*}/dconf-$DconfVersion.tar.xz"
fetch gvfs "https://download.gnome.org/sources/gvfs/${GVfsVersion%.*}/gvfs-$GVfsVersion.tar.xz"
fetch gjs "https://download.gnome.org/sources/gjs/${GjsVersion%.*}/gjs-$GjsVersion.tar.xz"
printf '%s\n' "Sources stored in $ROOT"
