#pragma once
#include <cstdint>

#define EROFS_SUPER_MAGIC_V1 0xE0F5E1E2
#define EROFS_BLKSIZ         4096

// EROFS Szuperblokk struktúra (1024 bájtos offseten a lemezen)
struct [[gnu::packed]] ErofsSuperBlock {
    uint32_t magic;           // EROFS_SUPER_MAGIC_V1
    uint32_t checksum;
    uint32_t feature_compat;
    uint8_t  blkszbits;       // Blokkméret bit eltolás (általában 12 -> 4096)
    uint8_t  sb_extslots;
    uint16_t root_nid;        // Root inode azonosító (NID)
    uint64_t inos;            // Összes inode száma
    uint64_t blocks;          // Összes blokk száma
    uint32_t meta_blkaddr;    // Metaadatok kezdő címe
    uint32_t xattr_blkaddr;
    uint8_t  uuid[16];
    uint8_t  volume_name[16];
    uint32_t feature_incompat;
    union {
        uint8_t reserved[44];
        uint16_t empty_highbits;
    };
};

// Inode típusok
enum ErofsInodForm : uint16_t {
    EROFS_INODE_LAYOUT_COMPACT = 0,
    EROFS_INODE_LAYOUT_EXTENDED = 1
};

// Kompakt Inode struktúra (32 bájt)
struct [[gnu::packed]] ErofsInodeCompact {
    uint16_t format;          // Inode elrendezés típusa (Compact/Extended, típus)
    uint16_t xattr_isof;
    uint16_t mode;            // Jogosultságok és fájltípus (S_IFDIR, S_IFREG)
    uint16_t nlink;
    uint32_t size;            // Fájlméret bájtban
    uint32_t reserved;
    union {
        uint32_t raw_blkaddr; // Ha nem tömörített: a kezdő adatblokk címe
        uint32_t compressed_blocks;
    } u;
    uint32_t i_ino;           // Inode szám az r/osdev stat-hoz
};

// Könyvtár bejegyzés struktúra (Directory Entry)
struct [[gnu::packed]] ErofsDirEntry {
    uint64_t nid;             // Erre a névre mutató Inode NID
    uint16_t nameoff;         // Név offset a névtömbön belül
    uint8_t  file_type;       // Fájl típusa (Regular, Directory, stb.)
    uint8_t  reserved;
};
