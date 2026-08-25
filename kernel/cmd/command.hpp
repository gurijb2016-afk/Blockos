#pragma once

#include <stddef.h>

#include "console.hpp"
#include "shell.hpp"

namespace blockos::cmd
{

using CommandFn = int (*)(const Args& args, Console& out);

struct CommandEntry
{
    const char* name;
    CommandFn fn;
    const char* help;
};

const CommandEntry* find_command(const char* name);

const CommandEntry* command_at(size_t index);

size_t command_count();

bool run_registered(const Args& args, Console& out, int* status);

} // namespace blockos::cmd
