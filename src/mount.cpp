#include "mount.hpp"
#include "gui.hpp"

extern Ext4Reader filesystem; // Külső hivatkozás az előzőleg megírt EXT4 modulra
extern GuiEngine desktop;

MountManager::MountManager() : active_mounts_count(0) {
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        mount_table[i].is_active = false;
        mount_table[i].path[0] = '\0';
        mount_table[i].type = FS_TYPE_NONE;
        mount_table[i].device_id = 0;
    }
}

// Biztonságos sztring összehasonlító helper (Külső C-lib hiánya miatt)
bool MountManager::string_compare(const char* str1, const char* str2) {
    int i = 0;
    while (str1[i] != '\0' || str2[i] != '\0') {
        if (str1[i] != str2[i]) return false;
        i++;
    }
    return true;
}

// Biztonságos sztring másoló helper
void MountManager::string_copy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (src[i] != '\0' && i < (max_len - 1)) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// ESZKÖZ CSATLAKOZTATÁSA A NATIVE VFS-be [source: 1]
bool MountManager::vfs_mount(const char* source_dev, const char* target_path, FileSystemType fs_type) {
    if (active_mounts_count >= MAX_MOUNT_POINTS || !target_path) return false;

    // Megkeressük az első szabad mount slotot
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (!mount_table[i].is_active) {
            
            // Ha EXT4-et mountolunk, meghívjuk az alacsony szintű fizikai mountot [source: 1]
            if (fs_type == FS_TYPE_EXT4) {
                if (!filesystem.mount()) {
                    return false; // Fizikai EXT4 Superblock sérült vagy nem található [source: 1]
                }
            }

            // Regisztráljuk a VFS táblába
            string_copy(mount_table[i].path, target_path, 64);
            mount_table[i].type = fs_type;
            mount_table[i].is_active = true;
            mount_table[i].device_id = (source_dev[0] == 'v') ? 1 : 0; // pl. vda -> 1, sda -> 0

            active_mounts_count++;
            return true;
        }
    }
    return false;
}

// ESZKÖZ LEVÁLASZTÁSA (UNMOUNT)
bool MountManager::vfs_unmount(const char* target_path) {
    if (!target_path) return false;

    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (mount_table[i].is_active && string_compare(mount_table[i].path, target_path)) {
            mount_table[i].is_active = false;
            mount_table[i].path[0] = '\0';
            mount_table[i].type = FS_TYPE_NONE;
            
            active_mounts_count--;
            return true;
        }
    }
    return false;
}

// ABSTRACKT VFS FÁJLOLVASÁS (Keresztülvezeti a kérést az adott mount pont alrendszerén)
bool MountManager::vfs_read_file(const char* absolute_path, uint8_t* dest, size_t max_size) {
    if (!absolute_path || !dest) return false;

    // Alapértelmezett bejárás: Megkeressük, melyik mount ponthoz passzol az abszolút útvonal eleje
    // Egyszerűsített verzió: Ha a root (/) van mountolva EXT4-ként, átirányítjuk az ext4.cpp-hez
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (mount_table[i].is_active && mount_table[i].type == FS_TYPE_EXT4) {
            if (absolute_path[0] == '/') {
                uint32_t inode = filesystem.find_file_inode(absolute_path);
                if (inode == 0) return false; // Fájl nem található az EXT4 fában
                return filesystem.load_file_by_inode(inode, dest, max_size);
            }
        }
    }
    return false;
}

// Kilistázza a csatlakoztatott eszközöket a grafikus asztalra vizuális anchor-ként
void MountManager::list_mounts() {
    // Ha van aktív mount, egy kis szoftveres "Meghajtó" ikont (szürke négyzetet) villantunk fel a GUI-n
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (mount_table[i].is_active) {
            desktop.draw_rect(20, 40 + (i * 40), 32, 32, COLOR_ARGB(255, 128, 128, 128)); // Eszköz ikon
            desktop.render();
        }
    }
}

MountManager vfs_system;
