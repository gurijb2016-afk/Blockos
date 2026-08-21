#include "doomgeneric.h"

#include <stdint.h>
#include <stddef.h>

extern "C" {


/*
 * Return the physical/logical framebuffer address.
 */
void* blockos_framebuffer(void) __attribute__((weak));


/*
 * Return framebuffer width.
 */
int blockos_framebuffer_width(void) __attribute__((weak));


/*
 * Return framebuffer height.
 */
int blockos_framebuffer_height(void) __attribute__((weak));


/*
 * Return framebuffer pitch in bytes.
 */
size_t blockos_framebuffer_pitch(void) __attribute__((weak));


/*
 * Present/flush framebuffer.
 */
void blockos_framebuffer_present(void) __attribute__((weak));


/*
 * Return monotonic time in milliseconds.
 */
uint64_t blockos_time_ms(void) __attribute__((weak));


/*
 * Sleep for approximately 'milliseconds'.
 */
void blockos_sleep_ms(uint32_t milliseconds)
    __attribute__((weak));


/*
 * Poll one keyboard event.
 *
 * Return 1 if an event was returned.
 * Return 0 if there is no event.
 *
 * pressed:
 *   1 = key down
 *   0 = key up
 *
 * key:
 *   Doomgeneric/Doom key code.
 */
int blockos_keyboard_poll(
    unsigned char* pressed,
    unsigned char* key)
    __attribute__((weak));


/*
 * Optional window title hook.
 */
void blockos_set_window_title(
    const char* title)
    __attribute__((weak));

}


/*
 * ============================================================
 * Local state
 * ============================================================
 */

static uint32_t* g_framebuffer = nullptr;

static int g_width = 0;
static int g_height = 0;
static size_t g_pitch = 0;

static uint64_t g_start_time = 0;


/*
 * Doomgeneric's internal framebuffer is RGB888/RGBA-style
 * 32-bit pixels depending on the version.
 *
 * Keep a local buffer so that the BlockOS framebuffer does not
 * have to be directly writable by Doom.
 */
static uint32_t* g_doom_buffer = nullptr;

static size_t g_doom_buffer_pixels = 0;


/*
 * ============================================================
 * Small local memory helpers
 * ============================================================
 */

static void copy_pixels(
    uint32_t* dst,
    const uint32_t* src,
    size_t count)
{
    for (size_t i = 0; i < count; ++i)
        dst[i] = src[i];
}


/*
 * ============================================================
 * DG_Init
 * ============================================================
 */

void DG_Init(void)
{
    /*
     * Verify BlockOS framebuffer API.
     */
    if (!blockos_framebuffer ||
        !blockos_framebuffer_width ||
        !blockos_framebuffer_height ||
        !blockos_framebuffer_pitch)
    {
        return;
    }

    g_framebuffer =
        static_cast<uint32_t*>(
            blockos_framebuffer());

    g_width =
        blockos_framebuffer_width();

    g_height =
        blockos_framebuffer_height();

    g_pitch =
        blockos_framebuffer_pitch();


    if (!g_framebuffer ||
        g_width <= 0 ||
        g_height <= 0)
    {
        g_framebuffer = nullptr;
        return;
    }


    /*
     * Doomgeneric normally renders at DOOMGENERIC_RESX x
     * DOOMGENERIC_RESY.
     */
    g_doom_buffer_pixels =
        static_cast<size_t>(
            DOOMGENERIC_RESX) *
        static_cast<size_t>(
            DOOMGENERIC_RESY);


    /*
     * doomgeneric's framebuffer is supplied by the
     * doomgeneric core through DG_DrawFrame().
     *
     * We do not allocate another framebuffer here.
     */


    if (blockos_time_ms)
        g_start_time =
            blockos_time_ms();
    else
        g_start_time = 0;
}


/*
 * ============================================================
 * DG_DrawFrame
 * ============================================================
 *
 * Doom calls this once for every rendered frame.
 */

void DG_DrawFrame(void)
{
    /*
     * doomgeneric provides this global framebuffer.
     *
     * Depending on the exact doomgeneric version this is:
     *
     *     DG_ScreenBuffer
     *
     * containing DOOMGENERIC_RESX * DOOMGENERIC_RESY
     * 32-bit pixels.
     */
    if (!g_framebuffer)
        return;


    /*
     * Source framebuffer supplied by doomgeneric.
     */
    uint32_t* source =
        reinterpret_cast<uint32_t*>(
            DG_ScreenBuffer);


    /*
     * Direct 1:1 copy if BlockOS framebuffer has the same
     * resolution.
     */
    if (g_width == DOOMGENERIC_RESX &&
        g_height == DOOMGENERIC_RESY &&
        g_pitch ==
            static_cast<size_t>(g_width) *
            sizeof(uint32_t))
    {
        copy_pixels(
            g_framebuffer,
            source,
            g_doom_buffer_pixels);

        if (blockos_framebuffer_present)
            blockos_framebuffer_present();

        return;
    }


    /*
     * ========================================================
     * Scaling path
     * ========================================================
     *
     * Nearest-neighbour scaling allows Doom to run on a
     * different BlockOS display resolution.
     */

    if (g_width <= 0 ||
        g_height <= 0)
    {
        return;
    }


    const size_t pitch_pixels =
        g_pitch / sizeof(uint32_t);


    for (int y = 0; y < g_height; ++y)
    {
        int src_y =
            (y * DOOMGENERIC_RESY) /
            g_height;

        if (src_y >= DOOMGENERIC_RESY)
            src_y = DOOMGENERIC_RESY - 1;


        uint32_t* dst =
            g_framebuffer +
            static_cast<size_t>(y) *
            pitch_pixels;


        for (int x = 0; x < g_width; ++x)
        {
            int src_x =
                (x * DOOMGENERIC_RESX) /
                g_width;

            if (src_x >= DOOMGENERIC_RESX)
                src_x = DOOMGENERIC_RESX - 1;


            dst[x] =
                source[
                    static_cast<size_t>(src_y) *
                    DOOMGENERIC_RESX +
                    static_cast<size_t>(src_x)
                ];
        }
    }


    if (blockos_framebuffer_present)
        blockos_framebuffer_present();
}


/*
 * ============================================================
 * DG_SleepMs
 * ============================================================
 */

void DG_SleepMs(uint32_t milliseconds)
{
    if (blockos_sleep_ms)
    {
        blockos_sleep_ms(milliseconds);
        return;
    }


    /*
     * Fallback busy wait.
     *
     * Only used if BlockOS has not yet connected its timer.
     */
    if (blockos_time_ms)
    {
        uint64_t start =
            blockos_time_ms();

        while (
            blockos_time_ms() -
            start <
            milliseconds)
        {
            asm volatile("pause");
        }
    }
}


/*
 * ============================================================
 * DG_GetKey
 * ============================================================
 *
 * Return:
 *
 *   1 -> event available
 *   0 -> no event
 */

int DG_GetKey(
    int* pressed,
    unsigned char* doomKey)
{
    if (!pressed || !doomKey)
        return 0;

    if (!blockos_keyboard_poll)
        return 0;


    unsigned char down = 0;
    unsigned char key = 0;


    if (!blockos_keyboard_poll(
            &down,
            &key))
    {
        return 0;
    }


    *pressed =
        down ? 1 : 0;

    *doomKey =
        key;


    return 1;
}


/*
 * ============================================================
 * DG_SetWindowTitle
 * ============================================================
 */

void DG_SetWindowTitle(
    const char* title)
{
    if (!title)
        return;

    if (blockos_set_window_title)
        blockos_set_window_title(title);
}
