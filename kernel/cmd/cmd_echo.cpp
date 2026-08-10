#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int echo_main(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        if (i) std::cout << ' ';
        std::cout << (argv[i] ? argv[i] : "");
    }
    std::cout << '\n';
    return 0;
}
} // namespace blockos::cmd
