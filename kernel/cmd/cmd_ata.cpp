#include "command.hpp"

#include <libc/include/stdio.h>
#include <string.h>

#include "ata_devices.hpp"
#include "ata_pio.hpp"

namespace
{

uint8_t sector[AtaPio::SECTOR_SIZE];

AtaPio& disk()
{
    return ata_data_disk();
}

bool ready(Console& out)
{
    if (disk().present())
        return true;

    out.print("ata: device not initialized\n");

    return false;
}


void fail(Console& out, const char* what)
{
    out.print(what);
    out.print(": ");
    out.print(AtaPio::error_name(disk().error_at(0)));
    out.newline();
}


void hexdump(const uint8_t* data, size_t output_length, uint32_t offset = 0)
{
    const size_t hex_per_row = 16;

    for (size_t i = 0; i < output_length - offset; i += hex_per_row)
    {
        printf("%08X", (unsigned int) (offset + i));

        for (size_t j = 0; j < hex_per_row && i + j < output_length; ++j)
        {
            printf(" %02X", (unsigned int) data[i + j + offset]);
        }

        printf(" ");
        for (size_t j = 0; j < hex_per_row && i + j < output_length; ++j)
        {
            const char c = (char) data[i + j + offset];
            (c >= 32 && c < 127) ? printf("%c", c) : printf(".");
        }

        printf("\n");
    }
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

    if (!disk().read_sectors(lba, 1, sector))
    {
        fail(out, "ata: read failed");
        return;
    }

    hexdump(sector, sizeof(sector), 0);

    out.newline();
}

void write(const Args& args, Console& out)
{
    if (!ready(out))
        return;

    uint32_t lba = 0;

    // Get second argument (<lba>) and if present convert it to an unsigned integer, else return
    if (!args.uint(1, &lba) || args.count < 3)
    {
        out.print("usage: ata-write <lba> <text>\n");
        return;
    }

    // zero initialize sector
    memset(sector, 0, sizeof(sector));

    size_t pos = 0;

    // Parse command as string, but output raw binary
    for (size_t i = 2; i < args.count && pos < sizeof(sector); ++i)
    {
        if (i > 2)
            sector[pos++] = ' ';

        const char* token = args.argv[i];

        for (size_t j = 0; token[j] != '\0' && pos < sizeof(sector); ++j)
            sector[pos++] = (uint8_t) token[j];
    }

    if (!disk().write_sectors(lba, 1, sector))
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


namespace blockos::cmd
{

extern "C" int ata_read_main(const Args& args, Console& out)
{
    read(args, out);

    return 0;
}

extern "C" int ata_write_main(const Args& args, Console& out)
{
    write(args, out);

    return 0;
}

} // namespace blockos::cmd
