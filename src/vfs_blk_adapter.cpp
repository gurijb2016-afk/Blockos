#include "vfs_blk_adapter.hpp"
#include "virtio_blk_full.hpp"
#include <efi.h>
#include <efilib.h>

bool vfs_blk_adapter::init_backend() {
    return virtio_blk_full::init();
}

bool vfs_blk_adapter::read_sector_to_vfs(uint64_t sector, uint8_t* buf) {
    return virtio_blk_full::read_sector(sector, buf);
}

bool vfs_blk_adapter::write_sector_from_vfs(uint64_t sector, const uint8_t* buf) {
    return virtio_blk_full::write_sector(sector, buf);
}
