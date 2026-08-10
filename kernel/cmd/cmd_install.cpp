#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int install_main(int argc, char** argv) {
    if (argc == 0) {
        std::cout << "BlockOS installer: choose a target path or disk.\n";
        std::cout << "Usage: install <target>\n";
        return 0;
    }
    std::cout << "BlockOS installer target: " << argv[0] << '\n';
    std::cout << "Would copy blockos.img contents here after confirmation.\n";
    return 0;
}
} // namespace blockos::cmd
