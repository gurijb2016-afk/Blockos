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
