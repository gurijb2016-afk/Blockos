#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
STAGE=${GNOME42_STAGE:-$ROOT/stage}
DEST=${BLOCKOS_ROOTFS:-.}
[ -d "$STAGE/System" ] || { echo "Build 11/12/13 first: $STAGE/System missing" >&2; exit 1; }
mkdir -p "$DEST"
cp -a "$STAGE/System/." "$DEST/System/"
# Install the BlockOS-side runtime configuration.
cd "$ROOT/../../.."
cp -a rootfs/system/etc/. "$DEST/System/etc/" 2>/dev/null || true
cp -a rootfs/system/share/. "$DEST/System/share/" 2>/dev/null || true
cp -a rootfs/system/etc/profile.d/. "$DEST/System/etc/profile.d/" 2>/dev/null || true
printf '%s\n' "Installed 11/12/13 runtime into $DEST/System"
