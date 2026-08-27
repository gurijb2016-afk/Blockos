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

constexpr size_t CMD_PATH_MAX = 256;

// Makes a command argument absolute: relative names resolve against the current directory
bool resolve_arg(const Console& console, const char* arg, char* out, size_t capacity);

const CommandEntry* find_command(const char* name);

const CommandEntry* command_at(size_t index);

size_t command_count();

bool run_registered(const Args& args, Console& out, int* status);

} // namespace blockos::cmd
