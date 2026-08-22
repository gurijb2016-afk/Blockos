#pragma once

#include "console.hpp"
#include "shell.hpp"

// Returns false if args is not one of the ata-* commands
bool ata_command(const Args& args, Console& out);
