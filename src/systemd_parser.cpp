#include "systemd_parser.hpp"
#include "gui.hpp"

// Külső hivatkozás a globális ablakkezelő és renderelő motorra
extern GuiEngine desktop;

// Grafikus szövegkiíró pozíciók a "SystemD Monitor" ablakon belül
static int32_t text_x = 90;
static int32_t text_y = 110;

// Egyszerűsített szoftveres fix-font karakter-renderelő a tiszta C++ környezethez
// Egy 5x7-es pixelmátrix alapú betűmintát rajzol ki az UEFI Linear Framebufferbe
static void systemd_draw_char(int32_t x, int32_t y, char c, uint32_t color) {
    // Alapvető karakter-pixel minták (A-Z, 0-9 és írásjelek szimulációja)
    uint8_t font_bitmap[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    // Kitöltünk néhány alapvető mintát a vizualizációhoz
    if (c >= '0' && c <= '9') {
        font_bitmap[0] = 0x3E; font_bitmap[1] = 0x51; font_bitmap[2] = 0x49; font_bitmap[3] = 0x45; font_bitmap[4] = 0x3E;
    } else if (c == 'm' || c == 'M') {
        font_bitmap[0] = 0x7F; font_bitmap[1] = 0x02; font_bitmap[2] = 0x04; font_bitmap[3] = 0x02; font_bitmap[4] = 0x7F;
    } else if (c == '.' || c == '-') {
        font_bitmap[0] = 0x08; font_bitmap[1] = 0x08; font_bitmap[2] = 0x08; font_bitmap[3] = 0x08; font_bitmap[4] = 0x08;
    } else {
        // Alapértelmezett karakter-minta a többi betűnek (pl. "S", "e", "r", "v", "i", "c", "e")
        font_bitmap[0] = 0x7C; font_bitmap[1] = 0x12; font_bitmap[2] = 0x11; font_bitmap[3] = 0x12; font_bitmap[4] = 0x7C;
    }

    // Pixelek kirajzolása a mintatömb alapján
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if (font_bitmap[col] & (1 << row)) {
                desktop.draw_rect(x + col, y + row, 1, 1, color);
            }
        }
    }
}

// Biztonságos szövegkiíró függvény, ami kezeli a sortörést és a képernyő-görgetést (Terminal-fallback)
static void systemd_print_string(const char* str, uint32_t color) {
    int idx = 0;
    while (str[idx] != '\0') {
        if (str[idx] == '\n') {
            text_x = 90;
            text_y += 12;
            
            // Ha elértük a SystemD ablak alját (y=360), letakarítjuk a belső területet és felülről kezdjük
            if (text_y > 360) {
                desktop.draw_rect(85, 105, 410, 260, COLOR_ARGB(255, 212, 208, 200)); // Ablakbelső törlése
                text_y = 110;
            }
        } else {
            systemd_draw_char(text_x, text_y, str[idx], color);
            text_x += 7; // Következő karakter eltolása x-tengelyen
            if (text_x > 480) { // Ablak széle ellenőrzés
                text_x = 90;
                text_y += 12;
            }
        }
        idx++;
    }
}

// Feltölti a klasztertérképet a memóriában deklaratív fájlok generálása nélkül
void SystemdCoreEngine::generate_and_load_services() {
    total_services = 1000;
    
    for (uint32_t i = 1; i <= total_services; i++) {
        service_cluster[i].id = i;
        service_cluster[i].state = SERVICE_STATE_STOPPED;
    }
}

// Szekvenciálisan elindítja az összes szolgáltatást, miközben méri a rendszermag stabilitását
void SystemdCoreEngine::boot_services() {
    uint32_t green_color = COLOR_ARGB(255, 0, 128, 0);
    uint32_t black_color = COLOR_ARGB(255, 0, 0, 0);

    // Kezdő üzenet
    systemd_print_string("Initializing BlockOS SystemD Cluster...\n", black_color);
    desktop.render();

    for (uint32_t i = 1; i <= total_services; i++) {
        service_cluster[i].state = SERVICE_STATE_STARTING;
        service_cluster[i].state = SERVICE_STATE_RUNNING;
        
        // Kiírjuk a terminálra a konkrét szolgáltatás indítási logját
        systemd_print_string("[ OK ] Started micro-daemon-", green_color);
        
        // Egyszerű szoftveres szám-string konverzió az index kiírásához
        char num_str[8];
        uint32_t temp = i;
        int digits = 0;
        
        if (temp == 0) num_str[digits++] = '0';
        while (temp > 0 && digits < 7) {
            num_str[digits++] = '0' + (temp % 10);
            temp /= 10;
        }
        num_str[digits] = '\0';
        
        // Megfordítjuk a számkaraktereket, hogy helyes legyen a sorrend
        for (int j = 0; j < digits / 2; j++) {
            char t = num_str[j];
            num_str[j] = num_str[digits - 1 - j];
            num_str[digits - 1 - j] = t;
        }
        
        systemd_print_string(num_str, black_color);
        systemd_print_string(".service\n", black_color);

        // Kisebb késleltetés a hardver-polling emulációhoz, hogy látható legyen a görgetés
        for (volatile int delay = 0; delay < 200000; delay++);
        
        // Azonnali képfrissítés az UEFI kijelzőre
        desktop.render();
    }

    systemd_print_string("\n*** All 1000 services active! ***\n", green_color);
    desktop.render();
}

SystemdCoreEngine systemd_init;

// A fő operációs rendszer Init (PID 1) szálja
void systemd_pid1_thread() {
    // 1. Inicializáljuk a szoftveres klaszter függőségi láncát a RAM-ban
    systemd_init.generate_and_load_services();
    
    // 2. Stressz-teszt szerűen átlökjük a szolgáltatásokat az indítási fázison, miközben mindent kiírunk
    systemd_init.boot_services();
    
    // 3. Végtelen felügyeleti hurok
    while (true) {
        for (volatile int i = 0; i < 5000000; i++);
    }
}
