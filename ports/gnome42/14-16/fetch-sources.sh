#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$HERE/versions.env"
SRC="$HERE/sources"
mkdir -p "$SRC"
fetch() {
    name="$1"
    url="$2"
    out="$SRC/$name"
    if [ ! -f "$out" ]; then
        curl -fL --retry 3 --retry-delay 2 -o "$out" "$url"
    fi
}
fetch "mutter-$MUTTER_VERSION.tar.xz" "https://download.gnome.org/sources/mutter/42/mutter-$MUTTER_VERSION.tar.xz"
fetch "gnome-shell-$GNOME_SHELL_VERSION.tar.xz" "https://download.gnome.org/sources/gnome-shell/42/gnome-shell-$GNOME_SHELL_VERSION.tar.xz"
fetch "gnome-session-$GNOME_SESSION_VERSION.tar.xz" "https://download.gnome.org/sources/gnome-session/42/gnome-session-$GNOME_SESSION_VERSION.tar.xz"
# GNOME publishes checksum files beside releases. Verify when available.
verify() {
    archive="$1"
    checksum_url="$2"
    checksum="$SRC/$(basename "$checksum_url")"
    if curl -fL --retry 3 -o "$checksum" "$checksum_url" 2>/dev/null; then
        grep "$(basename "$archive")$" "$checksum" | (cd "$SRC" && sha256sum -c -)
    else
        echo "warning: could not retrieve checksum file for $(basename "$archive")" >&2
    fi
}
verify "$SRC/mutter-$MUTTER_VERSION.tar.xz" "https://download.gnome.org/sources/mutter/42/mutter-$MUTTER_VERSION.sha256sum"
verify "$SRC/gnome-shell-$GNOME_SHELL_VERSION.tar.xz" "https://download.gnome.org/sources/gnome-shell/42/gnome-shell-$GNOME_SHELL_VERSION.sha256sum"
verify "$SRC/gnome-session-$GNOME_SESSION_VERSION.tar.xz" "https://download.gnome.org/sources/gnome-session/42/gnome-session-$GNOME_SESSION_VERSION.sha256sum"
printf '%s\n' "Sources downloaded and checked where checksum files are available."
