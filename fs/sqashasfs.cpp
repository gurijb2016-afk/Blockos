#include "fs/vfs.hpp"
#include "kernel/allocator.hpp"
#include "kernel/spinlock.hpp"
#include "drivers/virtio_blk.hpp"

#include <stdint.h>
#include <stddef.h>

namespace Blockos {

/* ============================================================
 * Freestanding string/memory segédfüggvények
 * ============================================================ */

static size_t str_len(const char* s)
{
    if (!s)
        return 0;

    size_t n = 0;

    while (s[n] != '\0')
        ++n;

    return n;
}

static void mem_copy(void* dst, const void* src, size_t n)
{
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    for (size_t i = 0; i < n; ++i)
        d[i] = s[i];
}

static int str_cmp(const char* a, const char* b)
{
    if (!a || !b)
        return (a == b) ? 0 : 1;

    size_t i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];

        ++i;
    }

    return (unsigned char)a[i] - (unsigned char)b[i];
}


/* ============================================================
 * SquashFS superblock
 * ============================================================ */

struct squashfs_super_block
{
    uint32_t s_magic;
    uint32_t inodes;
    uint32_t mkfs_time;
    uint32_t block_size;
    uint32_t fragments;

    uint16_t compression;
    uint16_t block_log;
    uint16_t flags;
    uint16_t no_ids;
    uint16_t s_major;
    uint16_t s_minor;

    uint64_t root_inode;
    uint64_t bytes_used;

    uint64_t id_table_start;
    uint64_t xattr_table_start;
    uint64_t inode_table_start;
    uint64_t directory_table_start;
    uint64_t fragment_table_start;
    uint64_t lookup_table_start;
};


/* ============================================================
 * Egyszerű dekompressziós interfész
 *
 * FONTOS:
 * Ez jelenleg csak olyan blokkot tud közvetlenül másolni,
 * amely ténylegesen nincs tömörítve.
 * ============================================================ */

static bool squashfs_decompress(
    uint16_t algo,
    const uint8_t* src,
    size_t src_len,
    uint8_t* dest,
    size_t dest_len)
{
    if (!src || !dest)
        return false;

    if (algo != 1)
        return false;

    if (src_len > dest_len)
        src_len = dest_len;

    mem_copy(dest, src, src_len);

    return true;
}


/*
 * ============================================================
 * IDE NE ÍRJUNK MÉG VFS::FileSystem ÖRÖKLÉST
 * ============================================================
 *
 * A te buildedben:
 *
 *     VFS has not been declared
 *
 * Ez azt jelenti, hogy a fs/vfs.hpp nem azt az API-t tartalmazza,
 * amelyre a korábbi SquashFS kód épült.
 *
 * Ezért a következő lépés előtt a tényleges fs/vfs.hpp alapján
 * kell megírni a SquashFS adaptert.
 */

} // namespace Blockos
