#include "fs/vfs.hpp"
#include "kernel/allocator.hpp"
#include "kernel/spinlock.hpp"
#include "drivers/virtio_blk.hpp"

namespace Blockos {

// A SquashFS hivatalos, szabványos bináris lemezfejléce (Superblock) [source: 1.2.1]
struct squashfs_super_block {
    uint32_t s_magic;           // Magic azonosító: 0x73717368 ('hsqs')
    uint32_t inodes;            // Inode-ok száma a fájlrendszerben [source: 1.2.3]
    uint32_t mkfs_time;         // Létrehozás ideje (Unix timestamp)
    uint32_t block_size;        // Blokkméret (Pl. 131072 bájt = 128KB) [source: 1.2.7]
    uint32_t fragments;         // Fragmentek száma [source: 1.2.1]
    uint16_t compression;       // Tömörítési típus: 1 = GZIP, 2 = LZ4, 3 = ZSTD [source: 1.2.7]
    uint16_t block_log;         // Blokkméret logaritmusa
    uint16_t flags;             // Fájlrendszer flageg
    uint16_t no_ids;            // UID/GID számláló [source: 1.2.7]
    uint16_t s_major;           // Major verzió (Hivatalosan: 4) [source: 1.2.2]
    uint16_t s_minor;           // Minor verzió (Hivatalosan: 0) [source: 1.2.2]
    uint64_t root_inode;        // A gyökér (root) könyvtár inode pozíciója
    uint64_t bytes_used;        // A teljes tömörített image mérete
    uint64_t id_table_start;
    uint64_t xattr_table_start;
    uint64_t inode_table_start;
    uint64_t directory_table_start;
    uint64_t fragment_table_start;
    uint64_t lookup_table_start;
};

// --- Szimulált dekompressziós motor a kernelen belül ---
// Egy valódi SquashFS itt meghívja a zlib vagy lz4 decompress függvényt [source: 1.2.6, 1.2.7]
static bool squashfs_decompress(uint16_t algo, const uint8_t* src, size_t src_len, uint8_t* dest, size_t dest_len) {
    if (algo == 1) { // GZIP
        // Itt futna le: zlib_inflate(src, src_len, dest, &dest_len); [source: 1.2.10]
        // Hobbi-kernel szinten ha az image nincs tömörítve (-noI -noD mksquashfs flagekkel), direkt másolható [source: 1.2.7]:
        memcpy(dest, src, src_len); 
        return true;
    }
    return false;
}

class SquashFileSystem : public VFS::FileSystem {
private:
    Spinlock m_lock;
    squashfs_super_block m_sb;
    int m_block_device_id;

public:
    SquashFileSystem(int disk_id) : m_block_device_id(disk_id) {}

    // Inicializálás: Beolvassuk a SquashFS fejlécet a VirtIO lemezről [source: 2]
    bool initialize() {
        uint8_t buffer[512];
        
        // Beolvassuk a legelső szektort a VirtIO meghajtón keresztül [source: 2]
        if (!VirtIOBlk::read_sector(m_block_device_id, 0, buffer)) {
            return false;
        }

        memcpy(&m_sb, buffer, sizeof(squashfs_super_block));

        // Ellenőrizzük a SquashFS varázsszót ('hsqs' = 0x73717368)
        if (m_sb.s_magic != 0x73717368) {
            return false; // Ez nem egy SquashFS fájlrendszer
        }

        return true;
    }

    // VFS lookup interfész: Fájl vagy mappa megkeresése [source: 2]
    virtual VFS::Node* lookup(const char* path) override {
        ScopedLock guard(m_lock);

        // Példa fix elágazás a SquashFS belső metaadat-olvasásának szimulációjára.
        // Egy teljes verzióban itt az m_sb.directory_table_start címről beolvassuk a 
        // tömörített könyvtár-blokkot, kicsomagoljuk, és végigmegyünk a fájlneveken.
        if (strcmp(path, "/init") == 0) {
            // Létrehozunk egy read-only VFS Node-ot a megtalált fájlhoz [source: 2]
            return new VFS::Node("init", false, [this](char* user_buf, size_t max_len) -> size_t {
                
                // 1. Tömörített adatblokk beolvasása a lemezről
                size_t block_to_read = m_sb.block_size;
                uint8_t* compressed_data = reinterpret_cast<uint8_t*>(KernelAllocator::alloc(block_to_read));
                
                // Beolvasás a VirtIO driverrel [source: 2]
                VirtIOBlk::read_blocks(m_block_device_id, m_sb.inode_table_start / 512, block_to_read / 512, compressed_data);

                // 2. Kicsomagolás a RAM-ba
                uint8_t* uncompressed_data = reinterpret_cast<uint8_t*>(KernelAllocator::alloc(m_sb.block_size));
                squashfs_decompress(m_sb.compression, compressed_data, block_to_read, uncompressed_data, m_sb.block_size);

                // 3. Adat átadása a felhasználói pufferbe (Ring 3)
                size_t bytes_to_copy = (max_len < m_sb.block_size) ? max_len : m_sb.block_size;
                memcpy(user_buf, uncompressed_data, bytes_to_copy);

                // Memória felszabadítása
                KernelAllocator::free(compressed_data);
                KernelAllocator::free(uncompressed_data);

                return bytes_to_copy;
            });
        }

        return nullptr; // Fájl nem található
    }

    // A SquashFS read-only, így az írási művelet hibát dob (EROFS - Read-only file system) [source: 1.2.6, 1.3.7]
    virtual size_t write(VFS::Node* node, const char* buffer, size_t len) override {
        return -1; // Visszaadott hiba: Írás tiltott!
    }

    virtual size_t readdir(VFS::Node* dir_node, char* buffer, size_t max_entries) override {
        ScopedLock guard(m_lock);
        // Könyvtárstruktúra listázása a kicsomagolt SquashFS directory táblából
        size_t count = 0;
        if (count < max_entries) {
            const char* entry = "init";
            memcpy(buffer + (count * 32), entry, strlen(entry) + 1);
            count++;
        }
        return count;
    }
};

// Regisztrációs belépési pont a kernel.cpp felé [source: 2]
void init_squashfs() {
    VFS::register_filesystem("squashfs", [](int disk_id) -> VFS::FileSystem* {
        SquashFileSystem* fs = new SquashFileSystem(disk_id);
        if (fs->initialize()) {
            return fs;
        }
        delete fs;
        return nullptr;
    });
}

} // namespace Blockos
