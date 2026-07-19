#include "init.hpp"
#include "mount.hpp"
#include "gui.hpp"

extern MountManager vfs_system;
extern GuiEngine desktop;

// Külső hivatkozás a systemd_parser-ben megírt szövegkiíróra a vizuális egységért
static int32_t init_log_x = 90;
static int32_t init_log_y = 240;

InitManager::InitManager() : env_count(0), current_phase(PHASE_VFS_CHECK) {
    for (int i = 0; i < 16; i++) {
        env_table[i].is_active = false;
    }
}

bool InitManager::internal_strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' || s2[i] != '\0') {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return true;
}

void InitManager::internal_strcpy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (src[i] != '\0' && i < (max_len - 1)) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Logüzenetek kiírása a SystemD/Init grafikus ablak alsó szekciójába
void InitManager::log_to_init_window(const char* message, uint32_t color) {
    int idx = 0;
    while (message[idx] != '\0') {
        if (message[idx] == '\n') {
            init_log_x = 90;
            init_log_y += 12;
            if (init_log_y > 380) init_log_y = 240;
        } else {
            // Karakterenkénti pixel-blokk rajzolás
            desktop.draw_rect(init_log_x, init_log_y, 6, 8, color);
            init_log_x += 7;
            if (init_log_x > 470) {
                init_log_x = 90;
                init_log_y += 12;
            }
        }
        idx++;
    }
    desktop.render();
}

bool InitManager::set_env(const char* name, const char* value) {
    if (!name || !value) return false;
    
    // Frissítés, ha már létezik
    for (uint32_t i = 0; i < env_count; i++) {
        if (env_table[i].is_active && internal_strcmp(env_table[i].name, name)) {
            internal_strcpy(env_table[i].value, value, 64);
            return true;
        }
    }
    
    // Új változó beszúrása
    if (env_count < 16) {
        internal_strcpy(env_table[env_count].name, name, 32);
        internal_strcpy(env_table[env_count].value, value, 64);
        env_table[env_count].is_active = true;
        env_count++;
        return true;
    }
    return false;
}

const char* InitManager::get_env(const char* name) {
    if (!name) return nullptr;
    for (uint32_t i = 0; i < env_count; i++) {
        if (env_table[i].is_active && internal_strcmp(env_table[i].name, name)) {
            return env_table[i].value;
        }
    }
    return nullptr;
}

// A FELHASZNÁLÓI KÖRNYEZET SZEKVENCIÁLIS BETÖLTÉSE (USER-SPACE BOOT LÁNCOZAT)
bool InitManager::execute_user_boot() {
    uint32_t c_info = COLOR_ARGB(255, 0, 0, 255);
    uint32_t c_success = COLOR_ARGB(255, 0, 128, 0);

    // 1. FÁZIS: Virtuális Fájlrendszer (VFS) integritás ellenőrzése
    current_phase = PHASE_VFS_CHECK;
    log_to_init_window("[Init] Checking active mount points...\n", c_info);
    vfs_system.list_mounts();

    // 2. FÁZIS: Alapértelmezett környezeti változók (POSIX-like PATH) felállítása
    current_phase = PHASE_ENV_SETUP;
    log_to_init_window("[Init] Setting up system environment...\n", c_info);
    set_env("PATH", "/bin:/usr/bin");
    set_env("HOME", "/root");
    set_env("SHELL", "/bin/micropython");

    // 3. FÁZIS: Konfigurációs fájlok parszolása (Emulált /etc/rc.local és sysctl)
    current_phase = PHASE_CONFIG_LOAD;
    log_to_init_window("[Init] Loading /etc/boot.conf configurations...\n", c_info);
    for (volatile int d = 0; d < 400000; d++); // Konfigurációs I/O olvasás késleltetés-szimuláció

    // 4. FÁZIS: Felhasználói alrendszerek és szoftveres démonok indítása
    current_phase = PHASE_SUBSYSTEMS_START;
    log_to_init_window("[Init] Spawning user-space shells...\n", c_info);

    // 5. FÁZIS: Boot folyamat sikeres lezárása
    current_phase = PHASE_BOOT_COMPLETE;
    log_to_init_window("[ SUCCESS ] User environment is ready.\n", c_success);
    
    return true;
}

InitManager user_init;

// Az aszinkron módon futó Init User Thread magja
void user_space_init_thread() {
    // Kisebb várakozás, hogy a kernel hardveres megszakításai és a SystemD alapjai felálljanak
    for (volatile int i = 0; i < 3000000; i++);

    // Felhasználói boot láncolat végrehajtása
    user_init.execute_user_boot();

    while (true) {
        // Alacsony prioritású háttér-felügyelet (idle loop tehermentesítéssel)
        for (volatile int i = 0; i < 1000000; i++);
    }
}
