#include "systemd_graph.hpp"
#include "gui.hpp"

// Szoftveres pszeudo-véletlenszám generáló (Mivel hardveres RNG vagy C standard lib nincs a kernelben)
uint32_t SuperSystemdEngine::pseudo_rand(uint32_t seed) {
    static uint32_t state = 0xACE1u;
    state = (state >> 1) ^ (-(state & 1u) & 0xB400u);
    return state ^ seed;
}

SuperSystemdEngine::SuperSystemdEngine() : total_loaded(0), history_index(0) {
    for(int i=0; i<64; i++) cpu_history[i] = 0;
}

// 1. TOPOLÓGIA GENERÁLÁS: Felépítjük az 1000 szolgáltatás koordinátáit egy pókháló-szerű hálózatba
void SuperSystemdEngine::generate_topology_tree() {
    total_loaded = MAX_DAEMONS;
    for (uint32_t i = 0; i < total_loaded; i++) {
        units[i].id = i + 1;
        units[i].state = 0; // INACTIVE
        units[i].assigned_core = (i % CORE_COUNT);
        units[i].load_time_us = 1200 + (pseudo_rand(i) % 4500);

        // Körkörös, fraktál-alapú elhelyezkedés kiszámítása az ablak közepéhez igazítva
        uint32_t radius = 25 + ((i * 110) / total_loaded);
        uint32_t angle_scaled = (i * 628) / 100; // Fix pontos radián szimuláció (2 * PI * 100)
        
        // Egyszerűsített szoftveres szinusz/koszinusz közelítés Taylor-sor nélkül a gyors hálózati elrendezéshez
        int32_t x_shift = ((int32_t)(radius * (100 - (angle_scaled % 100)))) / 100;
        int32_t y_shift = ((int32_t)(radius * (angle_scaled % 100))) / 100;
        
        if ((i % 4) == 0) { x_shift = -x_shift; y_shift = -y_shift; }
        else if ((i % 4) == 1) { x_shift = -x_shift; }
        else if ((i % 4) == 2) { y_shift = -y_shift; }

        units[i].x = 240 + x_shift; // Az ablak bal oldalán lévő gráf központja
        units[i].y = 220 + y_shift;
    }
}

// Egyedi pixel-vonalhúzó algoritmus (Bresenham-szerű vonalrajzoló függőségek összekötéséhez)
void SuperSystemdEngine::draw_node_link(uint8_t* bb, uint32_t fb_w, int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > x1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    uint32_t* buf32 = (uint32_t*)bb;

    while (true) {
        if (x1 >= 0 && x1 < 1024 && y1 >= 0 && y1 < 768) {
            buf32[y1 * fb_w + x1] = color;
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

// 2. ÉLŐ DASHBOARD KIRENDERELÉSE: Kirajzolja a gráfokat, a mag-statisztikákat és a CPU grafikont
void SuperSystemdEngine::render_dashboard_frame(uint8_t* bb, uint32_t fb_w, int win_x, int win_y, int win_w, int win_h, uint32_t current_index) {
    uint32_t c_bg      = 0x00111111; // Sötét Cyberpunk téma
    uint32_t c_gray    = 0x00333333;
    uint32_t c_green   = 0x0000FF00;
    uint32_t c_blue    = 0x0000D0FF;
    uint32_t c_yellow  = 0x00FFD700;
    uint32_t c_magenta = 0x00FF00FF;

    // Ablak belső tisztítása
    bb_draw_rect(bb, fb_w, win_x + 2, win_y + 26, win_w - 4, win_h - 28, c_bg);

    // --- INTERAKTÍV TOPOLÓGIA GRÁF ---
    // Összekötjük a node-okat halvány szürke vonalakkal, szimulálva az Init függőségeket
    for (uint32_t i = 0; i < current_index; i += 8) {
        if (i + 1 < current_index) {
            draw_node_link(bb, fb_w, win_x + units[i].x, win_y + units[i].y, win_x + units[i+1].x, win_y + units[i+1].y, 0x00222222);
        }
    }

    // Kirajzoljuk a szolgáltatások pontjait (Node) az aktuális állapotuk szerinti színben
    for (uint32_t i = 0; i < current_index; i++) {
        uint32_t node_color = c_green;
        if (i == current_index - 1) node_color = c_magenta; // Az éppen élesedő szolgáltatás lilán villog
        else if (i % 120 == 0) node_color = c_yellow;       // Szórványos figyelmeztetések

        // Apró 2x2-es négyzet-node rajzolása
        bb_draw_rect(bb, fb_w, win_x + units[i].x, win_y + units[i].y, 3, 3, node_color);
    }

    // --- MULTI-CORE PROCESSZOR MONITOR (Jobb felső panel) ---
    int panel_x = win_x + 360;
    bb_draw_rect(bb, fb_w, panel_x, win_y + 35, 140, 90, c_gray);
    
    auto print_msg = [&](const char* text, int tx, int ty, uint32_t col) {
        while(*text) { bb_draw_char(bb, fb_w, tx, ty, *text, col); tx += 8; text++; }
    };

    print_msg("CORE MONITOR", panel_x + 10, win_y + 40, c_blue);
    
    // Szimulált szál pörgés vizualizáció magonként
    for(int m=0; m<CORE_COUNT; m++) {
        uint32_t load_val = 40 + (pseudo_rand(current_index + m) % 55);
        if (current_index >= MAX_DAEMONS) load_val = 0;
        
        int bar_w = (load_val * 60) / 100;
        int bar_y = win_y + 55 + (m * 10);
        
        bb_draw_rect(bb, fb_w, panel_x + 10, bar_y, 60, 6, 0x00222222);
        bb_draw_rect(bb, fb_w, panel_x + 10, bar_y, bar_w, 6, (m % 2 == 0) ? c_green : c_blue);
    }

    // --- VALÓS IDEJŰ CPU TERHELÉSI GRAFIKON (Jobb alsó panel) ---
    int graph_x = win_x + 360;
    int graph_y = win_y + 140;
    bb_draw_rect(bb, fb_w, graph_x, graph_y, 140, 100, c_gray);
    print_msg("SYSTEM LOAD HISTORY", graph_x + 5, graph_y + 5, c_blue);

    // Frissítjük a történeti buffer értéket
    uint32_t current_load = 30 + (pseudo_rand(current_index) % 65);
    if (current_index >= MAX_DAEMONS) current_load = 2; // Nyugalmi állapot a boot után
    cpu_history[history_index % 64] = current_load;
    history_index++;

    // Kirajzoljuk a grafikont oszlopdiagramként
    for (int g = 0; g < 62; g++) {
        uint32_t val = cpu_history[(history_index + g) % 64];
        int h = (val * 70) / 100;
        bb_draw_rect(bb, fb_w, graph_x + 5 + (g * 2), graph_y + 90 - h, 1, h, c_magenta);
    }

    // --- ALSÓ PROGRESS MANAGER ---
    // Futó szöveges diagnosztika
    print_msg("LAUNCHING ASYNCHRONOUS DAEMON CLUSTER...", win_x + 15, win_y + win_h - 45, c_blue);
}

// 3. BRUTÁLIS STRESSZ TESZT VEZÉRLŐ: Ezt hívja meg az 'S' gomb, és ez kezeli az aszinkron animációs ciklust
void SuperSystemdEngine::run_heavy_boot_stress(uint8_t* bb, uint32_t fb_w, int win_x, int win_y, int win_w, int win_h, void* fb_struct) {
    generate_topology_tree();
    
    // Explicit típuskonverzió az src/main.cpp Framebuffer struktúrájához a blit-eléshez
    typedef void (*blit_func_ptr)(void*, const uint8_t*);
    // Dinamikus fallback a főhurok és a GOP Linear buffer szinkronizálására
    
    for (uint32_t i = 1; i <= MAX_DAEMONS; i++) {
        // Legeneráljuk és kirajzoljuk a High-Tech Dashboard aktuális képkockáját (Frame)
        render_dashboard_frame(bb, fb_w, win_x, win_y, win_w, win_h, i);
        
        // Csak az ablak belső koordinátáit frissítjük a kijelzőre a brutálisan gyors, akadásmentes 60 FPS-ért
        // Mivel a `bb_blit_rect_to_fb` közvetlenül elérhető az `efi_main`-ben:
        extern "C" void bb_blit_rect_to_fb(void* fb, const uint8_t* bb, int x, int y, int w, int h);
        bb_blit_rect_to_fb(fb_struct, bb, win_x + 2, win_y + 26, win_w - 4, win_h - 28);
        
        // Extrém finom késleltetés a valós idejű skálázódáshoz
        for (volatile int d = 0; d < 12000; d++);
    }

    // Végső állapot rögzítése a képernyőn
    print_msg("STATUS: SYSTEMD BOOT success! 1000 UNITS ONLINE.", win_x + 15, win_y + win_h - 25, 0x0000FF00);
    extern "C" void bb_blit_to_fb(void* fb, const uint8_t* bb);
    bb_blit_to_fb(fb_struct, bb);
}

SuperSystemdEngine advanced_systemd;
