#include "command.hpp"

#include <string.h>

#include "fs/fat32.hpp"
#include "kernel/console.hpp"
#include "kernel/shell.hpp"


namespace blockos::cmd
{
extern "C" int cd_main(const Args& args, Console& console)
{
    if (args.count < 2)
    {
        console.print("Usage: cd <directory_name>\n");
        return 1;
    }

    const char* dir_name = args.at(1);

    // Handled here because to_short_name rejects "." so resolve_path never matches it
    if (strcmp(dir_name, ".") == 0)
    {
        console.print("\n");
        return 0;
    }

    if (strcmp(dir_name, "..") == 0)
    {
        char path[256];
        strcpy(path, console.get_current_directory());

        char* last = strrchr(path, '/');

        if (last == nullptr || last == path)
        {
            path[0] = '/';
            path[1] = '\0';
        }
        else
        {
            *last = '\0';
        }

        console.set_current_directory(path);
        console.print("\n");

        return 0;
    }

    char path[CMD_PATH_MAX];

    if (!resolve_arg(console, dir_name, path, sizeof(path)))
    {
        console.print("cd: path too long\n");
        return 1;
    }

    if (legacy_fat32_fs.is_valid_path(path))
    {
        console.set_current_directory(path);
        console.print("\n");
        return 0;
    }
    else
    {
        console.print("Directory does not exist: ");
        console.print(dir_name);
        console.print("\n");
        return 1;
    }
}
} // namespace blockos::cmd
