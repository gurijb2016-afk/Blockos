#pragma once
#include <cstdint>
#include <string_view>

namespace blockos::cmd {

using CommandFn = int (*)(int argc, char** argv);

struct CommandEntry {
    std::string_view name;
    CommandFn fn;
    std::string_view help;
};

int print_usage(std::string_view name, std::string_view help);
bool arg_equals(const char* s, std::string_view expected);
int parse_int(const char* s, int fallback = 0);

} // namespace blockos::cmd
