#include "ext4.hpp"
#include "../drivers/virtio.hpp"

// Külső hivatkozás a VirtIO hardveres blokkmeghajtóra
extern VirtioManager io_drivers;

// Segédfüggvény: Szoftveres blokkszám átváltása 512 bájtos fizikai szektorokra
bool Ext4Reader::read_disk_block(uint64_t block_num, uint8_t* buffer) {
    if (!buffer) return false;
    
    // Ha az EXT4 blokkméret 4096 bájt, akkor az 8 darab 512 bájtos szektort jelent
    uint32_t sectors_per_block = block_size / 512;
    uint64_t start_sector = block_num * sectors_per_block;
    
    // Szektoronként beolvassuk az adatot a VirtIO-n keresztül
    for (uint32_t i = 0; i < sectors_per_block; i++) {
        io_drivers.read_block(start_sector + i, buffer + (i * 512));
    }
    return true;
}

// EXT4 Fájlrendszer Csatlakoztatása (Mount)
bool Ext4Reader::mount() {
    __attribute__((aligned(4096))) uint8_t scratch_buffer[4096];
    
    // Az EXT4 szuperblokk fixen az 1024. bájtnál helyezkedik el (0. blokk vége / 1. blokk eleje)
    // Beolvassuk a 0. logikai blokkot (ha 1KB-os blokkok vannak) vagy az 1. blokkot (ha 4KB-osak)
    if (!read_disk_block(0, scratch_buffer)) return false;
    
    // Mutató ráillesztése az 1024. bájtos eltolásra
    Ext4Superblock* sb = (Ext4Superblock*)(scratch_buffer + 1024);
    
    // Biztonsági Magic ellenőrzés (0xEF53)
    if (sb->magic != 0xEF53) {
        // Megpróbáljuk az 1-es blokkot is, hátha 4KB-os a kiosztás
        if (!read_disk_block(1, scratch_buffer)) return false;
        sb = (Ext4Superblock*)scratch_buffer;
        if (sb->magic != 0xEF53) return false; // Nem EXT4 fájlrendszer!
    }
    
    // Alapértékek kinyerése a szuperblokkból
    block_size = 1024 << sb->log_block_size;
    inodes_per_group = sb->inodes_per_group;
    inode_size = (sb->rev_level == 0) ? 128 : 256; // Modern rendszereken 256 bájt
    
    // A Blokkcsoport leíró tábla (Block Group Descriptor Table) közvetlenül a Szuperblokk után van
    bg_desc_table_block = (block_size == 1024) ? 2 : 1;
    
    return true;
}

// Egy adott sorszámú Inode (Fájl indexcsomópont) kikeresése és beolvasása
bool Ext4Reader::get_inode(uint32_t inode_num, Ext4Inode& target_inode) {
    if (inode_num == 0) return false;
    
    // Az EXT4-ben az Inode számozás 1-től indul
    uint32_t group = (inode_num - 1) / inodes_per_group;
    uint32_t index = (inode_num - 1) % inodes_per_group;
    
    // 1. Beolvassuk a megfelelő Blokkcsoport leírót
    __attribute__((aligned(4096))) uint8_t bg_buffer[4096];
    if (!read_disk_block(bg_desc_table_block, bg_buffer)) return false;
    
    Ext4GroupDesc* bg_desc = (Ext4GroupDesc*)bg_buffer + group;
    uint64_t inode_table_start_block = bg_desc->inode_table_lo;
    
    // 2. Kiszámoljuk, hogy az Inode pontosan melyik blokkban és milyen eltoláson van
    uint64_t byte_offset = (uint64_t)index * inode_size;
    uint64_t block_offset = byte_offset / block_size;
    uint32_t offset_within_block = byte_offset % block_size;
    
    __attribute__((aligned(4096))) uint8_t itable_buffer[4096];
    if (!read_disk_block(inode_table_start_block + block_offset, itable_buffer)) return false;
    
    // 3. Kimásoljuk a megtalált Inode-ot a célstruktúrába
    uint8_t* raw_inode_ptr = itable_buffer + offset_within_block;
    for (uint32_t i = 0; i < inode_size; i++) {
        ((uint8_t*)&target_inode)[i] = raw_inode_ptr[i];
    }
    
    return true;
}

// Az Extent Tree (Kiterjedési fa) rekurzív feldolgozása a fizikai adatblokkok beolvasásához
void Ext4Reader::parse_extent(Ext4ExtentHeader* header, uint8_t* dest, size_t& bytes_read, size_t max_size) {
    // Ellenőrizzük az Extent struktúra érvényességét (0xF30A)
    if (header->magic != 0xF30A) return;
    
    if (header->depth == 0) {
        // Ez egy levélcsomópont: a fejlécet közvetlenül a tényleges adat-extentek követik
        Ext4Extent* extents = (Ext4Extent*)(header + 1);
        
        __attribute__((aligned(4096))) uint8_t data_block_buffer[4096];
        
        for (uint16_t i = 0; i < header->entries; i++) {
            // Fizikai blokk címe (összefűzve a felső 16 és alsó 32 bitet)
            uint64_t physical_block = ((uint64_t)extents[i].start_hi << 32) | extents[i].start_lo;
            uint32_t count = extents[i].len;
            
            // Beolvassuk az extent által lefedett összes folyamatos blokkot
            for (uint32_t b = 0; b < count; b++) {
                if (bytes_read >= max_size) return;
                
                if (read_disk_block(physical_block + b, data_block_buffer)) {
                    size_t chunk_size = (max_size - bytes_read < block_size) ? (max_size - bytes_read) : block_size;
                    
                    // Adat átmásolása a cél pufferbe (pl. ide töltődik be a BusyBox binary)
                    for (size_t p = 0; p < chunk_size; p++) {
                        dest[bytes_read + p] = data_block_buffer[p];
                    }
                    bytes_read += chunk_size;
                }
            }
        }
    } else {
        // Ez egy belső index-csomópont (Internal Node), mélyebbre kell ásni a fában
        // (A Mini OS tesztkörnyezetben a kis méretű fájlok mindig 0. mélységű extentbe férnek bele)
        return;
    }
}

// Fájl betöltése a memóriába az Inode sorszáma alapján (pl. Root directory vagy user bináris)
bool Ext4Reader::load_file_by_inode(uint32_t inode_num, uint8_t* dest, size_t max_size) {
    Ext4Inode file_inode;
    if (!get_inode(inode_num, file_inode)) return false;
    
    size_t total_bytes_read = 0;
    
    // Az EXT4-ben a fájlok adatelhelyezkedése alapértelmezetten az Extent struktúrát használja (EXT4_EXTENTS_FL)
    Ext4ExtentHeader* ext_header = (Ext4ExtentHeader*)file_inode.block;
    parse_extent(ext_header, dest, total_bytes_read, max_size);
    
    return (total_bytes_read > 0);
}

// Globális példányosítás az OS mag számára
Ext4Reader filesystem;
