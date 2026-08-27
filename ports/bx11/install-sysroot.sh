#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT=$(CDPATH= cd -- "$ROOT/../.." && pwd)
SYSROOT=${BLOCKOS_SYSROOT:-$PROJECT/build/sysroot}
mkdir -p "$SYSROOT/usr/include/X11/extensions" "$SYSROOT/usr/include/xcb" "$SYSROOT/usr/include/X11" "$SYSROOT/usr/lib" "$SYSROOT/usr/lib/pkgconfig"
cp -a "$ROOT/include/X11/." "$SYSROOT/usr/include/X11/"
cp -a "$ROOT/include/xcb/." "$SYSROOT/usr/include/xcb/"
make -C "$ROOT" host-test >/dev/null
OBJS="$ROOT/libX11/display.host.o $ROOT/libX11/extensions.host.o $ROOT/libX11/xft.host.o $ROOT/render/framebuffer.host.o $ROOT/input/input.host.o $ROOT/server/server.host.o $ROOT/server/protocol.host.o $ROOT/ipc/transport.host.o"
ar rcs "$SYSROOT/usr/lib/libX11-bx11.a" $OBJS
ar rcs "$SYSROOT/usr/lib/libxcb-bx11.a" "$ROOT/libxcb/xcb.host.o" "$ROOT/server/server.host.o" "$ROOT/render/framebuffer.host.o" "$ROOT/input/input.host.o" "$ROOT/ipc/transport.host.o"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libX11.a"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libXext.a"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libXrandr.a"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libXinerama.a"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libXrender.a"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libXfixes.a"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libXext.a"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libXshape.a"
cp -f "$SYSROOT/usr/lib/libX11-bx11.a" "$SYSROOT/usr/lib/libXft.a"
cp -f "$SYSROOT/usr/lib/libxcb-bx11.a" "$SYSROOT/usr/lib/libxcb.a"
cat > "$SYSROOT/usr/lib/pkgconfig/x11.pc" <<PC
prefix=/usr
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: x11
Description: BlockOS BX11 compatibility Xlib
Version: 1.0
Cflags: -I\${includedir}
Libs: -L\${libdir} -lX11 -lstdc++
PC
cat > "$SYSROOT/usr/lib/pkgconfig/xft.pc" <<PC
prefix=/usr
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: xft
Description: BlockOS BX11 Xft compatibility shim
Version: 1.0
Cflags: -I\${includedir}
Libs: -L\${libdir} -lXft -lX11 -lstdc++
PC
printf '%s\n' "BX11 headers, X11 extensions, Xft shim and compatibility archives installed into $SYSROOT"
