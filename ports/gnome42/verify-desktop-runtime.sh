#!/bin/sh
set -u
fail=0
need() {
    p=$1
    if [ -x "$p" ]; then echo "OK   $p"; else echo "MISS $p"; fail=1; fi
}
need /System/bin/dbusd
need /System/bin/gnome-session
need /System/bin/gnome-shell
need /System/bin/mutter
need /System/bin/gjs
need /System/libexec/gvfsd
need /System/bin/dconf
need /System/bin/glib-compile-schemas
[ -f /System/share/gnome-session/sessions/gnome-blockos.session ] || { echo "MISS gnome-blockos.session"; fail=1; }
[ -f /System/share/glib-2.0/schemas/gschemas.compiled ] || { echo "MISS gschemas.compiled"; fail=1; }
exit "$fail"
