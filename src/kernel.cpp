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

    // Allocate pool for memory map + backbuffer + allocator heap
    mapSize += descSize * 16;
    void *memMap = NULL;
    void *backbuf = NULL;
    void *heapbuf = NULL;
    size_t heap_size = 4 * 1024 * 1024; // 4MB heap
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

    // allocate heap for allocator
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, heap_size, &heapbuf);
    if (EFI_ERROR(status)) {
        Print(L"AllocatePool failed for heap: %r\n", status);
        return EFI_ABORTED;
    }

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mapSize, memMap, &mapKey, &descSize, &descVersion);
    if (EFI_ERROR(status)) {
        Print(L"GetMemoryMap failed on second call: %r\n", status);
        return EFI_ABORTED;
    }

    // Initialize allocator
    extern void allocator::init(void*, size_t);
    allocator::init(heapbuf, heap_size);

    // Initialize VFS from embedded ramfs
    extern void vfs_init_from_ramfs();
    vfs_init_from_ramfs();

    // Exit Boot Services
    status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, mapKey);
    if (EFI_ERROR(status)) {
        Print(L"ExitBootServices failed: %r\n", status);
        return EFI_ABORTED;
    }

    // Initialize VFS / ramfs
    // Note: vfs was initialized before ExitBootServices using allocator

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

    // show initial file content
    uint32_t file_size = 0;
    const uint8_t* file_data = vfs::read_file("readme.txt", &file_size);
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

    // Initial blit full screen
    bb_blit_to_fb(&fb, (const uint8_t*)backbuf);

    int cursor_x = fb.Width/2; int cursor_y = fb.Height/2;

    // mouse packet assembly
    uint8_t packet[3]; int pk_idx = 0;
    bool left_pressed = false;

    // Main loop: poll mouse and keyboard, process
    while (1) {
        int8_t mb = mouse.read_byte_nonblocking();
        if (mb != INT8_MIN) {
            packet[pk_idx++] = (uint8_t)mb;
            if (pk_idx == 3) {
                pk_idx = 0;
                uint8_t st = packet[0];
                int8_t dx = (int8_t)packet[1];
                int8_t dy = (int8_t)packet[2];
                int old_cursor_x = cursor_x, old_cursor_y = cursor_y;
                cursor_x += dx; cursor_y -= dy;
                if (cursor_x < 0) cursor_x = 0; if (cursor_x >= (int)fb.Width) cursor_x = fb.Width-1;
                if (cursor_y < 0) cursor_y = 0; if (cursor_y >= (int)fb.Height) cursor_y = fb.Height-1;
                bool new_left = (st & 0x1) != 0;

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
                    int oldx = win.x, oldy = win.y;
                    win.x = nx; win.y = ny;

                    // compute dirty rect covering old window and new window
                    int dx1 = oldx < win.x ? oldx : win.x;
                    int dy1 = oldy < win.y ? oldy : win.y;
                    int dx2 = (oldx + win.w) > (win.x + win.w) ? (oldx + win.w) : (win.x + win.w);
                    int dy2 = (oldy + win.h) > (win.y + win.h) ? (oldy + win.h) : (win.y + win.h);
                    int dirty_w = dx2 - dx1;
                    int dirty_h = dy2 - dy1;

                    // redraw only dirty region into backbuffer
                    // For simplicity, clear and redraw window area within dirty rect
                    // First clear dirty region
                    for (int yy = dy1; yy < dy1 + dirty_h; ++yy) {
                        for (int xx = dx1; xx < dx1 + dirty_w; ++xx) {
                            if (xx >= 0 && xx < (int)fb.Width && yy >= 0 && yy < (int)fb.Height) {
                                uint32_t bg = 0x00303030;
                                uint8_t *p = (uint8_t*)backbuf + ((yy * fb.Width + xx) * 4);
                                memcpy(p, &bg, 4);
                            }
                        }
                    }
                    // draw new window content inside dirty region
                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, win.h, 0x00C0C0C0);
                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, 24, 0x00008080);
                    if (file_size>0) {
                        const char *buf = (const char*)file_data;
                        int text_x = win.x + 8;
                        int text_y = win.y + 32;
                        int cols = (win.w - 16) / 8;
                        int cx = 0;
                        for (size_t i = 0; i < (size_t)file_size; ++i) {
                            char c = buf[i];
                            if (c == '\n' || cx >= cols) { cx = 0; text_y += 10; if (c=='\n') continue; }
                            bb_draw_char((uint8_t*)backbuf, fb.Width, text_x + cx*8, text_y, c, 0x00000000);
                            ++cx;
                        }
                    }
                    // blit only dirty region
                    bb_blit_region_to_fb(&fb, (const uint8_t*)backbuf, dx1, dy1, dirty_w, dirty_h);
                } else {
                    // not dragging: only need to update cursor region and small areas
                    // blit region around cursor: previous and current
                    int cx1 = old_cursor_x - 2; int cy1 = old_cursor_y - 2;
                    int cx2 = cursor_x - 2; int cy2 = cursor_y - 2;
                    // blit area covering both
                    int rx = cx1 < cx2 ? cx1 : cx2;
                    int ry = cy1 < cy2 ? cy1 : cy2;
                    int rw = ( (cx1+12) > (cx2+12) ) ? (cx1+12 - rx) : (cx2+12 - rx);
                    int rh = ( (cy1+12) > (cy2+12) ) ? (cy1+12 - ry) : (cy2+12 - ry);
                    if (rx < 0) rx = 0; if (ry < 0) ry = 0;
                    if (rx + rw > (int)fb.Width) rw = fb.Width - rx;
                    if (ry + rh > (int)fb.Height) rh = fb.Height - ry;
                    bb_blit_region_to_fb(&fb, (const uint8_t*)backbuf, rx, ry, rw, rh);
                    // draw cursor on top directly
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
            // map scancode to ascii
            char ch = PS2Keyboard::scancode_to_ascii((uint8_t)kb);
            if (ch) {
                // if 'l' or 'L' -> list files
                if (ch == 'l' || ch == 'L') {
                    // clear window content area in backbuffer
                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x+4, win.y+28, win.w-8, win.h-36, 0x00FFFFFF);
                    int tx = win.x + 8; int ty = win.y + 32;
                    size_t nf = vfs::count_files();
                    for (size_t i=0;i<nf;++i) {
                        const char* name = vfs::name_at(i);
                        bb_draw_text((uint8_t*)backbuf, fb.Width, tx, ty + (int)i*10, name, 0x00000000);
                    }
                    // blit window region only
                    bb_blit_region_to_fb(&fb, (const uint8_t*)backbuf, win.x, win.y, win.w, win.h);
                } else {
                    // display typed char at top-right small box
                    char buf[2] = { ch, 0 };
                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x + win.w - 48, win.y + 4, 40, 16, 0x00FFFFC0);
                    bb_draw_text((uint8_t*)backbuf, fb.Width, win.x + win.w - 44, win.y + 6, buf, 0x00000000);
                    bb_blit_region_to_fb(&fb, (const uint8_t*)backbuf, win.x + win.w - 48, win.y + 4, 40, 16);
                }
            }
        }

        __asm__ volatile ("hlt");
    }

    return EFI_SUCCESS;
}
