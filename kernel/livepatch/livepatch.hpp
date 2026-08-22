#pragma once

#include <stddef.h>
#include <stdint.h>

namespace livepatch
{

constexpr size_t MAX_PATCHES = 64;

enum class Error : uint8_t
{
    None = 0,
    InvalidArgument,
    AlreadyRegistered,
    NotFound,
    AlreadyApplied,
    NotApplied,
    NoFreeSlot,
    InvalidState
};

struct Patch
{
    const char* name = nullptr;

    void** target = nullptr;
    void* replacement = nullptr;
    void* original = nullptr;

    bool registered = false;
    bool applied = false;
};

struct Status
{
    size_t registered = 0;
    size_t applied = 0;
    size_t capacity = MAX_PATCHES;
};

bool init();

bool register_patch(
    const char* name,
    void** target,
    void* replacement
);

bool unregister_patch(
    const char* name
);

bool apply(
    const char* name
);

bool rollback(
    const char* name
);

bool apply_all();

bool rollback_all();

bool is_applied(
    const char* name
);

const Patch* find(
    const char* name
);

Status status();

Error last_error();

const char* error_name(
    Error error
);

/*
 * Patchable function slot helper.
 *
 * Example:
 *
 * using Fn = int(*)(int);
 * Fn my_function = original_function;
 *
 * livepatch::register_slot(
 *     "my_function",
 *     &my_function,
 *     reinterpret_cast<void*>(patched_function)
 * );
 */
template<typename Function>
bool register_slot(
    const char* name,
    Function* slot,
    Function replacement)
{
    return register_patch(
        name,
        reinterpret_cast<void**>(slot),
        reinterpret_cast<void*>(
            replacement
        )
    );
}

}
