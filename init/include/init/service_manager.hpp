#pragma once

#include "service.hpp"

#include <stddef.h>
#include <stdint.h>

namespace blockos::init {

class ServiceManager
{
public:
    static constexpr size_t MAX_SERVICES = 64;

    ServiceManager();

    bool add(const ServiceSpec& spec);

    bool start(const char* name);
    bool stop(const char* name);
    bool restart(const char* name);

    void start_all();
    void stop_all();

    void supervise();
    void reap_children();

    Service* find(const char* name);
    const Service* find(const char* name) const;

    size_t count() const noexcept;

private:
    Service services_[MAX_SERVICES];
    size_t service_count_;

    bool start_service(Service& service);
    bool stop_service(Service& service);

    void handle_child_exit(Service& service, int status);

    bool should_restart(const Service& service) const;
    bool restart_allowed(const Service& service) const;
};

} // namespace blockos::init
