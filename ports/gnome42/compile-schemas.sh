#!/bin/sh
set -eu
SCHEMAS=${1:-/System/share/glib-2.0/schemas}
exec /System/bin/glib-compile-schemas "$SCHEMAS"
