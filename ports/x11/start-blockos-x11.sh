#!/bin/sh
set -eu
mkdir -p /tmp/.X11-unix
export DISPLAY=${DISPLAY:-:0}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/tmp/runtime-blockos}
mkdir -p "$XDG_RUNTIME_DIR"
exec /system/bin/X "$DISPLAY" -ac "$@"
