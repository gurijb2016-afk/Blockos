#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$HERE"
sh -n fetch-sources.sh
sh -n build.sh
sh -n runtime-check.sh
printf '%s\n' 'GNOME 42 14-16 scripts: syntax OK'
