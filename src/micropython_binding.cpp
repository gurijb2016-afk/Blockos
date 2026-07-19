#include "micropython.hpp"
#include "../gui/gui.hpp"

// Hivatkozás a globális aszinkron ablakkezelő motorra
extern GuiEngine desktop;

// Statikus koordináták a Python konzol szövegének kirajzolásához a SystemD ablakon belül
static int32_t console_x = 90;
static int32_t console_y = 110;

// --- MicroPython Hardver Absztrakciós Réteg (HAL) Portolás ---
// Ezt a függvényt hívja meg a MicroPython belső 'print()' metódusa, amikor karaktereket akar kiírni
extern "C" void mp_hal_stdout_tx_strn(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            console_x = 90;
            console_y += 16; // Új sor: lejjebb lépünk 16 pixellel
            if (console_y > 380) console_y = 110; // Egyszerű képernyőgörgetés-fallback
        } else if (str[i] == '\r') {
            console_x = 90;
        } else {
            // Karakterenkénti pixel-blokk rajzolás a grafikus felületre (egyszerűsített fix font szimuláció)
            // Egy 8x8-as kis négyzetet rajzolunk minden karakter helyére a vizualizációhoz
            uint32_t text_color = COLOR_ARGB(255, 0, 0, 0); // Fekete betűszín
            desktop.draw_rect(console_x, console_y, 8, 12, text_color);
            console_x += 10; // Következő karakter eltolása
            if (console_x > 460) { // Ablak széle ellenőrzés
                console_x = 90;
                console_y += 16;
            }
        }
    }
}

// Inicializálja a virtuális gépet és ráköti a Garbage Collectort a dedikált memóriára
void BlockOsMicroPython::boot_interpreter() {
    if (is_initialized) return;
    
    // Átadjuk a MicroPythonnak a dedikált 256 KB-os memóriaterületet
    gc_init(&python_heap[0], &python_heap[sizeof(python_heap) - 1]);
    
    // VM mag indítása
    mp_init();
    is_initialized = true;
    
    // Üdvözlő üzenet kiküldése a BlockOS grafikus konzoljára
    const char* welcome_msg = "=== MicroPython v1.22 Embedded in BlockOS ===\n";
    mp_hal_stdout_tx_strn(welcome_msg, 45);
}

// Lefuttat egy tetszőleges Python szkriptet a rendszermagon belül
bool BlockOsMicroPython::run_script(const char* source_code) {
    if (!is_initialized || !source_code) return false;
    
    // Végrehajtás a belső parseren és compileren keresztül
    int result = mp_execute_from_str(source_code);
    
    return (result == 0);
}

// Biztonságos leállás
void BlockOsMicroPython::shutdown_interpreter() {
    if (!is_initialized) return;
    mp_deinit();
    is_initialized = false;
}

// Globális interpreter példány élesítése a rendszer számára
BlockOsMicroPython python_runtime;
