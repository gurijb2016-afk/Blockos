#!/bin/sh
set -eu
export PREFIX=/System
export PKG_CONFIG_SYSROOT_DIR=/
export PKG_CONFIG_PATH=/System/lib/pkgconfig:/System/share/pkgconfig
export ACLOCAL_PATH=/System/share/aclocal
export CFLAGS="${CFLAGS:-} -D_BLOCKOS_ -I/System/include"
export CPPFLAGS="${CPPFLAGS:-} -D_BLOCKOS_ -I/System/include"
export LDFLAGS="${LDFLAGS:-} -L/System/lib -Wl,-rpath,/System/lib"
