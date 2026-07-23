#include <stdio.h>

// Hivatkozás a Blockos meglévő ELF betöltőjére (Blockos/kernel/elf_loader.cpp)
extern "C" int elf_loader_exec(const char* path, const char* argv[]);

void command_launch_brave() {
    printf("[Blockos]: Inicializálás... Brave böngésző indítása.\n");

    // A valódi Linuxos Brave bináris elvárja ezeket a kapcsolókat egyedi kernel alatt
    const char* brave_arguments[] = { 
        "/system/bin/brave", 
        "--no-sandbox",          // A Linux kernel-szintű névterek kikapcsolása
        "--disable-gpu",         // Szoftveres kirajzolási mód kényszerítése
        "--disable-software-rasterizer",
        nullptr 
    };

    // Betöltés a te fájlrendszeredből (Blockos/filesystem/system/)
    int process_id = elf_loader_exec("/system/bin/brave", brave_arguments);

    if (process_id > 0) {
        printf("[Blockos]: A Brave sikeresen fut a háttérben. PID: %d\n", process_id);
    } else {
        printf("[Hiba]: Nem sikerült betölteni a Brave ELF binárist! Ellenőrizd a /system/bin/ mappát.\n");
    }
}
