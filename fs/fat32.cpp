#include "fat32.hpp"

#include <cstdint>

#include "libc/include/stdio.h"
#include "libc/include/string.h"


constexpr uint8_t ATTR_READ_ONLY = 0x01;
constexpr uint8_t ATTR_HIDDEN = 0x02;
constexpr uint8_t ATTR_SYSTEM = 0x04;
constexpr uint8_t ATTR_VOLUME_ID = 0x08;
constexpr uint8_t ATTR_DIRECTORY = 0x10;
constexpr uint8_t ATTR_ARCHIVE = 0x20;
constexpr uint8_t ATTR_LFN = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID; // 0x0F, marks a Long File Name entry
constexpr size_t COMPONENT_MAX = 16;

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

bool Fat32Reader::initialize()
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

    // // printf everything for manual verification against mtools
    // printf("OEM name:            %.8s\n", bpb.oem_name);
    // printf("Bytes/sector:        %u\n", bpb.bytes_per_sector);
    // printf("Sectors/cluster:     %u\n", bpb.sectors_per_cluster);
    // printf("Reserved sectors:    %u\n", bpb.reserved_sector_count);
    // printf("Num FATs:            %u\n", bpb.num_fats);
    // printf("Total sectors (32):  %u\n", bpb.total_sectors_32);
    // printf("FAT size (32):       %u\n", bpb.fat_size_32);
    // printf("Root cluster:        %u\n", bpb.root_cluster);

    // --- derive and store the fields Fat32Reader actually needs ---
    sectors_per_cluster = bpb.sectors_per_cluster;
    bytes_per_sector = bpb.bytes_per_sector;
    reserved_sectors = bpb.reserved_sector_count;
    fat_size = bpb.fat_size_32;
    fat_start_sector = reserved_sectors;
    data_start_sector = reserved_sectors + (bpb.num_fats * fat_size);
    root_cluster_num = bpb.root_cluster;
    cluster_size_bytes = bpb.sectors_per_cluster * AtaPio::SECTOR_SIZE;

    // printf("Computed FAT start:  sector %u\n", fat_start_sector);
    // printf("Computed data start: sector %u\n", data_start_sector);

    is_initialized = true;

    return true;
}

bool Fat32Reader::directory_exists(const char* path)
{
    uint32_t tmp = root_cluster_num;
    return resolve_path(path, tmp);
}

// Reads a single sector of the FAT, indexed relative to the start of the FAT region
bool Fat32Reader::read_fat_table(uint64_t sector, uint8_t* buffer)
{
    uint32_t lba = fat_start_sector + sector;
    return ata_driver->read_sectors(lba, 1, buffer);
}

bool Fat32Reader::write_fat_table(uint64_t sector, const uint8_t* buffer)
{
    uint32_t lba = fat_start_sector + sector;
    return ata_driver->write_sectors(lba, 1, buffer);
}

uint32_t Fat32Reader::cluster_to_lba(uint32_t cluster)
{
    return data_start_sector + (cluster - 2) * sectors_per_cluster;
}

uint32_t Fat32Reader::next_cluster_in_chain(uint32_t cluster)
{
    // Each cluster entry is 4 bytes (32 bits) in FAT32, but only the lower 28 bits are used for the cluster number
    uint32_t fat_entry = cluster * 4;
    uint32_t sector_offset = fat_entry / bytes_per_sector;
    uint32_t sector_entry = fat_entry % bytes_per_sector;

    uint8_t sector_buffer[AtaPio::SECTOR_SIZE];
    if (!read_fat_table(sector_offset, sector_buffer))
    {
        return 0x0FFFFFFF;
    }

    uint32_t next = 0;
    memcpy(&next, &sector_buffer[sector_entry], sizeof(next));

    return next & 0x0FFFFFFF;
}

uint32_t Fat32Reader::get_first_free_cluster()
{
    uint8_t sector_buffer[AtaPio::SECTOR_SIZE];
    uint32_t entries_per_sector = AtaPio::SECTOR_SIZE / 4;

    // Start searching from cluster 2, as clusters 0 and 1 are reserved
    for (uint32_t sector = 2; sector < fat_size; sector++)
    {
        if (!read_fat_table(sector, sector_buffer))
        {
            printf("fat32: failed to read FAT sector %u\n", sector);
            return 0xFFFFFFFF;
        }

        for (uint32_t entry_offset = 0; entry_offset < AtaPio::SECTOR_SIZE; entry_offset += 4)
        {
            uint32_t value = *reinterpret_cast<uint32_t*>(&sector_buffer[entry_offset]);

            if (value == 0x00000000)
            {
                uint32_t entry_index = entry_offset / 4;
                return sector * entries_per_sector + entry_index;
            }
        }
    }

    return 0xFFFFFFFF; // No free cluster found in entire FAT
}

bool Fat32Reader::write_fat_entry(uint32_t cluster, uint32_t value)
{
    // Each cluster entry is 4 bytes (32 bits) in FAT32, but only the lower 28 bits are used for the cluster number
    uint32_t fat_entry = cluster * 4;
    uint32_t sector_offset = fat_entry / bytes_per_sector;
    uint32_t sector_entry = fat_entry % bytes_per_sector;

    uint8_t sector_buffer[AtaPio::SECTOR_SIZE];
    if (!read_fat_table(sector_offset, sector_buffer))
    {
        return false;
    }

    uint32_t old_value = 0;
    memcpy(&old_value, &sector_buffer[sector_entry], sizeof(old_value));
    uint8_t preserved_top_bits = old_value & 0xF0000000;
    uint32_t new_value = (value & 0x0FFFFFFF) | preserved_top_bits;

    memcpy(&sector_buffer[sector_entry], &new_value, sizeof(new_value));

    if (!write_fat_table(sector_offset, sector_buffer))
    {
        return false;
    }

    return true;
}

bool Fat32Reader::insert_directory_entry(uint32_t cluster, const Fat32DirEntry& entry)
{
    uint8_t sector_buffer[AtaPio::SECTOR_SIZE];
    uint32_t entries_per_sector = bytes_per_sector / sizeof(Fat32DirEntry);

    while (cluster >= 2 && cluster < 0x0FFFFFF8)
    {
        uint32_t lba = cluster_to_lba(cluster);

        // Find the first free directory entry
        for (uint32_t s = 0; s < sectors_per_cluster; s++)
        {
            if (!ata_driver->read_sectors(lba + s, 1, sector_buffer))
            {
                printf("fat32: read failed at lba %u\n", lba + s);
                return false;
            }

            Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(sector_buffer);
            for (uint32_t e = 0; e < entries_per_sector; e++)
            {
                if (static_cast<uint8_t>(entries[e].name[0]) == 0x00 || static_cast<uint8_t>(entries[e].name[0]) == 0xE5)
                {
                    memcpy(&entries[e], &entry, sizeof(Fat32DirEntry));
                    return ata_driver->write_sectors(lba + s, 1, sector_buffer);
                }
            }
        }

        uint32_t next_cluster = next_cluster_in_chain(cluster);

        if (next_cluster >= 2 && next_cluster < 0x0FFFFFF8)
        {
            cluster = next_cluster;
            continue;
        }

        uint32_t new_cluster = get_first_free_cluster();
        if (new_cluster == 0xFFFFFFFF)
        {
            printf("fat32: no free clusters available\n");
            return false;
        }

        if (!write_fat_entry(new_cluster, 0x0FFFFFFF))
        {
            printf("fat32: failed to claim cluster %u\n", new_cluster);
            return false;
        }

        memset(sector_buffer, 0, AtaPio::SECTOR_SIZE);
        uint32_t new_cluster_lba = cluster_to_lba(new_cluster);

        for (uint32_t s = 0; s < sectors_per_cluster; s++)
        {
            if (!ata_driver->write_sectors(new_cluster_lba + s, 1, sector_buffer))
            {
                printf("fat32: write failed at lba %u\n", new_cluster_lba + s);
                return false;
            }
        }

        if (!write_fat_entry(cluster, new_cluster))
        {
            printf("fat32: failed to link cluster %u to %u\n", cluster, new_cluster);
            return false;
        }

        cluster = new_cluster;
    }

    return false;
}

// TODO: FAT1/FAT2 mirroring is not implemented. This implementation only writes to FAT1.

void Fat32Reader::print_directory(const char* path)
{
    if (!is_initialized)
    {
        printf("fat32: not initialized\n");
        return;
    }

    uint32_t current_cluster = root_cluster_num;
    if (!resolve_path(path, current_cluster))
    {
        printf("fat32: no such directory: %s\n", path);
        return;
    }

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

        current_cluster = next_cluster_in_chain(current_cluster);
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

static const char* final_component(const char* path)
{
    const char* last = strrchr(path, '/');
    return last ? last + 1 : path;
}

bool Fat32Reader::find_entry_in_directory(uint32_t dir_cluster, const char* short_name, Fat32DirEntry& out)
{
    uint8_t sector_buffer[AtaPio::SECTOR_SIZE];
    uint32_t entries_per_sector = bytes_per_sector / sizeof(Fat32DirEntry);

    while (dir_cluster >= 2 && dir_cluster < 0x0FFFFFF8)
    {
        uint32_t lba = cluster_to_lba(dir_cluster);

        for (uint32_t s = 0; s < sectors_per_cluster; s++)
        {
            if (!ata_driver->read_sectors(lba + s, 1, sector_buffer))
            {
                printf("fat32: read failed at lba %u\n", lba + s);
                return false;
            }

            Fat32DirEntry* entries = reinterpret_cast<Fat32DirEntry*>(sector_buffer);
            for (uint32_t e = 0; e < entries_per_sector; e++)
            {
                if (static_cast<uint8_t>(entries[e].name[0]) == 0x00) return false;
                if (static_cast<uint8_t>(entries[e].name[0]) == 0xE5) continue;
                if (entries[e].attr == ATTR_LFN) continue;
                if (entries[e].attr & ATTR_VOLUME_ID) continue;

                if (memcmp(entries[e].name, short_name, 11) == 0)
                {
                    memcpy(&out, &entries[e], sizeof(Fat32DirEntry));
                    return true;
                }
            }
        }

        dir_cluster = next_cluster_in_chain(dir_cluster);
    }

    return false;
}

bool Fat32Reader::resolve_path(const char* path, uint32_t& current_cluster)
{
    if (!is_initialized) return false;

    current_cluster = root_cluster_num;

    while (*path == '/')
        path++;

    while (*path != '\0')
    {
        const char* delim = strchr(path, '/');
        size_t length = delim ? static_cast<size_t>(delim - path) : strlen(path);

        if (length == 0 || length >= COMPONENT_MAX) return false;

        char component[COMPONENT_MAX];
        memcpy(component, path, length);
        component[length] = '\0';

        char short_name[11];
        if (!to_short_name(component, short_name)) return false;

        Fat32DirEntry entry;
        if (!find_entry_in_directory(current_cluster, short_name, entry)) return false;
        if (!(entry.attr & ATTR_DIRECTORY)) return false;

        current_cluster = entry.data_cluster_lo | (static_cast<uint32_t>(entry.data_cluster_hi) << 16);

        // A ".." entry in a directory whose parent is the root stores cluster 0
        if (current_cluster == 0) current_cluster = root_cluster_num;

        path += length;
        while (*path == '/')
            path++;
    }

    return true;
}

// NOTE: dest is unbounded. Nothing stops this function from writing past the end of dest.
bool Fat32Reader::read_file(const char* path, uint8_t* dest, size_t* bytes_read)
{
    if (!is_initialized)
    {
        printf("fat32: not initialized\n");
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

    if (!get_parent_directory_cluster(path, current_cluster))
    {
        printf("fat32: cannot resolve parent of %s\n", path);
        return false;
    }

    char target_name[11];
    if (!to_short_name(final_component(path), target_name))
    {
        printf("fat32: not a valid 8.3 name: %s\n", path);
        return false;
    }

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
                if (directory_entry[e].attr & ATTR_DIRECTORY) continue;

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

        if (!file_found) current_cluster = next_cluster_in_chain(current_cluster);
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

        file_start_cluster = next_cluster_in_chain(file_start_cluster);
    }

    printf("fat32: cluster chain ended with %u bytes left of %u\n", bytes_remaining, file_size);

    return false;
}

// Returns cluster containing parent directories data
bool Fat32Reader::get_parent_directory_cluster(const char* path, uint32_t& parent_cluster)
{
    if (!is_initialized)
    {
        printf("fat32: not initialized\n");
        return false;
    }

    const char* last = strrchr(path, '/');

    if (last == nullptr || last == path)
    {
        parent_cluster = root_cluster_num;
        return true;
    }

    size_t length = static_cast<size_t>(last - path);

    char parent_path[256];
    if (length >= sizeof(parent_path)) return false;

    memcpy(parent_path, path, length);
    parent_path[length] = '\0';

    return resolve_path(parent_path, parent_cluster);
}

// TODO: . and .. entries -> A spec-conformant FAT32 subdirectory
bool Fat32Reader::create_directory(const char* path)
{
    if (!is_initialized)
    {
        printf("fat32: not initialized\n");
        return false;
    }

    uint32_t current_cluster = root_cluster_num;
    if (!get_parent_directory_cluster(path, current_cluster))
    {
        printf("Cannot resolve parent path");
        return false;
    }

    uint32_t first_free_cluster = get_first_free_cluster();
    if (first_free_cluster == 0xFFFFFFFF)
    {
        printf("fat32: no free clusters available\n");
        return false;
    }

    char target_name[11];
    if (!to_short_name(final_component(path), target_name))
    {
        printf("fat32: not a valid 8.3 name: %s\n", path);
        return false;
    }

    if (!write_fat_entry(first_free_cluster, 0x0FFFFFFF))
    {
        printf("fat32: failed to claim cluster %u\n", first_free_cluster);
        return false;
    }

    uint8_t empty_sector[AtaPio::SECTOR_SIZE] = {};
    uint32_t new_cluster_lba = cluster_to_lba(first_free_cluster);

    for (uint32_t s = 0; s < sectors_per_cluster; s++)
    {
        if (!ata_driver->write_sectors(new_cluster_lba + s, 1, empty_sector))
        {
            printf("fat32: write failed at lba %u\n", new_cluster_lba + s);
            return false;
        }
    }

    Fat32DirEntry new_entry = {};
    memcpy(new_entry.name, target_name, 11);
    new_entry.attr = ATTR_DIRECTORY;
    new_entry.data_cluster_lo = static_cast<uint16_t>(first_free_cluster & 0xFFFF);
    new_entry.data_cluster_hi = static_cast<uint16_t>((first_free_cluster >> 16) & 0xFFFF);

    // Write the directory entry to the root directory
    if (!insert_directory_entry(current_cluster, new_entry))
    {
        printf("fat32: failed to write directory entry for %s\n", path);
        return false;
    }

    return true;
}

// TODO: check if dir/filename already exists before writing
bool Fat32Reader::write_file(const char* name, const uint8_t* src, size_t length)
{
    if (!is_initialized)
    {
        printf("fat32: not initialized\n");
        return false;
    }

    uint32_t current_cluster = root_cluster_num;

    if (!get_parent_directory_cluster(name, current_cluster))
    {
        printf("fat32: cannot resolve parent of %s\n", name);
        return false;
    }

    char target_name[11];
    if (!to_short_name(final_component(name), target_name))
    {
        printf("fat32: not a valid 8.3 name: %s\n", name);
        return false;
    }

    if (length == 0)
    {
        Fat32DirEntry empty_entry = {};
        memcpy(empty_entry.name, target_name, 11);
        empty_entry.attr = ATTR_ARCHIVE;

        if (!insert_directory_entry(current_cluster, empty_entry))
        {
            printf("fat32: failed to write directory entry for %s\n", name);
            return false;
        }

        return true;
    }

    uint32_t first_free_cluster = get_first_free_cluster();
    if (first_free_cluster == 0xFFFFFFFF)
    {
        printf("fat32: no free clusters available\n");
        return false;
    }

    Fat32DirEntry new_entry = {};
    memcpy(new_entry.name, target_name, 11);
    new_entry.attr = ATTR_ARCHIVE;
    new_entry.data_cluster_lo = static_cast<uint16_t>(first_free_cluster & 0xFFFF);
    new_entry.data_cluster_hi = static_cast<uint16_t>((first_free_cluster >> 16) & 0xFFFF);
    new_entry.file_size = static_cast<uint32_t>(length);

    if (!insert_directory_entry(current_cluster, new_entry))
    {
        printf("fat32: failed to write directory entry for %s\n", name);
        return false;
    }

    // TODO: handle 0 byte write

    size_t bytes_written = 0;
    while (bytes_written < length)
    {
        uint32_t cluster_lba = cluster_to_lba(first_free_cluster);

        for (uint32_t s = 0; s < sectors_per_cluster; s++)
        {
            if (bytes_written >= length) break;

            size_t bytes_to_write = (length - bytes_written) < AtaPio::SECTOR_SIZE ? (length - bytes_written) : AtaPio::SECTOR_SIZE;

            if (!ata_driver->write_sectors(cluster_lba + s, 1, src + bytes_written))
            {
                printf("fat32: write failed at lba %u\n", cluster_lba + s);
                return false;
            }

            bytes_written += bytes_to_write;
        }

        // Get the next free cluster for the next iteration
        uint32_t next_free_cluster = get_first_free_cluster();
        if (next_free_cluster == 0xFFFFFFFF)
        {
            printf("fat32: no more free clusters available\n");
            return false;
        }

        // Update the FAT entry for the current cluster to point to the next free cluster
        if (!write_fat_entry(current_cluster, next_free_cluster))
        {
            printf("fat32: failed to update FAT entry for cluster %u\n", current_cluster);
            return false;
        }

        current_cluster = next_free_cluster;
    }

    // Mark the last cluster as end-of-chain
    if (!write_fat_entry(current_cluster, 0x0FFFFFFF))
    {
        printf("fat32: failed to mark end-of-chain for cluster %u\n", current_cluster);
        return false;
    }

    return true;
}