#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$BASE/ports/blockos/blockos-env.sh"

mkdir -p "$BLOCKOS_STAGING/System/lib" "$BLOCKOS_STAGING/System/include" "$BLOCKOS_STAGING/System/share/pkgconfig"
# This script deliberately does not copy into a live OS image. It only creates
# a deterministic staging tree. Copy/merge BLOCKOS_STAGING/System into the
# BlockOS rootfs during your normal image build.
find "$BLOCKOS_STAGING/System" -maxdepth 1 -type d -print
