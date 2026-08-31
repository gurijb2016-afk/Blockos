#include "amd_dcn6.hpp"

#include <stdint.h>
#include <stddef.h>

namespace amd_dcn6 {

// ============================================================
// RegisterIO
// ============================================================

void RegisterIO::write_mask(
    uint32_t reg,
    uint32_t mask,
    uint32_t value
) {
    uint32_t old_value = read32(reg);

    uint32_t new_value =
        (old_value & ~mask) |
        (value & mask);

    write32(reg, new_value);
}

// ============================================================
// MMIO
// ============================================================

MmioRegisterIO::MmioRegisterIO()
    : mmio_base(nullptr),
      mmio_size(0)
{
}

bool MmioRegisterIO::init(
    uintptr_t base,
    size_t size
) {
    if (base == 0)
        return false;

    if (size < sizeof(uint32_t))
        return false;

    mmio_base =
        reinterpret_cast<volatile uint8_t*>(base);

    mmio_size = size;

    return true;
}

uint32_t MmioRegisterIO::read32(
    uint32_t reg
) {
    if (!valid(reg))
        return 0;

    volatile uint32_t* ptr =
        reinterpret_cast<volatile uint32_t*>(
            mmio_base + reg
        );

    return *ptr;
}

void MmioRegisterIO::write32(
    uint32_t reg,
    uint32_t value
) {
    if (!valid(reg))
        return;

    volatile uint32_t* ptr =
        reinterpret_cast<volatile uint32_t*>(
            mmio_base + reg
        );

    *ptr = value;
}

bool MmioRegisterIO::valid(
    uint32_t reg
) const {
    if (!mmio_base)
        return false;

    if (reg > mmio_size)
        return false;

    if (mmio_size - reg < sizeof(uint32_t))
        return false;

    return (reg & 3U) == 0;
}

uintptr_t MmioRegisterIO::base() const {
    return reinterpret_cast<uintptr_t>(mmio_base);
}

size_t MmioRegisterIO::size() const {
    return mmio_size;
}

// ============================================================
// Controller
// ============================================================

Controller::Controller()
    : regs(nullptr),
      map{},
      caps{},
      outputs{},
      initialized(false),
      running(false),
      output_count(0)
{
}

bool Controller::check_mmio() const {
    if (!regs)
        return false;

    if (!regs->valid(map.dchub_control))
        return false;

    if (!regs->valid(map.optc_control))
        return false;

    return true;
}

Status Controller::initialize(
    RegisterIO* io,
    const RegisterMap& register_map
) {
    if (initialized)
        return Status::AlreadyInitialized;

    if (!io)
        return Status::InvalidArgument;

    regs = io;
    map = register_map;

    if (!check_mmio()) {
        regs = nullptr;
        return Status::HardwareError;
    }

    caps.pipes = 1;
    caps.streams = 1;

    caps.supports_dp = true;
    caps.supports_hdmi = true;
    caps.supports_edp = true;

    caps.supports_page_flip = true;
    caps.supports_vblank = true;
    caps.supports_atomic_update = true;

    initialized = true;
    running = false;
    output_count = 0;

    return Status::Ok;
}

void Controller::set_capabilities(
    const Capabilities& capabilities
) {
    caps = capabilities;
}

const Capabilities&
Controller::get_capabilities() const {
    return caps;
}

Status Controller::add_output(
    uint32_t id,
    ConnectorType type
) {
    if (!initialized)
        return Status::HardwareError;

    if (output_count >= 8)
        return Status::Busy;

    if (type == ConnectorType::Unknown)
        return Status::InvalidArgument;

    if (type == ConnectorType::DP && !caps.supports_dp)
        return Status::NotSupported;

    if (type == ConnectorType::HDMI && !caps.supports_hdmi)
        return Status::NotSupported;

    if (type == ConnectorType::EDP && !caps.supports_edp)
        return Status::NotSupported;

    Output& output = outputs[output_count];

    output.id = id;
    output.type = type;
    output.connected = true;
    output.enabled = false;
    output.mode = {};

    ++output_count;

    return Status::Ok;
}

// ============================================================
// Timing programming
// ============================================================

Status Controller::program_timing(
    const DisplayMode& mode
) {
    if (!regs)
        return Status::HardwareError;

    if (mode.width == 0 ||
        mode.height == 0 ||
        mode.refresh_hz == 0)
        return Status::InvalidArgument;

    if (mode.htotal <= mode.width)
        return Status::InvalidArgument;

    if (mode.vtotal <= mode.height)
        return Status::InvalidArgument;

    /*
     * Horizontal timing
     */
    regs->write32(
        map.optc_h_total,
        mode.htotal
    );

    uint32_t hsync =
        ((mode.hsync_start & 0xFFFFU) << 16) |
        (mode.hsync_end & 0xFFFFU);

    regs->write32(
        map.optc_h_sync_a,
        hsync
    );

    regs->write32(
        map.optc_h_sync_b,
        0
    );

    /*
     * Vertical timing
     */
    regs->write32(
        map.optc_v_total,
        mode.vtotal
    );

    uint32_t vsync =
        ((mode.vsync_start & 0xFFFFU) << 16) |
        (mode.vsync_end & 0xFFFFU);

    regs->write32(
        map.optc_v_sync_a,
        vsync
    );

    regs->write32(
        map.optc_v_sync_b,
        0
    );

    return Status::Ok;
}

// ============================================================
// Plane
// ============================================================

Status Controller::program_plane(
    const PlaneConfig& plane
) {
    if (!regs)
        return Status::HardwareError;

    if (plane.framebuffer == 0)
        return Status::InvalidArgument;

    if (plane.width == 0 ||
        plane.height == 0 ||
        plane.pitch == 0)
        return Status::InvalidArgument;

    /*
     * DCN6 hardware-specific address width can differ
     * between ASICs. Keep the abstraction generic.
     */

    uint32_t fb_low =
        static_cast<uint32_t>(
            plane.framebuffer & 0xFFFFFFFFULL
        );

    uint32_t fb_high =
        static_cast<uint32_t>(
            plane.framebuffer >> 32
        );

    /*
     * The actual high/low surface-address registers
     * must be supplied by the ASIC-specific register map.
     *
     * For this BlockOS abstraction the same logical
     * address field is programmed through the base
     * surface register.
     */

    regs->write32(
        map.hubp_surface_addr,
        fb_low
    );

    regs->write32(
        map.hubp_surface_pitch,
        plane.pitch
    );

    /*
     * Basic viewport.
     */
    uint32_t viewport =
        ((plane.x & 0xFFFFU) << 16) |
        (plane.y & 0xFFFFU);

    regs->write32(
        map.viewport_x,
        viewport
    );

    viewport =
        ((plane.output_width & 0xFFFFU) << 16) |
        (plane.output_height & 0xFFFFU);

    regs->write32(
        map.viewport_y,
        viewport
    );

    /*
     * Pixel format.
     */
    uint32_t format = 0;

    switch (plane.format) {
        case PixelFormat::XRGB8888:
            format = 0;
            break;

        case PixelFormat::ARGB8888:
            format = 1;
            break;

        case PixelFormat::RGB565:
            format = 2;
            break;

        case PixelFormat::XRGB2101010:
            format = 3;
            break;

        default:
            return Status::InvalidArgument;
    }

    if (regs->valid(map.opp_pixel_format))
        regs->write32(
            map.opp_pixel_format,
            format
        );

    return Status::Ok;
}

// ============================================================
// Enable
// ============================================================

Status Controller::enable_output(
    uint32_t output
) {
    if (output >= output_count)
        return Status::NotFound;

    Output& out = outputs[output];

    if (!out.connected)
        return Status::NotFound;

    /*
     * Enable OPTC / output pipeline.
     */
    regs->write_mask(
        map.optc_control,
        0x1,
        0x1
    );

    out.enabled = true;
    running = true;

    return Status::Ok;
}

Status Controller::set_mode(
    uint32_t output,
    const DisplayMode& mode
) {
    if (!initialized)
        return Status::HardwareError;

    if (output >= output_count)
        return Status::NotFound;

    Status result =
        program_timing(mode);

    if (result != Status::Ok)
        return result;

    outputs[output].mode = mode;

    return Status::Ok;
}

Status Controller::set_plane(
    uint32_t output,
    const PlaneConfig& plane
) {
    if (!initialized)
        return Status::HardwareError;

    if (output >= output_count)
        return Status::NotFound;

    return program_plane(plane);
}

// ============================================================
// VBlank
// ============================================================

Status Controller::wait_vblank(
    uint32_t timeout
) {
    if (!regs)
        return Status::HardwareError;

    if (!caps.supports_vblank)
        return Status::NotSupported;

    for (uint32_t i = 0; i < timeout; ++i) {
        uint32_t status =
            regs->read32(map.vblank_status);

        if (status & 1U)
            return Status::Ok;
    }

    return Status::Timeout;
}

// ============================================================
// Page flip
// ============================================================

Status Controller::page_flip(
    uint32_t output,
    uint64_t framebuffer
) {
    if (!initialized)
        return Status::HardwareError;

    if (!caps.supports_page_flip)
        return Status::NotSupported;

    if (framebuffer == 0)
        return Status::InvalidArgument;

    if (output >= output_count)
        return Status::NotFound;

    Status result =
        wait_vblank(1000000);

    if (result != Status::Ok)
        return result;

    uint32_t low =
        static_cast<uint32_t>(
            framebuffer & 0xFFFFFFFFULL
        );

    regs->write32(
        map.hubp_surface_addr,
        low
    );

    if (regs->valid(map.vblank_ack))
        regs->write32(
            map.vblank_ack,
            1
        );

    return Status::Ok;
}

// ============================================================
// Output enable/disable
// ============================================================

Status Controller::enable(
    uint32_t output
) {
    if (!initialized)
        return Status::HardwareError;

    if (output >= output_count)
        return Status::NotFound;

    return enable_output(output);
}

Status Controller::disable(
    uint32_t output
) {
    if (!initialized)
        return Status::HardwareError;

    if (output >= output_count)
        return Status::NotFound;

    regs->write_mask(
        map.optc_control,
        0x1,
        0
    );

    outputs[output].enabled = false;

    bool any_enabled = false;

    for (uint32_t i = 0; i < output_count; ++i) {
        if (outputs[i].enabled) {
            any_enabled = true;
            break;
        }
    }

    running = any_enabled;

    return Status::Ok;
}

bool Controller::is_initialized() const {
    return initialized;
}

bool Controller::is_running() const {
    return running;
}

// ============================================================
// AMD detection
// ============================================================

bool is_amd_display_device(
    const PciIdentity& pci
) {
    /*
     * AMD vendor ID.
     */
    if (pci.vendor != 0x1002)
        return false;

    /*
     * PCI display controller class.
     */
    if (pci.class_code != 0x03)
        return false;

    return true;
}

bool is_dcn6_candidate(
    const PciIdentity& pci
) {
    if (!is_amd_display_device(pci))
        return false;

    /*
     * DCN generation detection should ultimately come from
     * the ASIC/IP discovery layer rather than guessing from
     * one PCI device ID.
     *
     * Keep this function conservative until BlockOS has its
     * complete AMD ASIC-ID table.
     */
    return true;
}

// ============================================================
// Global instance
// ============================================================

Controller dcn6;

} // namespace amd_dcn6
