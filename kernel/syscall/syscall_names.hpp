#pragma once

#include <cstdint>
#include <cstddef>

namespace blockos::syscall {
const char* syscall_name(std::uint64_t nr);
std::size_t syscall_count();
}
