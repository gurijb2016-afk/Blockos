#pragma once

extern "C"
{
#include <efi.h>
}

namespace uefi_boot
{
    struct BootInfo
    {
        EFI_HANDLE image_handle;
        EFI_SYSTEM_TABLE* system_table;
        EFI_BOOT_SERVICES* boot_services;

        EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;

        void* memory_map;
        UINTN memory_map_size;
        UINTN memory_map_key;
        UINTN memory_descriptor_size;
        UINT32 memory_descriptor_version;

        EFI_PHYSICAL_ADDRESS framebuffer;
        UINT32 framebuffer_width;
        UINT32 framebuffer_height;
        UINT32 framebuffer_pixels_per_scanline;
    };

    EFI_STATUS initialize(
        EFI_HANDLE image_handle,
        EFI_SYSTEM_TABLE* system_table,
        BootInfo* info);

    EFI_STATUS get_memory_map(
        BootInfo* info);

    EFI_STATUS exit_boot_services(
        BootInfo* info);
}
