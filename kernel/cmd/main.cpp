#include "command.hpp"
#include <iostream>
#include <string_view>

namespace blockos::cmd {
int run_command(std::string_view name, int argc, char** argv);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "BlockOS command shell\nUsage: blockos <command> [args...]\nTry: blockos help\n";
        return 0;
    }
    return blockos::cmd::run_command(argv[1], argc - 2, argv + 2);
}
