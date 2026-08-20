#pragma once

#include <stdint.h>

namespace sysmem
{

struct SystemMemoryRecord
{
    uint64_t total; // installed DRAM -> excludes MMIO and reserved ranges
    uint64_t free; // EfiConventionalMemory -> usable immediately
    uint64_t reclaimable; // EfiBootServicesCode + Data -> usable after exit
    uint64_t kernel; // EfiLoaderCode + Data -> image, heap, backbuffer, map
    uint64_t firmware; // RuntimeServices*, ACPI reclaim and NVS
    uint64_t largest_free; // largest single conventional run
    uint64_t highest_addr; // top of the highest DRAM descriptor
    uint64_t regions; // descriptors in the map
};

const SystemMemoryRecord& get_record();
void set_record(const SystemMemoryRecord& record);

} // namespace sysmem
