#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int update_main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "BlockOS: updating package lists\n";
    return 0;
}
} // namespace blockos::cmd
