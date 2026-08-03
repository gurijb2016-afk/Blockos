#pragma once

#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)

struct VirtqDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct VirtqAvail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[0];
};

struct VirtqUsedElem {
    uint32_t id;
    uint32_t len;
};

struct VirtqUsed {
    uint16_t flags;
    uint16_t idx;
    VirtqUsedElem ring[0];
};

#pragma pack(pop)

namespace virtqueue {
    VirtqDesc* alloc_virtqueue(
        void* mem,
        uint32_t qsize,
        uint32_t* desc_count
    );
}
