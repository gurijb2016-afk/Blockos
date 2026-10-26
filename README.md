BlockOS

BlockOS is an independently developed, experimental x86-64 operating system written primarily in C++. It has its own kernel and is not based on the Linux kernel.

- Architecture: x86-64
- Boot: UEFI
- Kernel language: primarily C++, with assembly where required
- Status: active development; experimental and not production-ready
- License: GNU GPL-2.0 — see ""LICENSE"" (LICENSE)

Repository: https://github.com/gurijb2016-afk/Blockos

Project direction

The long-term goal is to develop BlockOS into a practical operating system for desktop computers, workstations, and servers. This requires a dependable kernel, native user space, hardware drivers, networking, storage, application compatibility, and extensive testing.

BlockOS is developed independently of the Linux kernel. It is not a Linux distribution. Compatibility interfaces may be implemented where useful, but the operating system is built around its own kernel and system components.

The codebase is evolving. A component being present in the repository does not necessarily mean it is complete, enabled in the default build, or reliable on physical hardware. The current source code and build scripts define what is actually built.

Kernel and boot process

BlockOS targets 64-bit x86 systems and uses UEFI for booting. The build produces "build/BOOTX64.EFI" from the kernel build output.

Kernel development includes:

- Early initialization and hardware discovery
- Memory management and paging
- Interrupt and exception handling
- Process execution and user-mode support
- System-call handling
- Device and storage integration

The completeness of individual subsystems depends on the current branch and build configuration.

Processes, ELF loading, and system calls

BlockOS contains development work for a native ELF64 loader and user-space process execution.

Development areas include:

- Loading and starting executable programs
- Process creation and termination
- Waiting for child processes
- Process control and signals
- System-call interfaces
- User-space execution and runtime support

The goal is to run real programs as processes under BlockOS, not to simulate an operating system inside another environment.

BlockOS is developing its own system-call ABI and user-space interfaces. Compatibility is incremental: a successful test of one executable or system call does not imply complete POSIX compatibility or the ability to run arbitrary Linux binaries.

Shell and user space

The repository contains work on "/bin/sh", native user-space programs, C runtime support, and libraries.

The user-space environment is intended to provide command-line utilities, file and process operations, networking tools, system administration commands, and the libraries required by applications.

Programs may still depend on system calls, runtime facilities, libraries, or filesystem content that are incomplete. Compatibility must be verified against the current build.

Filesystems and storage

Storage-related development includes:

- Virtual filesystem (VFS) integration
- File and directory operations
- Storage-device discovery
- ATA and AHCI-related driver work
- VirtIO storage development
- ext4-related filesystem work
- Persistent storage and filesystem integration

Support depends on the controller, device, filesystem implementation, configuration, and testing performed. The presence of code for a particular technology does not guarantee complete support for every device.

Networking

BlockOS includes networking and socket-related development. Work covers network interfaces, packet handling, socket operations, and protocol support.

The long-term objective is reliable networking for desktop applications and server workloads. Protocol coverage, driver support, interoperability, and reliability require continued implementation and testing.

A successful network test in QEMU does not guarantee compatibility with all physical network adapters or network configurations.

Graphics and X11

BlockOS has an X11-related codebase under ""kernel/x11/"" (kernel/x11/).

Development in this area includes integrating the X server with BlockOS, framebuffer and display output, keyboard and mouse input, and the runtime and libraries needed by X11 software.

BlockOS will not have its own custom GUI or its own custom window manager. The direction is to use existing X11 software and existing window managers instead of creating a separate BlockOS-only graphical environment.

The components that currently work together must be verified against the checked-out revision; the existence of X11-related code does not guarantee that every X11 application or desktop environment will run.

Long-term desktop compatibility

GNOME and KDE Plasma are long-term compatibility goals, not claims of production-ready support today.

Running full desktop environments requires substantial work across executable loading, dynamic linking, libc, threading, IPC and D-Bus, graphics, input, services, and application dependencies.

The repository contains GNOME integration and status documentation. Consult those documents for the specific prerequisites and limitations of that work.

Drivers and hardware

Hardware support is a major development priority.

Areas of work include:

- PCI device enumeration
- Storage controllers and disks
- Ethernet and other network adapters
- USB controllers and input devices
- Display hardware
- Audio devices
- Keyboard, mouse, and other peripherals

The project focuses on implementing and testing drivers incrementally. Not every PC, graphics card, network adapter, storage controller, or peripheral is supported.

Desktop and server goals

The long-term objectives include:

- A usable desktop with existing X11 applications and window managers
- Future compatibility with GNOME and KDE Plasma
- Native command-line and development tools
- Networking, persistent storage, and background services
- Support for long-running server workloads
- Stronger security boundaries and process isolation
- Better diagnostics, recovery, stability, and hardware compatibility

These are development goals, not a claim that every feature is already implemented or ready for production use.

Build requirements

The current top-level "Makefile" uses GNU-EFI and expects GNU-EFI headers and libraries in common Debian-style locations. The default compiler toolchain is GNU; a Clang option is also provided.

On Debian or Ubuntu, install the baseline tools:

sudo apt update
sudo apt install build-essential gnu-efi ovmf \
    qemu-system-x86 mtools dosfstools python3

This is a starting dependency list. Optional ports and desktop components may require additional libraries and tools.

Check the GNU-EFI paths:

make check-efi

If GNU-EFI is installed elsewhere, adjust the corresponding paths in the "Makefile".

Build BlockOS

From the repository root:

make

The default build generates the initramfs from "rootfs/", compiles the configured source files, links "build/kernel.so", and converts it into "build/BOOTX64.EFI".

The exact source list is controlled by the current "Makefile". Not every source file or optional port is necessarily included in the default target.

To select Clang where supported:

make TOOLCHAIN=clang

Run in QEMU

make run

The run script uses QEMU with OVMF UEFI firmware and prepares the boot and storage images according to the current script.

The exact image layout, controller configuration, and runtime options can change. Check ""build_and_run.sh"" (build_and_run.sh) and the "Makefile" for the behavior of the revision you are using.

The script may also support experimental USB input devices:

sh build_and_run.sh usb

This depends on the current script and QEMU configuration. Virtual hardware matters: a driver written for one controller model will not automatically support another.

Repository structure

Path| Purpose
"arch/"| Architecture-specific code
"drivers/"| Device and controller drivers
"fs/"| Filesystem and VFS-related code
"kernel/"| Kernel implementation, including X11-related work
"net/"| Networking code
"libc/"| C library development
"userspace/"| User-space runtime and components
"userspace/crt/"| C runtime startup support
"userspace/ldso/"| Dynamic-linker development
"rootfs/"| Files assembled into the initramfs
"ports/"| External software ports and integration work
"scripts/"| Build and helper scripts
"docs/"| Project documentation

Directory contents and build integration may change as development progresses.

Related documentation

- ""DESKTOP_STACK_STATUS.md"" (DESKTOP_STACK_STATUS.md) — GNOME desktop integration assumptions and runtime prerequisites.
- ""GNOME42_BLOCKOS_STATUS.md"" (GNOME42_BLOCKOS_STATUS.md) — GNOME 42 integration status.
- ""RUN_GNOME42.md"" (RUN_GNOME42.md) — instructions and requirements for GNOME 42 work.
- ""VIRTIO_ROADMAP.md"" (VIRTIO_ROADMAP.md) — VirtIO development notes.
- ""LUA_RING3_BUILD.md"" (LUA_RING3_BUILD.md) — Lua user-mode build notes.
- ""LICENSE"" (LICENSE) — project license.

These documents may describe experiments, prerequisites, or planned work. The presence of a document does not prove that its subject is complete.

Development principles

BlockOS development focuses on:

1. Making kernel and user-space interfaces work together.
2. Testing changes in QEMU and, where possible, on physical hardware.
3. Keeping the kernel independent of the Linux kernel.
4. Expanding native application support through a coherent ABI and runtime.
5. Recording known limitations accurately.
6. Avoiding claims of complete compatibility until it has been demonstrated.

Reporting problems

When reporting a build or runtime issue, include the branch and commit, host operating system, compiler/toolchain versions, exact command, and relevant error output. State whether the problem occurs in QEMU, on physical hardware, or both.

License

BlockOS is distributed under the GNU General Public License, version 2. See ""LICENSE"" (LICENSE) for the applicable terms.
