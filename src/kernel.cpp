// Minimal UEFI kernel that locates GOP, fetches the memory map, calls ExitBootServices,
// then draws directly to the framebuffer. Freestanding C++ code using GNU-EFI headers.

extern "C" {
    #include <efi.h>
    #include <efilib.h>
}

#include "fb.h"
#include <stdint.h>

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

    // Now draw directly to the framebuffer
    fb_draw_clear(&fb, 0x00102030); // dark bluish background

    // Draw a centered rectangle as a simple GUI element
    uint32_t rw = fb.Width / 2;
    uint32_t rh = fb.Height / 6;
    uint32_t rx = (fb.Width - rw) / 2;
    uint32_t ry = (fb.Height - rh) / 2;
    fb_draw_rect(&fb, rx, ry, rw, rh, 0x00FFD700); // gold-ish bar

    // Draw a smaller inner rect
    fb_draw_rect(&fb, rx + 10, ry + 10, rw - 20, rh - 20, 0x00008000); // dark green

    // Simple pattern in top-left
    for (uint32_t y = 0; y < 50; ++y) {
        for (uint32_t x = 0; x < 50; ++x) {
            if ((x + y) % 2 == 0) fb_put_pixel(&fb, x, y, 0x00FF0000);
        }
    }

    // Halt forever
    while (1) {
        __asm__ volatile ("hlt");
    }

    return EFI_SUCCESS;
}
