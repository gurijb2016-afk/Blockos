#include <string.h>

#include "command.hpp"

#define DECLARE_CMD(name) extern "C" int name##_main(const Args& args, Console& out);

namespace blockos::cmd
{

DECLARE_CMD(clear)
DECLARE_CMD(help)
DECLARE_CMD(ls)
DECLARE_CMD(cat)
DECLARE_CMD(touch)
DECLARE_CMD(ata_read)
DECLARE_CMD(ata_write)
DECLARE_CMD(forth_enter)
DECLARE_CMD(mkdir)
DECLARE_CMD(cd)
DECLARE_CMD(uptime)

static const CommandEntry commands[] = {
    {"clear", clear_main, "Clear the console."},
    {"help", help_main, "List commands, or show help for one."},
    {"ls", ls_main, "List files in the VFS."},
    {"cat", cat_main, "Hex dump a file from the FAT32 volume."},
    {"touch", touch_main, "Create a zero byte file in the FAT32 root directory."},
    {"ata-read", ata_read_main, "Read sectors from the ATA disk."},
    {"ata-write", ata_write_main, "Write a sector to the ATA disk."},
    {"forth", forth_enter_main, "Enter the Forth interpreter."},
    {"mkdir", mkdir_main, "Create a directory in the FAT32 root directory."},
    {"cd", cd_main, "Change the current working directory."},
    {"uptime", uptime_main, "Show time elapsed since the timer started."}};

static constexpr size_t COMMAND_COUNT = sizeof(commands) / sizeof(commands[0]);

bool resolve_arg(const Console& console, const char* arg, char* out, size_t capacity)
{
    if (arg[0] == '/')
    {
        memcpy(out, arg, strlen(arg) + 1);
        return true;
    }

    const char* current = console.get_current_directory();
    size_t base = strlen(current);

    while (base > 0 && current[base - 1] == '/')
        base--;

    size_t len = strlen(arg);

    if (base + 1 + len >= capacity)
        return false;

    memcpy(out, current, base);
    out[base] = '/';
    memcpy(out + base + 1, arg, len);
    out[base + 1 + len] = '\0';

    return true;
}

size_t command_count()
{
    return COMMAND_COUNT;
}

const CommandEntry* command_at(size_t index)
{
    return index < COMMAND_COUNT ? &commands[index] : nullptr;
}

const CommandEntry* find_command(const char* name)
{
    if (!name)
        return nullptr;

    for (size_t i = 0; i < COMMAND_COUNT; ++i)
    {
        if (strcmp(commands[i].name, name) == 0)
            return &commands[i];
    }

    return nullptr;
}

bool run_registered(const Args& args, Console& out, int* status)
{
    if (args.count == 0)
        return false;

    const CommandEntry* entry = find_command(args.argv[0]);

    if (!entry)
        return false;

    const int result = entry->fn(args, out);

    if (status)
        *status = result;

    return true;
}

} // namespace blockos::cmd
