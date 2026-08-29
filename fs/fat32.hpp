#pragma once
#include <stddef.h>
#include <stdint.h>

#include "drivers/ata_pio.hpp"

struct Fat32BPB
{
    uint8_t boot_jmp[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
} __attribute__((packed));

struct Fat32DirEntry
{
    char name[11];
    uint8_t attr;
    uint8_t nt_res;
    uint8_t crt_time_ten;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t data_cluster_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t data_cluster_lo;
    uint32_t file_size;
} __attribute__((packed));

class Fat32Reader
{
   private:
    uint32_t sectors_per_cluster = 0;
    uint32_t bytes_per_sector = 0;
    uint32_t reserved_sectors = 0;
    uint32_t fat_start_sector = 0;
    uint32_t data_start_sector = 0;
    uint32_t fat_size = 0;
    uint32_t root_cluster_num = 0;
    uint32_t cluster_size_bytes = 0;
    Fat32BPB bpb = {};
    AtaPio* ata_driver = nullptr;
    bool is_initialized = false;

    bool read_fat_table(uint64_t sector, uint8_t* buffer);
    bool write_fat_table(uint64_t sector, const uint8_t* buffer);
    // Helpers
    uint32_t cluster_to_lba(uint32_t cluster);
    uint32_t next_cluster_in_chain(uint32_t current_cluster);
    uint32_t get_first_free_cluster();
    bool write_fat_entry(uint32_t cluster, uint32_t value);
    bool insert_directory_entry(uint32_t cluster, const Fat32DirEntry& entry);
    bool get_parent_directory_cluster(const char* path, uint32_t& parent_cluster);
    bool resolve_path(const char* path, uint32_t& current_cluster);
    bool find_entry_in_directory(uint32_t dir_cluster, const char* short_name, Fat32DirEntry& out);

   public:
    Fat32Reader();
    void attach(AtaPio& disk);
    bool initialize();
    bool ready() const;
    bool read_file(const char* name, uint8_t* dest, size_t* bytes_read = nullptr);
    bool write_file(const char* name, const uint8_t* src, size_t length);
    bool create_directory(const char* path);
    void print_directory(const char* path);
    bool directory_exists(const char* path);
};

extern Fat32Reader legacy_fat32_fs;
