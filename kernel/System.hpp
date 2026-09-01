#pragma once

#include <stdint.h>

namespace sysmem
{

struct SystemMemoryRecord
{
    uint64_t total;
    uint64_t free;
    uint64_t reclaimable;
    uint64_t kernel;
    uint64_t firmware;

    uint64_t largest_free;
    uint64_t highest_addr;

    uint64_t regions;
};

const SystemMemoryRecord& get_record();

void set_record(
    const SystemMemoryRecord& record);

} // namespace sysmem
