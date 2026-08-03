#include "systemd_graph.hpp"
#include "gui.hpp"
#include "backbuffer.h"

#include <stdint.h>
#include <stddef.h>

/*
 * A backbuffer.h-nak ezeket a függvényeket kell biztosítania:
 *
 *   bb_draw_rect()
 *   bb_draw_char()
 *   bb_draw_text()
 *   bb_blit_to_fb()
 *   bb_blit_region_to_fb()
 *
 * Ha a saját backbuffer.h más neveket használ, azokat igazítsd hozzá.
 */

// -----------------------------------------------------------------------------
// Framebuffer blit deklarációk
// -----------------------------------------------------------------------------

extern "C" void bb_blit_to_fb(
    void* fb,
    const uint8_t* bb
);

extern "C" void bb_blit_rect_to_fb(
    void* fb,
    const uint8_t* bb,
    int x,
    int y,
    int w,
    int h
);

// -----------------------------------------------------------------------------
// Egyszerű szövegkiíró helper
// -----------------------------------------------------------------------------

static void draw_msg(
    uint8_t* bb,
    uint32_t fb_w,
    const char* text,
    int tx,
    int ty,
    uint32_t color
)
{
    if (!bb || !text)
        return;

    while (*text) {
        bb_draw_char(
            bb,
            fb_w,
            tx,
            ty,
            *text,
            color
        );

        tx += 8;
        ++text;
    }
}

// -----------------------------------------------------------------------------
// Pseudo random
// -----------------------------------------------------------------------------

uint32_t SuperSystemdEngine::pseudo_rand(uint32_t seed)
{
    static uint32_t state = 0xACE1u;

    state =
        (state >> 1) ^
        (-(state & 1u) & 0xB400u);

    return state ^ seed;
}

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------

SuperSystemdEngine::SuperSystemdEngine()
    : total_loaded(0),
      history_index(0)
{
    for (int i = 0; i < 64; ++i)
        cpu_history[i] = 0;
}

// -----------------------------------------------------------------------------
// Generate topology
// -----------------------------------------------------------------------------

void SuperSystemdEngine::generate_topology_tree()
{
    total_loaded = MAX_DAEMONS;

    for (uint32_t i = 0; i < total_loaded; ++i) {

        units[i].id = i + 1;
        units[i].state = 0;

        units[i].assigned_core =
            i % CORE_COUNT;

        units[i].load_time_us =
            1200 + (pseudo_rand(i) % 4500);

        /*
         * Egyszerű, kernelbarát koordináta-generálás.
         * Nincs szükség lebegőpontos matematikára.
         */

        uint32_t radius =
            25 + ((i * 110) / total_loaded);

        uint32_t angle_scaled =
            (i * 628) / 100;

        int32_t x_shift =
            ((int32_t)(
                radius *
                (100 - (angle_scaled % 100))
            )) / 100;

        int32_t y_shift =
            ((int32_t)(
                radius *
                (angle_scaled % 100)
            )) / 100;

        switch (i % 4) {

            case 0:
                x_shift = -x_shift;
                y_shift = -y_shift;
                break;

            case 1:
                x_shift = -x_shift;
                break;

            case 2:
                y_shift = -y_shift;
                break;

            default:
                break;
        }

        units[i].x = 240 + x_shift;
        units[i].y = 220 + y_shift;
    }
}

// -----------------------------------------------------------------------------
// Node link
// -----------------------------------------------------------------------------

void SuperSystemdEngine::draw_node_link(
    uint8_t* bb,
    uint32_t fb_w,
    int x1,
    int y1,
    int x2,
    int y2,
    uint32_t color
)
{
    if (!bb)
        return;

    int dx =
        (x2 > x1)
            ? (x2 - x1)
            : (x1 - x2);

    int dy =
        (y2 > y1)
            ? (y2 - y1)
            : (y1 - y2);

    int sx =
        (x1 < x2)
            ? 1
            : -1;

    int sy =
        (y1 < y2)
            ? 1
            : -1;

    int err = dx - dy;

    uint32_t* buf32 =
        reinterpret_cast<uint32_t*>(bb);

    /*
     * A backbuffer tényleges szélességét használjuk.
     * A magasságot nem ismerjük itt, ezért a koordinátákat
     * a dashboard által használt tartományra korlátozzuk.
     */

    while (true) {

        if (x1 >= 0 &&
            x1 < (int)fb_w &&
            y1 >= 0 &&
            y1 < 768) {

            buf32[
                ((uint32_t)y1 * fb_w) +
                (uint32_t)x1
            ] = color;
        }

        if (x1 == x2 && y1 == y2)
            break;

        int e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }

        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// -----------------------------------------------------------------------------
// Dashboard frame
// -----------------------------------------------------------------------------

void SuperSystemdEngine::render_dashboard_frame(
    uint8_t* bb,
    uint32_t fb_w,
    int win_x,
    int win_y,
    int win_w,
    int win_h,
    uint32_t current_index
)
{
    if (!bb)
        return;

    uint32_t c_bg =
        0x00111111;

    uint32_t c_gray =
        0x00333333;

    uint32_t c_green =
        0x0000FF00;

    uint32_t c_blue =
        0x0000D0FF;

    uint32_t c_yellow =
        0x00FFD700;

    uint32_t c_magenta =
        0x00FF00FF;

    // -----------------------------------------------------------------
    // Belső terület törlése
    // -----------------------------------------------------------------

    if (win_w > 4 && win_h > 28) {

        bb_draw_rect(
            bb,
            fb_w,
            win_x + 2,
            win_y + 26,
            win_w - 4,
            win_h - 28,
            c_bg
        );
    }

    // -----------------------------------------------------------------
    // Topology
    // -----------------------------------------------------------------

    uint32_t visible =
        current_index;

    if (visible > total_loaded)
        visible = total_loaded;

    for (uint32_t i = 0;
         i < visible;
         i += 8) {

        if (i + 1 < visible) {

            draw_node_link(
                bb,
                fb_w,

                win_x + units[i].x,
                win_y + units[i].y,

                win_x + units[i + 1].x,
                win_y + units[i + 1].y,

                0x00222222
            );
        }
    }

    // -----------------------------------------------------------------
    // Nodes
    // -----------------------------------------------------------------

    for (uint32_t i = 0;
         i < visible;
         ++i) {

        uint32_t node_color =
            c_green;

        if (visible > 0 &&
            i == visible - 1) {

            node_color =
                c_magenta;
        }
        else if ((i % 120) == 0) {

            node_color =
                c_yellow;
        }

        bb_draw_rect(
            bb,
            fb_w,

            win_x + units[i].x,
            win_y + units[i].y,

            3,
            3,

            node_color
        );
    }

    // -----------------------------------------------------------------
    // CPU monitor
    // -----------------------------------------------------------------

    int panel_x =
        win_x + 360;

    int panel_y =
        win_y + 35;

    bb_draw_rect(
        bb,
        fb_w,
        panel_x,
        panel_y,
        140,
        90,
        c_gray
    );

    draw_msg(
        bb,
        fb_w,
        "CORE MONITOR",
        panel_x + 10,
        panel_y + 5,
        c_blue
    );

    for (int m = 0;
         m < CORE_COUNT;
         ++m) {

        uint32_t load_val =
            40 +
            (pseudo_rand(
                current_index + (uint32_t)m
            ) % 55);

        if (current_index >= MAX_DAEMONS)
            load_val = 0;

        int bar_w =
            (int)((load_val * 60) / 100);

        int bar_y =
            panel_y + 20 + (m * 10);

        bb_draw_rect(
            bb,
            fb_w,
            panel_x + 10,
            bar_y,
            60,
            6,
            0x00222222
        );

        uint32_t bar_color =
            ((m % 2) == 0)
                ? c_green
                : c_blue;

        bb_draw_rect(
            bb,
            fb_w,
            panel_x + 10,
            bar_y,
            bar_w,
            6,
            bar_color
        );
    }

    // -----------------------------------------------------------------
    // CPU history graph
    // -----------------------------------------------------------------

    int graph_x =
        win_x + 360;

    int graph_y =
        win_y + 140;

    bb_draw_rect(
        bb,
        fb_w,
        graph_x,
        graph_y,
        140,
        100,
        c_gray
    );

    draw_msg(
        bb,
        fb_w,
        "SYSTEM LOAD HISTORY",
        graph_x + 5,
        graph_y + 5,
        c_blue
    );

    uint32_t current_load =
        30 +
        (pseudo_rand(current_index) % 65);

    if (current_index >= MAX_DAEMONS)
        current_load = 2;

    cpu_history[
        history_index % 64
    ] = current_load;

    history_index++;

    for (int g = 0; g < 62; ++g) {

        uint32_t index =
            (history_index + (uint32_t)g) % 64;

        uint32_t val =
            cpu_history[index];

        int h =
            (int)((val * 70) / 100);

        if (h < 0)
            h = 0;

        if (h > 70)
            h = 70;

        bb_draw_rect(
            bb,
            fb_w,

            graph_x + 5 + (g * 2),
            graph_y + 90 - h,

            1,
            h,

            c_magenta
        );
    }

    // -----------------------------------------------------------------
    // Status
    // -----------------------------------------------------------------

    draw_msg(
        bb,
        fb_w,
        "LAUNCHING ASYNCHRONOUS DAEMON CLUSTER...",
        win_x + 15,
        win_y + win_h - 45,
        c_blue
    );
}

// -----------------------------------------------------------------------------
// Heavy boot stress
// -----------------------------------------------------------------------------

void SuperSystemdEngine::run_heavy_boot_stress(
    uint8_t* bb,
    uint32_t fb_w,
    int win_x,
    int win_y,
    int win_w,
    int win_h,
    void* fb_struct
)
{
    if (!bb)
        return;

    generate_topology_tree();

    // -------------------------------------------------------------
    // 1000 unit "boot"
    // -------------------------------------------------------------

    for (uint32_t i = 1;
         i <= MAX_DAEMONS;
         ++i) {

        render_dashboard_frame(
            bb,
            fb_w,
            win_x,
            win_y,
            win_w,
            win_h,
            i
        );

        /*
         * Fontos:
         * az extern "C" deklarációk a fájl tetején vannak.
         * Nem deklaráljuk őket a függvény belsejében.
         */

        if (fb_struct) {

            bb_blit_rect_to_fb(
                fb_struct,
                bb,
                win_x + 2,
                win_y + 26,
                win_w - 4,
                win_h - 28
            );
        }

        // Egyszerű kernelbarát késleltetés
        for (volatile int d = 0;
             d < 12000;
             ++d) {
            asm volatile("" ::: "memory");
        }
    }

    // -------------------------------------------------------------
    // Final status
    // -------------------------------------------------------------

    draw_msg(
        bb,
        fb_w,
        "STATUS: SYSTEMD BOOT SUCCESS! 1000 UNITS ONLINE.",
        win_x + 15,
        win_y + win_h - 25,
        0x0000FF00
    );

    if (fb_struct) {

        bb_blit_to_fb(
            fb_struct,
            bb
        );
    }
}

// -----------------------------------------------------------------------------
// Global instance
// -----------------------------------------------------------------------------

SuperSystemdEngine advanced_systemd;