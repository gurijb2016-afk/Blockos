# BlockOS GNOME 42 – full dependency build

This package builds the GNOME 42 dependency stack from upstream source for BlockOS x86-64.
It does NOT embed upstream source archives; `fetch-sources.sh` downloads the pinned source
archives into `sources/`, and `build-all.sh` configures/builds/installs them into a BlockOS
sysroot/staging tree.

## Prerequisites

- BlockOS x86-64 cross compiler/toolchain
- Meson + Ninja on the build host
- pkg-config (cross environment)
- Python 3
- curl or wget
- tar/xz
- a BlockOS sysroot containing libc, crt objects, headers and `/system/lib/ld.so`

Set `BLOCKOS_SYSROOT` and `BLOCKOS_TOOLCHAIN` (or edit `config/environment.sh`).

## Build

    ./fetch-sources.sh
    ./build-all.sh
    ./verify.sh

Install result is under `out/rootfs/System` and libraries/binaries under the BlockOS
sysroot configured by `BLOCKOS_SYSROOT`.

The script stops on the first failed package. This is intentional: a successful script
run means every requested package completed its configure/build/install stage; it does
not claim that the resulting desktop has been runtime-tested until QEMU runs it.
