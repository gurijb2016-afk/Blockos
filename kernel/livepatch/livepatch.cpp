#include "livepatch.hpp"

#include <stddef.h>
#include <stdint.h>

namespace
{

using livepatch::Patch;
using livepatch::Error;

static Patch patches[
    livepatch::MAX_PATCHES
];

static Error current_error =
    Error::None;

static bool initialized = false;

static bool names_equal(
    const char* a,
    const char* b)
{
    if (!a || !b)
        return false;

    while (*a && *b)
    {
        if (*a != *b)
            return false;

        ++a;
        ++b;
    }

    return *a == '\0' &&
           *b == '\0';
}

static size_t patch_count()
{
    size_t count = 0;

    for (size_t i = 0;
         i < livepatch::MAX_PATCHES;
         ++i)
    {
        if (patches[i].registered)
            ++count;
    }

    return count;
}

static size_t applied_count()
{
    size_t count = 0;

    for (size_t i = 0;
         i < livepatch::MAX_PATCHES;
         ++i)
    {
        if (patches[i].registered &&
            patches[i].applied)
        {
            ++count;
        }
    }

    return count;
}

static Patch* find_mutable(
    const char* name)
{
    if (!name)
        return nullptr;

    for (size_t i = 0;
         i < livepatch::MAX_PATCHES;
         ++i)
    {
        if (!patches[i].registered)
            continue;

        if (names_equal(
                patches[i].name,
                name))
        {
            return &patches[i];
        }
    }

    return nullptr;
}

static Patch* free_slot()
{
    for (size_t i = 0;
         i < livepatch::MAX_PATCHES;
         ++i)
    {
        if (!patches[i].registered)
            return &patches[i];
    }

    return nullptr;
}

static void set_error(
    Error error)
{
    current_error = error;
}

}

namespace livepatch
{

bool init()
{
    for (size_t i = 0;
         i < MAX_PATCHES;
         ++i)
    {
        patches[i] = Patch{};
    }

    current_error = Error::None;
    initialized = true;

    return true;
}

bool register_patch(
    const char* name,
    void** target,
    void* replacement)
{
    if (!initialized)
    {
        set_error(Error::InvalidState);
        return false;
    }

    if (!name ||
        !target ||
        !replacement)
    {
        set_error(Error::InvalidArgument);
        return false;
    }

    if (find_mutable(name))
    {
        set_error(Error::AlreadyRegistered);
        return false;
    }

    Patch* slot = free_slot();

    if (!slot)
    {
        set_error(Error::NoFreeSlot);
        return false;
    }

    slot->name = name;
    slot->target = target;
    slot->replacement = replacement;
    slot->original = *target;

    slot->registered = true;
    slot->applied = false;

    set_error(Error::None);

    return true;
}

bool unregister_patch(
    const char* name)
{
    Patch* patch =
        find_mutable(name);

    if (!patch)
    {
        set_error(Error::NotFound);
        return false;
    }

    if (patch->applied)
    {
        set_error(Error::AlreadyApplied);
        return false;
    }

    *patch = Patch{};

    set_error(Error::None);

    return true;
}

bool apply(
    const char* name)
{
    Patch* patch =
        find_mutable(name);

    if (!patch)
    {
        set_error(Error::NotFound);
        return false;
    }

    if (patch->applied)
    {
        set_error(Error::AlreadyApplied);
        return false;
    }

    if (!patch->target ||
        !patch->replacement)
    {
        set_error(Error::InvalidArgument);
        return false;
    }

    /*
     * The actual live switch.
     *
     * All calls made through the registered function slot
     * will now go to replacement().
     */
    *patch->target =
        patch->replacement;

    patch->applied = true;

    set_error(Error::None);

    return true;
}

bool rollback(
    const char* name)
{
    Patch* patch =
        find_mutable(name);

    if (!patch)
    {
        set_error(Error::NotFound);
        return false;
    }

    if (!patch->applied)
    {
        set_error(Error::NotApplied);
        return false;
    }

    if (!patch->target)
    {
        set_error(Error::InvalidArgument);
        return false;
    }

    *patch->target =
        patch->original;

    patch->applied = false;

    set_error(Error::None);

    return true;
}

bool apply_all()
{
    for (size_t i = 0;
         i < MAX_PATCHES;
         ++i)
    {
        if (!patches[i].registered ||
            patches[i].applied)
        {
            continue;
        }

        if (!apply(
                patches[i].name))
        {
            return false;
        }
    }

    set_error(Error::None);
    return true;
}

bool rollback_all()
{
    /*
     * Roll back in reverse order.
     */
    for (size_t i = MAX_PATCHES;
         i > 0;
         --i)
    {
        Patch& patch =
            patches[i - 1];

        if (!patch.registered ||
            !patch.applied)
        {
            continue;
        }

        if (!rollback(
                patch.name))
        {
            return false;
        }
    }

    set_error(Error::None);
    return true;
}

bool is_applied(
    const char* name)
{
    Patch* patch =
        find_mutable(name);

    return patch &&
           patch->applied;
}

const Patch* find(
    const char* name)
{
    return find_mutable(name);
}

Status status()
{
    Status result{};

    result.registered =
        patch_count();

    result.applied =
        applied_count();

    result.capacity =
        MAX_PATCHES;

    return result;
}

Error last_error()
{
    return current_error;
}

const char* error_name(
    Error error)
{
    switch (error)
    {
        case Error::None:
            return "none";

        case Error::InvalidArgument:
            return "invalid argument";

        case Error::AlreadyRegistered:
            return "already registered";

        case Error::NotFound:
            return "not found";

        case Error::AlreadyApplied:
            return "already applied";

        case Error::NotApplied:
            return "not applied";

        case Error::NoFreeSlot:
            return "no free slot";

        case Error::InvalidState:
            return "invalid state";
    }

    return "unknown";
}

}
