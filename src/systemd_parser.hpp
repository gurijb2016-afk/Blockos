#pragma once
#include <stdint.h>

// Szolgáltatások futási állapotai
#define SERVICE_STATE_STOPPED  0
#define SERVICE_STATE_STARTING 1
#define SERVICE_STATE_RUNNING  2

struct VirtualService {
    uint32_t id;
    uint32_t state;
};

class SystemdCoreEngine {
private:
    // A belső szoftveres stressz-teszt klaszter táblája (1000 szolgáltatás részére)
    VirtualService service_cluster[1005];
    uint32_t total_services;

public:
    SystemdCoreEngine() : total_services(0) {}

    void generate_and_load_services();
    void boot_services();
};

// Globális init motor példány deklarációja
extern SystemdCoreEngine systemd_init;

// A Scheduler által indított első éles rendszerszál prototípusa
void systemd_pid1_thread();
