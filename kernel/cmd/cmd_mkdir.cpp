#include "command.hpp"
#include "fs/fat32.hpp"
#include "kernel/console.hpp"
#include "kernel/shell.hpp"

namespace blockos::cmd
{
extern "C" int mkdir_main(const Args& args, Console& console)
{
    if (args.count < 2)
    {
        console.print("Usage: mkdir <directory_name>\n");
        return 1;
    }

    const char* dir_name = args.at(1);

    char path[CMD_PATH_MAX];

    if (!resolve_arg(console, dir_name, path, sizeof(path)))
    {
        console.print("mkdir: path too long\n");
        return 1;
    }

    if (!legacy_fat32_fs.create_directory(path))
    {
        console.print("Failed to create directory: ");
        console.print(dir_name);
        console.print("\n");
        return 1;
    }

    console.print("Directory created: ");
    console.print(dir_name);
    console.print("\n");
    return 0;
}
} // namespace blockos::cmd
