#include "fs/vfs.hpp"
#include "kernel/allocator.hpp"
#include "drivers/virtio_blk.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace Blockos {

/*
 * EXT2 superblock.
 *
 * Az EXT2 superblock a lemez 1024. bájtján kezdődik.
 */
struct ext2_super_block {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;

    uint32_t s_first_ino;
    uint16_t s_inode_size;
};

/*
 * EXT2 block group descriptor.
 */
struct ext2_block_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;

    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;

    uint32_t bg_reserved[3];
};

/*
 * EXT2 inode.
 */
struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;

    uint32_t i_size;

    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;

    uint16_t i_gid;
    uint16_t i_links_count;

    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;

    uint32_t i_block[15];

    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;

    uint8_t i_osd2[12];
};

/*
 * EXT2 filesystem állapota.
 */
static ext2_super_block g_superblock;
static uint32_t g_block_size = 1024;
static int g_disk_id = 0;
static bool g_initialized = false;


/*
 * Egy 512 bájtos szektor beolvasása.
 */
static bool read_sector(
    uint64_t sector,
    uint8_t* buffer
) {
    if (buffer == nullptr)
        return false;

    return virtio_blk::read_sector(
        sector,
        buffer
    );
}


/*
 * EXT2 blokk beolvasása.
 *
 * Az EXT2 blokk több 512 bájtos szektorból állhat.
 */
static bool read_block(
    uint32_t block,
    uint8_t* buffer
) {
    if (buffer == nullptr)
        return false;

    if (g_block_size == 0)
        return false;

    uint32_t sectors_per_block =
        g_block_size / 512;

    if (sectors_per_block == 0)
        return false;

    uint64_t first_sector =
        static_cast<uint64_t>(block) *
        sectors_per_block;

    for (uint32_t i = 0;
         i < sectors_per_block;
         ++i) {

        if (!read_sector(
                first_sector + i,
                buffer + (i * 512)
            )) {
            return false;
        }
    }

    return true;
}


/*
 * EXT2 superblock beolvasása.
 */
static bool read_superblock()
{
    uint8_t* buffer =
        reinterpret_cast<uint8_t*>(
            allocator::alloc(1024)
        );

    if (buffer == nullptr)
        return false;

    /*
     * Az EXT2 superblock a 1024. bájtnál
     * kezdődik, tehát a 2-es 512 bájtos
     * szektortól kell olvasni.
     */
    if (!read_sector(2, buffer) ||
        !read_sector(3, buffer + 512)) {

        /*
         * A projekt allocator API-ja nem
         * tartalmaz free()-t.
         *
         * Ezért itt nem hívunk allocator::free()-t.
         */
        return false;
    }

    memcpy(
        &g_superblock,
        buffer,
        sizeof(ext2_super_block)
    );

    /*
     * EXT2 magic.
     */
    if (g_superblock.s_magic != 0xEF53)
        return false;

    /*
     * EXT2 blokkméret:
     *
     * 1024 << s_log_block_size
     */
    if (g_superblock.s_log_block_size > 6)
        return false;

    g_block_size =
        1024u << g_superblock.s_log_block_size;

    if (g_block_size < 1024)
        return false;

    if ((g_block_size % 512) != 0)
        return false;

    return true;
}


/*
 * Block Group Descriptor beolvasása.
 */
static bool read_group_descriptor(
    uint32_t group,
    ext2_block_group_desc* descriptor
) {
    if (descriptor == nullptr)
        return false;

    uint8_t* buffer =
        reinterpret_cast<uint8_t*>(
            allocator::alloc(g_block_size)
        );

    if (buffer == nullptr)
        return false;

    /*
     * 1024 bájtos blokkméretnél a group
     * descriptor a 2-es blokkon kezdődik.
     *
     * Nagyobb blokkméretnél az 1-es blokkon.
     */
    uint32_t descriptor_block =
        (g_block_size == 1024)
            ? 2
            : 1;

    if (!read_block(
            descriptor_block,
            buffer
        )) {
        return false;
    }

    size_t offset =
        static_cast<size_t>(group) *
        sizeof(ext2_block_group_desc);

    if (offset +
        sizeof(ext2_block_group_desc) >
        g_block_size) {
        return false;
    }

    memcpy(
        descriptor,
        buffer + offset,
        sizeof(ext2_block_group_desc)
    );

    return true;
}


/*
 * Inode beolvasása.
 */
static bool read_inode(
    uint32_t inode_number,
    ext2_inode* inode
) {
    if (inode == nullptr)
        return false;

    if (inode_number == 0)
        return false;

    if (g_superblock.s_inodes_per_group == 0)
        return false;

    uint32_t group =
        (inode_number - 1) /
        g_superblock.s_inodes_per_group;

    uint32_t index =
        (inode_number - 1) %
        g_superblock.s_inodes_per_group;

    ext2_block_group_desc descriptor;

    if (!read_group_descriptor(
            group,
            &descriptor
        )) {
        return false;
    }

    uint32_t inode_size = 128;

    if (g_superblock.s_rev_level >= 1 &&
        g_superblock.s_inode_size != 0) {

        inode_size =
            g_superblock.s_inode_size;
    }

    if (inode_size < sizeof(ext2_inode))
        return false;

    uint32_t byte_offset =
        index * inode_size;

    uint32_t inode_block =
        descriptor.bg_inode_table +
        (byte_offset / g_block_size);

    uint32_t offset_in_block =
        byte_offset % g_block_size;

    uint8_t* buffer =
        reinterpret_cast<uint8_t*>(
            allocator::alloc(g_block_size)
        );

    if (buffer == nullptr)
        return false;

    if (!read_block(
            inode_block,
            buffer
        )) {
        return false;
    }

    if (offset_in_block +
        sizeof(ext2_inode) >
        g_block_size) {
        return false;
    }

    memcpy(
        inode,
        buffer + offset_in_block,
        sizeof(ext2_inode)
    );

    return true;
}


/*
 * EXT2 inicializálása.
 */
bool ext2_initialize(
    int disk_id
) {
    g_disk_id = disk_id;

    if (!read_superblock())
        return false;

    g_initialized = true;

    return true;
}


/*
 * EXT2 inode lekérése.
 */
bool ext2_get_inode(
    uint32_t inode_number,
    ext2_inode* inode
) {
    if (!g_initialized)
        return false;

    return read_inode(
        inode_number,
        inode
    );
}


/*
 * EXT2 blokkméret lekérése.
 */
uint32_t ext2_get_block_size()
{
    return g_block_size;
}


/*
 * EXT2 magic ellenőrzése.
 */
bool ext2_is_initialized()
{
    return g_initialized;
}


/*
 * EXT2 root inode.
 *
 * Az EXT2 root inode mindig 2.
 */
bool ext2_read_root_inode(
    ext2_inode* inode
) {
    return ext2_get_inode(
        2,
        inode
    );
}

} // namespace Blockos
