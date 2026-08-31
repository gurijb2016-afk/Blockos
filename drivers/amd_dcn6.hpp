#pragma once

#include <stdint.h>
#include <stddef.h>

namespace amd_dcn6 {

// ============================================================
// Basic types
// ============================================================

enum class Status : int {
    Ok                 = 0,
    InvalidArgument    = -1,
    NotFound           = -2,
    NotSupported       = -3,
    AlreadyInitialized = -4,
    HardwareError      = -5,
    Timeout            = -6,
    Busy               = -7,
    PermissionDenied   = -8
};

enum class PixelFormat : uint32_t {
    XRGB8888 = 0,
    ARGB8888 = 1,
    RGB565   = 2,
    XRGB2101010 = 3,
};

enum class ConnectorType : uint32_t {
    Unknown = 0,
    DP      = 1,
    HDMI    = 2,
    EDP     = 3
};

// ============================================================
// Display mode
// ============================================================

struct DisplayMode {
    uint32_t width;
    uint32_t height;

    uint32_t refresh_hz;

    uint32_t htotal;
    uint32_t hsync_start;
    uint32_t hsync_end;

    uint32_t vtotal;
    uint32_t vsync_start;
    uint32_t vsync_end;

    uint32_t pixel_clock_khz;

    bool interlaced;
};

// ============================================================
// DCN6 register abstraction
// ============================================================

class RegisterIO {
public:
    virtual ~RegisterIO() = default;

    virtual uint32_t read32(uint32_t reg) = 0;
    virtual void write32(uint32_t reg, uint32_t value) = 0;

    virtual void write_mask(
        uint32_t reg,
        uint32_t mask,
        uint32_t value
    );

    virtual bool valid(uint32_t reg) const = 0;
};

// ============================================================
// MMIO register implementation
// ============================================================

class MmioRegisterIO final : public RegisterIO {
private:
    volatile uint8_t* mmio_base;
    size_t mmio_size;

public:
    MmioRegisterIO();

    bool init(
        uintptr_t base,
        size_t size
    );

    uint32_t read32(uint32_t reg) override;

    void write32(
        uint32_t reg,
        uint32_t value
    ) override;

    bool valid(uint32_t reg) const override;

    uintptr_t base() const;
    size_t size() const;
};

// ============================================================
// DCN6 register description
//
// IMPORTANT:
// Actual offsets are supplied by the ASIC platform.
// ============================================================

struct RegisterMap {
    uint32_t dchub_control;
    uint32_t dchub_size;

    uint32_t hubp_surface_addr;
    uint32_t hubp_surface_pitch;

    uint32_t dpp_control;

    uint32_t opp_control;
    uint32_t opp_pixel_format;

    uint32_t optc_control;

    uint32_t optc_h_total;
    uint32_t optc_h_sync_a;
    uint32_t optc_h_sync_b;

    uint32_t optc_v_total;
    uint32_t optc_v_sync_a;
    uint32_t optc_v_sync_b;

    uint32_t optc_underflow_status;

    uint32_t viewport_x;
    uint32_t viewport_y;

    uint32_t vblank_status;
    uint32_t vblank_ack;
};

// ============================================================
// Capabilities
// ============================================================

struct Capabilities {
    uint32_t pipes;
    uint32_t streams;

    bool supports_dp;
    bool supports_hdmi;
    bool supports_edp;

    bool supports_page_flip;
    bool supports_vblank;

    bool supports_atomic_update;
};

// ============================================================
// Plane
// ============================================================

struct PlaneConfig {
    uint64_t framebuffer;

    uint32_t width;
    uint32_t height;

    uint32_t pitch;

    PixelFormat format;

    uint32_t x;
    uint32_t y;

    uint32_t output_width;
    uint32_t output_height;
};

// ============================================================
// Output
// ============================================================

struct Output {
    uint32_t id;

    ConnectorType type;

    bool connected;
    bool enabled;

    DisplayMode mode;
};

// ============================================================
// DCN6 controller
// ============================================================

class Controller {
private:
    RegisterIO* regs;
    RegisterMap map;
    Capabilities caps;

    Output outputs[8];

    bool initialized;
    bool running;

    uint32_t output_count;

    bool check_mmio() const;

    Status wait_vblank(
        uint32_t timeout
    );

    Status program_timing(
        const DisplayMode& mode
    );

    Status program_plane(
        const PlaneConfig& plane
    );

    Status enable_output(
        uint32_t output
    );

public:
    Controller();

    Status initialize(
        RegisterIO* io,
        const RegisterMap& register_map
    );

    void set_capabilities(
        const Capabilities& capabilities
    );

    const Capabilities& get_capabilities() const;

    Status add_output(
        uint32_t id,
        ConnectorType type
    );

    Status set_mode(
        uint32_t output,
        const DisplayMode& mode
    );

    Status set_plane(
        uint32_t output,
        const PlaneConfig& plane
    );

    Status page_flip(
        uint32_t output,
        uint64_t framebuffer
    );

    Status enable(
        uint32_t output
    );

    Status disable(
        uint32_t output
    );

    bool is_initialized() const;
    bool is_running() const;
};

// ============================================================
// AMD PCI detection
// ============================================================

struct PciIdentity {
    uint16_t vendor;
    uint16_t device;
    uint8_t revision;
    uint8_t class_code;
    uint8_t subclass;
};

bool is_amd_display_device(
    const PciIdentity& pci
);

bool is_dcn6_candidate(
    const PciIdentity& pci
);

// ============================================================
// Global driver
// ============================================================

extern Controller dcn6;

} // namespace amd_dcn6
