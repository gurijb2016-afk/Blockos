#include "command.hpp"

#include "fs/fat32.hpp"

namespace blockos
{
namespace cmd
{

extern "C" int ls_main(const Args& args, Console& out)
{
    (void)args;

    if (!legacy_fat32_fs.ready())
    {
        out.print("ls: no FAT32 volume mounted");
        out.newline();

        return 1;
    }

    legacy_fat32_fs.list_root_directories();

    return 0;
}

} // namespace cmd
} // namespace blockos
