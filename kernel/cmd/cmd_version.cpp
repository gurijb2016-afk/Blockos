#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int version_main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "BlockOS version 0.1 command suite\n";
    return 0;
}
} // namespace blockos::cmd
