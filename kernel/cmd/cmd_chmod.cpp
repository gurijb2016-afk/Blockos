#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int chmod_main(int argc, char** argv) {
    std::cout << "BlockOS chmod";
    for (int i = 0; i < argc; ++i) {
        std::cout << (i ? " " : ": ") << (argv[i] ? argv[i] : "");
    }
    std::cout << '\n';
    return 0;
}
} // namespace blockos::cmd
