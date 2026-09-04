#pragma once

#include <cstddef>
#include <cstdint>

namespace vfs {

enum class NodeType : std::uint8_t {
    RegularFile = 0,
    Device      = 1
};

struct NodeInfo {
    NodeType type;
    std::uint32_t size;
};

size_t count_files();

const char* name_at(size_t idx);

const std::uint8_t* read_file(
    const char* name,
    std::uint32_t* out_size
);

bool exists(const char* name);

bool stat_node(
    const char* name,
    NodeInfo* out_info
);

bool create_file(
    const char* name,
    const std::uint8_t* data,
    std::uint32_t size
);

bool create_regular_file(
    const char* name,
    const std::uint8_t* data,
    std::uint32_t size
);

bool create_device_node(
    const char* name
);

bool write_file(
    const char* name,
    const std::uint8_t* data,
    std::uint32_t size
);

bool is_device(
    const char* name
);

}
