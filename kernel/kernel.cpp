#include "drivers/hardware_autoprobe.hpp"
#include "allocator.hpp"
#include "arch/86_64x/hardware_tables.hpp"
#include "backbuffer.h"
#include "cmd/cmd_forth.hpp"
#include "cmd/command.hpp"
#include "console.hpp"
#include "drivers/keymap.hpp"
#include "drivers/ata_devices.hpp"
#include "drivers/acpi.hpp"
#include "drivers/pci.hpp"
#include "events.hpp"
#include "font8x8.h"
#include "fs/fat32.hpp"
#include "proc.hpp"
#include "shell.hpp"
#include "sysmem.hpp"
#include "vfs.hpp"
#include "process.hpp"
#include "virtio_input.hpp"
#include "input_bridge.hpp"
#include "drivers/dhcp_dns_stack.hpp"
#include "drivers/virtio_blk.hpp"
#include "drivers/nvme.hpp"
#include "drivers/audio.hpp"
#include "drivers/power.hpp"
#include "fs/vfs_blk_adapter.hpp"
#include "net/net.hpp"
#include "drivers/uefi_smp.hpp"

extern "C"
{
#include <efi.h>
}

extern "C" bool blockos_space_init(EFI_SYSTEM_TABLE*);
extern "C" bool blockos_storage_init();
extern "C" bool blockos_space_recovery_requested();
extern "C"
{
#include <efilib.h>
}

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static inline void early_serial_out(char c)
{
    asm volatile("outb %0, $0x3F8" : : "a"(c));
}

static void early_serial_str(const char* s)
{
    while (*s) early_serial_out(*s++);
}

__attribute__((constructor(101)))
static void boot_marker_a(void) { early_serial_str("A"); }

__attribute__((constructor(200)))
static void boot_marker_b(void) { early_serial_str("B"); }

__attribute__((constructor(65000)))
static void boot_marker_c(void) { early_serial_str("C\n"); }

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

extern "C" void blockos_tty_input_char(uint8_t c);
extern "C" void blockos_tty_input_bytes(const void* data, size_t length);

static Framebuffer* g_flush_fb = nullptr;
static void* g_flush_backbuf = nullptr;

static void flush_console();

static Console* g_console_sink = nullptr;
static Keymap g_tty_keymap;

static void console_sink(const char* data, size_t length, void*)
{
    if (!g_console_sink || !data)
        return;

    for (size_t i = 0; i < length; ++i)
        g_console_sink->putc(data[i]);

    flush_console();
}

static void tty_send_escape(const char* sequence)
{
    blockos_tty_input_bytes(
        sequence,
        strlen(sequence));
}

static void route_keyboard_event_to_tty(
    const blockos::input::Event& ev)
{
    if (ev.type != blockos::input::EVENT_KEYBOARD)
        return;

    KeyEvent kev{};
    kev.scancode = static_cast<uint8_t>(ev.scancode & 0xFFu);
    kev.is_pressed = ev.pressed != 0;
    kev.is_extended = ev.extended != 0;

    const KeyPress press =
        g_tty_keymap.translate(kev);

    if (!kev.is_pressed)
        return;

    if (press.ch != 0)
    {
        blockos_tty_input_char(
            static_cast<uint8_t>(press.ch));
        return;
    }

    switch (press.key)
    {
        case NonCharacterKey::Backspace:
            blockos_tty_input_char(0x7F);
            break;

        case NonCharacterKey::Enter:
            blockos_tty_input_char('\n');
            break;

        case NonCharacterKey::Tab:
            blockos_tty_input_char('\t');
            break;

        case NonCharacterKey::Escape:
            blockos_tty_input_char(0x1B);
            break;

        case NonCharacterKey::Delete:
            tty_send_escape("\x1b[3~");
            break;

        case NonCharacterKey::Up:
            tty_send_escape("\x1b[A");
            break;

        case NonCharacterKey::Down:
            tty_send_escape("\x1b[B");
            break;

        case NonCharacterKey::Right:
            tty_send_escape("\x1b[C");
            break;

        case NonCharacterKey::Left:
            tty_send_escape("\x1b[D");
            break;

        case NonCharacterKey::Home:
            tty_send_escape("\x1b[H");
            break;

        case NonCharacterKey::End:
            tty_send_escape("\x1b[F");
            break;

        case NonCharacterKey::PageUp:
            tty_send_escape("\x1b[5~");
            break;

        case NonCharacterKey::PageDown:
            tty_send_escape("\x1b[6~");
            break;

        case NonCharacterKey::None:
        default:
            break;
    }
}

static void service_tty_input()
{
    blockos::input::poll_hardware();

    blockos::input::Event events[32]{};

    for (;;)
    {
        const size_t n =
            blockos::input::read(
                events,
                sizeof(events) / sizeof(events[0]));

        if (n == 0)
            break;

        for (size_t i = 0; i < n; ++i)
            route_keyboard_event_to_tty(events[i]);
    }
}

extern "C" void blockos_kernel_poll_input()
{
    service_tty_input();
}

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

/* Show a useful diagnostic instead of silently halting on a black screen. */
static void boot_error_halt(Console& console, const char* reason)
{
    console.clear();
    console.print("BlockOS boot error:");
    console.newline();
    console.print(reason);
    console.newline();
    flush_console();

    /* Keep a second copy for QEMU's serial log/debug console. */
    early_serial_str("BlockOS boot error: ");
    early_serial_str(reason);
    early_serial_out('\n');

    while (1)
    {
        __asm__ volatile("cli; hlt");
    }
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
        legacy_fat32_fs.initialize();
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

    char path[128];
    size_t n = 0;

    path[n++] = '/';
    path[n++] = 'b';
    path[n++] = 'i';
    path[n++] = 'n';
    path[n++] = '/';

    for (size_t i = 0; name[i] && n + 1 < sizeof(path); ++i)
        path[n++] = name[i];

    path[n] = '\0';

    uint32_t elf_size = 0;

    const uint8_t* elf =
        vfs::read_file(
            path,
            &elf_size);

    if (elf)
    {
        process::Process* p =
            process::create(
                elf,
                elf_size);

        if (!p)
        {
            out.print("exec: ELF load failed");
            out.newline();
            return;
        }

        process::run(p);
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
    const int text_w =
        (int) (text_length(text) * 8);

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

    bb_clear(
        buffer,
        width,
        height,
        0x00101820);

    // clang-format off
    const char* banner[5] = {
    "    _|_|_|    _|                      _|          _|_|      _|_|_|",
    "    _|    _|  _|    _|_|      _|_|_|  _|  _|    _|    _|  _|",
    "    _|_|_|    _|  _|    _|  _|        _|_|      _|    _|    _|_|",
    "    _|    _|  _|  _|    _|  _|        _|  _|    _|    _|        _|",
    "    _|_|_|    _|    _|_|      _|_|_|  _|    _|    _|_|    _|_|_|"
    };
    // clang-format on

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

    const int title_x = w / 16;
    const int title_y = h / 10;
    const int title_w = 34 * 8;

    if (title_x + title_w <= w)
    {
        draw_block_art(
            buffer,
            width,
            title_x,
            title_y,
            banner,
            5,
            line_h,
            0x0000C0C0);

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

    if (art_x > title_x + title_w + 16 &&
        art_y >= 0 &&
        art_y + art_h <= h)
    {
        draw_block_art(
            buffer,
            width,
            art_x,
            art_y,
            saturn,
            36,
            8,
            0x00C02828);
    }

    draw_centered(
        buffer,
        width,
        0,
        w,
        h - 48,
        "press any key to continue",
        0x00FFFFFF);
}


/*
 * ============================================================
 * PCIe ECAM / ACPI MCFG discovery
 * ============================================================
 */

static const acpi::Rsdp* find_uefi_rsdp(EFI_SYSTEM_TABLE* table)
{
    if (!table)
        return nullptr;

    // ACPI 2.0 GUID: 8868E871-E4F1-11D3-BC22-0080C73C8881
    EFI_GUID acpi20 = {
        0x8868e871, 0xe4f1, 0x11d3,
        {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}
    };

    // ACPI 1.0 GUID: EB9D2D30-2D88-11D3-9A16-0090273FC14D
    EFI_GUID acpi10 = {
        0xeb9d2d30, 0x2d88, 0x11d3,
        {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}
    };

    for (UINTN i = 0; i < table->NumberOfTableEntries; ++i)
    {
        EFI_CONFIGURATION_TABLE& entry = table->ConfigurationTable[i];

        if (CompareGuid(&entry.VendorGuid, &acpi20) ||
            CompareGuid(&entry.VendorGuid, &acpi10))
        {
            return reinterpret_cast<const acpi::Rsdp*>(entry.VendorTable);
        }
    }

    return nullptr;
}

static void configure_pci_ecam(EFI_SYSTEM_TABLE* table)
{
    const acpi::Rsdp* rsdp = find_uefi_rsdp(table);
    if (!rsdp)
        return;

    uint64_t ecam = 0;
    uint16_t segment = 0;
    uint8_t start_bus = 0;
    uint8_t end_bus = 0;

    if (acpi::parse_mcfg(
            rsdp,
            &ecam,
            &segment,
            &start_bus,
            &end_bus))
    {
        if (segment == 0 && ecam != 0 && end_bus >= start_bus)
            pci_set_ecam(ecam, start_bus, end_bus);
    }
}

/*
 * ============================================================
 * EFI entry point
 * ============================================================
 */

/* crt0-efi-x86_64 calls efi_main with the System V ABI (rdi/rsi), so this must
 * NOT be ms_abi (EFIAPI) even though GNU_EFI_USE_MS_ABI is defined. */
extern "C" EFI_STATUS efi_main(
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

#ifndef BLOCKOS_SERVER_MODE
    if (EFI_ERROR(status) || gop == NULL)
    {
        Print((CHAR16*) L"Couldn't locate GOP\n");
        return EFI_ABORTED;
    }
#endif

    const bool have_gop = !EFI_ERROR(status) && gop != NULL;

    /*
     * ========================================================
     * Framebuffer
     * ========================================================
     */

    Framebuffer fb{};

    if (have_gop)
    {
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
    }

    /*
     * ========================================================
     * Backbuffer
     * ========================================================
     */

    UINTN backbuffer_size = have_gop
        ? (UINTN) fb.Width * (UINTN) fb.Height * 4
        : 0;

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

    if (backbuffer_size != 0)
    {
        status =
            (EFI_STATUS) uefi_call_wrapper(
                (void*) BS->AllocatePool,
                3,
                EfiLoaderData,
                backbuffer_size,
                &backbuf);

        if (EFI_ERROR(status))
        {
            Print((CHAR16*) L"AllocatePool failed for backbuffer: %r\n");
            return EFI_ABORTED;
        }
    }

    /*
     * ========================================================
     * Kernel heap
     * ========================================================
     */

#ifdef BLOCKOS_SERVER_MODE
    const size_t heap_size = 32 * 1024 * 1024;
#else
    const size_t heap_size = 4 * 1024 * 1024;
#endif

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

    // Discover PCIe ECAM from ACPI MCFG before leaving firmware services.
    configure_pci_ecam(SystemTable);

    /* Bring up as many compatible hardware backends as possible before
       ExitBootServices(). Every backend is best-effort and may fail safely. */
    blockos_hw::pre_exit_bootstrap();

#ifdef BLOCKOS_SERVER_MODE
    // Start secondary CPUs before the final memory-map query so their
    // firmware allocations are included in the ExitBootServices map key.
    uefi_smp::start_aps(SystemTable);
#endif

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
            (EFI_MEMORY_DESCRIPTOR*)memMap,
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

#ifdef BLOCKOS_SERVER_MODE
    // Initialize hardware while UEFI console/boot services are still live.
    // The allocated DMA queues/buffers are then included in the final memory map.
    const bool server_blk_ready = virtio_blk::init();
    const bool server_nvme_ready = nvme::init();
    const bool server_audio_ready = audio::init();
    const bool server_power_ready = power::init(SystemTable);
    const bool server_net_ready = (blockos::net::init(), blockos::net::is_initialized());
    (void)server_blk_ready;
    (void)server_nvme_ready;
    (void)server_audio_ready;
    (void)server_power_ready;
    (void)server_net_ready;

    /* Register the controller-neutral storage backend after all controllers
       have had a chance to probe, then expose it to VFS. */
    (void)blockos_storage_init();
    (void)vfs_blk_adapter::init_backend();
    blockos_space_init(SystemTable);
#endif

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
     * GDT/IDT
     * ========================================================
     */

    cpu_tables.init();

#ifdef BLOCKOS_SERVER_MODE
    uefi_smp::release_aps();
#endif

    /*
     * ========================================================
     * Input for boot splash only
     * ========================================================
     *
     * The kernel no longer owns a GUI/editor/shell window.
     * The only keyboard handling here is dismissing the boot
     * splash before entering the first userspace program.
     */

    /*
     * ========================================================
     * TTY / userspace runtime
     * ========================================================
     */

    /* Create the framebuffer console before any fatal boot checks below.
     * boot_error_halt() needs a live Console reference, and the VFS mount
     * can fail before userspace or TTY initialization is complete. */
    Console console;

    if (have_gop)
    {
        console.attach(
            0,
            0,
            static_cast<int>(fb.Width),
            static_cast<int>(fb.Height));
    }
    else
    {
        console.attach(0, 0, 640, 480);
    }

    console.set_colors(
        0x00FFFFFF,
        0x00000000);
    console.clear();
    g_console_sink = &console;

    /*
     * The project-level vfs.hpp declares vfs_init_from_ramfs() as void.
     * Initialize the VFS using that API; do not treat its return value as a
     * status code or make the boot path depend on /bin/sh being present.
     */
    vfs_init_from_ramfs();
    blockos_tty_init();
    blockos::input::init();

    /*
     * Publish framebuffer to the userspace X11 KDrive backend.
     */

    if (have_gop)
    {
        struct BlockOSDisplayInfo
        {
            uint32_t magic;
            uint32_t version;
            uint64_t framebuffer_phys;
            uint64_t framebuffer_size;
            uint32_t width;
            uint32_t height;
            uint32_t stride;
            uint32_t bpp;
            uint32_t depth;
        };

        BlockOSDisplayInfo dinfo{
            0x424F5346u,
            1u,
            (uint64_t)(uintptr_t)fb.Base,
            (uint64_t)fb.PixelsPerScanLine *
                (uint64_t)fb.Height *
                4ull,
            fb.Width,
            fb.Height,
            fb.PixelsPerScanLine,
            32u,
            32u
        };

        vfs::write_file(
            "/system/display.info",
            reinterpret_cast<const uint8_t*>(&dinfo),
            sizeof(dinfo));

        if (!vfs::is_device("/devices/display"))
        {
            vfs::DeviceNodeInfo di{};

            di.type = vfs::DEVICE_GPU;
            di.device_id = 0;
            di.base = (uint64_t)(uintptr_t)fb.Base;
            di.size = dinfo.framebuffer_size;

            vfs::create_device_node(
                "/devices/display",
                di);
        }

        if (!vfs::is_device("/devices/x11-input"))
        {
            vfs::DeviceNodeInfo ii{};

            ii.type = vfs::DEVICE_INPUT;
            ii.device_id = 0;

            vfs::create_device_node(
                "/devices/x11-input",
                ii);
        }

    }

    process::init();

    /*
     * Keep block-device initialization because the filesystem/VFS
     * layer may depend on the discovered storage devices.
     *
     * The Console is the framebuffer-backed TTY output sink.
     */

    blockos_tty_set_output_callback(
        console_sink,
        nullptr);

#ifndef BLOCKOS_SERVER_MODE
    init_block_devices(
        console);
#endif

#ifdef BLOCKOS_SERVER_MODE
    // Runtime server path: expose the already-initialized VirtIO disk and
    // obtain an OCI/LAN address by DHCP without using UEFI console services.
    if (virtio_blk::is_ready())
    {
        vfs::DeviceNodeInfo di{};
        di.type = vfs::DEVICE_DISK;
        di.device_id = 0;
        di.size = virtio_blk::capacity_sectors() * 512ULL;
        vfs::create_device_node("/devices/virtio-blk0", di);
    }

    if (nvme::is_ready())
    {
        vfs::DeviceNodeInfo nvme_di{};
        nvme_di.type = vfs::DEVICE_DISK;
        nvme_di.device_id = 1;
        nvme_di.size = nvme::capacity_sectors() * 512ULL;
        vfs::create_device_node("/devices/nvme0", nvme_di);
    }

    if (audio::is_ready())
    {
        vfs::DeviceNodeInfo audio_di{};
        audio_di.type = vfs::DEVICE_GENERIC;
        audio_di.device_id = 0;
        audio_di.size = 0;
        vfs::create_device_node("/devices/audio0", audio_di);
    }

    if (blockos::net::is_initialized())
        dynamic_net_stack.configure(1500000);
#else

    /*
     * ========================================================
     * Planet / Saturn boot splash
     * ========================================================
     */

    g_flush_fb = &fb;
    g_flush_backbuf = backbuf;

    draw_splash(
        (uint8_t*) backbuf,
        fb.Width,
        fb.Height);

    bb_blit_to_fb(
        &fb,
        (const uint8_t*) backbuf);

    /*
     * Wait for a key to dismiss the splash.
     */

    while (1)
    {
        blockos::input::poll_hardware();

        blockos::input::Event ev{};

        bool dismiss = false;

        while (blockos::input::read(&ev, 1) == 1)
        {
            if (ev.type ==
                    blockos::input::EVENT_KEYBOARD &&
                ev.pressed)
            {
                dismiss = true;
                break;
            }
        }

        if (dismiss)
        {
            /*
             * Clear the boot splash from the real framebuffer before
             * entering userspace. The splash is rendered through the
             * backbuffer, so simply breaking out of the loop would leave
             * the last splash frame visible on the GOP framebuffer.
             */
            bb_clear(
                (uint8_t*) backbuf,
                fb.Width,
                fb.Height,
                0x00000000);

            bb_blit_to_fb(
                &fb,
                (const uint8_t*) backbuf);

            break;
        }

        __asm__ volatile("pause");
    }

#endif

    /*
     * ========================================================
     * First userspace program: /bin/sh
     * ========================================================
     */

    uint32_t shell_size = 0;

    const char* first_user = blockos_space_recovery_requested()
        ? "/bin/recovery"
        : "/bin/sh";

    const uint8_t* shell_elf =
        vfs::read_file(
            first_user,
            &shell_size);

    if ((!shell_elf || shell_size == 0) && blockos_space_recovery_requested()) {
        first_user = "/bin/sh";
        shell_elf = vfs::read_file(first_user, &shell_size);
    }

    if (!shell_elf || shell_size == 0)
    {
        boot_error_halt(
            console,
            "/bin/sh could not be loaded from the embedded RAMFS");
    }

    process::Process* shell_process =
        process::create(
            shell_elf,
            (size_t) shell_size,
            first_user);

    if (!shell_process)
    {
        boot_error_halt(
            console,
            "process::create failed for /bin/sh");
    }

    /*
     * Enter ring3 and run /bin/sh as the first userspace process.
     * Keyboard input is also serviced from the userspace syscall path
     * through blockos_kernel_poll_input().
     */
    service_tty_input();

    process::run(
        shell_process);

    /* Do not fail silently if the first userspace process returns. */
    boot_error_halt(
        console,
        "/bin/sh returned unexpectedly");

    return EFI_SUCCESS;
}
