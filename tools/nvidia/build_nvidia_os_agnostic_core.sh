#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    echo "Usage: $0 /path/to/open-gpu-kernel-modules [output-dir]" >&2
    echo "Builds NVIDIA's upstream src/nvidia OS-agnostic core object only." >&2
}

if [[ $# -lt 1 || $# -gt 2 ]]; then usage; exit 2; fi
NVIDIA_ROOT="$(cd "$1" && pwd)"
OUTPUT_DIR="${2:-$PWD/build/nvidia-core}"

for f in "$NVIDIA_ROOT/Makefile" "$NVIDIA_ROOT/utils.mk" \
         "$NVIDIA_ROOT/src/nvidia/Makefile" "$NVIDIA_ROOT/src/nvidia/srcs.mk"; do
    if [[ ! -f "$f" ]]; then echo "ERROR: expected NVIDIA source file not found: $f" >&2; exit 3; fi
done

command -v make >/dev/null || { echo 'ERROR: make is required' >&2; exit 4; }
command -v gcc >/dev/null || { echo 'ERROR: gcc is required' >&2; exit 4; }
command -v g++ >/dev/null || { echo 'ERROR: g++ is required' >&2; exit 4; }
command -v readelf >/dev/null || { echo 'ERROR: readelf is required for object verification' >&2; exit 4; }

# NVIDIA's published open module snapshot provides a Linux kernel-module
# package. This target compiles only src/nvidia's relocatable OS-agnostic core;
# deliberately do NOT call the root `modules` target or kernel-open/kbuild.
BUILD_SUBDIR="_out/BlockOS_x86_64"
make -C "$NVIDIA_ROOT/src/nvidia" \
    TARGET_OS=Linux TARGET_ARCH=x86_64 \
    CC="${CC:-gcc}" CXX="${CXX:-g++}" \
    OUTPUTDIR="$BUILD_SUBDIR" \
    OUTPUTDIR_ABSOLUTE="$NVIDIA_ROOT/src/nvidia/$BUILD_SUBDIR" \
    NV_VERBOSE=1

OBJECT="$NVIDIA_ROOT/src/nvidia/$BUILD_SUBDIR/nv-kernel.o"
if [[ ! -s "$OBJECT" ]]; then
    echo "ERROR: NVIDIA make completed without expected object: $OBJECT" >&2
    find "$NVIDIA_ROOT/src/nvidia" -type f -name 'nv-kernel.o' -print >&2 || true
    exit 5
fi

readelf -h "$OBJECT" | grep -Eq 'Class:[[:space:]]+ELF64' || {
    echo 'ERROR: output is not ELF64' >&2; exit 6;
}
readelf -h "$OBJECT" | grep -Eq 'Machine:[[:space:]]+Advanced Micro Devices X86-64' || {
    echo 'ERROR: output is not x86-64' >&2; exit 6;
}
mkdir -p "$OUTPUT_DIR"
cp -f "$OBJECT" "$OUTPUT_DIR/nv-kernel.o"
readelf -h "$OUTPUT_DIR/nv-kernel.o" > "$OUTPUT_DIR/nv-kernel.elf-header.txt"
echo "BUILT: $OUTPUT_DIR/nv-kernel.o"
echo 'NOTE: this is an upstream relocatable core object, not a BlockOS-loadable driver.'
echo '      It still requires the BlockOS OS-abstraction/PCI/DMA/IRQ/GSP integration.'
