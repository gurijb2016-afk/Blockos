#include "command.hpp"

#include <string.h>

#define DECLARE_CMD(name) extern "C" int name##_main(const Args& args, Console& out);

namespace blockos::cmd
{

DECLARE_CMD(clear)
DECLARE_CMD(help)
DECLARE_CMD(ls)
DECLARE_CMD(ata_read)
DECLARE_CMD(ata_write)
DECLARE_CMD(forth_enter)

static const CommandEntry commands[] = {
    {"clear", clear_main, "Clear the console."},
    {"help", help_main, "List commands, or show help for one."},
    {"ls", ls_main, "List files in the VFS."},
    {"ata-read", ata_read_main, "Read sectors from the ATA disk."},
    {"ata-write", ata_write_main, "Write a sector to the ATA disk."},
    {"forth", forth_enter_main, "Enter the Forth interpreter."},
};

static constexpr size_t COMMAND_COUNT = sizeof(commands) / sizeof(commands[0]);

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
