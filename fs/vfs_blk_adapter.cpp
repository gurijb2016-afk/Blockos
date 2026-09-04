#include "vfs_blk_adapter.hpp"
#include "drivers/virtio_blk_full.hpp"
#include "drivers/virtio_blk.hpp"

namespace vfs_blk_adapter {

static virtio_blk_full g_backend;
static bool g_initialized = false;

bool init_backend()
{
    if (g_initialized)
        return true;

    if (!virtio_blk::init())
        return false;

    g_initialized = true;
    return true;
}

bool read_sector_to_vfs(uint64_t sector, uint8_t* buf)
{
    if (!buf)
        return false;

    if (!init_backend())
        return false;

    return g_backend.read_sector(sector, buf);
}

bool write_sector_from_vfs(uint64_t sector, const uint8_t* buf)
{
    if (!buf)
        return false;

    if (!init_backend())
        return false;

    return g_backend.write_sector(sector, buf);
}

bool read_sectors_to_vfs(
    uint64_t sector,
    uint32_t count,
    uint8_t* buf
)
{
    if (!buf || count == 0)
        return false;

    if (!init_backend())
        return false;

    return g_backend.read_sectors(
        sector,
        count,
        buf
    );
}

bool write_sectors_from_vfs(
    uint64_t sector,
    uint32_t count,
    const uint8_t* buf
)
{
    if (!buf || count == 0)
        return false;

    if (!init_backend())
        return false;

    return g_backend.write_sectors(
        sector,
        count,
        buf
    );
}

bool flush_backend()
{
    return init_backend();
}

}
