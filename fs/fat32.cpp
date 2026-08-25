#include "fat32.hpp"

#include <cstdint>

#include "libc/include/stdio.h"
#include "libc/include/string.h"


Fat32Reader::Fat32Reader()
{
}

Fat32Reader legacy_fat32_fs;

void Fat32Reader::attach(AtaPio& disk)
{
    ata_driver = &disk;
    is_initialized = false;
}

bool Fat32Reader::ready() const
{
    return is_initialized;
}

bool Fat32Reader::initialize_fat32()
{
    is_initialized = false;

    if (!ata_driver)
    {
        printf("fat32: no disk attached\n");
        return false;
    }

    uint8_t boot_sector[AtaPio::SECTOR_SIZE];

    if (!ata_driver->read_sectors(0, 1, boot_sector))
    {
        printf("fat32: failed to read boot sector\n");
        return false;
    }

    memcpy(&bpb, boot_sector, sizeof(bpb));

    //sanity checks
    if (bpb.bytes_per_sector != AtaPio::SECTOR_SIZE)
    {
        printf("fat32: unexpected bytes_per_sector: %d\n", bpb.bytes_per_sector);
        return false;
    }
    if (bpb.fat_size_32 == 0)
    {
        printf("fat32: fat_size_32 == 0, this may be FAT16, not FAT32\n");
        return false;
    }
    if (bpb.sectors_per_cluster == 0)
    {
        printf("fat32: sectors_per_cluster == 0\n");
        return false;
    }

    // printf everything for manual verification against mtools
    printf("OEM name:            %.8s\n", bpb.oem_name);
    printf("Bytes/sector:        %u\n", bpb.bytes_per_sector);
    printf("Sectors/cluster:     %u\n", bpb.sectors_per_cluster);
    printf("Reserved sectors:    %u\n", bpb.reserved_sector_count);
    printf("Num FATs:            %u\n", bpb.num_fats);
    printf("Total sectors (32):  %u\n", bpb.total_sectors_32);
    printf("FAT size (32):       %u\n", bpb.fat_size_32);
    printf("Root cluster:        %u\n", bpb.root_cluster);

    // --- derive and store the fields Fat32Reader actually needs ---
    sectors_per_cluster = bpb.sectors_per_cluster;
    bytes_per_sector = bpb.bytes_per_sector;
    reserved_sectors = bpb.reserved_sector_count;
    fat_size = bpb.fat_size_32;
    fat_start_sector = reserved_sectors;
    data_start_sector = reserved_sectors + (bpb.num_fats * fat_size);
    root_cluster_num = bpb.root_cluster;
    cluster_size_bytes = bpb.sectors_per_cluster * AtaPio::SECTOR_SIZE;

    printf("Computed FAT start:  sector %u\n", fat_start_sector);
    printf("Computed data start: sector %u\n", data_start_sector);

    is_initialized = true;

    return true;
}

bool Fat32Reader::read_fat_sector(uint64_t sector, uint8_t* buffer)
{
    uint32_t lba = fat_start_sector + sector;
    return ata_driver->read_sectors(lba, 1, buffer);
}


uint32_t Fat32Reader::cluster_to_lba(uint32_t cluster)
{
    return data_start_sector + (cluster - 2) * sectors_per_cluster;
}

uint32_t Fat32Reader::get_next_cluster(uint32_t cluster)
{
    // Each cluster entry is 4 bytes (32 bits) in FAT32, but only the lower 28 bits are used for the cluster number
    uint32_t fat_entry = cluster * 4;
    uint32_t sector_offset = fat_entry / bytes_per_sector;
    uint32_t sector_entry = fat_entry % bytes_per_sector;

    uint8_t entry[AtaPio::SECTOR_SIZE];
    if (!read_fat_sector(sector_offset, entry))
    {
        return 0x0FFFFFFF;
    }

    uint32_t next = 0;
    memcpy(&next, &entry[sector_entry], sizeof(next));

    return next & 0x0FFFFFFF;
}

void Fat32Reader::list_root_directories()
{
    if (!is_initialized)
    {
        printf("fat32: not initialized\n");
        return;
    }

    uint32_t current_cluster = root_cluster_num;

    uint8_t sector_buffer[AtaPio::SECTOR_SIZE];

    while (current_cluster >= 2 && current_cluster < 0x0FFFFFF8)
    {
        uint32_t lba = cluster_to_lba(current_cluster);

        for (uint32_t s = 0; s < sectors_per_cluster; s++)
        {
            if (!ata_driver->read_sectors(lba + s, 1, sector_buffer))
            {
                printf("fat32: read failed at lba %u\n", lba + s);
                return;
            }

            Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(sector_buffer);
            uint32_t entries_per_sector = bytes_per_sector / sizeof(Fat32DirEntry);

            for (uint32_t i = 0; i < entries_per_sector; i++)
            {
                if (static_cast<uint8_t>(entries[i].name[0]) == 0x00) return; // blank cluster
                if (static_cast<uint8_t>(entries[i].name[0]) == 0xE5) continue; // deleted entry
                if (entries[i].attr == 0x0F) continue; // LFN entry, skip for now

                char name[12];
                memcpy(name, entries[i].name, 11);
                name[11] = '\0';

                uint32_t cluster =
                    entries[i].data_cluster_lo |
                    (static_cast<uint32_t>(entries[i].data_cluster_hi) << 16);

                printf("%s  size=%u  cluster=%u  attr=%02X\n",
                       name,
                       entries[i].file_size,
                       cluster,
                       entries[i].attr);
            }
        }

        current_cluster = get_next_cluster(current_cluster);
    }

    return;
}

// Current implementation assumes the file name is in 8.3 format (8 uppercase characters for the name, 3 for the extension)
static bool to_short_name(const char* name, char* out)
{
    for (uint32_t i = 0; i < 11; i++)
        out[i] = ' ';

    uint32_t base = 0;
    while (name[base] != '\0' && name[base] != '.')
    {
        if (base >= 8) return false;
        char c = name[base];
        out[base] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
        base++;
    }

    if (base == 0) return false;
    if (name[base] == '\0') return true;

    uint32_t ext = 0;
    while (name[base + 1 + ext] != '\0')
    {
        if (ext >= 3) return false;
        char c = name[base + 1 + ext];
        out[8 + ext] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
        ext++;
    }

    return true;
}

// NOTE: dest is unbounded. Nothing stops this function from writing past the end of dest.
bool Fat32Reader::read_fat32_file(const char* name, uint8_t* dest, size_t* bytes_read)
{
    if (!is_initialized)
    {
        printf("fat32: not initialized\n");
        return false;
    }

    char target_name[11];
    if (!to_short_name(name, target_name))
    {
        printf("fat32: not a valid 8.3 name: %s\n", name);
        return false;
    }

    uint32_t current_cluster = root_cluster_num;
    uint32_t file_start_cluster = 0;
    uint8_t sector_buffer[AtaPio::SECTOR_SIZE];
    uint8_t scratch_buffer[AtaPio::SECTOR_SIZE];
    size_t dest_length = 0;
    uint32_t file_size = 0;
    uint32_t bytes_remaining = 0;
    uint32_t entries_per_sector = bytes_per_sector / sizeof(Fat32DirEntry);

    bool file_found = false;

    while (current_cluster >= 2 && current_cluster < 0x0FFFFFF8 && !file_found)
    {
        uint32_t lba = cluster_to_lba(current_cluster);

        for (uint32_t s = 0; s < sectors_per_cluster && !file_found; s++)
        {
            if (!ata_driver->read_sectors(lba + s, 1, sector_buffer))
            {
                printf("fat32: read failed at lba %u\n", lba + s);
                return false;
            }

            Fat32DirEntry* directory_entry = reinterpret_cast<Fat32DirEntry*>(sector_buffer);
            for (uint32_t e = 0; e < entries_per_sector; e++)
            {
                if (static_cast<uint8_t>(directory_entry[e].name[0]) == 0x00) return false; // end of directory, no entries follow
                if (static_cast<uint8_t>(directory_entry[e].name[0]) == 0xE5) continue; // deleted entry
                if (directory_entry[e].attr == 0x0F) continue; // LFN entry, not a real 8.3 record
                if (directory_entry[e].attr & 0x08) continue; // volume label, not a file
                if (directory_entry[e].attr & 0x10) continue; // subdirectory, not a file
                if (memcmp(directory_entry[e].name, target_name, 11) == 0)
                {
                    file_found = true;
                    file_start_cluster = directory_entry[e].data_cluster_lo | (static_cast<uint32_t>(directory_entry[e].data_cluster_hi) << 16);
                    file_size = directory_entry[e].file_size;
                    bytes_remaining = file_size;
                    break;
                }
            }
        }

        if (!file_found) current_cluster = get_next_cluster(current_cluster);
    }

    if (!file_found) return false;
    if (bytes_remaining == 0)
    {
        if (bytes_read) *bytes_read = dest_length;
        return true;
    }

    while (file_start_cluster >= 2 && file_start_cluster < 0x0FFFFFF8)
    {
        uint32_t cluster_lba = cluster_to_lba(file_start_cluster);

        for (uint32_t s = 0; s < sectors_per_cluster; s++)
        {
            if (!ata_driver->read_sectors(cluster_lba + s, 1, scratch_buffer))
            {
                printf("fat32: read failed at lba %u\n", cluster_lba + s);
                return false;
            }

            uint32_t bytes_to_copy = bytes_remaining < AtaPio::SECTOR_SIZE ? bytes_remaining : AtaPio::SECTOR_SIZE;
            memcpy(dest + dest_length, scratch_buffer, bytes_to_copy);
            dest_length += bytes_to_copy;
            bytes_remaining -= bytes_to_copy;

            if (bytes_remaining == 0)
            {
                if (bytes_read) *bytes_read = dest_length;
                return true;
            }
        }

        file_start_cluster = get_next_cluster(file_start_cluster);
    }

    printf("fat32: cluster chain ended with %u bytes left of %u\n", bytes_remaining, file_size);

    return false;
}