#pragma once
#include <stdint.h>

// PCI config space helpers (legacy CF8/CFC I/O mechanism)
uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_cfg_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t pci_cfg_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

void pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void pci_cfg_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);
void pci_cfg_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value);

uint64_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar_index);

struct PciAddress { uint8_t bus, slot, func; };

bool pci_device_exists(uint8_t bus, uint8_t slot, uint8_t func);
