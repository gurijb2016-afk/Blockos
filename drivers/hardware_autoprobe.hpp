#pragma once

#include <stdint.h>


namespace blockos_hw {

/*
 * Probe and initialize hardware backends which are safe to bring up before
 * ExitBootServices().  The routine is deliberately best-effort: a missing
 * controller or a failed driver must never prevent BlockOS from booting.
 */
struct Report {
    uint32_t pci_devices;
    bool virtio_block;
    bool nvme;
    bool ahci;
    bool virtio_net;
    bool native_net;
    bool xhci;
    bool hda;
    bool virtio_input;
    bool virtio_rng;
    bool serial;
};

void pre_exit_bootstrap(Report* out_report = nullptr);
const Report& last_report();

} // namespace blockos_hw
