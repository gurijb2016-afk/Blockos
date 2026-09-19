#!/bin/sh
set -eu
: "${BLOCKOS_CC:?set BLOCKOS_CC}"
: "${BLOCKOS_CXX:?set BLOCKOS_CXX}"
: "${BLOCKOS_AR:?set BLOCKOS_AR}"
: "${BLOCKOS_STRIP:?set BLOCKOS_STRIP}"
: "${BLOCKOS_SYSROOT:?set BLOCKOS_SYSROOT}"
: "${PKG_CONFIG:?set PKG_CONFIG}"
BASE=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
OUT="$BASE/ports/gnome42/blockos-x86_64.ini"
: "${BLOCKOS_CC}" : "${BLOCKOS_CXX}" : "${BLOCKOS_AR}" : "${BLOCKOS_STRIP}" : "${BLOCKOS_SYSROOT}" : "${PKG_CONFIG}"
sed \
  -e "s|__CC__|$BLOCKOS_CC|g" \
  -e "s|__CXX__|$BLOCKOS_CXX|g" \
  -e "s|__AR__|$BLOCKOS_AR|g" \
  -e "s|__STRIP__|$BLOCKOS_STRIP|g" \
  -e "s|__PKG_CONFIG__|$PKG_CONFIG|g" \
  -e "s|__SYSROOT__|$BLOCKOS_SYSROOT|g" \
  "$BASE/ports/gnome42/meson-cross.template.ini" > "$OUT"
echo "$OUT"
