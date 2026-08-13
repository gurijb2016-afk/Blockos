#pragma once

#include <stdint.h>
#include <stddef.h>

namespace blockos::init {

enum class ServiceState : uint8_t
{
    Stopped = 0,
    Starting,
    Running,
    Stopping,
    Failed
};

enum class RestartPolicy : uint8_t
{
    Never = 0,
    OnFailure,
    Always
};

struct ServiceSpec
{
    const char* name;
    const char* path;
    const char* const* argv;
    const char* const* envp;
    RestartPolicy restart_policy;
    uint32_t restart_delay_ms;
    uint32_t max_restarts;
    bool required;
};

struct Service
{
    ServiceSpec spec{};
    int pid = -1;
    ServiceState state = ServiceState::Stopped;
    uint32_t restart_count = 0;
    uint64_t last_start_time_ms = 0;
    int last_status = 0;
};

} // namespace blockos::init
