#include "command.hpp"
#include <iostream>
#include <string_view>

namespace blockos::cmd {
const CommandEntry* find_command(std::string_view name);

extern "C" int help_main(int argc, char** argv) {
    if (argc == 0) {
        std::cout << "BlockOS help\nTry: help <command>\n";
        return 0;
    }
    if (const auto* c = find_command(argv[0])) return print_usage(c->name, c->help);
    std::cout << "No help found for: " << argv[0] << '\n';
    return 1;
}
} // namespace blockos::cmd
