#include "fs/vfs.hpp"
#include "kernel/allocator.hpp"
#include "kernel/spinlock.hpp"
#include "drivers/virtio_blk.hpp"

namespace Blockos {

// Windows/NTFS szabványos Volume Boot Record (VBR) struktúra [source: 1.1.1]
struct ntfs_boot_sector {
    uint8_t  jmp_boot[3];
    char     oem_name[8];          // Mindig "NTFS    "
    uint16_t bytes_per_sector;     // Általában 512
    uint8_t  sectors_per_cluster;  // Általában 8 (4KB-os klaszterek)
    uint16_t reserved_sectors;     // Mindig 0 NTFS-nél
    uint8_t  media_descriptor;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;     // Nem használt
    uint8_t  bios_drive_num;
    uint8_t  chkdsk_flags;
    uint8_t  extended_boot_signature;
    uint8_t  pad;
    uint64_t total_sectors_64;     // A kötet teljes mérete szektorokban
    uint64_t mft_cluster_num;      // A $MFT első klaszterének száma (Brutálisan fontos!)
    uint64_t mft_mirror_cluster;   // A $MFTMirror első klasztere
    int8_t   clusters_per_mft_record; // Egy MFT rekord mérete (ha pozitív: klaszterekben, ha negatív: 2^(-X) bájtban. Általában -10 = 1024 bájt)
    uint8_t  clusters_per_index_buffer;
    uint64_t volume_serial_num;
    uint32_t checksum;
} __attribute__((packed));

// Általános MFT Rekord Fejléc (MFT Record Header) [source: 1.1.1]
struct ntfs_mft_record_header {
    char     magic[4];             // Mindig "FILE"
    uint16_t update_sequence_offset;
    uint16_t update_sequence_size;
    uint64_t logfile_sequence_num;
    uint16_t sequence_num;
    uint16_t hard_link_count;
    uint16_t attribute_offset;     // Itt kezdődnek az attribútumok a rekordon belül
    uint16_t flags;                // 0x01 = Használatban, 0x02 = Könyvtár
    uint32_t record_real_size;     // A rekord tényleges mérete
    uint32_t record_allocated_size;// Alokált méret (Általában 1024 bájt)
    uint64_t base_mft_record;
    uint16_t next_attribute_id;
} __attribute__((packed));

// NTFS Attribútum Közös Fejléc [source: 1.1.1]
struct ntfs_attribute_header {
    uint32_t type;                 // Típus (pl. 0x10 = $STANDARD_INFORMATION, 0x30 = $FILE_NAME, 0x80 = $DATA)
    uint32_t length;               // Teljes attribútum hossz
    uint8_t  non_resident;         // 0 = Resident (RAM-ban van), 1 = Non-resident (Lemezadatokra mutat)
    uint8_t  name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attribute_id;
    // Resident attribútum esetén a folytatás:
    uint32_t attribute_length;     // A tiszta adat hossza
    uint16_t attribute_offset;     // Az adat offsete a fejléc elejétől mérve
} __attribute__((packed));

class NtfsFileSystem : public VFS::FileSystem {
private:
    Spinlock m_lock;
    ntfs_boot_sector m_boot;
    int m_disk_id;
    uint32_t m_cluster_size;
    uint32_t m_mft_record_size;
    uint64_t m_mft_start_sector;

    // Segédfunkció szektorok olvasásához a VirtIO lemezmeghajtódból [source: 2]
    bool read_sectors(uint64_t start_sector, uint32_t count, uint8_t* buffer) {
        return VirtIOBlk::read_blocks(m_disk_id, start_sector, count, buffer);
    }

public:
    NtfsFileSystem(int disk_id) : m_disk_id(disk_id), m_cluster_size(0), m_mft_record_size(1024), m_mft_start_sector(0) {}

    bool initialize() {
        uint8_t* sector_0 = reinterpret_cast<uint8_t*>(KernelAllocator::alloc(512));
        
        // 1. A legelső szektor (Boot Sector) beolvasása [source: 2]
        if (!read_sectors(0, 1, sector_0)) {
            KernelAllocator::free(sector_0);
            return false;
        }

        memcpy(&m_boot, sector_0, sizeof(ntfs_boot_sector));
        KernelAllocator::free(sector_0);

        // 2. NTFS ellenőrzés az OEM Name alapján ("NTFS    ")
        if (memcmp(m_boot.oem_name, "NTFS    ", 8) != 0) {
            return false;
        }

        // 3. Alapértékek kiszámítása
        m_cluster_size = m_boot.bytes_per_sector * m_boot.sectors_per_cluster;
        
        if (m_boot.clusters_per_mft_record < 0) {
            m_mft_record_size = 1 << (-m_boot.clusters_per_mft_record); // Pl. 1 << 10 = 1024 bájt
        } else {
            m_mft_record_size = m_boot.clusters_per_mft_record * m_cluster_size;
        }

        m_mft_start_sector = m_boot.mft_cluster_num * m_boot.sectors_per_cluster;
        return true;
    }

    virtual VFS::Node* lookup(const char* path) override {
        ScopedLock guard(m_lock);

        // Alapszintű szimuláció a Windows állományok beolvasásához
        // Egy teljes verzióban itt beolvassuk a $MFT 5-ös számú rekordját (ami a Root Directory "/") [source: 1.1.1],
        // kicsomagoljuk az Index Root/Allocation attribútumokat, és megkeressük a fájlnevet.
        if (strcmp(path, "/windows-init.exe") == 0) {
            return new VFS::Node("windows-init.exe", false, [this](char* user_buf, size_t max_len) -> size_t {
                
                // Beolvassuk a kért fájl MFT rekordját (Pl. a lemezen a 32-es indexű MFT bejegyzés)
                uint32_t sectors_per_record = m_mft_record_size / 512;
                uint64_t file_record_sector = m_mft_start_sector + (32 * sectors_per_record);

                uint8_t* record_buf = reinterpret_cast<uint8_t*>(KernelAllocator::alloc(m_mft_record_size));
                read_sectors(file_record_sector, sectors_per_record, record_buf);

                auto* mft_rec = reinterpret_cast<ntfs_mft_record_header*>(record_buf);
                
                size_t copied_bytes = 0;

                if (memcmp(mft_rec->magic, "FILE", 4) == 0) {
                    // Végiggyalogolunk a rekord attribútumain, hogy megtaláljuk a nyers adatot (0x80 = $DATA) [source: 1.1.1]
                    uint32_t attr_offset = mft_rec->attribute_offset;
                    while (attr_offset < mft_rec->record_real_size) {
                        auto* attr = reinterpret_cast<ntfs_attribute_header*>(record_buf + attr_offset);
                        if (attr->type == 0xFFFFFFFF || attr->length == 0) break;

                        if (attr->type == 0x80) { // $DATA attribútum megtalálva! [source: 1.1.1]
                            if (attr->non_resident == 0) { // Resident (Az adat benne van magában az MFT-ben) [source: 1.1.1]
                                size_t data_len = attr->attribute_length;
                                size_t to_copy = (max_len < data_len) ? max_len : data_len;
                                memcpy(user_buf, record_buf + attr_offset + attr->attribute_offset, to_copy);
                                copied_bytes = to_copy;
                            }
                            break;
                        }
                        attr_offset += attr->length;
                    }
                }

                KernelAllocator::free(record_buf);
                return copied_bytes;
            });
        }

        return nullptr;
    }

    virtual size_t readdir(VFS::Node* dir_node, char* buffer, size_t max_entries) override {
        ScopedLock guard(m_lock);
        size_t count = 0;
        
        if (count < max_entries) {
            const char* entry = "windows-init.exe";
            memcpy(buffer + (count * 32), entry, strlen(entry) + 1);
            count++;
        }
        return count;
    }
};

void init_ntfs_fs() {
    VFS::register_filesystem("ntfs", [](int disk_id) -> VFS::FileSystem* {
        NtfsFileSystem* fs = new NtfsFileSystem(disk_id);
        if (fs->initialize()) {
            return fs;
        }
        delete fs;
        return nullptr;
    });
}

} // namespace Blockos
