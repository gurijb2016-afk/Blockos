# BlockOS GNOME 42 runtime integration

This package contains the BlockOS-side runtime layers and build pipeline for a GNOME 42/X11 port.

## Build

Use the BlockOS cross toolchain from the development environment:

```sh
cd Blockos-uefi-kernel-scaffold
export CC=x86_64-elf-gcc
export AS=x86_64-elf-as
export AR=x86_64-elf-ar
./build-gnome42.sh
```

The build pipeline covers:

1. dynamic libc + libpthread + TLS
2. `ld.so`
3. D-Bus bootstrap service
4. dynamic TLS/pthread ELF smoke test
5. BlockOS X11 KDrive integration
6. GNOME 42 dependency stack and session components

The intended runtime library paths are:

```text
/system/lib/ld.so
/system/lib/libc.so
/system/lib/libpthread.so
...
```

and the D-Bus service paths are:

```text
/run/dbus/system_bus_socket
/run/user/0/bus
```

The X11 BlockOS devices are:

```text
/devices/display
/devices/x11-input
```

## Dynamic ELF smoke test

The test binary is built as a dynamic ELF with `PT_INTERP`, `PT_TLS`, `DT_NEEDED` and TLS relocations. The prebuilt host-validated copies are under:

```text
userspace/tests/prebuilt/
```

## Important limitation

The integration code and build pipeline are included, but a full GNOME 42 boot was not claimed in this container because the user's `x86_64-elf-*` cross toolchain, Meson installation and QEMU runtime are not available here. The final validation must be done in the BlockOS development environment/QEMU (or on target hardware).
