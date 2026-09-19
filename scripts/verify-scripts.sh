#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for s in "$BASE"/scripts/*.sh; do
  sh -n "$s"
done
printf '%s\n' "All shell scripts pass sh -n."
