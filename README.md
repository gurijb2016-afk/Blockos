BlockOS

BlockOS is an independently developed, experimental 64-bit x86-64 operating system written primarily in C++.

BlockOS is built around its own kernel, system call interface, executable loader, user-space environment, device support, networking stack, and graphical system. It is not based on the Linux kernel.

The long-term vision is to develop BlockOS into a reliable, self-contained operating system for desktop computers, workstations, and servers, with support for everyday applications and full desktop environments.

Status: Active development — experimental

BlockOS is not yet a production-ready general-purpose operating system. Development focuses on implementing, integrating, and testing individual components while expanding hardware and software compatibility.

Goals

The primary goal is to build a complete operating system that can eventually be used for everyday computing and long-running server workloads.

Desktop and everyday use

BlockOS aims to support a complete graphical desktop and become suitable for long-term daily use.

Long-term goals include:

- Running full desktop environments such as GNOME and KDE Plasma.
- Supporting software and components from the KDE neon ecosystem, where their dependencies and interfaces can be provided.
- Providing a graphical desktop, window management, keyboard and mouse input, audio, networking, and display support.
- Supporting web browsers, multimedia applications, development tools, and general-purpose desktop software.
- Providing a consistent native user-space environment and system libraries.
- Improving stability, usability, installation, upgrades, and hardware compatibility.

KDE neon is a Linux-based distribution rather than a desktop environment itself. The relevant goal for BlockOS is compatibility with KDE Plasma and the applications and components associated with that ecosystem.

Server use

BlockOS also aims to become suitable for server environments, including systems that need to run continuously.

Long-term goals include:

- Running long-lived background services and server applications.
- Providing reliable TCP/IP networking and socket interfaces.
- Supporting storage devices and filesystems.
- Offering process isolation, permissions, resource management, and security controls.
- Supporting remote administration and automated system management.
- Improving reliability, diagnostics, recovery, and performance under sustained workloads.

Independent operating system

BlockOS is intended to remain independent of the Linux kernel.

The project aims to develop its own kernel and native system components, including the system call interface, executable loading, device drivers, user-space libraries, and system utilities. Compatibility layers and application interfaces may be implemented where needed to support additional software.

Core development areas

Kernel and boot process

BlockOS targets 64-bit x86-64 systems and uses UEFI for booting.

Kernel development includes:

- UEFI boot and early initialization
- Memory management and paging
- Process and thread management
- User-mode execution
- System call handling
- Interrupt and exception handling
- Hardware discovery and device management

Kernel components are under active development, and their completeness depends on the current branch and build.

Processes and executable loading

BlockOS includes work on a native ELF64 loader and user-space process execution.

Development areas include:

- Loading and starting executable programs
- Process creation and termination
- Waiting for child processes
- Signals and process control
- System call interfaces
- User-space memory and execution context management

The objective is to execute real programs as user-space processes through BlockOS's own interfaces rather than simulate an operating system inside another environment.

Shell and user space

BlockOS has development work on a real "/bin/sh" and native user-space programs.

The user-space environment is being expanded to provide:

- A command-line shell
- Native executable utilities
- Process and file operations
- Networking tools
- System administration commands
- Libraries and APIs required by applications

Compatibility and functionality vary by program. A successful test of one executable does not imply that all POSIX applications or Linux binaries are compatible.

System calls and libraries

BlockOS is developing its own system call layer and user-space library infrastructure.

The goals include:

- A consistent system call ABI
- Process and file operations
- Memory management interfaces
- Socket and networking interfaces
- Signals and synchronization primitives
- Runtime support for native executables
- Libraries needed by increasingly complex applications

The project aims to avoid unnecessary dependence on Linux-specific runtime assumptions and to provide the interfaces required by its own user space.

Virtual filesystem and storage

Filesystem and storage development includes:

- Virtual filesystem (VFS) integration
- File and directory operations
- Storage device discovery
- ATA and AHCI-related driver work
- VirtIO storage support
- ext4-related filesystem work
- Device nodes, mount points, and persistent storage

Filesystem support and storage compatibility must be verified against the actual implementation and target hardware.

Networking

BlockOS includes networking and socket-related development.

Areas of work include:

- Network interface support
- Ethernet and packet handling
- TCP/IP networking
- Socket operations
- Client-server communication
- DNS and other network services
- Network device drivers

Further development is required to expand protocol coverage, improve interoperability, and establish reliability for everyday and server use.

Graphics and desktop

BlockOS is developing a graphical environment based on X11.

Development includes:

- Framebuffer and display-device integration
- X server compatibility
- Keyboard and mouse input
- Window manager support
- Cursor and font integration
- X11 applications and supporting libraries

The immediate direction is a lightweight graphical desktop. Larger desktop environments such as GNOME and KDE Plasma are long-term goals and will require substantial additional work on graphics, input, libraries, system services, application interfaces, and hardware support.

Hardware and drivers

Hardware support is a major part of BlockOS development.

Target areas include:

- PCI device enumeration
- Storage controllers and disks
- Ethernet and other network adapters
- USB controllers and USB input devices
- Display adapters and graphics hardware
- Audio devices
- Keyboard, mouse, and other peripherals

The project prioritizes modular, testable driver implementations rather than adding large amounts of unverified code. Device support depends on the hardware, driver implementation, and testing performed.

Not every PC, graphics card, network adapter, or peripheral is supported.

Security and reliability

Security and reliability are essential to the long-term goal of daily desktop and server use.

Planned development areas include:

- Process isolation and privilege boundaries
- User and group management
- Permissions and access control
- Memory safety and fault handling
- Resource limits and process management
- Logging and diagnostics
- Service supervision and recovery
- Testing under different workloads and hardware configurations

These areas require continued implementation and testing before BlockOS can be considered suitable for production workloads.

Building and running

The build system and its requirements may change as kernel and user-space components evolve. Always check the current Makefile and build scripts for the exact requirements of the branch you are using.

The development environment uses a C++ toolchain, GNU-EFI, GNU Make, and QEMU with OVMF firmware.

On Debian or Ubuntu, a starting set of dependencies is:

sudo apt update
sudo apt install build-essential gnu-efi ovmf \
    qemu-system-x86 mtools dosfstools python3

The repository may require additional tools or libraries depending on the current source tree and enabled components.

Check the EFI toolchain

make check-efi

This checks the GNU-EFI paths expected by the build system. If your distribution installs the files elsewhere, adjust the relevant paths in the Makefile.

Build

make

This invokes the repository's build system to compile and link the components configured for the current branch.

Run in QEMU

make run

Where supported by the current build scripts, this launches BlockOS in QEMU using OVMF UEFI firmware.

The exact boot image, storage layout, and runtime configuration depend on the checked-out branch. Consult "Makefile" and "build_and_run.sh" for the current behavior.

Run with experimental USB input devices

If the current run script supports the USB option:

sh build_and_run.sh usb

QEMU machine type and controller configuration affect which virtual hardware is available. For example, a legacy ATA driver and an AHCI controller do not use the same hardware interface.

Important: Begin testing in a virtual machine with disposable disk images. Experimental operating-system code can crash, fail to boot, or corrupt storage when tested on physical hardware.

Cleaning build output

The following targets are available where defined by the current Makefile:

make clean
make rebuild

"make clean" removes the build artifacts covered by the current clean target. It does not necessarily remove persistent disk images, generated configuration files, or unrelated Autotools output.

Before deleting any image, check whether it contains data you need to preserve.

Development workflow

BlockOS is developed incrementally, with an emphasis on integration and testing.

Typical areas of work include:

1. Kernel boot and initialization
2. Memory management and CPU facilities
3. Process execution and system calls
4. Executable loading and native libraries
5. VFS and filesystem support
6. Hardware discovery and device drivers
7. Networking and sockets
8. Shell and user-space utilities
9. Graphics, X11, and window management
10. Security, reliability, and application compatibility

These areas are interconnected and may be developed in parallel. This list is not a claim that each subsystem is complete.

Contributing

Contributions, testing, bug reports, and technical discussion are welcome.

Areas where help is particularly valuable include:

- x86-64 kernel development
- Storage and filesystem drivers
- PCI, USB, and input-device support
- Ethernet and network drivers
- Graphics and audio drivers
- Networking protocols
- ELF loading and native libraries
- X11 compatibility and window managers
- Testing, debugging, and documentation

If you want to contribute, open a GitHub issue or discussion explaining the subsystem you are interested in and what you would like to work on.

Contributions should be understandable, reviewable, and tested where possible. Please distinguish tested behavior from assumptions and incomplete implementations.

BlockOS is an operating-system development project, not an advertisement for a product for sale.

Roadmap

Long-term development goals include:

- More complete process and thread management
- Expanded native system call and library coverage
- Broader filesystem and storage support
- More network protocols and device drivers
- Improved user and group management
- Stronger security and process isolation
- Reliable service and background-process management
- A more complete native user-space environment
- A stable X11-based graphical desktop
- Support for larger desktop environments, including GNOME and KDE Plasma
- Compatibility with software from the KDE ecosystem, including relevant KDE neon components
- Better installer, system administration, and developer tools
- Improved reliability for everyday desktop and server workloads

The roadmap is subject to change as implementation, testing, and hardware support progress.

License

BlockOS is licensed under the GNU General Public License, version 2 (GPL-2.0).

See "LICENSE" (LICENSE) for the full license text.

Credits

BlockOS is an independently developed operating-system project built through systems programming, experimentation, debugging, and testing.

Thanks to everyone who studies the project, reports issues, shares technical knowledge, tests hardware compatibility, and contributes to independent operating-system development.
