#pragma once
#include "virtio_blk_full.hpp"

namespace virtio_blk_full {
    bool init();
    bool read_sector(uint64_t sector, uint8_t* out_buf);
    bool write_sector(uint64_t sector, const uint8_t* in_buf);
    // Read multiple contiguous sectors into out_buf (sectors * 512 bytes). Returns true on success.
    bool read_sectors(uint64_t sector, uint32_t count, uint8_t* out_buf);
    // Write multiple contiguous sectors from in_buf. Returns true on success.
    bool write_sectors(uint64_t sector, uint32_t count, const uint8_t* in_buf);
    // Flush device caches (if supported). Returns true on success.
    bool flush();
}
