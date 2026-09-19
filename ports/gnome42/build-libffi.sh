#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
. "$BASE/ports/gnome42/sources.env"
: "${BLOCKOS_SYSROOT:?set BLOCKOS_SYSROOT}"
: "${BLOCKOS_PREFIX:=$BLOCKOS_SYSROOT/System}"
: "${BLOCKOS_CC:?set BLOCKOS_CC}"
: "${BLOCKOS_AR:?set BLOCKOS_AR}"
: "${BLOCKOS_RANLIB:?set BLOCKOS_RANLIB}"
SRC="$BASE/sources/libffi-$LIBFFI_VERSION"
TAR="$BASE/sources/libffi-$LIBFFI_VERSION.tar.gz"
[ -d "$SRC" ] || tar -xzf "$TAR" -C "$BASE/sources"
cd "$SRC"
[ -x configure ] || ./autogen.sh
./configure \
  --host=x86_64-blockos \
  --prefix="$BLOCKOS_PREFIX" \
  --disable-docs
make -j"${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
make install
