#include "uefi_boot.hpp"

extern "C"
{
#include <efilib.h>
}

#include <stddef.h>
#include <stdint.h>

namespace uefi_boot
{

static EFI_STATUS locate_gop(
    EFI_SYSTEM_TABLE* system_table,
    EFI_GRAPHICS_OUTPUT_PROTOCOL** out_gop)
{
    if (system_table == nullptr ||
        system_table->BootServices == nullptr ||
        out_gop == nullptr)
    {
        return EFI_INVALID_PARAMETER;
    }

    *out_gop = nullptr;

    EFI_GUID gop_guid =
        EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    EFI_STATUS status =
        (EFI_STATUS) uefi_call_wrapper(
            (void*) system_table->BootServices->LocateProtocol,
            3,
            &gop_guid,
            nullptr,
            (void**) out_gop);

    if (EFI_ERROR(status))
        return status;

    if (*out_gop == nullptr ||
        (*out_gop)->Mode == nullptr ||
        (*out_gop)->Mode->Info == nullptr)
    {
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

EFI_STATUS initialize(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE* system_table,
    BootInfo* info)
{
    if (system_table == nullptr ||
        info == nullptr)
    {
        return EFI_INVALID_PARAMETER;
    }

    info->image_handle = image_handle;
    info->system_table = system_table;
    info->boot_services = system_table->BootServices;

    info->gop = nullptr;

    info->memory_map = nullptr;
    info->memory_map_size = 0;
    info->memory_map_key = 0;
    info->memory_descriptor_size = 0;
    info->memory_descriptor_version = 0;

    info->framebuffer = 0;
    info->framebuffer_width = 0;
    info->framebuffer_height = 0;
    info->framebuffer_pixels_per_scanline = 0;

    if (info->boot_services == nullptr)
        return EFI_UNSUPPORTED;

    EFI_STATUS status =
        locate_gop(
            system_table,
            &info->gop);

    if (EFI_ERROR(status))
        return status;

    info->framebuffer =
        info->gop->Mode->FrameBufferBase;

    info->framebuffer_width =
        info->gop->Mode->Info->HorizontalResolution;

    info->framebuffer_height =
        info->gop->Mode->Info->VerticalResolution;

    info->framebuffer_pixels_per_scanline =
        info->gop->Mode->Info->PixelsPerScanLine;

    return EFI_SUCCESS;
}

EFI_STATUS get_memory_map(
    BootInfo* info)
{
    if (info == nullptr ||
        info->boot_services == nullptr)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_BOOT_SERVICES* bs =
        info->boot_services;

    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;

    EFI_STATUS status =
        (EFI_STATUS) uefi_call_wrapper(
            (void*) bs->GetMemoryMap,
            5,
            &map_size,
            nullptr,
            &map_key,
            &descriptor_size,
            &descriptor_version);

    if (status != EFI_BUFFER_TOO_SMALL)
        return status;

    if (descriptor_size == 0)
        return EFI_LOAD_ERROR;

    /*
     * Allocate enough room for allocations that happen
     * before the final GetMemoryMap().
     */
    const UINTN extra_descriptors = 32;

    map_size +=
        descriptor_size * extra_descriptors;

    void* map = nullptr;

    status =
        (EFI_STATUS) uefi_call_wrapper(
            (void*) bs->AllocatePool,
            3,
            EfiLoaderData,
            map_size,
            &map);

    if (EFI_ERROR(status))
        return status;

    /*
     * Get the map again using the allocated buffer.
     */
    UINTN actual_size = map_size;

    status =
        (EFI_STATUS) uefi_call_wrapper(
            (void*) bs->GetMemoryMap,
            5,
            &actual_size,
            map,
            &map_key,
            &descriptor_size,
            &descriptor_version);

    if (EFI_ERROR(status))
    {
        uefi_call_wrapper(
            (void*) bs->FreePool,
            1,
            map);

        return status;
    }

    info->memory_map = map;
    info->memory_map_size = actual_size;
    info->memory_map_key = map_key;
    info->memory_descriptor_size = descriptor_size;
    info->memory_descriptor_version =
        descriptor_version;

    return EFI_SUCCESS;
}

EFI_STATUS exit_boot_services(
    BootInfo* info)
{
    if (info == nullptr ||
        info->boot_services == nullptr)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_BOOT_SERVICES* bs =
        info->boot_services;

    /*
     * ExitBootServices() may return
     * EFI_INVALID_PARAMETER if the memory-map key
     * became invalid because firmware changed the map.
     *
     * Refresh the memory map and retry.
     */
    for (unsigned int attempt = 0;
         attempt < 8;
         ++attempt)
    {
        EFI_STATUS status =
            (EFI_STATUS) uefi_call_wrapper(
                (void*) bs->ExitBootServices,
                2,
                info->image_handle,
                info->memory_map_key);

        if (!EFI_ERROR(status))
            return EFI_SUCCESS;

        if (status != EFI_INVALID_PARAMETER)
            return status;

        /*
         * Firmware changed the memory map.
         */
        if (info->memory_map != nullptr)
        {
            uefi_call_wrapper(
                (void*) bs->FreePool,
                1,
                info->memory_map);

            info->memory_map = nullptr;
        }

        status = get_memory_map(info);

        if (EFI_ERROR(status))
            return status;
    }

    return EFI_ABORTED;
}

}
