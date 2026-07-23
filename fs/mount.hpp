#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ext4.hpp"

// Maximálisan egyidejűleg csatlakoztatható fájlrendszerek száma
#define MAX_MOUNT_POINTS 8

// Támogatott fájlrendszer típusok
enum FileSystemType {
    FS_TYPE_NONE,
    FS_TYPE_EXT4,
    FS_TYPE_RAMFS
};

// Egy konkrét mount pont leíró struktúrája
struct MountPoint {
    char             path[64];       // Csatlakozási pont útvonala (pl. "/mnt/disk")
    FileSystemType   type;           // Fájlrendszer típusa
    uint32_t         device_id;      // Hardveres eszköz azonosító (VirtIO disk ID)
    bool             is_active;      // Aktív-e a mount pont
};

class MountManager {
private:
    MountPoint mount_table[MAX_MOUNT_POINTS];
    uint32_t   active_mounts_count;

    bool string_compare(const char* str1, const char* str2);
    void string_copy(char* dest, const char* src, size_t max_len);

public:
    MountManager();
    
    // Rendszerszintű Mount API [source: 1]
    bool vfs_mount(const char* source_dev, const char* target_path, FileSystemType fs_type);
    bool vfs_unmount(const char* target_path);
    
    // Egységesített VFS Fájlolvasó interfész, ami átirányít a megfelelő al-fájlrendszerhez
    bool vfs_read_file(const char* absolute_path, uint8_t* dest, size_t max_size);
    void list_mounts();
};

extern MountManager vfs_system;
