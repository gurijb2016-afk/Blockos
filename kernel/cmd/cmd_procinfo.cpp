#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int procinfo_main(int argc, char** argv) {
    std::cout << "BlockOS procinfo";
    for (int i = 0; i < argc; ++i) {
        std::cout << (i ? " " : ": ") << (argv[i] ? argv[i] : "");
    }
    std::cout << '\n';
    return 0;
}
} // namespace blockos::cmd
