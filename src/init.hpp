#pragma once
#include <stdint.h>
#include <stddef.h>

// Rendszerindítási fázisok státuszkódjai
enum BootPhase {
    PHASE_VFS_CHECK,
    PHASE_ENV_SETUP,
    PHASE_CONFIG_LOAD,
    PHASE_SUBSYSTEMS_START,
    PHASE_BOOT_COMPLETE
};

struct EnvVariable {
    char name[32];
    char value[64];
    bool is_active;
};

class InitManager {
private:
    EnvVariable env_table[16];
    uint32_t    env_count;
    BootPhase   current_phase;

    bool internal_strcmp(const char* s1, const char* s2);
    void internal_strcpy(char* dest, const char* src, size_t max_len);
    void log_to_init_window(const char* message, uint32_t color);

public:
    InitManager();

    // Felhasználói szintű boot szekvencia indítása
    bool execute_user_boot();

    // Környezeti változók kezelése (Environment Variables)
    bool set_env(const char* name, const char* value);
    const char* get_env(const char* name);
    
    BootPhase get_current_phase() { return current_phase; }
};

extern InitManager user_init;

// A Scheduler által futtatott aszinkron felhasználói init szál prototípusa
void user_space_init_thread();
