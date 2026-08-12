# BlockOS port

This repository contains NVIDIA's open GPU kernel-module source plus the
BlockOS port work.

The upstream source is kept intact under `kernel-open/`. The BlockOS port is
being built against the BlockOS kernel interfaces for PCI, DMA, IRQ, memory,
process/threading, MMIO and firmware loading.

Current concrete integration point:
- BlockOS PCI device discovery and NVIDIA display-device registration.

The complete driver port is not represented by empty stubs. Remaining Linux
kernel interface dependencies must be replaced by real BlockOS implementations
before the NVIDIA modules can be claimed as fully functional on BlockOS.

See the top-level NVIDIA license files and SPDX headers for the applicable
license terms.
