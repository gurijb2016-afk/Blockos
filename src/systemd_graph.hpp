#pragma once
#include <stdint.h>
#include <stddef.h>

#define MAX_DAEMONS 1000
#define CORE_COUNT 4

struct SystemdUnit {
    uint32_t id;
    uint8_t  assigned_core;
    uint8_t  state;         // 0: INACTIVE, 1: LOADING, 2: ACTIVE, 3: FAILED
    uint16_t x, y;          // Vizuális koordináták a topológia gráfban
    uint32_t load_time_us;  // Szimulált vagy valós betöltési idő mikromásodpercben
};

class SuperSystemdEngine {
private:
    SystemdUnit units[MAX_DAEMONS];
    uint32_t    total_loaded;
    uint32_t    cpu_history[64]; // CPU terhelési grafikon memória buffer
    uint32_t    history_index;

    uint32_t pseudo_rand(uint32_t seed);
    void draw_node_link(uint8_t* bb, uint32_t fb_w, int x1, int y1, int x2, int y2, uint32_t color);

public:
    SuperSystemdEngine();
    void generate_topology_tree();
    void render_dashboard_frame(uint8_t* bb, uint32_t fb_w, int win_x, int win_y, int win_w, int win_h, uint32_t current_index);
    void run_heavy_boot_stress(uint8_t* bb, uint32_t fb_w, int win_x, int win_y, int win_w, int win_h, void* fb_struct);
};

extern SuperSystemdEngine advanced_systemd;
