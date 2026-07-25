#pragma once

#include <cstdint>
#include <cstddef>
#include "syscall_numbers.hpp"

namespace blockos::syscall {

struct SyscallContext;
struct SyscallHooks;

class SyscallDispatcher {
public:
    static std::int64_t dispatch(std::uint64_t nr,
                                 std::uint64_t a1,
                                 std::uint64_t a2,
                                 std::uint64_t a3,
                                 std::uint64_t a4,
                                 std::uint64_t a5,
                                 std::uint64_t a6,
                                 SyscallContext& ctx,
                                 const SyscallHooks& hooks);
};

}
