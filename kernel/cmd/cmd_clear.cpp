#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int clear_main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "\033[2J\033[H";
    return 0;
}
} // namespace blockos::cmd
