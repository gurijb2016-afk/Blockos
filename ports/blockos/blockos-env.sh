#!/bin/sh
set -eu

# Override these from the shell when integrating into a BlockOS checkout.
: "${BLOCKOS_SYSROOT:=/opt/blockos/sysroot}"
: "${BLOCKOS_STAGING:=/opt/blockos/staging}"
: "${BLOCKOS_CC:=x86_64-unknown-blockos-gcc}"
: "${BLOCKOS_CXX:=x86_64-unknown-blockos-g++}"
: "${BLOCKOS_AR:=x86_64-unknown-blockos-ar}"
: "${BLOCKOS_RANLIB:=x86_64-unknown-blockos-ranlib}"
: "${BLOCKOS_STRIP:=x86_64-unknown-blockos-strip}"

export BLOCKOS_SYSROOT BLOCKOS_STAGING
export CC="$BLOCKOS_CC" CXX="$BLOCKOS_CXX" AR="$BLOCKOS_AR" RANLIB="$BLOCKOS_RANLIB" STRIP="$BLOCKOS_STRIP"
export PKG_CONFIG_SYSROOT_DIR="$BLOCKOS_SYSROOT"
export PKG_CONFIG_LIBDIR="$BLOCKOS_SYSROOT/System/lib/pkgconfig:$BLOCKOS_SYSROOT/System/share/pkgconfig"
export CFLAGS="-m64 -O2 -fPIC -fno-plt -ffunction-sections -fdata-sections --sysroot=$BLOCKOS_SYSROOT"
export CXXFLAGS="$CFLAGS -fno-exceptions -fno-rtti"
export LDFLAGS="--sysroot=$BLOCKOS_SYSROOT -Wl,--gc-sections -Wl,-z,now -Wl,-z,relro -Wl,-rpath,/System/lib -Wl,-dynamic-linker,/System/lib/ld.so"
export CPPFLAGS="-I$BLOCKOS_SYSROOT/System/include -I$BLOCKOS_SYSROOT/System/include/freetype2 -I$BLOCKOS_SYSROOT/System/include/harfbuzz"
