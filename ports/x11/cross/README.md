# BlockOS userland cross build (static, no ld.so)

Result so far: `hw/kdrive/blockos/X` links as a **static** BlockOS-target ELF
(no PT_INTERP, no dynamic section). It runs on BlockOS in QEMU: it starts, finishes
screen and keyboard setup (precompiled XKB keymap) and takes over the framebuffer
(`/devices/display`); the console stops repainting while X owns the display.

## Toolchain
- `ports/x11/cross/blockos-cc`: host gcc, `-ffreestanding -nostdinc`, BlockOS libc headers only.
- Link: `-static -nostdlib crt1.o libc.a libgcc.a` (see meson cross file below).
- `userspace/libc/build-libc.sh` (CC=gcc AS=as AR=ar) produces `libc.a`, `crt1.o`.
  `libc.so` is optional (duplicate symbols between sources; not needed for static).

## Cross files (absolute paths must be filled in for your checkout)
- `[binaries] c = '<root>/ports/x11/cross/blockos-cc'`, `ar = 'ar'`
- `[host_machine] system = 'blockos', cpu_family = 'x86_64'`
- `[built-in options] default_library = 'static'`
  `c_args = ['-isystem', <sysroot>/system/include, '-isystem', <sysroot>/system/include/pixman-1, '-pthread']`
  `c_link_args = ['-static','-nostdlib', crt1.o, '-L<libc.a dir>', '-L<sysroot>/system/lib', '-lc', libgcc.a]`
- pkg-config: `PKG_CONFIG_LIBDIR=<sysroot>/system/lib/pkgconfig` (the .pc files must point at the sysroot, not /system).
- X: `meson setup ... -Dxorg=false -Dblockos=true -Dxfbdev=false -Dxephyr=false -Dxvfb=false -Dxnest=false -Dglamor=false -Dgbm=false -Dglx=false -Ddri2=false -Ddri3=false -Dlisten_unix=true -Dlisten_local=true -Dsha1=libc`

## Dependencies built for BlockOS (sysroot)
- pixman-1 0.42.2 (meson, `-Dsse2=disabled` etc.) -> libpixman-1.a
- freetype 2.13.2 (meson, no zlib/png/bz2/harfbuzz/brotli) -> libfreetype.a
- libXau 1.0.9, libXdmcp 1.1.3 (compiled with `-DHASXDMAUTH=1`), libxkbfile 1.1.0,
  libxshmfence 1.3 (futex backend, config.h: HAVE_FUTEX=1, SHMDIR "/tmp"),
  libfontenc 1.1.8 (+ zlib shim), libXfont2 2.0.6 (fc/ and gunzip/bunzip2 excluded;
  stubs in `shims/xfont_stubs.c`), xtrans 1.4.0 (headers), X11 protocol headers.
- Empty `libm.a`, `libpthread.a`, `libdl.a`, `librt.a` in the sysroot (all functions live in libc.a).

## Shims (host-only workarounds used during analysis, not part of the build)
- presentproto/dri3proto/x11 `.pc` files saying 1.4 / x11 1.8.7 (headers on the host are older).
  Only needed for the host-side meson analysis; the cross build uses the sysroot headers.

## Status of the former blockers
1. Sockets (AF_UNIX, `/tmp/.X11-unix/X0`): working; `userspace/tests/x11_sock.c` passes.
2. XKB keymap: no `fork`/`xkbcomp` needed. `ddxLoad.c` (built with `-D__BLOCKOS__`, see
   `blockos-cc`) copies the precompiled `/system/share/X11/xkb/compiled/blockos-default.xkm`
   into the output directory with raw fds. Rules file: `/usr/share/X11/xkb/rules/evdev`.
3. XKB data and fonts are on the initramfs (`rootfs/system/share/X11/xkb`, `rootfs/usr/share/fonts/X11/misc`).
4. `.gz` / `.bz2` fonts: decoded by `shims/blockos_decomp.c` and `shims/blockos_bufio.c`.
   libXfont2 is built with `-DX_GZIP_FONT_COMPRESSION -DX_BZIP2_FONT_COMPRESSION`.
   Known fix: `PCF_SIZE_TO_INDEX` maps glyph pad 8 to index 3 (`include/X11/fonts/pcf.h`).

## Fixed kernel bugs found while bringing X up
- `brk`: shrinking the heap never unmapped pages, and growing it again re-mapped live
  pages to zeroed frames. The process now keeps `brk_mapped` (high-water mark).
- `map_4k` did not `invlpg`, so an alias created over an already-touched page (device
  mmap of `/devices/display`) stayed invisible through the TLB.
- `poll`/`select` never reported a listening unix socket as readable, so a server
  blocked in `select` never saw a pending connection. Fixed in `fd_readable`
  (`userspace/tests/x11_sock` passes with the fix).

## Client test (in progress)
- `userspace/tests/xclient.c`: raw X11 protocol client, no Xlib. Connects to `/tmp/.X11-unix/X0`
  (starts `/system/bin/X :0` itself if no server is listening), sends the connection setup,
  creates a window, maps it, creates a GC and fills a rectangle. Built into `rootfs/bin/xclient`.
- BlockOS sockets are non-blocking (empty read or full write returns `EAGAIN`); the client retries.
- Status: works. X accepts the client, the setup reply comes back, the window is created,
  mapped, filled (checked on screen: orange fill, blue border) and an Expose event arrives.
- `userspace/tests/epoll_sock.c` tests epoll on a listening unix socket (passes).

## Fixed in this round
- X stalled after `Screen(s) initialized`. Causes, all fixed:
  - `pthread_mutex` was not recursive; `input_lock()` re-entered from device init deadlocked.
    `pthread_mutex_t` now has owner/count/type fields and real recursive semantics.
  - `epoll_ctl` read the fd from `r8`; libc passes it in `rdx`. Also removed a check that
    rejected event pointers above 4 GB (the user stack is above 4 GB), so every ADD failed.
  - `epoll_wait`/`poll` did not report a listening socket with pending connections.
- libc: `stderr` was fully buffered, so server log lines were invisible. Now unbuffered.

## Still open
- twm needs libX11, libXt and libXmu (and libXext for shape). None of these are in the sysroot yet.
  The next step is building those ports against the BlockOS libc.
- The console flush is disabled while X owns the display; it is not re-enabled when X exits
  (only on panic / user fault flush).
