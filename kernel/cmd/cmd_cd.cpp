#include "command.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
namespace blockos::cmd {
extern "C" int cd_main(int argc, char** argv) {
    if (argc < 1) {
        std::cerr << "cd: missing operand\n";
        return 1;
    }
    std::error_code ec;
    fs::current_path(argv[0], ec);
    if (ec) {
        std::cerr << "cd: " << ec.message() << '\n';
        return 1;
    }
    return 0;
}
} // namespace blockos::cmd
