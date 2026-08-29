#include "command.hpp"

#include "fs/fat32.hpp"
#include "print_format.hpp"

namespace blockos
{
namespace cmd
{

namespace
{

constexpr size_t CAT_BUFFER_MAX = 64 * 1024;

uint8_t file_buffer[CAT_BUFFER_MAX];

} // namespace

extern "C" int cat_main(const Args& args, Console& out)
{
    if (args.count < 2)
    {
        out.print("usage: cat <name>");
        out.newline();

        return 1;
    }

    if (!legacy_fat32_fs.ready())
    {
        out.print("cat: no FAT32 volume mounted");
        out.newline();

        return 1;
    }

    char path[CMD_PATH_MAX];

    if (!resolve_arg(out, args.at(1), path, sizeof(path)))
    {
        out.print("cat: path too long");
        out.newline();

        return 1;
    }

    size_t bytes_read = 0;

    if (!legacy_fat32_fs.read_file(path, file_buffer, &bytes_read))
    {
        out.print("cat: cannot read ");
        out.print(args.at(1));
        out.newline();

        return 1;
    }

    if (bytes_read == 0)
    {
        out.print("cat: ");
        out.print(args.at(1));
        out.print(" is empty");
        out.newline();

        return 0;
    }

    hexdump(file_buffer, bytes_read, 0);

    out.newline();

    return 0;
}

} // namespace cmd
} // namespace blockos
