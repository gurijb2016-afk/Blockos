#pragma once
#include <stdint.h>

// Very small VFS adapter that maps a simple sector-based read/write API to the virtio block driver.
namespace vfs_blk_adapter {
    bool init_backend();
    bool read_sector_to_vfs(uint64_t sector, uint8_t* buf);
    bool write_sector_from_vfs(uint64_t sector, const uint8_t* buf);
}
