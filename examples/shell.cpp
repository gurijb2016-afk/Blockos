#include "shell.hpp"
#include "keyboard.hpp"
#include "gui.hpp"

// Külső hivatkozások a szöveg kiíráshoz
extern void systemd_print_string(const char* str, uint32_t color);
extern GuiEngine desktop;

void BlockOsShell::clear_buffer() {
    for (int i = 0; i < 128; i++) input_buffer[i] = '\0';
    buffer_index = 0;
}

void BlockOsShell::init_shell() {
    clear_buffer();
    systemd_print_string("\nBlockOS Interactive Shell v1.0\n", COLOR_ARGB(255, 0, 128, 255));
    systemd_print_string("Type 'help' for commands.\n\nroot@blockos:~$ ", COLOR_ARGB(255, 0, 0, 0));
    desktop.render();
}

void BlockOsShell::execute_command() {
    uint32_t c_black = COLOR_ARGB(255, 0, 0, 0);
    uint32_t c_green = COLOR_ARGB(255, 0, 180, 0);

    systemd_print_string("\n", c_black);

    // 1. HELP parancs
    if (input_buffer[0] == 'h' && input_buffer[1] == 'e' && input_buffer[2] == 'l' && input_buffer[3] == 'p') {
        systemd_print_string("Available commands:\n", c_green);
        systemd_print_string("  help     - Show this report\n", c_green);
        systemd_print_string("  clear    - Clear terminal\n", c_green);
        systemd_print_string("  uname    - Show OS architecture information\n", c_green);
    } 
    // 2. UNAME parancs
    else if (input_buffer[0] == 'u' && input_buffer[1] == 'n' && input_buffer[2] == 'a' && input_buffer[3] == 'm' && input_buffer[4] == 'e') {
        systemd_print_string("BlockOS 1.0.0-release x86_64 Monolithic Core C++17\n", c_black);
    }
    // 3. CLEAR parancs
    else if (input_buffer[0] == 'c' && input_buffer[1] == 'l' && input_buffer[2] == 'e' && input_buffer[3] == 'a' && input_buffer[4] == 'r') {
        desktop.draw_rect(85, 105, 410, 260, COLOR_ARGB(255, 212, 208, 200)); // Konzolfelület törlése
    }
    // Ismeretlen parancs
    else if (buffer_index > 0) {
        systemd_print_string("bash: command not found: ", COLOR_ARGB(255, 200, 0, 0));
        systemd_print_string(input_buffer, c_black);
        systemd_print_string("\n", c_black);
    }

    systemd_print_string("root@blockos:~$ ", COLOR_ARGB(255, 0, 0, 0));
    clear_buffer();
    desktop.render();
}

void BlockOsShell::update_shell() {
    char c = input_keyboard.read_char_polling();
    if (c == 0) return;

    if (c == '\n') {
        execute_command();
    } 
    else if (c == '\b') { // Backspace karakter kezelése
        if (buffer_index > 0) {
            buffer_index--;
            input_buffer[buffer_index] = '\0';
            systemd_print_string("\b", COLOR_ARGB(255, 0, 0, 0));
            desktop.render();
        }
    } 
    else if (buffer_index < 126) {
        input_buffer[buffer_index++] = c;
        char echo_str[2] = {c, '\0'};
        systemd_print_string(echo_str, COLOR_ARGB(255, 0, 0, 0));
        desktop.render();
    }
}

BlockOsShell system_shell;

// Aszinkron shell szál, amit rárakhatunk a schedulerre
void shell_runtime_thread() {
    input_keyboard.init_keyboard();
    system_shell.init_shell();

    while (true) {
        system_shell.update_shell();
        // Tehermentesítő polling késleltetés a billentyűzetnek
        for (volatile int i = 0; i < 50000; i++);
    }
}
