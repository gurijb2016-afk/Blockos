#include "EROFS_Structures.hpp"
#include <vector>
#include <string_view>
#include <cstring>
#include <memory>

// Tegyük fel, hogy a Blockos belső VFS/Driver interfésze valahogy így néz ki
class DiskDevice {
public:
    virtual bool read_blocks(uint64_t lba, uint32_t count, void* buffer) = 0;
};

class ErofsFileSystem {
private:
    std::shared_ptr<DiskDevice> m_device;
    ErofsSuperBlock m_sb;
    uint64_t m_block_size;

    // Segédfüggvény egy specifikus blokk beolvasásához a memóriába
    bool read_block(uint64_t block_id, void* buffer) {
        return m_device->read_blocks(block_id * (m_block_size / 512), m_block_size / 512, buffer);
    }

    // NID (Node Identifier) feloldása fizikai lemezcímmé
    uint64_t nid_to_blkaddr(uint64_t nid) {
        // EROFS specifikáció: Az inódok a meta_blkaddr után helyezkednek el, 32 bájtos egységekben
        return m_sb.meta_blkaddr + (nid * 32 / m_block_size);
    }

    uint32_t nid_to_offset(uint64_t nid) {
        return (nid * 32) % m_block_size;
    }

public:
    ErofsFileSystem(std::shared_ptr<DiskDevice> device) : m_device(device), m_block_size(EROFS_BLKSIZ) {}

    // 1. Inicializálás és Szuperblokk ellenőrzés
    bool initialize() {
        uint8_t buffer[EROFS_BLKSIZ];
        // Az EROFS szuperblokk a 0. blokk 1024. bájtjánál van
        if (!read_block(0, buffer)) {
            return false;
        }

        std::memcpy(&m_sb, buffer + 1024, sizeof(ErofsSuperBlock));

        // Mágikus szám ellenőrzése (0xE0F5E1E2)
        if (m_sb.magic != EROFS_SUPER_MAGIC_V1) {
            // Nem valid EROFS fájlrendszer
            return false; 
        }

        m_block_size = (1 << m_sb.blkszbits);
        return true;
    }

    // 2. Inode beolvasása NID alapján
    bool read_inode(uint64_t nid, ErofsInodeCompact& inode) {
        uint64_t blk = nid_to_blkaddr(nid);
        uint32_t offset = nid_to_offset(nid);

        std::vector<uint8_t> buffer(m_block_size);
        if (!read_block(blk, buffer.data())) return false;

        std::memcpy(&inode, buffer.data() + offset, sizeof(ErofsInodeCompact));
        return true;
    }

    // 3. Fájl olvasása (Nem tömörített, flat/flat-inline mód támogatással)
    bool read_file(const ErofsInodeCompact& inode, uint8_t* out_buffer) {
        // Ha a fájl mérete 0, nincs mit tenni
        if (inode.size == 0) return true;

        uint32_t blocks_to_read = (inode.size + m_block_size - 1) / m_block_size;
        uint32_t current_blk = inode.u.raw_blkaddr;

        for (uint32_t i = 0; i < blocks_to_read; ++i) {
            if (!read_block(current_blk + i, out_buffer + (i * m_block_size))) {
                return false;
            }
        }
        return true;
    }

    // 4. Directory Lookup (Fájl keresése mappában név alapján)
    uint64_t lookup(uint64_t dir_nid, std::string_view name) {
        ErofsInodeCompact dir_inode;
        if (!read_inode(dir_nid, dir_inode)) return 0;
      
        if ((dir_inode.mode & 0xF000) != 0x4000) return 0;

        std::vector<uint8_t> dir_data(dir_inode.size);
        if (!read_file(dir_inode, dir_data.data())) return 0;

        uint32_t offset = 0;
        while (offset < dir_inode.size) {
            ErofsDirEntry entry;
            std::memcpy(&entry, dir_data.data() + offset, sizeof(ErofsDirEntry));

            if (entry.nid == 0) break; // Nincs több bejegyzés

            // A név a bejegyzés után/mellett helyezkedik el a specifikáció szerint
            const char* entry_name = reinterpret_cast<const char*>(dir_data.data() + entry.nameoff);
            
            if (name == entry_name) {
                return entry.nid; // Megtaláltuk az inode NID-jét!
            }

            offset += sizeof(ErofsDirEntry);
        }

        return 0; // Nem található
    }

    uint64_t get_root_nid() const {
        return m_sb.root_nid;
    }
};
