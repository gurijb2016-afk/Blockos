#!/bin/sh
# Simple stress test script that repeatedly calls a userspace test binary or UEFI app
# The script assumes you have a test program `virtio_net_stress` inside the guest that
# sends many small packets using the virtio-net driver. This script runs QEMU and
# provides a guideline for stress testing.

# Example usage (host): ./scripts/run_virtio_net_stress.sh disk.img
IMG=${1:-disk.img}

qemu-system-x86_64 \
  -bios /usr/share/ovmf/OVMF_CODE.fd \
  -drive file=${IMG},format=raw,id=drive0 \
  -device virtio-blk-pci,drive=drive0 \
  -netdev user,id=net0 \
  -device virtio-net-pci,netdev=net0 \
  -m 1024 \
  -nographic

# Inside the guest, run the stress binary (not provided here):
# virtio_net_stress --packets 100000 --size 64
