#include "command.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
namespace blockos::cmd {
extern "C" int pwd_main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << fs::current_path().string() << '\n';
    return 0;
}
} // namespace blockos::cmd
