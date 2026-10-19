#pragma once
#include <stdint.h>

// Small, kernel-resident PCI identity hints. Unknown IDs are deliberately
// preserved and reported numerically; absence from this table is not failure.
namespace pci_id_db {
const char* vendor_name(uint16_t vendor);
const char* device_name(uint16_t vendor, uint16_t device);
const char* class_name(uint8_t class_id);
const char* subclass_name(uint8_t class_id, uint8_t subclass, uint8_t prog_if);
const char* driver_hint(uint16_t vendor, uint16_t device,
                        uint8_t class_id, uint8_t subclass, uint8_t prog_if);
bool has_driver_hint(uint16_t vendor, uint16_t device,
                     uint8_t class_id, uint8_t subclass, uint8_t prog_if);
}
