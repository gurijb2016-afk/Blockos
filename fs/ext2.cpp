#include "fs/vfs.hpp"
#include "kernel/allocator.hpp"
#include "kernel/spinlock.hpp"
#include "drivers/virtio_blk.hpp"

namespace Blockos {

// Az Ext2 szabványos belső struktúrái (Hivatalos Linux specifikáció)
struct ext2_super_block {
    uint32_t s_inodes_count;      // Összes inode a fájlrendszerben
    uint32_t s_blocks_count;      // Összes blokk a fájlrendszerben
    uint32_t s_r_blocks_count;    // Foglalt blokkok a root számára
    uint32_t s_free_blocks_count; // Szabad blokkok száma
    uint32_t s_free_inodes_count; // Szabad inode-ok száma
    uint32_t s_first_data_block;  // Első adatblokk (0 vagy 1)
    uint32_t s_log_block_size;    // Blokkméret eltolás (1024 << s_log_block_size)
    uint32_t s_log_frag_size;     // Fragmentméret eltolás
    uint32_t s_blocks_per_group;  // Blokkok száma csoportonként
    uint32_t s_frags_per_group;   // Fragmentek száma csoportonként
    uint32_t s_inodes_per_group;  // Inode-ok száma csoportonként
    uint32_t s_mtime;             // Utolsó csatolás ideje
    uint32_t s_wtime;             // Utolsó írás ideje
    uint16_t s_mnt_count;         // Csatolások száma az utolsó ellenőrzés óta
    uint16_t s_max_mnt_count;     // Maximális csatolási szám az ellenőrzés előtt
    uint16_t s_magic;            // Magic szám: 0xEF53
    uint16_t s_state;            // Fájlrendszer állapota (1 = Tiszta, 2 = Hibás)
    uint16_t s_errors;           // Mi a teendő hiba esetén
    uint16_t s_minor_rev_level;   // Minor revíziós szint
    uint32_t s_lastcheck;         // Utolsó ellenőrzés ideje
    uint32_t s_checkinterval;     // Maximális idő az ellenőrzések között
    uint32_t s_creator_os;        // Létrehozó OS (0 = Linux)
    uint32_t s_rev_level;         // Revíziós szint (0 = Eredeti, 1 = Dinamikus)
    uint16_t s_def_resuid;        // Alapértelmezett UID a foglalt blokkokhoz
    uint16_t s_def_resgid;        // Alapértelmezett GID a foglalt blokkokhoz
    // Dinamikus revízió esetén (s_rev_level >= 1) az alábbi mezők is érvényesek:
    uint32_t s_first_ino;         // Első nem foglalt inode (Klasszikus EXT2-nél 11)
    uint16_t s_inode_size;        // Inode struktúra mérete bájtban (Klasszikus: 128)
};

struct ext2_block_group_desc {
    uint32_t bg_block_bitmap;      // Blokk bitmap blokkszáma
    uint32_t bg_inode_bitmap;      // Inode bitmap blokkszáma
    uint32_t bg_inode_table;       // Inode tábla első blokkszáma
    uint16_t bg_free_blocks_count; // Szabad blokkok száma a csoportban
    uint16_t bg_free_inodes_count; // Szabad inode-ok száma a csoportban
    uint16_t bg_used_dirs_count;   // Könyvtárak száma a csoportban
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
};

struct ext2_inode {
    uint16_t i_mode;        // Fájl típusa és hozzáférési jogok
    uint16_t i_uid;         // Felhasználói azonosító (Low 16 bits)
    uint32_t i_size;        // Fájl mérete bájtban
    uint32_t i_atime;       // Utolsó hozzáférés ideje
    uint32_t i_ctime;       // Létrehozás ideje
    uint32_t i_mtime;       // Utolsó módosítás ideje
    uint32_t i_dtime;       // Törlés ideje
    uint16_t i_gid;         // Csoport azonosító (Low 16 bits)
    uint16_t i_links_count; // Linkek száma
    uint32_t i_blocks;      // Blokkok száma (512 bájtos szektorokban kifejezve)
    uint32_t i_flags;       // Fájl flagek
    uint32_t i_osd1;        // OS specifikus adat
    uint32_t i_block[15];   // Pointerek az adatblokkokra (12 direkt, 1 egyszeres, 1 kétszeres, 1 háromszoros indirekt)
    uint32_t i_generation;  // Fájl verzió (NFS-hez)
    uint32_t i_file_acl;    // Fájl ACL
    uint32_t i_dir_acl;     // Könyvtár ACL (Könyvtáraknál), Fájl méret high (Fájloknál)
    uint32_t i_faddr;       // Fragment címe
    uint8_t  i_osd2[12];     // OS specifikus adat
};

struct ext2_dir_entry {
    uint32_t inode;     // Inode száma
    uint16_t rec_len;   // Bejegyzés hossza bájtban (Igazítva a következő elemhez)
    uint8_t  name_len;  // Fájlnév hossza
    uint8_t  file_type; // Fájl típusa (1 = Fájl, 2 = Könyvtár stb.)
    char     name[255]; // Fájlnév (Nem feltétlenül null-terminált lemezen)
};

class Ext2FileSystem : public VFS::FileSystem {
private:
    Spinlock m_lock;
    ext2_super_block m_sb;
    int m_disk_id;
    uint32_t m_block_size;

    // Segédfunkció egy konkrét blokk beolvasására a VirtIO meghajtóból [source: 2]
    bool read_block(uint32_t block_num, uint8_t* buffer) {
        uint32_t sectors_per_block = m_block_size / 512;
        uint32_t start_sector = block_num * sectors_per_block;
        return VirtIOBlk::read_blocks(m_disk_id, start_sector, sectors_per_block, buffer);
    }

    // Inode beolvasása az Ext2 Inode táblából a lemezről
    bool read_inode(uint32_t inode_num, ext2_inode& inode_out) {
        if (inode_num < 1) return false;

        // Kiszámoljuk, melyik Block Group-ban van az inode
        uint32_t group = (inode_num - 1) / m_sb.s_inodes_per_group;
        uint32_t index = (inode_num - 1) % m_sb.s_inodes_per_group;

        // Beolvassuk a Group Descriptort (A 2. blokkban kezdődnek, az 1024 bájtos boot szektor után)
        uint32_t gd_block = (m_block_size == 1024) ? 2 : 1;
        uint8_t* gd_buffer = reinterpret_cast<uint8_t*>(KernelAllocator::alloc(m_block_size));
        if (!read_block(gd_block, gd_buffer)) {
            KernelAllocator::free(gd_buffer);
            return false;
        }

        ext2_block_group_desc* bgd = &reinterpret_cast<ext2_block_group_desc*>(gd_buffer)[group];
        uint32_t inode_table_block = bgd->bg_inode_table;

        // Kiszámoljuk a pontos pozíciót az inode táblán belül
        uint32_t inode_size = (m_sb.s_rev_level >= 1) ? m_sb.s_inode_size : 128;
        uint32_t byte_offset = index * inode_size;
        uint32_t target_block = inode_table_block + (byte_offset / m_block_size);
        uint32_t offset_in_block = byte_offset % m_block_size;

        uint8_t* table_buffer = reinterpret_cast<uint8_t*>(KernelAllocator::alloc(m_block_size));
        if (!read_block(target_block, table_buffer)) {
            KernelAllocator::free(gd_buffer);
            KernelAllocator::free(table_buffer);
            return false;
        }

        memcpy(&inode_out, table_buffer + offset_in_block, sizeof(ext2_inode));

        KernelAllocator::free(gd_buffer);
        KernelAllocator::free(table_buffer);
        return true;
    }

public:
    Ext2FileSystem(int disk_id) : m_disk_id(disk_id), m_block_size(1024) {}

    bool initialize() {
        uint8_t* boot_sector = reinterpret_cast<uint8_t*>(KernelAllocator::alloc(1024));
        
        // Az Ext2 Superblock fixen az 1024-edik bájtnál kezdődik (a bootloader terület után)
        if (!VirtIOBlk::read_blocks(m_disk_id, 2, 2, boot_sector)) { // 2 szektor = 1024 bájt
            KernelAllocator::free(boot_sector);
            return false;
        }

        memcpy(&m_sb, boot_sector, sizeof(ext2_super_block));
        KernelAllocator::free(boot_sector);

        // Ellenőrizzük az Ext2 Magic értéket: 0xEF53
        if (m_sb.s_magic != 0xEF53) {
            return false;
        }

        // Blokkméret kiszámítása: 1024 << s_log_block_size
        m_block_size = 1024 << m_sb.s_log_block_size;
        return true;
    }

    virtual VFS::Node* lookup(const char* path) override {
        ScopedLock guard(m_lock);

        // Első lépésként beolvassuk a gyökér (Root) Inode-ot, ami az Ext2-nél fixen a 2-es számú
        ext2_inode root_inode;
        if (!read_inode(2, root_inode)) return nullptr;

        // Csak a "/" gyökér könyvtár alap-szimulációja és tesztfájl leképezése
        if (strcmp(path, "/linux-init") == 0) {
            return new VFS::Node("linux-init", false, [this](char* user_buf, size_t max_len) -> size_t {
                // Példa: beolvassuk a fájl első közvetlen adatblokkját (i_block[0])
                uint8_t* data_buffer = reinterpret_cast<uint8_t*>(KernelAllocator::alloc(m_block_size));
                
                // Csak az egyszerűség kedvéért fixen beégetett Inode 11 (az első szabad felhasználói inode)
                ext2_inode file_inode;
                read_inode(11, file_inode);

                read_block(file_inode.i_block[0], data_buffer);
                
                size_t to_copy = (max_len < file_inode.i_size) ? max_len : file_inode.i_size;
                memcpy(user_buf, data_buffer, to_copy);
                
                KernelAllocator::free(data_buffer);
                return to_copy;
            });
        }
        return nullptr;
    }

    virtual size_t readdir(VFS::Node* dir_node, char* buffer, size_t max_entries) override {
        ScopedLock guard(m_lock);
        size_t count = 0;
        
        // Gyökérkönyvtár bejegyzések listázása (Egyszerűsített fix makett az Ext2 bájtszerkezet alapján)
        if (count < max_entries) {
            const char* entry = "linux-init";
            memcpy(buffer + (count * 32), entry, strlen(entry) + 1);
            count++;
        }
        return count;
    }
};

void init_ext2_fs() {
    VFS::register_filesystem("ext2", [](int disk_id) -> VFS::FileSystem* {
        Ext2FileSystem* fs = new Ext2FileSystem(disk_id);
        if (fs->initialize()) {
            return fs;
        }
        delete fs;
        return nullptr;
    });
}

} // namespace Blockos
