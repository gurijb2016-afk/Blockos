#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$BASE/src"
for a in "$BASE"/sources/*.tar.xz; do
    [ -f "$a" ] || continue
    name=$(basename "$a" .tar.xz)
    rm -rf "$BASE/src/$name"
    tar -xf "$a" -C "$BASE/src"
done
printf 'Unpacked sources under %s/src\n' "$BASE"
