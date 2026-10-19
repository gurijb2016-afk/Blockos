#pragma once
#include <stddef.h>
#include <stdint.h>

struct PciDevice;

namespace hardware_inventory {
struct Entry {
    const PciDevice* device;
    const char* vendor;
    const char* model;
    const char* class_description;
    const char* subclass_description;
    const char* driver_hint;
    bool has_msi;
    bool has_msix;
};

// Read-only APIs. These functions never enable bus mastering or interrupts.
size_t count();
bool get(size_t index, Entry* out);
void print_report();
}
