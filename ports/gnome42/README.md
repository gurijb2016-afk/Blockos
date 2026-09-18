# BlockOS GNOME 42 graphics/userspace stack

This directory is the BlockOS-specific port/build glue for:

GLib -> GObject/GIO -> libffi -> Cairo -> Pango/HarfBuzz -> FreeType/Fontconfig -> GdkPixbuf -> GTK 4.

The upstream projects themselves are intentionally fetched at build time from their upstream release archives rather than copied into this repository/package.

Target sysroot: /system
Target architecture: x86-64 BlockOS

This package is a port layer, not a claim that all eight upstream projects are already runtime-proven on BlockOS.
