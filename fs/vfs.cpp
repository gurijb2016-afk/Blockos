#include "vfs.hpp"

#include "kernel/allocator.hpp"
#include "libc/include/string.h"
#include "ramfs.hpp"

#include <stddef.h>
#include <stdint.h>

namespace
{

constexpr size_t VFS_MAX_PATH = 256;

struct vfs_entry
{
    char* name;
    uint8_t* data;
    uint32_t size;

    vfs::NodeType type;

    vfs::DeviceNodeInfo device_info;
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

static size_t string_length(const char* str)
{
    if (!str)
        return 0;

    size_t len = 0;

    while (str[len] != '\0')
        ++len;

    return len;
}

static bool string_equal(const char* a, const char* b)
{
    if (!a || !b)
        return false;

    return strcmp(a, b) == 0;
}

static bool normalize_path(
    const char* input,
    char* output,
    size_t output_size)
{
    if (!input || !output || output_size < 2)
        return false;

    size_t out = 0;
    bool previous_slash = false;

    if (input[0] != '/')
    {
        output[out++] = '/';
        previous_slash = true;
    }

    for (size_t i = 0; input[i] != '\0'; ++i)
    {
        char c = input[i];

        if (c == '/')
        {
            if (previous_slash)
                continue;

            if (out + 1 >= output_size)
                return false;

            output[out++] = '/';
            previous_slash = true;
            continue;
        }

        if (out + 1 >= output_size)
            return false;

        output[out++] = c;
        previous_slash = false;
    }

    if (out == 0)
        output[out++] = '/';

    while (out > 1 && output[out - 1] == '/')
        --out;

    output[out] = '\0';

    return true;
}

static vfs_entry* find_entry(const char* path)
{
    if (!path)
        return nullptr;

    char normalized[VFS_MAX_PATH];

    if (!normalize_path(
            path,
            normalized,
            sizeof(normalized)))
    {
        return nullptr;
    }

    vfs_entry* current = vfs_root;

    while (current)
    {
        if (current->name &&
            string_equal(current->name, normalized))
        {
            return current;
        }

        current = current->next;
    }

    return nullptr;
}

static bool parent_exists(const char* path)
{
    if (!path)
        return false;

    char normalized[VFS_MAX_PATH];

    if (!normalize_path(
            path,
            normalized,
            sizeof(normalized)))
    {
        return false;
    }

    if (string_equal(normalized, "/"))
        return true;

    size_t len = string_length(normalized);
    size_t slash = len;

    while (slash > 0)
    {
        --slash;

        if (normalized[slash] == '/')
            break;
    }

    if (slash == 0)
        return find_entry("/") != nullptr;

    char parent[VFS_MAX_PATH];

    if (slash >= sizeof(parent))
        return false;

    for (size_t i = 0; i < slash; ++i)
        parent[i] = normalized[i];

    parent[slash] = '\0';

    return find_entry(parent) != nullptr;
}

static vfs_entry* allocate_entry(
    const char* path,
    vfs::NodeType type)
{
    if (!path)
        return nullptr;

    char normalized[VFS_MAX_PATH];

    if (!normalize_path(
            path,
            normalized,
            sizeof(normalized)))
    {
        return nullptr;
    }

    if (find_entry(normalized))
        return nullptr;

    if (!string_equal(normalized, "/"))
    {
        if (!parent_exists(normalized))
            return nullptr;
    }

    size_t name_len = string_length(normalized);

    char* name =
        static_cast<char*>(allocator::alloc(name_len + 1));

    if (!name)
        return nullptr;

    for (size_t i = 0; i <= name_len; ++i)
        name[i] = normalized[i];

    vfs_entry* entry =
        static_cast<vfs_entry*>(allocator::alloc(sizeof(vfs_entry)));

    if (!entry)
    {
        allocator::free(name);
        return nullptr;
    }

    entry->name = name;
    entry->data = nullptr;
    entry->size = 0;
    entry->type = type;
    entry->device_info = {};
    entry->has_device_info = false;
    entry->next = nullptr;

    if (!vfs_root)
    {
        vfs_root = entry;
    }
    else
    {
        vfs_entry* current = vfs_root;

        while (current->next)
            current = current->next;

        current->next = entry;
    }

    return entry;
}

static void free_entry(vfs_entry* entry)
{
    if (!entry)
        return;

    if (entry->data)
    {
        allocator::free(entry->data);
        entry->data = nullptr;
    }

    if (entry->name)
    {
        allocator::free(entry->name);
        entry->name = nullptr;
    }

    allocator::free(entry);
}

static bool make_device_path(
    const char* directory,
    uint32_t number,
    char* output,
    size_t output_size)
{
    if (!directory || !output || output_size == 0)
        return false;

    char number_buffer[16];
    size_t number_length = 0;

    if (number == 0)
    {
        number_buffer[number_length++] = '0';
    }
    else
    {
        char reverse[16];
        size_t reverse_length = 0;

        uint32_t value = number;

        while (value > 0 &&
               reverse_length < sizeof(reverse))
        {
            reverse[reverse_length++] =
                static_cast<char>('0' + (value % 10));

            value /= 10;
        }

        while (reverse_length > 0)
        {
            number_buffer[number_length++] =
                reverse[--reverse_length];
        }
    }

    number_buffer[number_length] = '\0';

    size_t directory_length =
        string_length(directory);

    if (directory_length +
            number_length +
            2 >
        output_size)
    {
        return false;
    }

    size_t pos = 0;

    for (size_t i = 0; i < directory_length; ++i)
        output[pos++] = directory[i];

    if (pos == 0 || output[pos - 1] != '/')
        output[pos++] = '/';

    for (size_t i = 0; i < number_length; ++i)
        output[pos++] = number_buffer[i];

    output[pos] = '\0';

    return true;
}

static bool register_device(
    vfs::DeviceType type,
    const char* directory,
    uint32_t* counter,
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device)
{
    if (!directory || !counter)
        return false;

    if (!vfs::exists("/devices"))
    {
        if (!vfs::create_directory("/devices"))
            return false;
    }

    if (!vfs::exists(directory))
    {
        if (!vfs::create_directory(directory))
            return false;
    }

    char path[VFS_MAX_PATH];

    if (!make_device_path(
            directory,
            *counter,
            path,
            sizeof(path)))
    {
        return false;
    }

    while (vfs::exists(path))
    {
        ++(*counter);

        if (!make_device_path(
                directory,
                *counter,
                path,
                sizeof(path)))
        {
            return false;
        }
    }

    vfs::DeviceNodeInfo info{};

    info.type = type;
    info.device_id = *counter;

    info.base = base;
    info.size = size;

    info.irq = irq;
    info.bus = bus;
    info.slot = slot;
    info.function = function;

    info.vendor = vendor;
    info.device = device;

    if (!vfs::create_device_node(path, info))
        return false;

    ++(*counter);

    return true;
}

} // namespace

namespace vfs
{

size_t count_files()
{
    size_t count = 0;

    vfs_entry* current = vfs_root;

    while (current)
    {
        if (current->type == NODE_FILE)
            ++count;

        current = current->next;
    }

    return count;
}

const char* name_at(size_t idx)
{
    size_t index = 0;

    vfs_entry* current = vfs_root;

    while (current)
    {
        if (current->type == NODE_FILE)
        {
            if (index == idx)
                return current->name;

            ++index;
        }

        current = current->next;
    }

    return nullptr;
}

const uint8_t* read_file(
    const char* name,
    uint32_t* out_size)
{
    if (out_size)
        *out_size = 0;

    if (!name)
        return nullptr;

    vfs_entry* entry = find_entry(name);

    if (!entry)
        return nullptr;

    if (entry->type != NODE_FILE)
        return nullptr;

    if (out_size)
        *out_size = entry->size;

    return entry->data;
}

bool create_file(
    const char* name,
    const uint8_t* data,
    uint32_t size)
{
    if (!name)
        return false;

    if (size > 0 && !data)
        return false;

    if (find_entry(name))
        return false;

    vfs_entry* entry =
        allocate_entry(name, NODE_FILE);

    if (!entry)
        return false;

    if (size > 0)
    {
        entry->data =
            static_cast<uint8_t*>(allocator::alloc(size));

        if (!entry->data)
        {
            vfs_entry* current = vfs_root;
            vfs_entry* previous = nullptr;

            while (current &&
                   current != entry)
            {
                previous = current;
                current = current->next;
            }

            if (current == entry)
            {
                if (previous)
                    previous->next = entry->next;
                else
                    vfs_root = entry->next;
            }

            free_entry(entry);

            return false;
        }

        for (uint32_t i = 0; i < size; ++i)
            entry->data[i] = data[i];
    }

    entry->size = size;

    return true;
}

bool write_file(
    const char* name,
    const uint8_t* data,
    uint32_t size)
{
    if (!name)
        return false;

    if (size > 0 && !data)
        return false;

    vfs_entry* entry = find_entry(name);

    if (!entry)
        return create_file(name, data, size);

    if (entry->type != NODE_FILE)
        return false;

    uint8_t* new_data = nullptr;

    if (size > 0)
    {
        new_data =
            static_cast<uint8_t*>(allocator::alloc(size));

        if (!new_data)
            return false;

        for (uint32_t i = 0; i < size; ++i)
            new_data[i] = data[i];
    }

    if (entry->data)
        allocator::free(entry->data);

    entry->data = new_data;
    entry->size = size;

    return true;
}

bool exists(const char* path)
{
    return find_entry(path) != nullptr;
}

bool is_directory(const char* path)
{
    vfs_entry* entry = find_entry(path);

    if (!entry)
        return false;

    return entry->type == NODE_DIRECTORY;
}

bool is_device(const char* path)
{
    vfs_entry* entry = find_entry(path);

    if (!entry)
        return false;

    return entry->type == NODE_DEVICE;
}

bool create_directory(const char* path)
{
    if (!path)
        return false;

    if (find_entry(path))
        return false;

    return allocate_entry(
        path,
        NODE_DIRECTORY) != nullptr;
}

size_t directory_entry_count(const char* directory)
{
    if (!directory || !is_directory(directory))
        return 0;
    char prefix[VFS_MAX_PATH];
    if (!normalize_path(directory, prefix, sizeof(prefix)))
        return 0;
    size_t plen = string_length(prefix);
    if (plen > 1 && prefix[plen - 1] == '/')
        prefix[--plen] = 0;
    size_t count = 0;
    for (vfs_entry* e = vfs_root; e; e = e->next) {
        if (!e->name || e->name[0] != '/') continue;
        if (string_equal(e->name, prefix)) continue;
        if (strncmp(e->name, prefix, plen) != 0) continue;
        if (prefix[0] == '/' && prefix[1] == 0) {
            if (e->name[1] == 0) continue;
        } else if (e->name[plen] != '/') continue;
        const char* rest = e->name + plen;
        if (*rest == '/') ++rest;
        if (!*rest) continue;
        bool direct = true;
        for (const char* q = rest; *q; ++q) if (*q == '/') { direct = false; break; }
        if (direct) ++count;
    }
    return count;
}

const char* directory_entry_name(const char* directory, size_t index)
{
    if (!directory || !is_directory(directory)) return nullptr;
    char prefix[VFS_MAX_PATH];
    if (!normalize_path(directory, prefix, sizeof(prefix))) return nullptr;
    size_t plen = string_length(prefix);
    if (plen > 1 && prefix[plen - 1] == '/') prefix[--plen] = 0;
    size_t cur = 0;
    for (vfs_entry* e = vfs_root; e; e = e->next) {
        if (!e->name || string_equal(e->name, prefix)) continue;
        if (strncmp(e->name, prefix, plen) != 0) continue;
        if (prefix[0] == '/' && prefix[1] == 0) {
            if (e->name[1] == 0) continue;
        } else if (e->name[plen] != '/') continue;
        const char* rest = e->name + plen;
        if (*rest == '/') ++rest;
        if (!*rest) continue;
        bool direct = true;
        for (const char* q = rest; *q; ++q) if (*q == '/') { direct = false; break; }
        if (!direct) continue;
        if (cur++ == index) return rest;
    }
    return nullptr;
}

bool remove_file(const char* path)
{
    if (!path) return false;
    char normalized[VFS_MAX_PATH];
    if (!normalize_path(path, normalized, sizeof(normalized))) return false;
    vfs_entry* cur = vfs_root;
    vfs_entry* prev = nullptr;
    while (cur) {
        if (cur->name && string_equal(cur->name, normalized)) {
            if (cur->type != NODE_FILE) return false;
            if (prev) prev->next = cur->next; else vfs_root = cur->next;
            free_entry(cur);
            return true;
        }
        prev = cur; cur = cur->next;
    }
    return false;
}

bool rename_path(const char* old_path, const char* new_path)
{
    if (!old_path || !new_path) return false;
    char oldn[VFS_MAX_PATH], newn[VFS_MAX_PATH];
    if (!normalize_path(old_path, oldn, sizeof(oldn)) || !normalize_path(new_path, newn, sizeof(newn))) return false;
    vfs_entry* e = find_entry(oldn);
    if (!e || find_entry(newn)) return false;
    size_t n = string_length(newn);
    if (n >= VFS_MAX_PATH) return false;
    memcpy(e->name, newn, n + 1);
    return true;
}

bool create_device_node(
    const char* path,
    const DeviceNodeInfo& info)
{
    if (!path)
        return false;

    if (find_entry(path))
        return false;

    vfs_entry* entry =
        allocate_entry(path, NODE_DEVICE);

    if (!entry)
        return false;

    entry->device_info = info;
    entry->has_device_info = true;

    return true;
}

bool remove_device_node(const char* path)
{
    if (!path)
        return false;

    char normalized[VFS_MAX_PATH];

    if (!normalize_path(
            path,
            normalized,
            sizeof(normalized)))
    {
        return false;
    }

    vfs_entry* current = vfs_root;
    vfs_entry* previous = nullptr;

    while (current)
    {
        if (current->name &&
            string_equal(
                current->name,
                normalized))
        {
            if (current->type != NODE_DEVICE)
                return false;

            if (previous)
                previous->next = current->next;
            else
                vfs_root = current->next;

            free_entry(current);

            return true;
        }

        previous = current;
        current = current->next;
    }

    return false;
}

bool get_device_info(
    const char* path,
    DeviceNodeInfo* out)
{
    if (!path || !out)
        return false;

    vfs_entry* entry = find_entry(path);

    if (!entry)
        return false;

    if (entry->type != NODE_DEVICE)
        return false;

    if (!entry->has_device_info)
        return false;

    *out = entry->device_info;

    return true;
}

uint32_t device_count()
{
    uint32_t count = 0;

    vfs_entry* current = vfs_root;

    while (current)
    {
        if (current->type == NODE_DEVICE)
            ++count;

        current = current->next;
    }

    return count;
}

const char* device_name_at(uint32_t index)
{
    uint32_t current_index = 0;

    vfs_entry* current = vfs_root;

    while (current)
    {
        if (current->type == NODE_DEVICE)
        {
            if (current_index == index)
                return current->name;

            ++current_index;
        }

        current = current->next;
    }

    return nullptr;
}

const DeviceNodeInfo* device_info_at(uint32_t index)
{
    uint32_t current_index = 0;

    vfs_entry* current = vfs_root;

    while (current)
    {
        if (current->type == NODE_DEVICE &&
            current->has_device_info)
        {
            if (current_index == index)
                return &current->device_info;

            ++current_index;
        }

        current = current->next;
    }

    return nullptr;
}

bool register_disk(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device)
{
    return register_device(
        DEVICE_DISK,
        "/devices/disk",
        &disk_counter,
        base,
        size,
        irq,
        bus,
        slot,
        function,
        vendor,
        device);
}

bool register_network_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device)
{
    return register_device(
        DEVICE_NETWORK,
        "/devices/net",
        &network_counter,
        base,
        size,
        irq,
        bus,
        slot,
        function,
        vendor,
        device);
}

bool register_usb_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device)
{
    return register_device(
        DEVICE_USB,
        "/devices/usb",
        &usb_counter,
        base,
        size,
        irq,
        bus,
        slot,
        function,
        vendor,
        device);
}

bool register_gpu_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device)
{
    return register_device(
        DEVICE_GPU,
        "/devices/gpu",
        &gpu_counter,
        base,
        size,
        irq,
        bus,
        slot,
        function,
        vendor,
        device);
}

bool register_input_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device)
{
    return register_device(
        DEVICE_INPUT,
        "/devices/input",
        &input_counter,
        base,
        size,
        irq,
        bus,
        slot,
        function,
        vendor,
        device);
}

bool register_audio_device(
    uint64_t base,
    uint64_t size,
    uint8_t irq,
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint16_t vendor,
    uint16_t device)
{
    return register_device(
        DEVICE_AUDIO,
        "/devices/audio",
        &audio_counter,
        base,
        size,
        irq,
        bus,
        slot,
        function,
        vendor,
        device);
}

void initialize_devices()
{
    if (!find_entry("/"))
        allocate_entry("/", NODE_DIRECTORY);

    if (!find_entry("/devices"))
        create_directory("/devices");

    if (!find_entry("/system"))
        create_directory("/system");

    if (!find_entry("/boot"))
        create_directory("/boot");

    if (!find_entry("/tmp"))
        create_directory("/tmp");

    if (!find_entry("/mnt"))
        create_directory("/mnt");

    if (!find_entry("/process"))
        create_directory("/process");

    if (!find_entry("/devices/disk"))
        create_directory("/devices/disk");

    if (!find_entry("/devices/net"))
        create_directory("/devices/net");

    if (!find_entry("/devices/usb"))
        create_directory("/devices/usb");

    if (!find_entry("/devices/gpu"))
        create_directory("/devices/gpu");

    if (!find_entry("/devices/input"))
        create_directory("/devices/input");

    if (!find_entry("/devices/audio"))
        create_directory("/devices/audio");

    if (!find_entry("/devices/console"))
        create_directory("/devices/console");
}

} // namespace vfs

void vfs_init_from_ramfs()
{
    vfs::initialize_devices();

    const size_t count = ramfs::count();

    for (size_t i = 0; i < count; ++i)
    {
        const char* name = ramfs::name_at(i);

        if (!name)
            continue;

        uint32_t size = 0;

        const uint8_t* data =
            ramfs::get(name, &size);

        if (!data && size != 0)
            continue;

        char path[VFS_MAX_PATH];

        if (!normalize_path(
                name,
                path,
                sizeof(path)))
        {
            continue;
        }

        /*
         * Automatikusan létrehozzuk a hiányzó
         * szülőkönyvtárakat.
         */
        const size_t len = string_length(path);

        for (size_t i = 1; i < len; ++i)
        {
            if (path[i] != '/')
                continue;

            char parent[VFS_MAX_PATH];

            if (i >= sizeof(parent))
                break;

            for (size_t j = 0; j < i; ++j)
                parent[j] = path[j];

            parent[i] = '\0';

            if (!vfs::exists(parent))
                vfs::create_directory(parent);
        }

        if (vfs::exists(path))
            continue;

        vfs::create_file(
            path,
            data,
            size);
    }
}
