#include "cmd_ata.hpp"

#include <string.h>

#include "ata_pio.hpp"

namespace
{

AtaPio disk;

uint8_t sector[AtaPio::SECTOR_SIZE];

constexpr AtaPio::Bus BUS = AtaPio::Bus::Primary;
constexpr AtaPio::Drive DRIVE = AtaPio::Drive::Slave;

constexpr uint32_t PREVIEW_BYTES = 64;


bool ready(Console& out)
{
    if (disk.present())
        return true;

    if (disk.init(BUS, DRIVE))
        return true;

    out.print("ata: ");
    out.print(AtaPio::error_name(disk.error_at(0)));
    out.newline();

    return false;
}


void fail(Console& out, const char* what)
{
    out.print(what);
    out.print(": ");
    out.print(AtaPio::error_name(disk.error_at(0)));
    out.newline();
}


void read(const Args& args, Console& out)
{
    if (!ready(out))
        return;

    uint32_t lba = 0;

    if (!args.uint(1, &lba))
    {
        out.print("usage: ata-read <lba>\n");
        return;
    }

    if (!disk.read_sectors(lba, 1, sector))
    {
        fail(out, "ata: read failed");
        return;
    }

    for (uint32_t i = 0; i < PREVIEW_BYTES; ++i)
    {
        const char c = (char) sector[i];
        const char text[2] = {(c >= 32 && c < 127) ? c : ' ', '\0'};

        out.print(text);
    }

    out.newline();
}


void write(const Args& args, Console& out)
{
    if (!ready(out))
        return;

    uint32_t lba = 0;

    if (!args.uint(1, &lba) || args.count < 3)
    {
        out.print("usage: ata-write <lba> <text>\n");
        return;
    }

    memset(sector, 0, sizeof(sector));

    size_t pos = 0;

    for (size_t i = 2; i < args.count && pos < sizeof(sector); ++i)
    {
        if (i > 2)
            sector[pos++] = ' ';

        const char* token = args.argv[i];

        for (size_t j = 0; token[j] != '\0' && pos < sizeof(sector); ++j)
            sector[pos++] = (uint8_t) token[j];
    }

    if (!disk.write_sectors(lba, 1, sector))
    {
        fail(out, "ata: write failed");
        return;
    }

    out.print("ata: wrote ");
    out.print_uint(pos);
    out.print(" bytes to sector ");
    out.print_uint(lba);
    out.newline();
}

} // namespace


bool ata_command(const Args& args, Console& out)
{
    if (args.count == 0)
        return false;

    if (strcmp(args.argv[0], "ata-read") == 0)
    {
        read(args, out);
        return true;
    }

    if (strcmp(args.argv[0], "ata-write") == 0)
    {
        write(args, out);
        return true;
    }

    return false;
}
