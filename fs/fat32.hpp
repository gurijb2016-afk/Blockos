#pragma once
#include <stdint.h>
#include <stddef.h>

struct Fat32Bpb {
    uint8_t  boot_jmp[3]; char oem_name[8];
    uint16_t bytes_per_sector; uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count; uint8_t num_fats;
    uint16_t root_entry_count; uint16_t total_sectors_16;
    uint8_t  media_type; uint16_t fat_size_16;
    uint16_t sectors_per_track; uint16_t num_heads;
    uint32_t hidden_sectors; uint32_t total_sectors_32;
    uint32_t fat_size_32; uint16_t ext_flags;
    uint16_t fs_version; uint32_t root_cluster;
} __attribute__((packed));

struct Fat32DirEntry {
    char     name[11]; uint8_t attr; uint8_t nt_res;
    uint8_t  crt_time_ten; uint16_t crt_time; uint16_t crt_date;
    uint16_t lst_acc_date; uint16_t cluster_hi;
    uint16_t wrt_time; uint16_t wrt_date; uint16_t cluster_lo;
    uint32_t file_size;
} __attribute__((packed));

class Fat32Reader {
private:
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t fat_size;
    uint32_t root_cluster_num;

    bool read_fat_sector(uint64_t sector, uint8_t* buffer);
    uint32_t get_next_cluster(uint32_t current_cluster);

public:
    Fat32Reader();
    bool initialize_fat32();
    bool read_fat32_file(const char* name, uint8_t* dest, size_t max_size);
};

extern Fat32Reader legacy_fat32_fs;
