#include "freebsd_syscall_metadata.hpp"
#include <cassert>
#include <cstdint>
int main() {
    using blockos::freebsd_abi::lookup;
    using blockos::freebsd_abi::count;
    assert(count() == 548);
    const auto* zero = lookup(0);
    assert(zero && zero->number == 0 && zero->name);
    const auto* read = lookup(3);
    assert(read && read->number == 3);
    assert(lookup(605) != nullptr);
    assert(lookup(606) == nullptr);
    assert(lookup(65535) == nullptr);
}
