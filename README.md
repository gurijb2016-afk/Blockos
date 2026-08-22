# BlockOS

BlockOS is an experimental operating system project built from scratch in C++.

It is designed as a learning and research OS with its own kernel, syscall layer, VFS work, service management ideas, networking work, and GUI direction. The project is still under active development.

## Status

**Experimental / work in progress**

BlockOS is not a general-purpose production operating system yet. Many parts are still being built, tested, and integrated.

## Goals

BlockOS aims to explore and implement:

* a custom kernel architecture
* process and thread management
* ELF loading
* a VFS layer
* filesystem support such as ext4-related work
* a service/init system
* a networking stack
* security and sandboxing ideas
* a modern GUI direction
* user-space programs and developer tools

## Features already under development

Depending on the current branch and build state, the repository may include work on:

* kernel boot and early init
* paging and memory management
* scheduler design
* syscall handling and emulation
* ELF loader
* VFS and filesystem integration
* ext4-related filesystem code
* VirtIO and other driver work
* networking components
* service/daemon management
* GUI / window manager experiments
* user-space utilities

The exact layout may change as the project evolves.

## Prerequisites

BlockOS boots as a UEFI application built against gnu-efi, and runs under QEMU with
OVMF firmware. You need a Linux development machine (WSL works) with:

| Tool | Used for |
| --- | --- |
| `g++`, `ld`, `objcopy` | compiling and linking the kernel |
| `make` | the build |
| gnu-efi | UEFI headers, `libefi.a`, `libgnuefi.a`, `crt0-efi-x86_64.o`, linker script |
| OVMF | UEFI firmware for QEMU; the default SeaBIOS is legacy-BIOS only |
| `qemu-system-x86_64` | running the image |
| `mtools` (`mmd`, `mcopy`) | populating the FAT boot image |
| `dosfstools` (`mkfs.vfat`) | creating the FAT boot image |
| `python3` | `make menuconfig` only |

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential gnu-efi ovmf qemu-system-x86 mtools dosfstools python3
```

The Makefile expects gnu-efi in the usual Debian locations (`/usr/include/efi`,
`/usr/lib`). To check them before building:

```bash
make check-efi
```

Adjust the paths at the top of the `Makefile` if your distribution installs
gnu-efi elsewhere.

## Build

```bash
make
```

This compiles the sources in `drivers/`, `fs/`, `kernel/`, `examples/`, and
`libc/src/`, links `build/kernel.so`, then converts it to `build/BOOTX64.EFI`.

Those directories are globbed one level deep only, so files in subdirectories are
not picked up automatically. Anything nested has to be added to `SRC` in the
`Makefile` by name.

Other targets:

```bash
make clean       # remove objects, dependency files, and build/
make rebuild     # clean, then build
make check-efi   # verify the gnu-efi toolchain paths
```

## Run

```bash
make run
```

This builds if needed, then runs `build_and_run.sh`, which:

1. Creates `disk.img`, a 16MB FAT image, and copies `BOOTX64.EFI` to
   `EFI/BOOT/BOOTX64.EFI` on it. Anything in `persistent/` is copied to the image
   root. This image is rebuilt from scratch on every run.
2. Creates `data.img`, a 16MB raw image, if it is missing or the wrong size.
   Unlike `disk.img` this one persists between runs.
3. Boots QEMU with OVMF, attaching both images as IDE drives.

The two images land in different IDE slots:

| Image | Slot | Purpose |
| --- | --- | --- |
| `disk.img` | primary master | EFI system partition, booted by OVMF |
| `data.img` | primary slave | raw storage for the ATA driver |

If OVMF is not found automatically, pass its path:

```bash
OVMF=/path/to/OVMF.fd make run
```

To run with experimental USB tablet/mouse devices, invoke the script directly:

```bash
sh build_and_run.sh usb
```

Note that this depends on QEMU's default i440fx machine, which provides the
legacy IDE controller at ports 0x1F0/0x170. The `q35` machine has AHCI instead
and the ATA driver will not find a drive there.

Once booted, type `help` in the console for the available commands.

## Cleaning

```bash
make clean       # remove build output
make rebuild     # clean, then build
```

`make clean` removes three things: the object files, the `.d` dependency files,
and the `build/` directory. Note that objects are compiled **in place**, next to
their sources rather than under `build/`, so a clean touches the source tree and
not just one output directory.

Neither disk image is removed:

* `disk.img` is rebuilt from scratch on every run, so there is nothing to clean.
* `data.img` is left alone deliberately. It is persistent storage, and anything
  written with `ata-write` lives there.

To reset persistent storage, or if `data.img` ever ends up corrupt or the wrong
size, just delete it:

```bash
rm data.img
```

The next run recreates it as a zeroed image of the correct size.

Autotools leftovers such as `config.log`, `config.status`, and `autom4te.cache/`
are not covered by `make clean`, and there is no `distclean` target. Remove those
by hand if you need to.

## Development workflow

A good way to work on BlockOS is to focus on one subsystem at a time:

1. kernel boot and early init
2. memory management
3. process/scheduler integration
4. syscall interface
5. VFS and filesystem support
6. drivers
7. networking
8. security
9. user space
10. GUI and applications

## Adding new code

When contributing new modules, try to keep the code:

* small and readable
* separated by subsystem
* documented with comments where needed
* easy to test in QEMU
* consistent with the existing style

## Troubleshooting

### QEMU boot problems

Check:

* bootloader configuration
* kernel image path
* initrd or rootfs path
* memory size passed to QEMU
* whether the image was rebuilt after code changes

### File permission issues

If scripts are not executable, fix them with: ``chmod +x *.sh``

## Roadmap

Possible future work includes:

* stronger user and group management
* capabilities and sandboxing
* audit logging
* a more complete POSIX layer
* TCP/IP networking features
* more device drivers
* package management
* service supervision
* graphical shell and desktop components
* better documentation for developers and testers

## License

BlockOS is licensed under the **GNU General Public License, version 2**. The full
text is in [LICENSE](LICENSE).

Because the GPL is a copyleft license, anything distributed as a combined work
with BlockOS is covered by the same terms, and source must be made available to
whoever receives a binary.

## Credits

BlockOS is a personal operating system project built with a lot of experimentation, debugging, and iteration.

If you want to build your own OS too, study the code, experiment carefully, and keep going.