#pragma once
#include "virtio_common.hpp"

// Detect MSI-X capability for the PCI device that corresponds to the provided DeviceHandle.
// If enable_if_found is true, attempt to clear the Function Mask to enable MSI-X (best-effort).
// Returns true if an MSI-X capability was found (and configured when requested).
bool pci_msix_detect_and_configure(virtio_common::DeviceHandle* h, bool enable_if_found);
