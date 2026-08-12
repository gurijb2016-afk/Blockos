# BlockOS NVIDIA integration

This directory contains the native BlockOS side of the NVIDIA driver port.

`blockos_nvidia.cpp` performs real PCI configuration-space enumeration using
BlockOS' existing `drivers/pci.*` backend and registers discovered NVIDIA
display hardware with `DeviceManager`.

The full NVIDIA open GPU kernel-module source is kept separately in the
`Nvidia.zip` package under `kernel-open/`. The NVIDIA source and its original
license/copyright notices are preserved.

This layer is intentionally not a fake GPU implementation. Hardware register
programming, DMA/VM setup, GSP/firmware startup, engines, modesetting and
userspace ABI still have to be ported from the NVIDIA open kernel modules to
the corresponding BlockOS kernel interfaces.
