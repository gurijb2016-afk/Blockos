#include "backbuffer.h"
#include "events.hpp"
#include "ps2mouse.hpp"
#include "ps2keyboard.hpp"
#include "vfs.hpp"
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

    // Compute backbuffer size
    UINTN bb_size = (UINTN)fb.Width * (UINTN)fb.Height * 4;

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

    // Allocate pool for memory map + backbuffer
    mapSize += descSize * 8;
    void *memMap = NULL;
    void *backbuf = NULL;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mapSize, &memMap);
    if (EFI_ERROR(status)) {
        Print(L"AllocatePool failed for memMap: %r\n", status);
        return EFI_ABORTED;
    }

    // Allocate backbuffer
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, bb_size, &backbuf);
    if (EFI_ERROR(status)) {
        Print(L"AllocatePool failed for backbuffer: %r\n", status);
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

    // Initialize VFS / ramfs
    uint32_t file_size = 0;
    const uint8_t* file_data = vfs::read_file("readme.txt", &file_size);

    // Initialize PS/2 mouse and keyboard
    PS2Mouse mouse;
    PS2Keyboard keyboard;
    mouse.init();
    keyboard.init();

    // GUI state
    Window win{ (int)fb.Width/4, (int)fb.Height/4, (int)(fb.Width/2), (int)(fb.Height/2), false, 0, 0 };

    // Render initial scene to backbuffer
    bb_clear((uint8_t*)backbuf, fb.Width, fb.Height, 0x00303030);
    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, win.h, 0x00C0C0C0);
    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, 24, 0x00008080);

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
            bb_draw_char((uint8_t*)backbuf, fb.Width, text_x + cx*8, text_y, c, 0x00000000);
            ++cx;
        }
    }

    // Initial blit
    bb_blit_to_fb(&fb, (const uint8_t*)backbuf);

    int cursor_x = fb.Width/2; int cursor_y = fb.Height/2;
    int prev_cursor_x = -1, prev_cursor_y = -1;

    // mouse packet assembly
    uint8_t packet[3]; int pk_idx = 0;
    bool left_pressed = false;

    // Main loop: poll mouse and keyboard, push events to global queue, process them
    while (1) {
        int8_t mb = mouse.read_byte_nonblocking();
        if (mb != INT8_MIN) {
            packet[pk_idx++] = (uint8_t)mb;
            if (pk_idx == 3) {
                pk_idx = 0;
                uint8_t st = packet[0];
                int8_t dx = (int8_t)packet[1];
                int8_t dy = (int8_t)packet[2];
                cursor_x += dx; cursor_y -= dy;
                if (cursor_x < 0) cursor_x = 0; if (cursor_x >= (int)fb.Width) cursor_x = fb.Width-1;
                if (cursor_y < 0) cursor_y = 0; if (cursor_y >= (int)fb.Height) cursor_y = fb.Height-1;
                bool new_left = (st & 0x1) != 0;

                Event ev{};
                ev.type = EventType::MouseMove;
                ev.data.mouse.x = cursor_x;
                ev.data.mouse.y = cursor_y;
                ev.data.mouse.buttons = (uint8_t)(st & 0x7);
                g_event_queue.push(ev);

                if (new_left && !left_pressed) {
                    // press start
                    if (cursor_x >= win.x && cursor_x < win.x + win.w && cursor_y >= win.y && cursor_y < win.y + 24) {
                        win.dragging = true;
                        win.drag_offset_x = cursor_x - win.x;
                        win.drag_offset_y = cursor_y - win.y;
                    }
                }
                if (!new_left && left_pressed) win.dragging = false;
                left_pressed = new_left;

                if (win.dragging) {
                    int nx = cursor_x - win.drag_offset_x;
                    int ny = cursor_y - win.drag_offset_y;
                    if (nx < 0) nx = 0; if (ny < 0) ny = 0;
                    if (nx + win.w > (int)fb.Width) nx = fb.Width - win.w;
                    if (ny + win.h > (int)fb.Height) ny = fb.Height - win.h;
                    win.x = nx; win.y = ny;

                    // redraw entire backbuffer
                    bb_clear((uint8_t*)backbuf, fb.Width, fb.Height, 0x00303030);
                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, win.h, 0x00C0C0C0);
                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, 24, 0x00008080);
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
                            bb_draw_char((uint8_t*)backbuf, fb.Width, text_x + cx*8, text_y, c, 0x00000000);
                            ++cx;
                        }
                    }
                    // blit after redraw
                    bb_blit_to_fb(&fb, (const uint8_t*)backbuf);
                }
                else {
                    // Blit cursor move only: we redraw entire backbuffer with cursor then blit
                    // For simplicity, blit full backbuffer and draw cursor in backbuffer
                    // First restore previous cursor area by re-blitting backbuffer (we always use backbuffer so it's clean)
                    // Draw cursor onto backbuffer copy
                    // Refresh backbuffer rendering of window content (no change), then draw cursor and blit
                    // Here we simply blit backbuffer and then draw cursor directly to framebuffer on top
                    bb_blit_to_fb(&fb, (const uint8_t*)backbuf);
                    // Draw cursor directly
                    // save area not necessary because we blit full backbuffer each time
                    for (int yy=0; yy<8; ++yy) for (int xx=0; xx<8; ++xx) {
                        uint32_t cx = (uint32_t)(cursor_x + xx);
                        uint32_t cy = (uint32_t)(cursor_y + yy);
                        if (cx < fb.Width && cy < fb.Height) {
                            uint8_t *pixel = fb.Base + (cy * fb.PixelsPerScanLine + cx) * fb.PixelsPerPixel;
                            uint32_t col = 0x00FFFFFF;
                            memcpy(pixel, &col, 4);
                        }
                    }
                }
            }
        }

        int8_t kb = keyboard.read_byte_nonblocking();
        if (kb != INT8_MIN) {
            Event ev{};
            ev.type = EventType::KeyPress;
            ev.data.key.scancode = (uint8_t)kb;
            g_event_queue.push(ev);

            // For demo: display scancode hex in top-right of window
            char buf[16];
            int n = 0;
            const char* hex = "0123456789ABCDEF";
            buf[n++] = hex[(ev.data.key.scancode >> 4) & 0xF];
            buf[n++] = hex[ev.data.key.scancode & 0xF];
            buf[n] = '\0';

            // draw onto backbuffer and blit
            // redraw window area to preserve content
            bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x + win.w - 48, win.y + 4, 40, 16, 0x00FFFFC0);
            bb_draw_text((uint8_t*)backbuf, fb.Width, win.x + win.w - 44, win.y + 6, buf, 0x00000000);
            bb_blit_to_fb(&fb, (const uint8_t*)backbuf);
        }

        __asm__ volatile ("hlt");
    }

    return EFI_SUCCESS;
}
