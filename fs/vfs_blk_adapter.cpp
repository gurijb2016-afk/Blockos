#include "vfs_blk_adapter.hpp"
#include "drivers/virtio_blk_full.hpp"

namespace vfs_blk_adapter {

/*
 * A virtio_blk_full jelenlegi API-ja példánymetódusokat használ.
 *
 * Nincs:
 *     virtio_blk_full::init()
 *     virtio_blk_full::flush()
 *
 * Ezért egyetlen backend példányt használunk.
 */

static virtio_blk_full g_backend;
static bool g_initialized = false;


/* ============================================================
 * Backend inicializálása
 * ============================================================ */

bool init_backend()
{
    /*
     * A jelenlegi virtio_blk_full.hpp alapján nincs külön
     * init() függvény.
     *
     * Az objektum konstrukciója önmagában nem jelez hibát,
     * ezért itt egyszerűen aktívnak tekintjük a backendet.
     */

    g_initialized = true;
    return true;
}


/* ============================================================
 * Egy szektor olvasása
 * ============================================================ */

bool read_sector_to_vfs(
    uint64_t sector,
    uint8_t* buf
)
{
    if (!buf) {
        return false;
    }

    if (!g_initialized) {
        if (!init_backend()) {
            return false;
        }
    }

    return g_backend.read_sector(
        sector,
        buf
    );
}


/* ============================================================
 * Egy szektor írása
 * ============================================================ */

bool write_sector_from_vfs(
    uint64_t sector,
    const uint8_t* buf
)
{
    if (!buf) {
        return false;
    }

    if (!g_initialized) {
        if (!init_backend()) {
            return false;
        }
    }

    return g_backend.write_sector(
        sector,
        buf
    );
}


/* ============================================================
 * Több szektor olvasása
 * ============================================================ */

bool read_sectors_to_vfs(
    uint64_t sector,
    uint32_t count,
    uint8_t* buf
)
{
    if (!buf || count == 0) {
        return false;
    }

    if (!g_initialized) {
        if (!init_backend()) {
            return false;
        }
    }

    return g_backend.read_sectors(
        sector,
        count,
        buf
    );
}


/* ============================================================
 * Több szektor írása
 * ============================================================ */

bool write_sectors_from_vfs(
    uint64_t sector,
    uint32_t count,
    const uint8_t* buf
)
{
    if (!buf || count == 0) {
        return false;
    }

    if (!g_initialized) {
        if (!init_backend()) {
            return false;
        }
    }

    return g_backend.write_sectors(
        sector,
        count,
        buf
    );
}


/* ============================================================
 * Flush
 * ============================================================ */

bool flush_backend()
{
    /*
     * A jelenlegi virtio_blk_full.hpp-ben nincs flush().
     *
     * Ezért jelenleg nincs külön flush művelet.
     *
     * A függvényt azért megtartjuk, hogy a VFS adapter API-ja
     * később bővíthető legyen anélkül, hogy a hívókat át kelljen
     * írni.
     */

    if (!g_initialized) {
        if (!init_backend()) {
            return false;
        }
    }

    return true;
}

} // namespace vfs_blk_adapter
