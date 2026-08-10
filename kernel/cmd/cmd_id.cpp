#include "command.hpp"
#include <iostream>

namespace blockos::cmd {
extern "C" int id_main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "uid=0(root) gid=0(root) groups=0(root)\n";
    return 0;
}
} // namespace blockos::cmd
