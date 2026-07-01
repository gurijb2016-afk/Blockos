#include "ps2mouse.hpp"
#include "fb.h"
#include "ramfs.hpp"
#include "font8x8.h"
#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <string.h>

struct Window {
    int x, y, w, h;
    bool dragging;
    int drag_offset_x, drag_offset_y;
};

// small buffer for saving cursor background (8x8x4)
static uint8_t cursor_save[8*8*4];
static int prev_cursor_x = -1, prev_cursor_y = -1;

extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    // Locate GOP
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void**)&gop);
    if (EFI_ERROR(status) || gop == NULL) {
        Print(L"Couldn't locate GOP\n");
        return EFI_ABORTED;
    }

    Framebuffer fb;
    fb.Base = (uint8_t*) (UINTN) gop->Mode->FrameBufferBase;
    fb.Width = gop->Mode->Info->HorizontalResolution;
    fb.Height = gop->Mode->Info->VerticalResolution;
    fb.PixelsPerScanLine = gop->Mode->Info->PixelsPerScanLine;
    fb.PixelsPerPixel = 4; // usually 32bpp

    // Get memory map size
    UINTN mapSize = 0;
    UINTN mapKey = 0;
    UINTN descSize = 0;
    UINT32 descVersion = 0;

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mapSize, NULL, &mapKey, &descSize, &descVersion);
    if (status != EFI_BUFFER_TOO_SMALL) {
        Print(L"Unexpected GetMemoryMap status: %r\n", status);
        return EFI_ABORTED;
    }

    // Allocate pool for memory map
    mapSize += descSize * 4;
    void *memMap = NULL;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mapSize, &memMap);
    if (EFI_ERROR(status)) {
        Print(L"AllocatePool failed: %r\n", status);
        return EFI_ABORTED;
    }

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mapSize, memMap, &mapKey, &descSize, &descVersion);
    if (EFI_ERROR(status)) {
        Print(L"GetMemoryMap failed on second call: %r\n", status);
        return EFI_ABORTED;
    }

    // Exit Boot Services
    status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, mapKey);
    if (EFI_ERROR(status)) {
        Print(L"ExitBootServices failed: %r\n", status);
        return EFI_ABORTED;
    }

    // Initialize ramfs (C++ wrapper)
    uint32_t file_size = 0;
    const uint8_t* file_data = ramfs::get("readme.txt", &file_size);

    // Initialize PS/2 mouse
    PS2Mouse mouse;
    mouse.init();

    // Initial GUI state
    fb_draw_clear(&fb, 0x00303030);

    Window win{ (int)fb.Width/4, (int)fb.Height/4, (int)(fb.Width/2), (int)(fb.Height/2), false, 0, 0 };

    // Draw window frame once
    fb_draw_rect(&fb, win.x, win.y, win.w, win.h, 0x00C0C0C0); // window background
    fb_draw_rect(&fb, win.x, win.y, win.w, 24, 0x00008080); // title bar

    // Display file content as text inside the window (simple wrapping)
    if (file_data) {
        size_t max_display = (size_t)file_size;
        const char *buf = (const char*)file_data; // our ramfs stores ASCII
        int text_x = win.x + 8;
        int text_y = win.y + 32;
        int cols = (win.w - 16) / 8;
        int cx = 0;
        for (size_t i = 0; i < max_display; ++i) {
            char c = buf[i];
            if (c == '\n' || cx >= cols) { cx = 0; text_y += 10; if (c=='\n') continue; }
            fb_draw_char(&fb, text_x + cx*8, text_y, c, 0x00000000);
            ++cx;
        }
    }

    // Cursor state
    int cursor_x = fb.Width / 2;
    int cursor_y = fb.Height / 2;

    // Main loop: poll mouse bytes and update cursor & window dragging
    uint8_t packet[3]; int pk_idx = 0;
    bool left_pressed = false;

    while (1) {
        int8_t b = mouse.read_byte_nonblocking();
        if (b != INT8_MIN) {
            packet[pk_idx++] = (uint8_t)b;
            if (pk_idx == 3) {
                pk_idx = 0;
                uint8_t st = packet[0];
                int8_t dx = (int8_t)packet[1];
                int8_t dy = (int8_t)packet[2];
                cursor_x += dx;
                cursor_y -= dy;
                if (cursor_x < 0) cursor_x = 0; if (cursor_x >= (int)fb.Width) cursor_x = fb.Width-1;
                if (cursor_y < 0) cursor_y = 0; if (cursor_y >= (int)fb.Height) cursor_y = fb.Height-1;

                bool new_left = (st & 0x1) != 0;
                // Detect press start
                if (new_left && !left_pressed) {
                    // If cursor is inside title bar, start dragging
                    if (cursor_x >= win.x && cursor_x < win.x + win.w && cursor_y >= win.y && cursor_y < win.y + 24) {
                        win.dragging = true;
                        win.drag_offset_x = cursor_x - win.x;
                        win.drag_offset_y = cursor_y - win.y;
                    }
                }
                // Release
                if (!new_left && left_pressed) {
                    win.dragging = false;
                }
                left_pressed = new_left;

                if (win.dragging) {
                    int nx = cursor_x - win.drag_offset_x;
                    int ny = cursor_y - win.drag_offset_y;
                    if (nx < 0) nx = 0; if (ny < 0) ny = 0;
                    if (nx + win.w > (int)fb.Width) nx = fb.Width - win.w;
                    if (ny + win.h > (int)fb.Height) ny = fb.Height - win.h;
                    win.x = nx; win.y = ny;

                    // redraw whole screen for simplicity
                    fb_draw_clear(&fb, 0x00303030);
                    fb_draw_rect(&fb, win.x, win.y, win.w, win.h, 0x00C0C0C0);
                    fb_draw_rect(&fb, win.x, win.y, win.w, 24, 0x00008080);
                    // redraw file text inside
                    if (file_data) {
                        size_t max_display = (size_t)file_size;
                        const char *buf = (const char*)file_data;
                        int text_x = win.x + 8;
                        int text_y = win.y + 32;
                        int cols = (win.w - 16) / 8;
                        int cx = 0;
                        for (size_t i = 0; i < max_display; ++i) {
                            char c = buf[i];
                            if (c == '\n' || cx >= cols) { cx = 0; text_y += 10; if (c=='\n') continue; }
                            fb_draw_char(&fb, text_x + cx*8, text_y, c, 0x00000000);
                            ++cx;
                        }
                    }
                }

                // Draw cursor as an 8x8 white square using save/restore
                // First restore previous cursor
                if (prev_cursor_x >= 0 && prev_cursor_y >= 0) {
                    fb_restore_area(&fb, prev_cursor_x, prev_cursor_y, 8, 8, cursor_save);
                }
                // Save new area
                fb_save_area(&fb, cursor_x, cursor_y, 8, 8, cursor_save);
                // Draw cursor
                fb_draw_rect(&fb, cursor_x, cursor_y, 8, 8, 0x00FFFFFF);
                prev_cursor_x = cursor_x; prev_cursor_y = cursor_y;
            }
        }
        __asm__ volatile ("hlt");
    }

    return EFI_SUCCESS;
}
