#pragma once

#include <stdint.h>

namespace blockos_hw {

enum class ProbeStatus : uint8_t {
    NotAvailable = 0, // Hardware not present on this machine
    Ok           = 1, // Hardware detected and driver initialized
    Failed       = 2, // Matching hardware present, but driver init failed
};

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
    bool virtio_gpu;
    bool serial;

    // 3-state status for boot hardware checklist ([  OK  ] / [ N/A  ] / [ FAIL ])
    ProbeStatus st_virtio_block;
    ProbeStatus st_nvme;
    ProbeStatus st_ahci;
    ProbeStatus st_virtio_net;
    ProbeStatus st_native_net;
    ProbeStatus st_xhci;
    ProbeStatus st_hda;
    ProbeStatus st_virtio_input;
    ProbeStatus st_virtio_rng;
    ProbeStatus st_virtio_gpu;
    ProbeStatus st_serial;
    ProbeStatus st_cpu_rdrand;
};

void pre_exit_bootstrap(Report* out_report = nullptr);
const Report& last_report();

// Emit color-coded ANSI [  OK  ] / [ N/A  ] / [ FAIL ] hardware boot checklist
// through the provided text sink (e.g., kernel framebuffer console + serial).
void print_boot_checklist(void (*emit)(const char* text, void* ctx), void* ctx = nullptr);

} // namespace blockos_hw
