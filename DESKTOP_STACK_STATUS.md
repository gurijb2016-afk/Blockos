# BlockOS GNOME 42 desktop integration

This package contains the BlockOS-specific integration layer for:

- dconf / GSettings
- GVfs
- GJS (required by GNOME Shell)
- Mutter 42.x on X11
- GNOME Shell 42.x on X11
- gnome-session 42.0

It includes fetch/build scripts, session files, dconf profile/configuration, service definitions, a desktop session launcher, and runtime verification.

The upstream GNOME sources are fetched by `ports/gnome42/fetch-desktop.sh` from the GNOME download server rather than copied into this archive.

## Important runtime requirements

The integration assumes the earlier BlockOS work already supplies:

- working dynamic ELF + ld.so + TLS
- stable libc + pthread + futex
- Unix sockets and D-Bus
- a working X11 server
- GLib/GIO/GObject/Cairo/Pango/GTK dependencies
- a graphics backend usable by Mutter
- X11 input events

GNOME Shell 42 also depends on GJS, so GJS is included here even though it was not in the original five-item list.
