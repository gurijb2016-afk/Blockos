#pragma once

namespace virtio_blk_full {
    bool init();
    bool read_sector(uint64_t sector, uint8_t* out_buf);
    bool write_sector(uint64_t sector, const uint8_t* in_buf);
}
