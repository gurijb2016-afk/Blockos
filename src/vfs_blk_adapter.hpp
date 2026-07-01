#pragma once

namespace vfs_blk_adapter {
    bool init_backend();
    bool read_sector_to_vfs(uint64_t sector, uint8_t* buf);
    bool write_sector_from_vfs(uint64_t sector, const uint8_t* buf);
    bool read_sectors_to_vfs(uint64_t sector, uint32_t count, uint8_t* buf);
    bool write_sectors_from_vfs(uint64_t sector, uint32_t count, const uint8_t* buf);
    bool flush_backend();
}
