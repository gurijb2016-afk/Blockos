#include "backbuffer.h"
#include "events.hpp"
#include "ps2mouse.hpp"
#include "ps2keyboard.hpp"
#include "virtio_input.hpp"
#include "vfs.hpp"
#include "allocator.hpp"
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
    mapSize += descSize * 20;
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
    allocator::init(heapbuf, heap_size);

    // Initialize VFS from embedded ramfs
    vfs_init_from_ramfs();

    // Exit Boot Services
    status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, mapKey);
    if (EFI_ERROR(status)) {
        Print(L"ExitBootServices failed: %r\n", status);
        return EFI_ABORTED;
    }

    // Initialize input devices
    PS2Mouse mouse;
    PS2Keyboard keyboard;
    VirtioInput virtio;
    mouse.init();
    keyboard.init();
    virtio.init();

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

    // editor state
    bool in_editor = false;
    char* editor_name = nullptr;
    char* editor_buf = nullptr;
    size_t editor_len = 0;
    size_t editor_cap = 0;
    int editor_cursor_x = 0;
    int editor_cursor_y = 0;

    // mouse packet assembly
    uint8_t packet[3]; int pk_idx = 0;
    bool left_pressed = false;

    // Main loop: poll mouse and keyboard, process
    while (1) {
        // Prefer virtio input if it provides data; fall back to PS/2 for keyboard/mouse
        int8_t vb = virtio.read_byte_nonblocking();
        int8_t kb = INT8_MIN;
        if (vb == INT8_MIN) kb = keyboard.read_byte_nonblocking();
        else kb = vb;

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
                    if (!in_editor) {
                        // if listing is currently visible (we used 'L'), clicking in list area opens editor
                        // compute list area coords
                        int list_x = win.x + 8;
                        int list_y = win.y + 32;
                        int list_w = win.w - 16;
                        int list_h = win.h - 36;
                        if (cursor_x >= list_x && cursor_x < list_x + list_w && cursor_y >= list_y && cursor_y < list_y + list_h) {
                            // compute index based on last displayed list: assume files were listed previously
                            int idx = (cursor_y - list_y) / 10;
                            size_t nf = vfs::count_files();
                            if ((size_t)idx < nf) {
                                const char* name = vfs::name_at(idx);
                                uint32_t fsz = 0;
                                const uint8_t* fdata = vfs::read_file(name, &fsz);
                                // open editor with a writable copy
                                editor_cap = fsz + 4096;
                                editor_buf = (char*)allocator::alloc(editor_cap);
                                if (!editor_buf) {
                                    // allocation failed
                                } else {
                                    memcpy(editor_buf, fdata, fsz);
                                    editor_len = fsz;
                                    editor_name = (char*)allocator::alloc(strlen(name)+1);
                                    strcpy(editor_name, name);
                                    in_editor = true;
                                    editor_cursor_x = 0; editor_cursor_y = 0;

                                    // draw editor UI into backbuffer
                                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, win.h, 0x00E0E0E0);
                                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, 24, 0x00006060);
                                    // title
                                    bb_draw_text((uint8_t*)backbuf, fb.Width, win.x+8, win.y+6, editor_name, 0x00FFFFFF);
                                    // file content area
                                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x+4, win.y+28, win.w-8, win.h-36, 0x00FFFFFF);
                                    // render content
                                    int tx = win.x + 8; int ty = win.y + 32; int cols = (win.w-16)/8; int cx = 0;
                                    for (size_t i=0;i<editor_len;++i) {
                                        char c = editor_buf[i];
                                        if (c == '\n' || cx >= cols) { cx = 0; ty += 10; if (c=='\n') continue; }
                                        bb_draw_char((uint8_t*)backbuf, fb.Width, tx + cx*8, ty, c, 0x00000000);
                                        ++cx;
                                    }
                                    bb_blit_region_to_fb(&fb, (const uint8_t*)backbuf, win.x, win.y, win.w, win.h);
                                }
                            }
                        }
                    }
                }
                if (!new_left && left_pressed) {
                    // release
                    // nothing for now
                }
                left_pressed = new_left;

                // redraw cursor region if not dragging etc.
                bb_blit_region_to_fb(&fb, (const uint8_t*)backbuf, cursor_x-4<0?0:cursor_x-4, cursor_y-4<0?0:cursor_y-4, 12, 12);
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

        if (kb != INT8_MIN) {
            char ch = PS2Keyboard::scancode_to_ascii((uint8_t)kb);
            if (in_editor) {
                if (ch) {
                    if (ch == '\n') {
                        if (editor_len + 1 < editor_cap) { editor_buf[editor_len++] = '\n'; }
                    } else {
                        if (editor_len + 1 < editor_cap) { editor_buf[editor_len++] = ch; }
                    }
                    // redraw editor content region (simple re-render)
                    bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x+4, win.y+28, win.w-8, win.h-36, 0x00FFFFFF);
                    int tx = win.x + 8; int ty = win.y + 32; int cols = (win.w-16)/8; int cx = 0;
                    for (size_t i=0;i<editor_len;++i) {
                        char c = editor_buf[i];
                        if (c == '\n' || cx >= cols) { cx = 0; ty += 10; if (c=='\n') continue; }
                        bb_draw_char((uint8_t*)backbuf, fb.Width, tx + cx*8, ty, c, 0x00000000);
                        ++cx;
                    }
                    bb_blit_region_to_fb(&fb, (const uint8_t*)backbuf, win.x, win.y, win.w, win.h);
                } else {
                    // handle special scancodes: Save on 's' or 'S'
                    // map scancode to lowercase char if applicable
                    char maybe = PS2Keyboard::scancode_to_ascii((uint8_t)kb);
                    if (maybe == 's' || maybe == 'S') {
                        // save
                        vfs::write_file(editor_name, (const uint8_t*)editor_buf, (uint32_t)editor_len);
                        // exit editor: redraw main window content from vfs
                        in_editor = false;
                        // redraw main window
                        bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, win.h, 0x00C0C0C0);
                        bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x, win.y, win.w, 24, 0x00008080);
                        uint32_t fsz=0; const uint8_t* fd = vfs::read_file("readme.txt", &fsz);
                        if (fd) {
                            int text_x = win.x + 8; int text_y = win.y + 32; int cols = (win.w-16)/8; int cx=0;
                            for (size_t i=0;i<(size_t)fsz;++i) {
                                char c = fd[i];
                                if (c=='\n' || cx>=cols) { cx=0; text_y+=10; if (c=='\n') continue; }
                                bb_draw_char((uint8_t*)backbuf, fb.Width, text_x + cx*8, text_y, c, 0x00000000);
                                ++cx;
                            }
                        }
                        bb_blit_region_to_fb(&fb, (const uint8_t*)backbuf, win.x, win.y, win.w, win.h);
                    }
                }
            } else {
                if (ch) {
                    // if 'l' or 'L' -> list files
                    if (ch == 'l' || ch == 'L') {
                        // clear file list area
                        bb_draw_rect((uint8_t*)backbuf, fb.Width, win.x+4, win.y+28, win.w-8, win.h-36, 0x00FFFFFF);
                        int tx = win.x + 8; int ty = win.y + 32;
                        size_t nf = vfs::count_files();
                        for (size_t i=0;i<nf;++i) {
                            const char* name = vfs::name_at(i);
                            bb_draw_text((uint8_t*)backbuf, fb.Width, tx, ty + (int)i*10, name, 0x00000000);
                        }
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
        }

        __asm__ volatile ("hlt");
    }

    return EFI_SUCCESS;
}
