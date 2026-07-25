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

The build environment can vary, but you will usually need a Linux development machine with tools such as:

* a C++ compiler (GCC or Clang)
* make
* autotools utilities if the project uses `configure.ac`
* binutils
* nasm or as
* qemu-system-x86_64
* grub or another bootloader toolchain depending on the current build flow

On Debian/Ubuntu-like systems, common packages may include:

```bash
sudo apt update
sudo apt install build-essential clang make autoconf automake libtool pkg-config qemu-system-x86 grub-pc-bin xorriso mtools
```

You may need additional packages depending on the branch.

## Build

If the repository uses autotools and contains `configure.ac`, generate the configure script first:

```bash
autoreconf -fi
```

Then configure and build:

```bash
./configure
make -j$(nproc)
```

If the project uses a different build system, follow the build files present in the repository.

## Run

The exact run command depends on the current boot flow and output image. Common patterns are:

```bash
make run
```

or

```bash
qemu-system-x86_64 -m 1024M -drive format=raw,file=arch/86_64x/kernel
```

If the repository provides a generated kernel ELF or ISO image, use the matching command for that output.

## Cleaning

Typical cleanup commands:

```bash
make clean
```

If those targets do not exist yet, remove generated artifacts manually.

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

### `./configure: No such file or directory`

If the repository contains `configure.ac`, run:

```bash
autoreconf -fi
./configure
```

### Missing compiler or toolchain

Install the required build tools and confirm that your cross-compiler or host compiler is available.

### QEMU boot problems

Check:

* bootloader configuration
* kernel image path
* initrd or rootfs path
* memory size passed to QEMU
* whether the image was rebuilt after code changes

### File permission issues

If scripts are not executable, fix them with:


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

## Screenshots

Add screenshots here when the GUI or boot output is stable enough to show.

## License

Add the license that matches your project policy.

## Credits

BlockOS is a personal operating system project built with a lot of experimentation, debugging, and iteration.

If you want to build your own OS too, study the code, experiment carefully, and keep going.
