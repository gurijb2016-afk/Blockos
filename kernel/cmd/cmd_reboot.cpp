#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int reboot_main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "BlockOS: reboot requested\n";
    return 0;
}
} // namespace blockos::cmd
