#pragma once
#include <cstdint>

namespace virtio_blk {
    bool init(); // returns true if a virtio-blk device was initialized
    // sector-level read/write (512-byte sectors)
    bool read_sector(uint64_t sector, uint8_t* out_buf);
    bool write_sector(uint64_t sector, const uint8_t* in_buf);
}
