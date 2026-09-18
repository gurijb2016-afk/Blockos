# BlockOS GNOME 42 runtime integration

This tree contains the BlockOS-side runtime required by a GNOME 42/X11 port:

- dynamic ELF loader (`userspace/ldso`), including PT_TLS, TLS relocations, GNU/ELF hash lookup and x86-64 dynamic relocations;
- kernel `arch_prctl(ARCH_SET_FS/GET_FS)`, per-thread FS base and scheduler preservation;
- dynamic `libc.so`, separately linked `libpthread.so`, TLS-backed `errno`, pthread clone/mutex/condition variables;
- futex wait/wake, clone, wait4, signals/procmask/action bootstrap;
- AF_UNIX stream sockets, socketpair, sendmsg/recvmsg, poll/epoll and D-Bus bootstrap daemon;
- BlockOS X11 KDrive framebuffer `/devices/display` + `/system/display.info` integration;
- BlockOS X11 input queue `/devices/x11-input` with the exact KDrive event ABI;
- GNOME 42 build scripts and pinned source fetcher.

The complete GNOME desktop stack still has to be cross-compiled and run on the real BlockOS toolchain/QEMU. The repository includes the integration and build pipeline, but this environment does not contain the user's `x86_64-elf-*` cross toolchain, so a successful BlockOS boot of GNOME 42 has not been claimed here.
