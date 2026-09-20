#!/bin/sh
set -eu
ROOT=${ROOT:-/System}
fail=0
need() { if [ ! -e "$1" ]; then echo "MISSING: $1"; fail=1; else echo "OK: $1"; fi; }
need "$ROOT/bin/mutter"
need "$ROOT/bin/gnome-shell"
need "$ROOT/bin/gnome-session"
need "$ROOT/share/xsessions/gnome.desktop"
need "$ROOT/share/gnome-session/sessions/gnome.session"
need "$ROOT/lib"
need "$ROOT/bin/X"
need "/run"
need "/tmp"
if [ -z "${DISPLAY:-}" ]; then echo "MISSING ENV: DISPLAY"; fail=1; fi
if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then echo "MISSING ENV: DBUS_SESSION_BUS_ADDRESS"; fail=1; fi
exit "$fail"
