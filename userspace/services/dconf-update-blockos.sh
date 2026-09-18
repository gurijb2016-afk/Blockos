#!/bin/sh
set -eu
DB=/System/etc/dconf/db/blockos
CONF=/System/etc/dconf/db/blockos.d
mkdir -p "$DB" "$CONF"
if [ -x /System/bin/dconf ]; then
    /System/bin/dconf update
fi
