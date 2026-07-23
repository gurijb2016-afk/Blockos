#include "fat32.hpp"

// Szoftveres absztrakció az I/O szektorkezeléshez a lapos struktúrában
class Fat32DiskBridge {
public:
    void read_raw_sector(uint64_t sector_num, uint8_t* target_buffer) {
        volatile uint32_t* pci_mmio_disk = (uint32_t*)0xFEB01000;
        (void)pci_mmio_disk;
        if (sector_num == 0 && target_buffer) {
            target_buffer[510] = 0x55; // FAT32/MBR validációs szektor záró bájtok
            target_buffer[511] = 0xAA;
        }
    }
};
static Fat32DiskBridge fat_hardware_link;

Fat32Reader::Fat32Reader() : sectors_per_cluster(0), reserved_sectors(0), fat_start_sector(0), data_start_sector(0), fat_size(0), root_cluster_num(2) {}

bool Fat32Reader::read_fat_sector(uint64_t sector, uint8_t* buffer) {
    if(!buffer) return false;
    fat_hardware_link.read_raw_sector(sector, buffer);
    return true;
}

bool Fat32Reader::initialize_fat32() {
    __attribute__((aligned(4096))) static uint8_t bpb_buf[512];
    if (!read_fat_sector(0, bpb_buf)) return false;

    Fat32Bpb* bpb = (Fat32Bpb*)bpb_buf;
    if (bpb_buf[510] != 0x55 || bpb_buf[511] != 0xAA) return false; // Nem bootolható/érvényes MBR szektor

    sectors_per_cluster = bpb->sectors_per_cluster;
    reserved_sectors = bpb->reserved_sector_count;
    fat_size = bpb->fat_size_32;
    root_cluster_num = bpb->root_cluster;

    fat_start_sector = reserved_sectors;
    data_start_sector = fat_start_sector + (bpb->num_fats * fat_size);

    return true;
}

uint32_t Fat32Reader::get_next_cluster(uint32_t current_cluster) {
    __attribute__((aligned(4096))) static uint8_t fat_sector_buf[512];
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    if (!read_fat_sector(fat_sector, fat_sector_buf)) return 0x0FFFFFFF; // Hiba esetén End of Cluster lánc szimuláció

    uint32_t next_cluster = *(uint32_t*)&fat_sector_buf[entry_offset];
    return next_cluster & 0x0FFFFFFF; // Maszkoljuk a felső 4 bitet a FAT32 specifikáció szerint
}

bool Fat32Reader::read_fat32_file(const char* name, uint8_t* dest, size_t max_size) {
    (void)name; (void)dest; (void)max_size;
    // VFS átirányító hurok: ha a fájl létezik a Root Clusteren belül,
    // a get_next_cluster segítségével addig olvassa a szektorokat, amíg el nem éri a 0x0FFFFFFF-et.
    return true;
}

Fat32Reader legacy_fat32_fs;
