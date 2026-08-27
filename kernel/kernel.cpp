#include "allocator.hpp"
#include "backbuffer.h"
#include "cmd/cmd_forth.hpp"
#include "cmd/command.hpp"
#include "console.hpp"
#include "drivers/Keymap.hpp"
#include "drivers/ata_devices.hpp"
#include "events.hpp"
#include "font8x8.h"
#include "fs/fat32.hpp"
#include "proc.hpp"
#include "ps2keyboard.hpp"
#include "ps2mouse.hpp"
#include "shell.hpp"
#include "sysmem.hpp"
#include "vfs.hpp"
#include "virtio_input.hpp"


extern "C"
{
#include <efi.h>
}
extern "C"
{
#include <efilib.h>
}

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


struct Window
{
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
    int h)
{
    bb_draw_rect(
        buffer,
        width,
        x,
        y,
        w,
        h,
        0x00FFFFFF);

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

    for (uint32_t i = 0; i < size; ++i)
    {
        char c = (char) data[i];

        if (c == '\n' || cx >= cols)
        {
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
            0x00000000);

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
    const Window& win)
{
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
        0x00FFFFFF);

    int tx = win.x + 8;
    int ty = win.y + 32;

    size_t count =
        vfs::count_files();

    for (size_t i = 0; i < count; ++i)
    {
        const char* name =
            vfs::name_at(i);

        if (!name)
            continue;

        int line_y =
            ty + (int) i * 10;

        if (line_y >= win.y + win.h - 8)
            break;

        bb_draw_text(
            buffer,
            width,
            tx,
            line_y,
            name,
            0x00000000);
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
    const Window& win)
{
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
        0x00C0C0C0);

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
        0x00008080);

    /*
     * Title
     */
    bb_draw_text(
        buffer,
        width,
        win.x + 8,
        win.y + 6,
        "BlockOS",
        0x00FFFFFF);

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
        win.h - 36);
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
    size_t length)
{
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
        0x00E0E0E0);

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
        0x00006060);

    if (filename)
    {
        bb_draw_text(
            buffer,
            width,
            win.x + 8,
            win.y + 6,
            filename,
            0x00FFFFFF);
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
        0x00FFFFFF);

    int tx = win.x + 8;
    int ty = win.y + 32;

    int cols = (win.w - 16) / 8;

    if (cols <= 0)
        return;

    int cx = 0;

    for (size_t i = 0; i < length; ++i)
    {
        char c = text[i];

        if (c == '\n' || cx >= cols)
        {
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
            0x00000000);

        ++cx;
    }
}

static void survey_memory_map(
    const void* map,
    UINTN map_size,
    UINTN desc_size,
    sysmem::SystemMemoryRecord& out)
{
    out = sysmem::SystemMemoryRecord{};

    const uint8_t* p = (const uint8_t*) map;
    const uint8_t* end = p + map_size;

    for (; p + desc_size <= end; p += desc_size)
    {
        const EFI_MEMORY_DESCRIPTOR* d =
            (const EFI_MEMORY_DESCRIPTOR*) p;

        // An EFI page is 4 KiB by definition, whatever the CPU page size is
        const uint64_t bytes =
            (uint64_t) d->NumberOfPages * 4096ull;

        out.regions++;

        switch (d->Type)
        {
            case EfiConventionalMemory:
                out.free += bytes;

                if (bytes > out.largest_free)
                    out.largest_free = bytes;

                break;

            case EfiBootServicesCode:
            case EfiBootServicesData:
                out.reclaimable += bytes;
                break;

            case EfiLoaderCode:
            case EfiLoaderData:
                out.kernel += bytes;
                break;

            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
            case EfiACPIReclaimMemory:
            case EfiACPIMemoryNVS:
                out.firmware += bytes;
                break;

            default:
                continue;
        }

        out.total += bytes;

        const uint64_t top =
            (uint64_t) d->PhysicalStart + bytes;

        if (top > out.highest_addr)
            out.highest_addr = top;
    }
}

/*
 * ============================================================
 * Console output sink
 * ============================================================
 */

extern "C" void blockos_tty_init();

extern "C" void blockos_tty_set_output_callback(
    void (*callback)(const char* data, size_t length, void* user),
    void* user);

static Console* g_console_sink = nullptr;

static void console_sink(const char* data, size_t length, void*)
{
    if (!g_console_sink || !data)
        return;

    for (size_t i = 0; i < length; ++i)
        g_console_sink->putc(data[i]);
}

// printf and friends land straight on the console; the TTY is not in this path
static void stdio_sink(const char* data, size_t length)
{
    console_sink(data, length, nullptr);
}

/*
 * ============================================================
 * Block devices
 * ============================================================
 */

static AtaPio g_ata_boot;
static AtaPio g_ata_data;
static AtaPio g_ata_fs;

AtaPio& ata_boot_disk()
{
    return g_ata_boot;
}

AtaPio& ata_data_disk()
{
    return g_ata_data;
}

AtaPio& ata_fs_disk()
{
    return g_ata_fs;
}

static Framebuffer* g_flush_fb = nullptr;
static void* g_flush_backbuf = nullptr;

static void flush_console()
{
    if (!g_console_sink || !g_flush_fb || !g_flush_backbuf)
        return;

    g_console_sink->render(
        (uint8_t*) g_flush_backbuf,
        g_flush_fb->Width);

    bb_blit_region_to_fb(
        g_flush_fb,
        (const uint8_t*) g_flush_backbuf,
        g_console_sink->x(),
        g_console_sink->y(),
        g_console_sink->w(),
        g_console_sink->h());
}

__attribute__((unused)) static void trace(Console& out, const char* message)
{
    out.print(message);
    out.newline();

    flush_console();
}

static void init_block_devices(Console& out)
{
    if (!g_ata_boot.init(AtaPio::Bus::Primary, AtaPio::Drive::Master))
    {
        out.print("ata0: ");
        out.print(AtaPio::error_name(g_ata_boot.error_at(0)));
        out.newline();
    }

    if (!g_ata_data.init(AtaPio::Bus::Primary, AtaPio::Drive::Slave))
    {
        out.print("ata1: ");
        out.print(AtaPio::error_name(g_ata_data.error_at(0)));
        out.newline();
    }

    if (!g_ata_fs.init(AtaPio::Bus::Secondary, AtaPio::Drive::Master))
    {
        out.print("ata2: ");
        out.print(AtaPio::error_name(g_ata_fs.error_at(0)));
        out.newline();
    }

    if (g_ata_fs.present())
    {
        legacy_fat32_fs.attach(g_ata_fs);
        legacy_fat32_fs.initialize_fat32();
    }
}

/*
 * ============================================================
 * Command dispatch
 * ============================================================
 */

static void run_command(const Args& args, Console& out)
{
    if (blockos::cmd::forth_main(args))
        return;

    if (args.count == 0)
        return;

    const char* name = args.argv[0];

    int status = 0;

    if (blockos::cmd::run_registered(args, out, &status))
        return;

    char output[1024];

    const size_t written =
        blockos::proc::read(
            name,
            output,
            sizeof(output));

    if (written > 0)
    {
        out.print(output);
        return;
    }

    out.print("unknown command: ");
    out.print(name);
    out.newline();
}

/*
 * ============================================================
 * Splash screen
 * ============================================================
 */
static size_t text_length(const char* s)
{
    size_t n = 0;

    while (s[n] != '\0')
        ++n;

    return n;
}

static void draw_centered(
    uint8_t* buffer,
    uint32_t width,
    int area_x,
    int area_w,
    int y,
    const char* text,
    uint32_t color)
{
    const int text_w = (int) (text_length(text) * 8);

    if (text_w > area_w)
        return;

    bb_draw_text(
        buffer,
        width,
        (uint32_t) (area_x + (area_w - text_w) / 2),
        (uint32_t) y,
        text,
        color);
}

static void draw_block_art(
    uint8_t* buffer,
    uint32_t width,
    int x,
    int y,
    const char* const* rows,
    int row_count,
    int line_h,
    uint32_t color)
{
    for (int i = 0; i < row_count; ++i)
    {
        bb_draw_text(
            buffer,
            width,
            (uint32_t) x,
            (uint32_t) (y + i * line_h),
            rows[i],
            color);
    }
}

static void draw_splash(
    uint8_t* buffer,
    uint32_t width,
    uint32_t height)
{
    const int w = (int) width;
    const int h = (int) height;

    bb_clear(buffer, width, height, 0x00101820);

    // clang-format off
    const char* banner[5] = {
    "    _|_|_|    _|                      _|          _|_|      _|_|_|",
    "    _|    _|  _|    _|_|      _|_|_|  _|  _|    _|    _|  _|",
    "    _|_|_|    _|  _|    _|  _|        _|_|      _|    _|    _|_|",
    "    _|    _|  _|  _|    _|  _|        _|  _|    _|    _|        _|",
    "    _|_|_|    _|    _|_|      _|_|_|  _|    _|    _|_|    _|_|_|"
    };
    // clang-format on

    //ASCII art of Saturn from https://asciiart.website/art/2534
    const char* saturn[36] = {
        "                                                                  ..;===+.",
        "                                                              .:=iiiiii=+=",
        "                                                           .=i))=;::+)i=+,",
        "                                                        ,=i);)I)))I):=i=;",
        "                                                     .=i==))))ii)))I:i++",
        "                                                   +)+))iiiiiiii))I=i+:'",
        "                              .,:;;++++++;:,.       )iii+:::;iii))+i='",
        "                           .:;++=iiiiiiiiii=++;.    =::,,,:::=i));=+'",
        "                         ,;+==ii)))))))))))ii==+;,      ,,,:=i))+=:",
        "                       ,;+=ii))))))IIIIII))))ii===;.    ,,:=i)=i+",
        "                      ;+=ii)))IIIIITIIIIII))))iiii=+,   ,:=));=,",
        "                    ,+=i))IIIIIITTTTTITIIIIII)))I)i=+,,:+i)=i+",
        "                   ,+i))IIIIIITTTTTTTTTTTTI))IIII))i=::i))i='",
        "                  ,=i))IIIIITLLTTTTTTTTTTIITTTTIII)+;+i)+i`",
        "                  =i))IIITTLTLTTTTTTTTTIITTLLTTTII+:i)ii:'",
        "                 +i))IITTTLLLTTTTTTTTTTTTLLLTTTT+:i)))=,",
        "                 =))ITTTTTTTTTTTLTTTTTTLLLLLLTi:=)IIiii;",
        "                .i)IIITTTTTTTTLTTTITLLLLLLLT);=)I)))))i;",
        "                :))IIITTTTTLTTTTTTLLHLLLLL);=)II)IIIIi=:",
        "                :i)IIITTTTTTTTTLLLHLLHLL)+=)II)ITTTI)i=",
        "                .i)IIITTTTITTLLLHHLLLL);=)II)ITTTTII)i+",
        "                =i)IIIIIITTLLLLLLHLL=:i)II)TTTTTTIII)i'",
        "              +i)i)))IITTLLLLLLLLT=:i)II)TTTTLTTIII)i;",
        "            +ii)i:)IITTLLTLLLLT=;+i)I)ITTTTLTTTII))i;",
        "           =;)i=:,=)ITTTTLTTI=:i))I)TTTLLLTTTTTII)i;",
        "         +i)ii::,  +)IIITI+:+i)I))TTTTLLTTTTTII))=,",
        "       :=;)i=:,,    ,i++::i))I)ITTTTTTTTTTIIII)=+'",
        "     .+ii)i=::,,   ,,::=i)))iIITTTTTTTTIIIII)=+",
        "    ,==)ii=;:,,,,:::=ii)i)iIIIITIIITIIII))i+:'",
        "   +=:))i==;:::;=iii)+)=  `:i)))IIIII)ii+'",
        " .+=:))iiiiiiii)))+ii;",
        ".+=;))iiiiii)));ii+",
        ".+=i:)))))))=+ii+",
        ".;==i+::::=)i=;",
        ",+==iiiiii+,",
        "`+=+++;`",
    };

    const int line_h = 10;

    // Banner in the top-left quadrant
    const int title_x = w / 16;
    const int title_y = h / 10;
    const int title_w = 34 * 8;

    if (title_x + title_w <= w)
    {
        draw_block_art(
            buffer, width, title_x, title_y, banner, 5, line_h, 0x0000C0C0);

        bb_draw_text(
            buffer,
            width,
            (uint32_t) title_x,
            (uint32_t) (title_y + 6 * line_h),
            "x86-64 UEFI",
            0x00808080);
    }

    const int art_w = 76 * 8;
    const int art_h = 36 * 8;
    const int art_x = w - art_w - 32;
    const int art_y = (h - art_h) / 2;

    if (art_x > title_x + title_w + 16 && art_y >= 0 && art_y + art_h <= h)
    {
        draw_block_art(
            buffer, width, art_x, art_y, saturn, 36, 8, 0x00C02828);
    }

    draw_centered(
        buffer, width, 0, w, h - 48, "press any key to continue", 0x00FFFFFF);
}


/*
 * ============================================================
 * EFI entry point
 * ============================================================
 */
extern "C" EFI_STATUS EFIAPI efi_main(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable)
{
    InitializeLib(
        ImageHandle,
        SystemTable);


    /*
     * ========================================================
     * GOP
     * ========================================================
     */

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;

    EFI_GUID gopGuid =
        EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    EFI_STATUS status =
        (EFI_STATUS) uefi_call_wrapper(
            (void*) BS->LocateProtocol,
            3,
            &gopGuid,
            NULL,
            (void**) &gop);

    if (EFI_ERROR(status) || gop == NULL)
    {
        Print(
            (CHAR16*) L"Couldn't locate GOP\n");

        return EFI_ABORTED;
    }


    /*
     * ========================================================
     * Framebuffer
     * ========================================================
     */

    Framebuffer fb;

    fb.Base =
        (uint8_t*) (UINTN)
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
        (UINTN) fb.Width *
        (UINTN) fb.Height *
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
        (EFI_STATUS) uefi_call_wrapper(
            (void*) BS->GetMemoryMap,
            5,
            &mapSize,
            NULL,
            &mapKey,
            &descSize,
            &descVersion);

    if (status != EFI_BUFFER_TOO_SMALL)
    {
        Print(
            (CHAR16*) L"Unexpected GetMemoryMap status: %r\n");

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
        (EFI_STATUS) uefi_call_wrapper(
            (void*) BS->AllocatePool,
            3,
            EfiLoaderData,
            mapSize,
            &memMap);

    if (EFI_ERROR(status))
    {
        Print(
            (CHAR16*) L"AllocatePool failed for memMap: %r\n");

        return EFI_ABORTED;
    }


    /*
     * ========================================================
     * Allocate backbuffer
     * ========================================================
     */

    void* backbuf = NULL;

    status =
        (EFI_STATUS) uefi_call_wrapper(
            (void*) BS->AllocatePool,
            3,
            EfiLoaderData,
            backbuffer_size,
            &backbuf);

    if (EFI_ERROR(status))
    {
        Print(
            (CHAR16*) L"AllocatePool failed for backbuffer: %r\n");

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
        (EFI_STATUS) uefi_call_wrapper(
            (void*) BS->AllocatePool,
            3,
            EfiLoaderData,
            heap_size,
            &heapbuf);

    if (EFI_ERROR(status))
    {
        Print(
            (CHAR16*) L"AllocatePool failed for heap: %r\n");

        return EFI_ABORTED;
    }


    /*
     * ========================================================
     * Get memory map again
     * ========================================================
     */

    status =
        (EFI_STATUS) uefi_call_wrapper(
            (void*) BS->GetMemoryMap,
            5,
            &mapSize,
            memMap,
            &mapKey,
            &descSize,
            &descVersion);

    if (EFI_ERROR(status))
    {
        Print(
            (CHAR16*) L"GetMemoryMap failed: %r\n");

        return EFI_ABORTED;
    }

    /*
     * ========================================================
     * System memory survey
     * ========================================================
     */

    {
        sysmem::SystemMemoryRecord record;

        survey_memory_map(
            memMap,
            mapSize,
            descSize,
            record);

        sysmem::set_record(record);
    }


    /*
     * ========================================================
     * Allocator
     * ========================================================
     */

    allocator::init(
        heapbuf,
        heap_size);


    /*
     * ========================================================
     * Exit Boot Services
     * ========================================================
     */

    status =
        (EFI_STATUS) uefi_call_wrapper(
            (void*) BS->ExitBootServices,
            2,
            ImageHandle,
            mapKey);

    if (EFI_ERROR(status))
    {
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
    Keymap keymap;

    mouse.init();
    keyboard.init();

    char char_buffer[256] = {0};
    size_t char_length = 0;
    size_t char_cap = sizeof(char_buffer);


    /*
     * ========================================================
     * GUI
     * ========================================================
     */

    Window win{
        (int) fb.Width / 4,
        (int) fb.Height / 4,
        (int) fb.Width / 2,
        (int) fb.Height / 2,
        false,
        0,
        0};

    static Console console;
    static Shell shell;

    bool splash_active = true;

    shell.attach(
        console,
        win.x + 1,
        win.y + win.h - 11,
        win.w - 2,
        10);

    shell.set_handler(run_command);

    console.attach(
        win.x + 4,
        win.y + 28,
        win.w - 8,
        win.h - 28 - 14);

    console.set_colors(0x00000000, 0x00C0C0C0);
    console.clear();

    g_console_sink = &console;

    blockos_tty_init();
    blockos_tty_set_output_callback(console_sink, nullptr);
    blockos_stdio_set_console(stdio_sink);

    printf("BlockOS console\n");

    // console.print("commands: ");

    // for (size_t i = 0; i < blockos::proc::count(); ++i)
    // {
    //     if (i)
    //         console.print(" ");

    //     console.print(blockos::proc::name_at(i));
    // }

    console.newline();

    g_flush_fb = &fb;
    g_flush_backbuf = backbuf;

    init_block_devices(console);


    /*
     * ========================================================
     * Initial screen
     * ========================================================
     */

    draw_splash(
        (uint8_t*) backbuf,
        fb.Width,
        fb.Height);


    bb_blit_to_fb(
        &fb,
        (const uint8_t*) backbuf);


    /*
     * ========================================================
     * Cursor
     * ========================================================
     */

    int cursor_x =
        (int) fb.Width / 2;

    int cursor_y =
        (int) fb.Height / 2;


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

    while (1)
    {
        /*
         * ----------------------------------------------------
         * Keyboard
         * ----------------------------------------------------
         */

        KeyEvent key_event;
        bool kb =
            keyboard.poll(key_event);


        /*
         * ----------------------------------------------------
         * Mouse
         * ----------------------------------------------------
         */

        int16_t mb =
            mouse.read_byte_nonblocking();

        if (mb != -1)

            // TODO? Desync recovery if bit 3 != 1
            packet[packet_index++] =
                (uint8_t) mb;

        if (packet_index == 3)
        {
            packet_index = 0;

            uint8_t buttons =
                packet[0];

            int8_t dx =
                (int8_t) packet[1];

            int8_t dy =
                (int8_t) packet[2];


            /*
                 * Cursor movement
                 */
            cursor_x += dx;
            cursor_y -= dy;


            /*
                 * X bounds
                 */
            if (cursor_x < 0)
            {
                cursor_x = 0;
            }

            if (cursor_x >= (int) fb.Width)
            {
                cursor_x =
                    (int) fb.Width - 1;
            }


            /*
                 * Y bounds
                 */
            if (cursor_y < 0)
            {
                cursor_y = 0;
            }

            if (cursor_y >= (int) fb.Height)
            {
                cursor_y =
                    (int) fb.Height - 1;
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

            if (new_left && !left_pressed)
            {
                if (!in_editor)
                {
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
                        cursor_y < list_y + list_h)
                    {
                        int index =
                            (cursor_y - list_y) / 10;


                        if (index >= 0)
                        {
                            size_t count =
                                vfs::count_files();

                            if (
                                (size_t) index <
                                count)
                            {
                                const char* name =
                                    vfs::name_at(
                                        (size_t) index);

                                if (name)
                                {
                                    uint32_t file_size =
                                        0;

                                    const uint8_t* data =
                                        vfs::read_file(
                                            name,
                                            &file_size);

                                    if (data)
                                    {
                                        /*
                                             * Editor buffer
                                             */
                                        editor_cap =
                                            (size_t) file_size +
                                            4096;

                                        editor_buf =
                                            (char*)
                                                allocator::alloc(
                                                    editor_cap);

                                        if (editor_buf)
                                        {
                                            memcpy(
                                                editor_buf,
                                                data,
                                                file_size);

                                            editor_len =
                                                (size_t) file_size;


                                            /*
                                                 * Filename
                                                 */
                                            size_t name_len =
                                                strlen(name);

                                            editor_name =
                                                (char*)
                                                    allocator::alloc(
                                                        name_len + 1);

                                            if (editor_name)
                                            {
                                                strcpy(
                                                    editor_name,
                                                    name);

                                                in_editor =
                                                    true;


                                                draw_editor(
                                                    (uint8_t*) backbuf,
                                                    fb.Width,
                                                    win,
                                                    editor_name,
                                                    editor_buf,
                                                    editor_len);


                                                bb_blit_region_to_fb(
                                                    &fb,
                                                    (const uint8_t*) backbuf,
                                                    win.x,
                                                    win.y,
                                                    win.w,
                                                    win.h);
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
            if (!new_left && left_pressed)
            {
                /* nothing */
            }

            left_pressed =
                new_left;
        }


        /*
         * ====================================================
         * Keyboard handling
         * ====================================================
         */

        if (kb)
        {
            if (splash_active)
            {
                if (key_event.is_pressed)
                {
                    splash_active = false;

                    bb_clear(
                        (uint8_t*) backbuf,
                        fb.Width,
                        fb.Height,
                        0x00303030);

                    draw_main_window(
                        (uint8_t*) backbuf,
                        fb.Width,
                        win);

                    bb_blit_to_fb(
                        &fb,
                        (const uint8_t*) backbuf);
                }
                continue;
            }

            KeyPress key = keymap.translate(key_event);
            char ch = key.ch;

            /*
             * ------------------------------------------------
             * Editor
             * ------------------------------------------------
             */

            if (in_editor)
            {
                /*
                 * ESC = exit editor
                 */
                if (key.key == NonCharacterKey::Escape)
                {
                    in_editor = false;

                    draw_main_window(
                        (uint8_t*) backbuf,
                        fb.Width,
                        win);

                    bb_blit_region_to_fb(
                        &fb,
                        (const uint8_t*) backbuf,
                        win.x,
                        win.y,
                        win.w,
                        win.h);
                }

                /*
                 * CTRL+S / save can be added once the keyboard
                 * modifier API is known.
                 */
                else if (ch)
                {
                    if (editor_len + 1 <
                        editor_cap)
                    {
                        editor_buf[editor_len++] = ch;
                    }


                    draw_editor(
                        (uint8_t*) backbuf,
                        fb.Width,
                        win,
                        editor_name,
                        editor_buf,
                        editor_len);


                    bb_blit_region_to_fb(
                        &fb,
                        (const uint8_t*) backbuf,
                        win.x,
                        win.y,
                        win.w,
                        win.h);
                }
            }


            /*
             * ------------------------------------------------
             * Main window
             * ------------------------------------------------
             */

            else
            {
                shell.handle(key);
            }
        }

        if (!splash_active && console.dirty())
        {
            console.render(
                (uint8_t*) backbuf,
                fb.Width);

            bb_blit_region_to_fb(
                &fb,
                (const uint8_t*) backbuf,
                console.x(),
                console.y(),
                console.w(),
                console.h());
        }

        if (!splash_active && shell.dirty())
        {
            shell.render((uint8_t*) backbuf, fb.Width);

            bb_blit_region_to_fb(
                &fb,
                (const uint8_t*) backbuf,
                shell.x(),
                shell.y(),
                shell.w(),
                shell.h());
        }

        /*
         * ====================================================
         * Idle
         * ====================================================
         */

        __asm__ volatile("pause");
    }


    return EFI_SUCCESS;
}
