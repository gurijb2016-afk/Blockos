#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int shutdown_main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "BlockOS: shutdown requested\n";
    return 0;
}
} // namespace blockos::cmd
