#include "command.hpp"

#include "fs/fat32.hpp"

namespace blockos
{
namespace cmd
{

extern "C" int touch_main(const Args& args, Console& out)
{
    if (args.count < 2)
    {
        out.print("usage: touch <name>");
        out.newline();

        return 1;
    }

    if (!legacy_fat32_fs.ready())
    {
        out.print("touch: no FAT32 volume mounted");
        out.newline();

        return 1;
    }

    char path[CMD_PATH_MAX];

    if (!resolve_arg(out, args.at(1), path, sizeof(path)))
    {
        out.print("touch: path too long");
        out.newline();

        return 1;
    }

    if (!legacy_fat32_fs.write_file(path, nullptr, 0))
    {
        out.print("touch: cannot create ");
        out.print(args.at(1));
        out.newline();

        return 1;
    }

    return 0;
}

} // namespace cmd
} // namespace blockos
