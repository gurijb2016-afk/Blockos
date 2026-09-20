#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$HERE/versions.env"
STAGE=${STAGE:-$HERE/stage}
SRC=${SRC:-$HERE/sources}
PREFIX=/System
NPROC=${NPROC:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}

CC=${CC:-cc}
CXX=${CXX:-c++}
AR=${AR:-ar}
STRIP=${STRIP:-strip}

mkdir -p "$STAGE" "$HERE/build"

unpack() {
    archive="$1"
    dir="$2"
    if [ ! -d "$dir" ]; then
        mkdir -p "$dir"
        tar -xf "$archive" -C "$dir" --strip-components=1
    fi
}

build_meson() {
    name="$1"
    version="$2"
    shift 2
    archive="$SRC/$name-$version.tar.xz"
    srcdir="$HERE/build/$name-$version"
    bdir="$srcdir/build"
    [ -f "$archive" ] || { echo "missing source: $archive" >&2; exit 1; }
    unpack "$archive" "$srcdir"
    rm -rf "$bdir"
    CC="$CC" CXX="$CXX" AR="$AR" STRIP="$STRIP" \
      meson setup "$bdir" "$srcdir" \
      --prefix="$PREFIX" \
      --libdir=lib \
      --bindir=bin \
      -Dtests=false \
      "$@"
    meson compile -C "$bdir" -j "$NPROC"
    DESTDIR="$STAGE" meson install -C "$bdir"
}

# These are GNOME-42/X11-oriented options. If a dependency build exposes a
# different option name, pass MESON_NATIVE_ARGS/MESON_EXTRA_ARGS in the caller.
build_meson mutter "$MUTTER_VERSION" -Dx11=true -Dwayland=false
build_meson gnome-shell "$GNOME_SHELL_VERSION"
build_meson gnome-session "$GNOME_SESSION_VERSION"

mkdir -p "$STAGE$PREFIX/share/xsessions" "$STAGE$PREFIX/share/gnome-session/sessions"
cp -f "$HERE/../../../../System/share/xsessions/gnome.desktop" "$STAGE$PREFIX/share/xsessions/gnome.desktop"
cp -f "$HERE/../../../../System/share/gnome-session/sessions/gnome.session" "$STAGE$PREFIX/share/gnome-session/sessions/gnome.session"

echo "Staged components under $STAGE$PREFIX"
