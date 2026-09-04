#include "vfs.hpp"

#include "kernel/allocator.hpp"
#include "libc/include/string.h"
#include "ramfs.hpp"

struct vfs_entry
{
    const char* name;
    uint8_t* data;
    uint32_t size;
    vfs::NodeType type;
    vfs_entry* next;
};

static vfs_entry* vfs_root = nullptr;

static bool is_device_path(const char* name)
{
    if (!name)
        return false;

    static constexpr char prefix[] = "/devices/";
    constexpr size_t prefix_len = sizeof(prefix) - 1;

    for (size_t i = 0; i < prefix_len; ++i)
    {
        if (name[i] != prefix[i])
            return false;
    }

    return name[prefix_len] != '\0';
}

static vfs_entry* find_entry(const char* name)
{
    if (!name)
        return nullptr;

    for (vfs_entry* e = vfs_root; e; e = e->next)
    {
        if (strcmp(e->name, name) == 0)
            return e;
    }

    return nullptr;
}

size_t vfs::count_files()
{
    size_t cnt = 0;

    for (vfs_entry* e = vfs_root; e; e = e->next)
        ++cnt;

    return cnt;
}

const char* vfs::name_at(size_t idx)
{
    size_t i = 0;

    for (vfs_entry* e = vfs_root; e; e = e->next)
    {
        if (i == idx)
            return e->name;

        ++i;
    }

    return nullptr;
}

const uint8_t* vfs::read_file(
    const char* name,
    uint32_t* out_size)
{
    if (out_size)
        *out_size = 0;

    vfs_entry* e = find_entry(name);

    if (!e)
        return nullptr;

    if (e->type == vfs::NodeType::Device)
        return nullptr;

    if (out_size)
        *out_size = e->size;

    return e->data;
}

bool vfs::create_file(
    const char* name,
    const uint8_t* data,
    uint32_t size)
{
    if (!name)
        return false;

    if (find_entry(name))
        return false;

    size_t name_len = strlen(name) + 1;

    vfs_entry* e =
        (vfs_entry*)allocator::alloc(sizeof(vfs_entry));

    if (!e)
        return false;

    char* n =
        (char*)allocator::alloc(name_len);

    if (!n)
        return false;

    memcpy(n, name, name_len);

    vfs::NodeType type =
        is_device_path(name)
            ? vfs::NodeType::Device
            : vfs::NodeType::RegularFile;

    uint8_t* d = nullptr;

    if (type == vfs::NodeType::RegularFile && size > 0)
    {
        d = (uint8_t*)allocator::alloc(size);

        if (!d)
            return false;

        if (!data)
            return false;

        memcpy(d, data, size);
    }

    e->name = n;
    e->data = d;
    e->size = type == vfs::NodeType::Device ? 0 : size;
    e->type = type;
    e->next = vfs_root;

    vfs_root = e;

    return true;
}

bool vfs::write_file(
    const char* name,
    const uint8_t* data,
    uint32_t size)
{
    vfs_entry* e = find_entry(name);

    if (!e)
        return create_file(name, data, size);

    if (e->type == vfs::NodeType::Device)
        return false;

    if (size > 0 && !data)
        return false;

    uint8_t* d = nullptr;

    if (size > 0)
    {
        d = (uint8_t*)allocator::alloc(size);

        if (!d)
            return false;

        memcpy(d, data, size);
    }

    e->data = d;
    e->size = size;

    return true;
}

bool vfs::exists(const char* name)
{
    return find_entry(name) != nullptr;
}

bool vfs::is_device(const char* name)
{
    vfs_entry* e = find_entry(name);

    if (!e)
        return false;

    return e->type == vfs::NodeType::Device;
}

bool vfs::stat_node(
    const char* name,
    NodeInfo* out_info)
{
    if (!out_info)
        return false;

    vfs_entry* e = find_entry(name);

    if (!e)
        return false;

    out_info->type = e->type;
    out_info->size = e->size;

    return true;
}

void vfs_init_from_ramfs()
{
    const struct ramfile* it = __ramfs_start;

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
