#pragma once

#include <cstddef>
#include <cstdint>

namespace vfs {

enum NodeType : uint8_t {
    NODE_FILE = 0,
    NODE_DIRECTORY,
    NODE_DEVICE
};

enum DeviceType : uint8_t {
    DEVICE_GENERIC = 0,
    DEVICE_DISK,
    DEVICE_NETWORK,
    DEVICE_USB,
    DEVICE_GPU,
    DEVICE_INPUT,
    DEVICE_AUDIO,
    DEVICE_CONSOLE
};

struct DeviceNodeInfo {
    DeviceType type;
    uint32_t device_id;
    uint64_t base;
    uint64_t size;
    uint8_t irq;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor;
    uint16_t device;
};

size_t count_files();
const char* name_at(size_t idx);

const uint8_t* read_file(
    const char* name,
    uint32_t* out_size
);

bool create_file(
    const char* name,
    const uint8_t* data,
    uint32_t size
);

bool write_file(
    const char* name,
    const uint8_t* data,
    uint32_t size
);

bool exists(const char* path);
bool is_directory(const char* path);
bool is_device(const char* path);

bool create_directory(const char* path);

bool create_device_node(
    const char* path,
    const DeviceNodeInfo& info
);

bool remove_device_node(
    const char* path
);

bool get_device_info(
    const char* path,
    DeviceNodeInfo* out
);

uint32_t device_count();

const char* device_name_at(
    uint32_t index
);

const DeviceNodeInfo* device_info_at(
    uint32_t index
);

bool register_disk(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
);

bool register_network_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
);

bool register_usb_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
);

bool register_gpu_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
);

bool register_input_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
);

bool register_audio_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
);

void initialize_devices();

}

void vfs_init_from_ramfs();
