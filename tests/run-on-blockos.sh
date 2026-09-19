#!/bin/sh
set -eu
# Run inside a booted BlockOS shell with /System/lib and /System/bin present.
for t in /System/tests/test-freetype /System/tests/test-harfbuzz /System/tests/test-pango; do
    echo "RUN $t"
    "$t"
done
