BlockOS

An independent x86-64 operating system built from its own kernel.

BlockOS is an experimental operating system project written primarily in C++ and assembly. It boots through UEFI and is developed independently of the Linux kernel.

- Architecture: x86-64
- Boot: UEFI
- Kernel: Custom BlockOS kernel
- Graphics: X11 integration
- Language: C++, C and assembly
- Status: Experimental — active development
- License: "GNU GPL-2.0" (LICENSE)

Repository: "gurijb2016-afk/Blockos" (https://github.com/gurijb2016-afk/Blockos)

Screenshots

Screenshots will be added here as verified images from BlockOS.

<!-- Replace these examples with actual screenshots committed to the repository. --><!--
![BlockOS X11](docs/screenshots/x11.png)
-->Features and development

BlockOS development includes:

- UEFI boot and x86-64 kernel initialization
- Memory management, interrupts and hardware discovery
- ELF64 program loading and user-space execution
- System-call and process-management development
- "/bin/sh" and native user-space components
- Virtual filesystem and storage support
- Networking and socket interfaces
- PCI and device-driver development
- X11 server integration and framebuffer output
- C library and runtime development

The codebase is under active development. A source file being present does not guarantee that its feature is complete or works on every machine. BlockOS is not production-ready, and Linux binary or full POSIX compatibility is not implied.

Desktop

BlockOS aims to run existing graphical software rather than introduce a separate, custom GUI or window manager. X11 and existing window managers are the intended direction.

GNOME and KDE Plasma compatibility are longer-term goals and must not be considered complete or production-ready.

Build requirements

The top-level Makefile uses GNU-EFI, a C++ compiler, GNU binutils and Python 3. QEMU and OVMF are used for virtual-machine testing.

Debian / Ubuntu

sudo apt update
sudo apt install build-essential gnu-efi ovmf \
    qemu-system-x86 mtools dosfstools python3 git

Arch Linux

sudo pacman -Syu
sudo pacman -S base-devel gnu-efi edk2-ovmf \
    qemu-system-x86 mtools dosfstools python git

Fedora

sudo dnf install gcc-c++ make binutils gnu-efi \
    edk2-ovmf qemu-system-x86 mtools dosfstools python3 git

Package names and GNU-EFI file locations may differ between distributions and releases. The current Makefile expects GNU-EFI files in common Debian-style locations by default.

Build and run

Clone the repository and enter the project directory:

git clone --branch uefi-kernel-scaffold \
    https://github.com/gurijb2016-afk/Blockos.git
cd Blockos

Check the GNU-EFI installation:

make check-efi

Build BlockOS:

make

Run using the repository's QEMU/UEFI script:

make run

The expected EFI output is:

build/BOOTX64.EFI

Build and runtime success depend on the current source revision and installed dependencies. Review the build output if a command fails.

Contributing

Contributions, bug reports and code improvements are welcome.

1. Fork the repository.
2. Create a branch for your changes.
3. Make a focused change.
4. Build and test it where possible.
5. Open a pull request describing the change and the tests performed.

For bugs, include the exact command, error output, host distribution, compiler version and whether the issue occurred in QEMU or on physical hardware.

Pull request guidelines

- Explain what changed and why.
- Keep changes focused and reviewable.
- Do not claim a feature works without testing it.
- Include relevant logs or screenshots when they help demonstrate the result.
- Disclose significant AI assistance where appropriate and review AI-generated code before submitting it.

Using AI to write code

AI tools may be used to help develop BlockOS, including generating code, explaining errors, debugging and preparing tests. AI-generated code is not automatically correct.

Contributors remain responsible for understanding, reviewing and testing their submissions. Pull requests are subject to the same review requirements regardless of whether the code was written manually or with AI assistance.

License

BlockOS is distributed under the "GNU General Public License version 2" (LICENSE). See the "LICENSE" file for the full license text.
