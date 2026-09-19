#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$BASE/scripts/fetch-sources.sh"
"$BASE/scripts/unpack-sources.sh"
"$BASE/scripts/build-freetype.sh"
"$BASE/scripts/build-harfbuzz.sh"
"$BASE/scripts/build-pango.sh"
"$BASE/scripts/verify-install.sh"
