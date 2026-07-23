#pragma once
#include <stdint.h>
#include <stddef.h>

// --- EXT4 Szuperblokk struktúra (1024 bájtnál kezdődik a lemezen) ---
struct Ext4Superblock {
    uint32_t inodes_count;
    uint32_t blocks_count_lo;
    uint32_t r_blocks_count_lo;
    uint32_t free_blocks_count_lo;
    uint32_t free_inodes_count_lo;
    uint32_t first_data_block;
    uint32_t log_block_size;      // Blokkméret = 1024 << log_block_size
    uint32_t log_cluster_size;
    uint32_t blocks_per_group;
    uint32_t clusters_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;              // EXT4 Magic szám: 0xEF53
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    // ... (további mezők elhagyva a tömörség kedvéért)
} __attribute__((packed));

// --- Blokkcsoport Leíró (Block Group Descriptor) ---
struct Ext4GroupDesc {
    uint32_t block_bitmap_lo;
    uint32_t inode_bitmap_lo;
    uint32_t inode_table_lo;      // Az Inode tábla kezdőblokkja (alsó 32 bit)
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

// --- EXT4 Inode (Indexcsomópont struktúra, 256 bájt) ---
struct Ext4Inode {
    uint16_t mode;               // Fájl típusa és hozzáférési jogok
    uint16_t uid;                // Tulajdonos ID
    uint32_t size_lo;            // Fájlméret (alsó 32 bit)
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks_lo;
    uint32_t flags;
    uint32_t osd1;
    uint8_t  block[60];          // Éles adatok vagy az Extent Tree (Kiterjedési fa) gyökere!
    uint32_t generation;
    uint32_t file_acl_lo;
    uint32_t size_high;
    uint32_t obso_faddr;
    // ... (további mezők 256 bájtra kiegészítve)
} __attribute__((packed));

// --- EXT4 Extent Fa Fejléc (Extent Header) ---
struct Ext4ExtentHeader {
    uint16_t magic;              // Extent Magic: 0xF30A
    uint16_t entries;            // Bejegyzések száma ebben a csomópontban
    uint16_t max;                // Maximális bejegyzésszám
    uint16_t depth;              // A fa mélysége (0, ha ez egy levélcsomópont)
    uint32_t generation;
} __attribute__((packed));

// --- EXT4 Extent Levél Bejegyzés (Tényleges adattömb) ---
struct Ext4Extent {
    uint32_t block;              // Logikai blokkszám a fájlon belül
    uint16_t len;                // Blokkok száma ebben a kiterjedésben
    uint16_t start_hi;           // Fizikai blokk felső 16 bitje
    uint32_t start_lo;           // Fizikai blokk alsó 32 bitje
} __attribute__((packed));

// --- EXT4 Fájlrendszer Kezelő Osztály ---
class Ext4Reader {
private:
    uint32_t block_size;
    uint32_t inodes_per_group;
    uint32_t inode_size;
    uint64_t bg_desc_table_block;

    bool read_disk_block(uint64_t block_num, uint8_t* buffer);
    bool get_inode(uint32_t inode_num, Ext4Inode& target_inode);
    void parse_extent(Ext4ExtentHeader* header, uint8_t* dest, size_t& bytes_read, size_t max_size);

public:
    Ext4Reader() : block_size(4096), inodes_per_group(0), inode_size(256), bg_desc_table_block(0) {}
    bool mount();
    bool load_file_by_inode(uint32_t inode_num, uint8_t* dest, size_t max_size);
};
