# What this completes

This package provides the source/build integration for:

- GLib: GList/GHashTable/GVariant/GMainLoop/GThread/GError and the rest of
  the GLib API surface.
- GObject: type system, properties, signals, reference counting and closures
  (part of the GLib source tree).
- GIO: GFile, GInputStream/GOutputStream, GDBus, GApplication and related
  I/O abstractions (part of the GLib source tree).
- libffi: x86_64 foreign-function calls used by the GNOME ecosystem.
- Cairo: image and optional X11 client rendering backends.

What is still runtime-dependent on BlockOS:

- the actual BlockOS libc must implement every required POSIX/pthread/time/
  dl* API used by these libraries;
- pkg-config metadata must be installed into the BlockOS sysroot;
- the X11 client ABI must exist before Cairo xlib/xcb backends are useful;
- dynamic loading must use the BlockOS ld.so already being ported;
- shared libraries must be loaded and executed by a real BlockOS/QEMU boot.
