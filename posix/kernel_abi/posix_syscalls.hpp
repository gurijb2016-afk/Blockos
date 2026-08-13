#pragma once
#include <stdint.h>
#include "../include/internal/syscall.h"
namespace blockos::kernel {
using syscall_handler_t = int64_t (*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
struct SyscallTable {
    syscall_handler_t handlers[128]{};
    void install(blockos::abi::Number n, syscall_handler_t h) { handlers[(uint64_t)n] = h; }
    int64_t dispatch(uint64_t n,uint64_t a0,uint64_t a1,uint64_t a2,uint64_t a3,uint64_t a4,uint64_t a5) const {
        if (n >= 128 || !handlers[n]) return -38;
        return handlers[n](a0,a1,a2,a3,a4,a5);
    }
};
}
