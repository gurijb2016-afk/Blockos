#include "tls.hpp"

namespace tls {

uint64_t read_fs_base() {
    uint32_t lo = 0, hi = 0;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(IA32_FS_BASE));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

void write_fs_base(uint64_t value) {
    uint32_t lo = static_cast<uint32_t>(value);
    uint32_t hi = static_cast<uint32_t>(value >> 32);
    asm volatile("wrmsr" :: "c"(IA32_FS_BASE), "a"(lo), "d"(hi));
}

void activate_for_process(uint64_t fs_base) {
    write_fs_base(fs_base);
}

}
