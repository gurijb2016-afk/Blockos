#pragma once
#include <stdint.h>

class BlockOsShell {
private:
    char input_buffer[128];
    uint32_t buffer_index;

    void execute_command();
    void clear_buffer();

public:
    BlockOsShell() : buffer_index(0) {}
    void init_shell();
    void update_shell(); // A scheduler vagy a főhurok hívja meg folyamatosan
};

extern BlockOsShell system_shell;
void shell_runtime_thread(); // Szál prototípus
