#include "vfs.hpp"

#include "kernel/allocator.hpp"
#include "libc/include/string.h"
#include "ramfs.hpp"

struct vfs_entry
{
    char* name;
    uint8_t* data;
    uint32_t size;
    NodeType type;
    DeviceNodeInfo device_info;
    bool has_device_info;
    vfs_entry* next;
};

static vfs_entry* vfs_root = nullptr;

static uint32_t disk_counter = 0;
static uint32_t network_counter = 0;
static uint32_t usb_counter = 0;
static uint32_t gpu_counter = 0;
static uint32_t input_counter = 0;
static uint32_t audio_counter = 0;

static bool string_equal(
    const char* a,
    const char* b
)
{
    if (!a || !b)
        return false;

    return strcmp(a, b) == 0;
}

static vfs_entry* find_entry(
    const char* name
)
{
    if (!name)
        return nullptr;

    for (vfs_entry* e = vfs_root;
         e;
         e = e->next)
    {
        if (string_equal(e->name, name))
            return e;
    }

    return nullptr;
}

static bool make_path(
    char* out,
    size_t out_size,
    const char* prefix,
    uint32_t number
)
{
    if (!out || !prefix || out_size < 2)
        return false;

    size_t p = 0;

    while (prefix[p] != '\0') {
        if (p + 1 >= out_size)
            return false;

        out[p] = prefix[p];
        ++p;
    }

    char digits[16];
    size_t d = 0;

    if (number == 0) {
        digits[d++] = '0';
    } else {
        while (number > 0 && d < sizeof(digits)) {
            digits[d++] =
                (char)('0' + (number % 10));
            number /= 10;
        }
    }

    while (d > 0) {
        if (p + 1 >= out_size)
            return false;

        out[p++] = digits[--d];
    }

    out[p] = '\0';

    return true;
}

static bool register_device_internal(
    const char* path,
    DeviceType type,
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
)
{
    DeviceNodeInfo info{};

    info.type = type;
    info.device_id = 0;
    info.base = base;
    info.size = size;
    info.irq = irq;
    info.bus = bus;
    info.slot = slot;
    info.function = function;
    info.vendor = vendor;
    info.device = device;

    return vfs::create_device_node(
        path,
        info
    );
}

size_t vfs::count_files()
{
    size_t cnt = 0;

    for (vfs_entry* e = vfs_root;
         e;
         e = e->next)
    {
        if (e->type != NODE_DIRECTORY)
            ++cnt;
    }

    return cnt;
}

const char* vfs::name_at(size_t idx)
{
    size_t i = 0;

    for (vfs_entry* e = vfs_root;
         e;
         e = e->next)
    {
        if (e->type == NODE_DIRECTORY)
            continue;

        if (i == idx)
            return e->name;

        ++i;
    }

    return nullptr;
}

const uint8_t* vfs::read_file(
    const char* name,
    uint32_t* out_size
)
{
    vfs_entry* e = find_entry(name);

    if (!e)
        return nullptr;

    if (e->type == NODE_DIRECTORY)
        return nullptr;

    if (e->type == NODE_DEVICE)
        return nullptr;

    if (out_size)
        *out_size = e->size;

    return e->data;
}

bool vfs::exists(
    const char* path
)
{
    return find_entry(path) != nullptr;
}

bool vfs::is_directory(
    const char* path
)
{
    vfs_entry* e = find_entry(path);

    if (!e)
        return false;

    return e->type == NODE_DIRECTORY;
}

bool vfs::is_device(
    const char* path
)
{
    vfs_entry* e = find_entry(path);

    if (!e)
        return false;

    return e->type == NODE_DEVICE;
}

bool vfs::create_directory(
    const char* path
)
{
    if (!path || path[0] == '\0')
        return false;

    if (find_entry(path))
        return true;

    size_t len = strlen(path) + 1;

    vfs_entry* e =
        (vfs_entry*)allocator::alloc(
            sizeof(vfs_entry)
        );

    if (!e)
        return false;

    char* name =
        (char*)allocator::alloc(len);

    if (!name)
        return false;

    memcpy(name, path, len);

    e->name = name;
    e->data = nullptr;
    e->size = 0;
    e->type = NODE_DIRECTORY;
    e->has_device_info = false;
    e->next = vfs_root;

    vfs_root = e;

    return true;
}

bool vfs::create_file(
    const char* name,
    const uint8_t* data,
    uint32_t size
)
{
    if (!name)
        return false;

    if (find_entry(name))
        return false;

    size_t name_len =
        strlen(name) + 1;

    vfs_entry* e =
        (vfs_entry*)allocator::alloc(
            sizeof(vfs_entry)
        );

    if (!e)
        return false;

    char* n =
        (char*)allocator::alloc(
            name_len
        );

    if (!n)
        return false;

    memcpy(n, name, name_len);

    uint8_t* d = nullptr;

    if (size > 0) {
        d =
            (uint8_t*)allocator::alloc(size);

        if (!d)
            return false;

        if (data)
            memcpy(d, data, size);
        else
            memset(d, 0, size);
    }

    e->name = n;
    e->data = d;
    e->size = size;
    e->type = NODE_FILE;
    e->has_device_info = false;
    e->next = vfs_root;

    vfs_root = e;

    return true;
}

bool vfs::write_file(
    const char* name,
    const uint8_t* data,
    uint32_t size
)
{
    vfs_entry* e = find_entry(name);

    if (!e)
        return create_file(
            name,
            data,
            size
        );

    if (e->type != NODE_FILE)
        return false;

    uint8_t* d = nullptr;

    if (size > 0) {
        d =
            (uint8_t*)allocator::alloc(size);

        if (!d)
            return false;

        if (data)
            memcpy(d, data, size);
        else
            memset(d, 0, size);
    }

    e->data = d;
    e->size = size;

    return true;
}

bool vfs::create_device_node(
    const char* path,
    const DeviceNodeInfo& info
)
{
    if (!path)
        return false;

    if (find_entry(path))
        return false;

    size_t len = strlen(path) + 1;

    vfs_entry* e =
        (vfs_entry*)allocator::alloc(
            sizeof(vfs_entry)
        );

    if (!e)
        return false;

    char* name =
        (char*)allocator::alloc(len);

    if (!name)
        return false;

    memcpy(name, path, len);

    e->name = name;
    e->data = nullptr;
    e->size = 0;
    e->type = NODE_DEVICE;
    e->device_info = info;
    e->has_device_info = true;
    e->next = vfs_root;

    vfs_root = e;

    return true;
}

bool vfs::remove_device_node(
    const char* path
)
{
    if (!path)
        return false;

    vfs_entry* previous = nullptr;
    vfs_entry* current = vfs_root;

    while (current) {
        if (strcmp(current->name, path) == 0) {

            if (current->type != NODE_DEVICE)
                return false;

            if (previous)
                previous->next =
                    current->next;
            else
                vfs_root =
                    current->next;

            return true;
        }

        previous = current;
        current = current->next;
    }

    return false;
}

bool vfs::get_device_info(
    const char* path,
    DeviceNodeInfo* out
)
{
    if (!path || !out)
        return false;

    vfs_entry* e = find_entry(path);

    if (!e)
        return false;

    if (e->type != NODE_DEVICE)
        return false;

    if (!e->has_device_info)
        return false;

    *out = e->device_info;

    return true;
}

uint32_t vfs::device_count()
{
    uint32_t count = 0;

    for (vfs_entry* e = vfs_root;
         e;
         e = e->next)
    {
        if (e->type == NODE_DEVICE)
            ++count;
    }

    return count;
}

const char* vfs::device_name_at(
    uint32_t index
)
{
    uint32_t current = 0;

    for (vfs_entry* e = vfs_root;
         e;
         e = e->next)
    {
        if (e->type != NODE_DEVICE)
            continue;

        if (current == index)
            return e->name;

        ++current;
    }

    return nullptr;
}

const DeviceNodeInfo* vfs::device_info_at(
    uint32_t index
)
{
    uint32_t current = 0;

    for (vfs_entry* e = vfs_root;
         e;
         e = e->next)
    {
        if (e->type != NODE_DEVICE)
            continue;

        if (current == index &&
            e->has_device_info)
            return &e->device_info;

        ++current;
    }

    return nullptr;
}

bool vfs::register_disk(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
)
{
    create_directory("/devices");

    char path[64];

    if (!make_path(
            path,
            sizeof(path),
            "/devices/disk",
            disk_counter))
        return false;

    DeviceNodeInfo info{};

    info.type = DEVICE_DISK;
    info.device_id = disk_counter;
    info.base = base;
    info.size = size;
    info.irq = irq;
    info.bus = bus;
    info.slot = slot;
    info.function = function;
    info.vendor = vendor;
    info.device = device;

    if (!create_device_node(
            path,
            info))
        return false;

    ++disk_counter;

    return true;
}

bool vfs::register_network_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
)
{
    create_directory("/devices");

    char path[64];

    if (!make_path(
            path,
            sizeof(path),
            "/devices/network",
            network_counter))
        return false;

    if (!register_device_internal(
            path,
            DEVICE_NETWORK,
            base,
            size,
            irq,
            bus,
            slot,
            function,
            vendor,
            device))
        return false;

    ++network_counter;

    return true;
}

bool vfs::register_usb_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
)
{
    create_directory("/devices");

    char path[64];

    if (!make_path(
            path,
            sizeof(path),
            "/devices/usb",
            usb_counter))
        return false;

    if (!register_device_internal(
            path,
            DEVICE_USB,
            base,
            size,
            irq,
            bus,
            slot,
            function,
            vendor,
            device))
        return false;

    ++usb_counter;

    return true;
}

bool vfs::register_gpu_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
)
{
    create_directory("/devices");

    char path[64];

    if (!make_path(
            path,
            sizeof(path),
            "/devices/gpu",
            gpu_counter))
        return false;

    if (!register_device_internal(
            path,
            DEVICE_GPU,
            base,
            size,
            irq,
            bus,
            slot,
            function,
            vendor,
            device))
        return false;

    ++gpu_counter;

    return true;
}

bool vfs::register_input_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
)
{
    create_directory("/devices");

    char path[64];

    if (!make_path(
            path,
            sizeof(path),
            "/devices/input",
            input_counter))
        return false;

    if (!register_device_internal(
            path,
            DEVICE_INPUT,
            base,
            size,
            irq,
            bus,
            slot,
            function,
            vendor,
            device))
        return false;

    ++input_counter;

    return true;
}

bool vfs::register_audio_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device
)
{
    create_directory("/devices");

    char path[64];

    if (!make_path(
            path,
            sizeof(path),
            "/devices/audio",
            audio_counter))
        return false;

    if (!register_device_internal(
            path,
            DEVICE_AUDIO,
            base,
            size,
            irq,
            bus,
            slot,
            function,
            vendor,
            device))
        return false;

    ++audio_counter;

    return true;
}

void vfs::initialize_devices()
{
    create_directory("/devices");
}

void vfs_init_from_ramfs()
{
    vfs::initialize_devices();

    const struct ramfile* it =
        __ramfs_start;

    while (it < __ramfs_end)
    {
        vfs::create_file(
            it->name,
            it->data,
            it->size
        );

        ++it;
    }
}
