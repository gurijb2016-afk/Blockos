#pragma once
#include <stdint.h>

// Minimal PCI config access helpers using CF8/CFC I/O ports.
// These functions assume access to I/O ports is permitted in the environment.

uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

// Find a PCI capability with given cap_id in the device's capability list.
// Returns offset of the capability (0 if not found).
uint8_t pci_find_capability(uint8_t bus, uint8_t device, uint8_t function, uint8_t cap_id);
