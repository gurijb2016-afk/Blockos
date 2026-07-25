#pragma once

#include <cstddef>
#include <cstdint>

namespace blockos::network {

class NetworkBuffer {
public:
    static constexpr std::size_t CAPACITY = 2048;

    void clear();
    std::size_t size() const { return size_; }
    std::uint8_t* data() { return buf_; }
    const std::uint8_t* data() const { return buf_; }

private:
    std::uint8_t buf_[CAPACITY]{};
    std::size_t size_ = 0;
};

} // namespace blockos::network
