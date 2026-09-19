#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
"$BASE/ports/gnome42/fetch-sources.sh"
"$BASE/ports/gnome42/build-libffi.sh"
"$BASE/ports/gnome42/build-glib.sh"
"$BASE/ports/gnome42/build-cairo.sh"
printf '%s\n' 'GLib/GObject/GIO + libffi + Cairo build sequence completed.'
