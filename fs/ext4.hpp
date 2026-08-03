#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * ============================================================
 * EXT4 Superblock
 * ============================================================
 *
 * Az EXT4 superblock a lemez 1024. bájtján kezdődik.
 *
 * Fontos:
 * A jelenlegi struktúra csak a reader számára szükséges
 * mezőket tartalmazza.
 */
struct Ext4Superblock
{
    uint32_t inodes_count;
    uint32_t blocks_count_lo;
    uint32_t r_blocks_count_lo;
    uint32_t free_blocks_count_lo;
    uint32_t free_inodes_count_lo;

    uint32_t first_data_block;

    uint32_t log_block_size;
    uint32_t log_cluster_size;

    uint32_t blocks_per_group;
    uint32_t clusters_per_group;
    uint32_t inodes_per_group;

    uint32_t mtime;
    uint32_t wtime;

    uint16_t mnt_count;
    uint16_t max_mnt_count;

    uint16_t magic;

    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;

    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;

    uint32_t rev_level;

} __attribute__((packed));


/*
 * ============================================================
 * EXT4 Group Descriptor
 * ============================================================
 */
struct Ext4GroupDesc
{
    uint32_t block_bitmap_lo;
    uint32_t inode_bitmap_lo;
    uint32_t inode_table_lo;

    uint16_t free_blocks_count_lo;
    uint16_t free_inodes_count_lo;
    uint16_t used_dirs_count_lo;

    uint16_t flags;

    uint32_t exclude_bitmap_lo;

    uint16_t block_bitmap_csum_lo;
    uint16_t inode_bitmap_csum_lo;

    uint16_t itable_unused_lo;

    uint16_t checksum;

} __attribute__((packed));


/*
 * ============================================================
 * EXT4 Inode
 * ============================================================
 *
 * A struktúra első 60 bájtja az i_block mező.
 *
 * EXT4 esetén ez tartalmazhat:
 *
 *   - direct block pointereket
 *   - vagy Extent Tree gyökeret
 */
struct Ext4Inode
{
    uint16_t mode;
    uint16_t uid;

    uint32_t size_lo;

    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;

    uint16_t gid;
    uint16_t links_count;

    uint32_t blocks_lo;

    uint32_t flags;

    uint32_t osd1;

    /*
     * EXT4 i_block = 60 bájt
     */
    uint8_t block[60];

    uint32_t generation;

    uint32_t file_acl_lo;

    uint32_t size_high;

    uint32_t obso_faddr;

} __attribute__((packed));


/*
 * ============================================================
 * EXT4 Extent Header
 * ============================================================
 */
struct Ext4ExtentHeader
{
    uint16_t magic;
    uint16_t entries;
    uint16_t max;
    uint16_t depth;

    uint32_t generation;

} __attribute__((packed));


/*
 * ============================================================
 * EXT4 Extent
 * ============================================================
 *
 * Egy extent megadja:
 *
 *   block    = logikai blokk
 *   len      = blokkok száma
 *   start_hi = fizikai blokk felső része
 *   start_lo = fizikai blokk alsó része
 */
struct Ext4Extent
{
    uint32_t block;

    uint16_t len;

    uint16_t start_hi;

    uint32_t start_lo;

} __attribute__((packed));


/*
 * ============================================================
 * EXT4 Directory Entry
 * ============================================================
 *
 * A könyvtárakban található bejegyzésekhez használható.
 */
struct Ext4DirectoryEntry
{
    uint32_t inode;

    uint16_t rec_len;

    uint8_t name_len;

    uint8_t file_type;

} __attribute__((packed));


/*
 * ============================================================
 * EXT4 Reader
 * ============================================================
 */
class Ext4Reader
{
private:

    /*
     * Aktuális EXT4 blokk mérete:
     *
     * 1024
     * 2048
     * 4096
     */
    uint32_t block_size;

    /*
     * Inode-ok száma blokkcsoportonként.
     */
    uint32_t inodes_per_group;

    /*
     * Egy inode mérete.
     *
     * Régi EXT4:
     *   128
     *
     * Modern EXT4:
     *   általában 256
     */
    uint32_t inode_size;

    /*
     * Block Group Descriptor Table kezdő blokkja.
     */
    uint64_t bg_desc_table_block;


    /*
     * Egy EXT4 logikai blokk beolvasása.
     *
     * A VirtIO driver 512 bájtos szektorokon keresztül
     * szolgáltat adatot.
     */
    bool read_disk_block(
        uint64_t block_num,
        uint8_t* buffer
    );


    /*
     * Inode beolvasása inode száma alapján.
     */
    bool get_inode(
        uint32_t inode_num,
        Ext4Inode& target_inode
    );


    /*
     * Extent Tree feldolgozása.
     */
    void parse_extent(
        Ext4ExtentHeader* header,
        uint8_t* dest,
        size_t& bytes_read,
        size_t max_size
    );


public:

    /*
     * Konstruktor.
     */
    Ext4Reader()
        : block_size(4096),
          inodes_per_group(0),
          inode_size(256),
          bg_desc_table_block(0)
    {
    }


    /*
     * EXT4 fájlrendszer csatolása.
     *
     * Ellenőrzi a superblockot és beállítja:
     *
     *   block_size
     *   inodes_per_group
     *   inode_size
     *   bg_desc_table_block
     */
    bool mount();


    /*
     * Fájl betöltése inode alapján.
     *
     * inode_num:
     *     EXT4 inode száma
     *
     * dest:
     *     célmemória
     *
     * max_size:
     *     maximálisan betölthető méret
     */
    bool load_file_by_inode(
        uint32_t inode_num,
        uint8_t* dest,
        size_t max_size
    );


    /*
     * Fájl vagy könyvtár inode-jának megkeresése
     * abszolút útvonal alapján.
     *
     * Példák:
     *
     *   /
     *   /boot
     *   /boot/kernel
     *   /bin/busybox
     */
    uint32_t find_file_inode(
        const char* path
    );
};


/*
 * Globális EXT4 fájlrendszer példány.
 *
 * Például:
 *
 *   filesystem.mount();
 *
 *   filesystem.find_file_inode("/bin/busybox");
 */
extern Ext4Reader filesystem;
