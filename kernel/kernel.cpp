#include "backbuffer.h"
#include "events.hpp"
#include "ps2mouse.hpp"
#include "ps2keyboard.hpp"
#include "virtio_input.hpp"
#include "vfs.hpp"
#include "allocator.hpp"
#include "font8x8.h"

extern "C" {
#include <efi.h>
}
extern "C" {
#include <efilib.h>
}

#include <stdint.h>
#include <stddef.h>
#include <string.h>


struct Window {
    int x;
    int y;
    int w;
    int h;

    bool dragging;
    int drag_offset_x;
    int drag_offset_y;
};


/*
 * ============================================================
 * Draw file content
 * ============================================================
 */
static void draw_file_content(
    uint8_t* buffer,
    uint32_t width,
    const char* filename,
    int x,
    int y,
    int w,
    int h
) {
    bb_draw_rect(
        buffer,
        width,
        x,
        y,
        w,
        h,
        0x00FFFFFF
    );

    uint32_t size = 0;

    const uint8_t* data =
        vfs::read_file(filename, &size);

    if (!data)
        return;

    int tx = x + 4;
    int ty = y + 4;

    int cols = (w - 8) / 8;

    if (cols <= 0)
        return;

    int cx = 0;

    for (uint32_t i = 0; i < size; ++i) {

        char c = (char)data[i];

        if (c == '\n' || cx >= cols) {

            cx = 0;
            ty += 10;

            if (c == '\n')
                continue;
        }

        if (ty >= y + h - 8)
            break;

        bb_draw_char(
            buffer,
            width,
            tx + cx * 8,
            ty,
            c,
            0x00000000
        );

        ++cx;
    }
}


/*
 * ============================================================
 * Draw file list
 * ============================================================
 */
static void draw_file_list(
    uint8_t* buffer,
    uint32_t width,
    const Window& win
) {
    int x = win.x + 4;
    int y = win.y + 28;

    int w = win.w - 8;
    int h = win.h - 36;

    bb_draw_rect(
        buffer,
        width,
        x,
        y,
        w,
        h,
        0x00FFFFFF
    );

    int tx = win.x + 8;
    int ty = win.y + 32;

    size_t count =
        vfs::count_files();

    for (size_t i = 0; i < count; ++i) {

        const char* name =
            vfs::name_at(i);

        if (!name)
            continue;

        int line_y =
            ty + (int)i * 10;

        if (line_y >= win.y + win.h - 8)
            break;

        bb_draw_text(
            buffer,
            width,
            tx,
            line_y,
            name,
            0x00000000
        );
    }
}


/*
 * ============================================================
 * Draw main window
 * ============================================================
 */
static void draw_main_window(
    uint8_t* buffer,
    uint32_t width,
    const Window& win
) {
    /*
     * Window
     */
    bb_draw_rect(
        buffer,
        width,
        win.x,
        win.y,
        win.w,
        win.h,
        0x00C0C0C0
    );

    /*
     * Title bar
     */
    bb_draw_rect(
        buffer,
        width,
        win.x,
        win.y,
        win.w,
        24,
        0x00008080
    );

    /*
     * Title
     */
    bb_draw_text(
        buffer,
        width,
        win.x + 8,
        win.y + 6,
        "BlockOS",
        0x00FFFFFF
    );

    /*
     * Content
     */
    draw_file_content(
        buffer,
        width,
        "readme.txt",
        win.x + 4,
        win.y + 28,
        win.w - 8,
        win.h - 36
    );
}


/*
 * ============================================================
 * Draw editor
 * ============================================================
 */
static void draw_editor(
    uint8_t* buffer,
    uint32_t width,
    const Window& win,
    const char* filename,
    const char* text,
    size_t length
) {
    /*
     * Window
     */
    bb_draw_rect(
        buffer,
        width,
        win.x,
        win.y,
        win.w,
        win.h,
        0x00E0E0E0
    );

    /*
     * Title
     */
    bb_draw_rect(
        buffer,
        width,
        win.x,
        win.y,
        win.w,
        24,
        0x00006060
    );

    if (filename) {

        bb_draw_text(
            buffer,
            width,
            win.x + 8,
            win.y + 6,
            filename,
            0x00FFFFFF
        );
    }

    /*
     * Editor area
     */
    int area_x = win.x + 4;
    int area_y = win.y + 28;
    int area_w = win.w - 8;
    int area_h = win.h - 36;

    bb_draw_rect(
        buffer,
        width,
        area_x,
        area_y,
        area_w,
        area_h,
        0x00FFFFFF
    );

    int tx = win.x + 8;
    int ty = win.y + 32;

    int cols = (win.w - 16) / 8;

    if (cols <= 0)
        return;

    int cx = 0;

    for (size_t i = 0; i < length; ++i) {

        char c = text[i];

        if (c == '\n' || cx >= cols) {

            cx = 0;
            ty += 10;

            if (c == '\n')
                continue;
        }

        if (ty >= win.y + win.h - 8)
            break;

        bb_draw_char(
            buffer,
            width,
            tx + cx * 8,
            ty,
            c,
            0x00000000
        );

        ++cx;
    }
}


/*
 * ============================================================
 * EFI entry point
 * ============================================================
 */
extern "C"
EFI_STATUS EFIAPI efi_main(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable
) {
    InitializeLib(
        ImageHandle,
        SystemTable
    );


    /*
     * ========================================================
     * GOP
     * ========================================================
     */

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;

    EFI_GUID gopGuid =
        EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    EFI_STATUS status =
        (EFI_STATUS)uefi_call_wrapper(
            (void*)BS->LocateProtocol,
            3,
            &gopGuid,
            NULL,
            (void**)&gop
        );

    if (EFI_ERROR(status) || gop == NULL) {

        Print(
            (CHAR16*)
            L"Couldn't locate GOP\n"
        );

        return EFI_ABORTED;
    }


    /*
     * ========================================================
     * Framebuffer
     * ========================================================
     */

    Framebuffer fb;

    fb.Base =
        (uint8_t*)(UINTN)
        gop->Mode->FrameBufferBase;

    fb.Width =
        gop->Mode->Info->HorizontalResolution;

    fb.Height =
        gop->Mode->Info->VerticalResolution;

    fb.PixelsPerScanLine =
        gop->Mode->Info->PixelsPerScanLine;

    fb.PixelsPerPixel = 4;


    /*
     * ========================================================
     * Backbuffer
     * ========================================================
     */

    UINTN backbuffer_size =
        (UINTN)fb.Width *
        (UINTN)fb.Height *
        4;


    /*
     * ========================================================
     * Memory map
     * ========================================================
     */

    UINTN mapSize = 0;
    UINTN mapKey = 0;
    UINTN descSize = 0;
    UINT32 descVersion = 0;

    status =
        (EFI_STATUS)uefi_call_wrapper(
            (void*)BS->GetMemoryMap,
            5,
            &mapSize,
            NULL,
            &mapKey,
            &descSize,
            &descVersion
        );

    if (status != EFI_BUFFER_TOO_SMALL) {

        Print(
            (CHAR16*)
            L"Unexpected GetMemoryMap status: %r\n"
        );

        return EFI_ABORTED;
    }


    /*
     * Leave extra space.
     */
    mapSize += descSize * 20;


    /*
     * ========================================================
     * Allocate memory map
     * ========================================================
     */

    void* memMap = NULL;

    status =
        (EFI_STATUS)uefi_call_wrapper(
            (void*)BS->AllocatePool,
            3,
            EfiLoaderData,
            mapSize,
            &memMap
        );

    if (EFI_ERROR(status)) {

        Print(
            (CHAR16*)
            L"AllocatePool failed for memMap: %r\n"
        );

        return EFI_ABORTED;
    }


    /*
     * ========================================================
     * Allocate backbuffer
     * ========================================================
     */

    void* backbuf = NULL;

    status =
        (EFI_STATUS)uefi_call_wrapper(
            (void*)BS->AllocatePool,
            3,
            EfiLoaderData,
            backbuffer_size,
            &backbuf
        );

    if (EFI_ERROR(status)) {

        Print(
            (CHAR16*)
            L"AllocatePool failed for backbuffer: %r\n"
        );

        return EFI_ABORTED;
    }


    /*
     * ========================================================
     * Kernel heap
     * ========================================================
     */

    const size_t heap_size =
        4 * 1024 * 1024;

    void* heapbuf = NULL;

    status =
        (EFI_STATUS)uefi_call_wrapper(
            (void*)BS->AllocatePool,
            3,
            EfiLoaderData,
            heap_size,
            &heapbuf
        );

    if (EFI_ERROR(status)) {

        Print(
            (CHAR16*)
            L"AllocatePool failed for heap: %r\n"
        );

        return EFI_ABORTED;
    }


    /*
     * ========================================================
     * Get memory map again
     * ========================================================
     */

    status =
        (EFI_STATUS)uefi_call_wrapper(
            (void*)BS->GetMemoryMap,
            5,
            &mapSize,
            memMap,
            &mapKey,
            &descSize,
            &descVersion
        );

    if (EFI_ERROR(status)) {

        Print(
            (CHAR16*)
            L"GetMemoryMap failed: %r\n"
        );

        return EFI_ABORTED;
    }


    /*
     * ========================================================
     * Allocator
     * ========================================================
     */

    allocator::init(
        heapbuf,
        heap_size
    );


    /*
     * ========================================================
     * Exit Boot Services
     * ========================================================
     */

    status =
        (EFI_STATUS)uefi_call_wrapper(
            (void*)BS->ExitBootServices,
            2,
            ImageHandle,
            mapKey
        );

    if (EFI_ERROR(status)) {

        return EFI_ABORTED;
    }



    /*
     * ========================================================
     * Input
     * ========================================================
     *
     * IMPORTANT:
     *
     * We intentionally do NOT instantiate VirtIO here.
     *
     * Your current virtio_input.hpp does not expose a type
     * named:
     *
     *     VirtioInput
     *
     * or:
     *
     *     virtio_input
     *
     * Therefore the PS/2 devices are used for now.
     */

    PS2Mouse mouse;
    PS2Keyboard keyboard;

    mouse.init();
    keyboard.init();


    /*
     * ========================================================
     * GUI
     * ========================================================
     */

    Window win{
        (int)fb.Width / 4,
        (int)fb.Height / 4,
        (int)fb.Width / 2,
        (int)fb.Height / 2,
        false,
        0,
        0
    };


    /*
     * ========================================================
     * Initial screen
     * ========================================================
     */

    bb_clear(
        (uint8_t*)backbuf,
        fb.Width,
        fb.Height,
        0x00303030
    );

    draw_main_window(
        (uint8_t*)backbuf,
        fb.Width,
        win
    );

    bb_blit_to_fb(
        &fb,
        (const uint8_t*)backbuf
    );


    /*
     * ========================================================
     * Cursor
     * ========================================================
     */

    int cursor_x =
        (int)fb.Width / 2;

    int cursor_y =
        (int)fb.Height / 2;


    /*
     * ========================================================
     * Editor
     * ========================================================
     */

    bool in_editor = false;

    char* editor_name = NULL;
    char* editor_buf = NULL;

    size_t editor_len = 0;
    size_t editor_cap = 0;


    /*
     * ========================================================
     * Mouse packet
     * ========================================================
     */

    uint8_t packet[3];

    int packet_index = 0;

    bool left_pressed = false;


    /*
     * ========================================================
     * Main loop
     * ========================================================
     */

    while (1) {

        /*
         * ----------------------------------------------------
         * Keyboard
         * ----------------------------------------------------
         */

        int8_t kb =
            keyboard.read_byte_nonblocking();


        /*
         * ----------------------------------------------------
         * Mouse
         * ----------------------------------------------------
         */

        int8_t mb =
            mouse.read_byte_nonblocking();

        if (mb != INT8_MIN) {

            packet[packet_index++] =
                (uint8_t)mb;

            if (packet_index == 3) {

                packet_index = 0;

                uint8_t buttons =
                    packet[0];

                int8_t dx =
                    (int8_t)packet[1];

                int8_t dy =
                    (int8_t)packet[2];


                /*
                 * Cursor movement
                 */
                cursor_x += dx;
                cursor_y -= dy;


                /*
                 * X bounds
                 */
                if (cursor_x < 0) {
                    cursor_x = 0;
                }

                if (cursor_x >= (int)fb.Width) {
                    cursor_x =
                        (int)fb.Width - 1;
                }


                /*
                 * Y bounds
                 */
                if (cursor_y < 0) {
                    cursor_y = 0;
                }

                if (cursor_y >= (int)fb.Height) {
                    cursor_y =
                        (int)fb.Height - 1;
                }


                /*
                 * Left mouse button
                 */
                bool new_left =
                    (buttons & 1) != 0;


                /*
                 * ------------------------------------------------
                 * Mouse press
                 * ------------------------------------------------
                 */

                if (new_left && !left_pressed) {

                    if (!in_editor) {

                        int list_x =
                            win.x + 8;

                        int list_y =
                            win.y + 32;

                        int list_w =
                            win.w - 16;

                        int list_h =
                            win.h - 36;


                        if (
                            cursor_x >= list_x &&
                            cursor_x < list_x + list_w &&
                            cursor_y >= list_y &&
                            cursor_y < list_y + list_h
                        ) {

                            int index =
                                (cursor_y - list_y) / 10;


                            if (index >= 0) {

                                size_t count =
                                    vfs::count_files();

                                if (
                                    (size_t)index <
                                    count
                                ) {

                                    const char* name =
                                        vfs::name_at(
                                            (size_t)index
                                        );

                                    if (name) {

                                        uint32_t file_size =
                                            0;

                                        const uint8_t* data =
                                            vfs::read_file(
                                                name,
                                                &file_size
                                            );

                                        if (data) {

                                            /*
                                             * Editor buffer
                                             */
                                            editor_cap =
                                                (size_t)file_size +
                                                4096;

                                            editor_buf =
                                                (char*)
                                                allocator::alloc(
                                                    editor_cap
                                                );

                                            if (editor_buf) {

                                                memcpy(
                                                    editor_buf,
                                                    data,
                                                    file_size
                                                );

                                                editor_len =
                                                    (size_t)file_size;


                                                /*
                                                 * Filename
                                                 */
                                                size_t name_len =
                                                    strlen(name);

                                                editor_name =
                                                    (char*)
                                                    allocator::alloc(
                                                        name_len + 1
                                                    );

                                                if (editor_name) {

                                                    strcpy(
                                                        editor_name,
                                                        name
                                                    );

                                                    in_editor =
                                                        true;


                                                    draw_editor(
                                                        (uint8_t*)backbuf,
                                                        fb.Width,
                                                        win,
                                                        editor_name,
                                                        editor_buf,
                                                        editor_len
                                                    );


                                                    bb_blit_region_to_fb(
                                                        &fb,
                                                        (const uint8_t*)backbuf,
                                                        win.x,
                                                        win.y,
                                                        win.w,
                                                        win.h
                                                    );
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }


                /*
                 * Mouse release
                 */
                if (!new_left && left_pressed) {
                    /* nothing */
                }

                left_pressed =
                    new_left;
            }
        }


        /*
         * ====================================================
         * Keyboard handling
         * ====================================================
         */

        if (kb != INT8_MIN) {

            uint8_t scan =
                (uint8_t)kb;

            char ch =
                PS2Keyboard::scancode_to_ascii(
                    scan
                );


            /*
             * ------------------------------------------------
             * Editor
             * ------------------------------------------------
             */

            if (in_editor) {

                /*
                 * ESC = exit editor
                 */
                if (scan == 0x01) {

                    in_editor = false;

                    draw_main_window(
                        (uint8_t*)backbuf,
                        fb.Width,
                        win
                    );

                    bb_blit_region_to_fb(
                        &fb,
                        (const uint8_t*)backbuf,
                        win.x,
                        win.y,
                        win.w,
                        win.h
                    );
                }

                /*
                 * CTRL+S / save can be added once the keyboard
                 * modifier API is known.
                 */
                else if (ch) {

                    if (editor_len + 1 <
                        editor_cap) {

                        editor_buf[
                            editor_len++
                        ] = ch;
                    }


                    draw_editor(
                        (uint8_t*)backbuf,
                        fb.Width,
                        win,
                        editor_name,
                        editor_buf,
                        editor_len
                    );


                    bb_blit_region_to_fb(
                        &fb,
                        (const uint8_t*)backbuf,
                        win.x,
                        win.y,
                        win.w,
                        win.h
                    );
                }
            }


            /*
             * ------------------------------------------------
             * Main window
             * ------------------------------------------------
             */

            else {

                if (ch) {

                    /*
                     * L = list files
                     */
                    if (
                        ch == 'l' ||
                        ch == 'L'
                    ) {

                        draw_file_list(
                            (uint8_t*)backbuf,
                            fb.Width,
                            win
                        );

                        bb_blit_region_to_fb(
                            &fb,
                            (const uint8_t*)backbuf,
                            win.x,
                            win.y,
                            win.w,
                            win.h
                        );
                    }

                    /*
                     * Any other character
                     */
                    else {

                        char text[2];

                        text[0] = ch;
                        text[1] = '\0';


                        bb_draw_rect(
                            (uint8_t*)backbuf,
                            fb.Width,
                            win.x + win.w - 48,
                            win.y + 4,
                            40,
                            16,
                            0x00FFFFC0
                        );


                        bb_draw_text(
                            (uint8_t*)backbuf,
                            fb.Width,
                            win.x + win.w - 44,
                            win.y + 6,
                            text,
                            0x00000000
                        );


                        bb_blit_region_to_fb(
                            &fb,
                            (const uint8_t*)backbuf,
                            win.x + win.w - 48,
                            win.y + 4,
                            40,
                            16
                        );
                    }
                }
            }
        }


        /*
         * ====================================================
         * Idle
         * ====================================================
         */

        __asm__ volatile("hlt");
    }


    return EFI_SUCCESS;
}
