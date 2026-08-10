#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int whoami_main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "root\n";
    return 0;
}
} // namespace blockos::cmd
