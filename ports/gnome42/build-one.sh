#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/ports/blockos-env.sh"
NAME=$1
SRC=$2
BUILD=$ROOT/build/$NAME
mkdir -p "$BUILD"
meson setup "$BUILD" "$SRC" --cross-file "$ROOT/ports/blockos-x86_64.txt" --prefix="$PREFIX" "$@"
meson compile -C "$BUILD"
meson install -C "$BUILD" --destdir="$ROOT/stage"
