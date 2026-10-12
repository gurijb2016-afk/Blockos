#include "usb_xhci.hpp"
#include "pcie.hpp"
#include "dma.hpp"
#include "usb_hid_report.hpp"
#include "usb_hub_protocol.hpp"
#include "hpet_timer.hpp"
#include "../kernel/input_bridge.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace usb::xhci {
namespace {

static inline uint32_t read32(volatile uint8_t* base, uint32_t offset)
{
    return *reinterpret_cast<volatile uint32_t*>(base + offset);
}
static inline uint64_t read64(volatile uint8_t* base, uint32_t offset)
{
    return *reinterpret_cast<volatile uint64_t*>(base + offset);
}
static inline void write32(volatile uint8_t* base, uint32_t offset, uint32_t v)
{
    *reinterpret_cast<volatile uint32_t*>(base + offset) = v;
}
static inline void write64(volatile uint8_t* base, uint32_t offset, uint64_t v)
{
    *reinterpret_cast<volatile uint64_t*>(base + offset) = v;
}
static inline void pause_cpu() { __asm__ volatile("pause"); }

static constexpr uint32_t REG_USBCMD = 0x00;
static constexpr uint32_t REG_USBSTS = 0x04;
static constexpr uint32_t REG_PAGESIZE = 0x08;
static constexpr uint32_t REG_CRCR = 0x18;
static constexpr uint32_t REG_DCBAAP = 0x30;
static constexpr uint32_t REG_CONFIG = 0x38;
static constexpr uint32_t USBSTS_HCH = 1u << 0;
static constexpr uint32_t USBSTS_CNR = 1u << 11;
static constexpr uint32_t USBCMD_RS = 1u << 0;
static constexpr uint32_t USBCMD_HCRST = 1u << 1;
static constexpr uint32_t USBCMD_INTE = 1u << 2;
static constexpr uint32_t TRB_TYPE_SHIFT = 10;
static constexpr uint32_t TRB_CYCLE = 1u;
static constexpr uint32_t TRB_TOGGLE_CYCLE = 1u << 1;
static constexpr uint32_t TRB_CHAIN = 1u << 4;
static constexpr uint32_t TRB_IOC = 1u << 5;
static constexpr uint32_t TRB_IDT = 1u << 6;
static constexpr uint32_t TRB_DIR_IN = 1u << 16;
static constexpr uint32_t PORTSC_CCS = 1u << 0;
static constexpr uint32_t PORTSC_PED = 1u << 1;
static constexpr uint32_t PORTSC_PP = 1u << 9;
static constexpr uint32_t PORTSC_PR = 1u << 4;
static constexpr uint32_t PORTSC_WPR = 1u << 31;
static constexpr uint32_t PORTSC_CHANGE = (1u << 17) | (1u << 18) | (1u << 19) |
                                             (1u << 20) | (1u << 21) | (1u << 22) | (1u << 23);
static constexpr uint32_t MAX_SLOTS = 16;
static constexpr uint32_t CMD_RING_TRBS = 256;
static constexpr uint32_t EVT_RING_TRBS = 256;
static constexpr uint32_t XFER_RING_TRBS = 64;
static constexpr unsigned MAX_HID_PER_SLOT = 4;
static constexpr unsigned MAX_HID_TOTAL = 32;

struct Trb { uint64_t parameter; uint32_t status; uint32_t control; } __attribute__((packed, aligned(16)));
struct ErstEntry { uint64_t base; uint32_t size; uint32_t reserved; } __attribute__((packed, aligned(16)));
struct Ring { Trb* trbs; uint32_t count; uint32_t enqueue; uint8_t cycle; };
struct KeyboardRuntime { hid::KeyboardState state; };
struct MouseRuntime { hid::MouseState state; };
struct HidEndpoint {
    bool active;
    bool keyboard;
    bool mouse;
    bool pending;
    uint8_t slot_id;
    uint8_t dci;
    uint8_t interface_number;
    uint16_t max_packet;
    Ring ring;
    uint8_t* report;
    KeyboardRuntime keyboard_state;
    MouseRuntime mouse_state;
};
struct SlotRuntime {
    bool addressed;
    uint8_t slot_id;
    uint8_t speed;
    uint8_t ep0_packet;
    uint8_t context_bytes;
    uint8_t root_port;
    uint8_t route_depth;
    uint8_t hub_port_count;
    uint8_t hub_tt_think_time;
    uint8_t tt_hub_slot_id;
    uint8_t tt_port_number;
    uint8_t tt_think_time;
    bool is_hub;
    uint32_t route_string;
    uint8_t* input_context;
    uint8_t* output_context;
    Ring ep0_ring;
    HidEndpoint hid[MAX_HID_PER_SLOT];
};
struct ControllerState {
    bool initialized;
    Controller* owner;
    uint8_t max_slots;
    uint8_t context_bytes;
    uint16_t event_index;
    uint8_t event_cycle;
    uint16_t max_ports;
    uint64_t* dcbaa;
    uint64_t* scratch_array;
    void** scratch_pages;
    uint32_t scratch_pages_allocated;
    Ring command_ring;
    Trb* event_ring;
    ErstEntry* erst;
    SlotRuntime slots[MAX_SLOTS + 1];
    size_t hid_count;
};

static ControllerState g_state{};

static bool alloc_ring(Ring* ring, uint32_t count)
{
    if (!ring || count < 16 || (count & (count - 1)) != 0) return false;
    Trb* memory = static_cast<Trb*>(dma::alloc(sizeof(Trb) * count, 4096));
    if (!memory) return false;
    memset(memory, 0, sizeof(Trb) * count);
    ring->trbs = memory;
    ring->count = count;
    ring->enqueue = 0;
    ring->cycle = 1;
    return true;
}

static Trb* ring_push(Ring* ring, const Trb& input)
{
    if (!ring || !ring->trbs || ring->count < 16) return nullptr;
    if (ring->enqueue == ring->count - 1) {
        Trb& link = ring->trbs[ring->enqueue];
        link.parameter = reinterpret_cast<uint64_t>(ring->trbs);
        link.status = 0;
        link.control = (6u << TRB_TYPE_SHIFT) | TRB_TOGGLE_CYCLE |
                       (ring->cycle ? TRB_CYCLE : 0u);
        ring->enqueue = 0;
        ring->cycle ^= 1u;
    }
    Trb* dst = &ring->trbs[ring->enqueue++];
    dst->parameter = input.parameter;
    dst->status = input.status;
    dst->control = (input.control & ~TRB_CYCLE) | (ring->cycle ? TRB_CYCLE : 0u);
    __asm__ volatile("mfence" ::: "memory");
    return dst;
}

static bool ring_pop_event(ControllerState* s, Trb* out)
{
    if (!s || !s->event_ring || !out) return false;
    Trb* event = &s->event_ring[s->event_index];
    __asm__ volatile("lfence" ::: "memory");
    if ((event->control & TRB_CYCLE) != (s->event_cycle ? 1u : 0u)) return false;
    *out = *event;
    ++s->event_index;
    if (s->event_index >= EVT_RING_TRBS) {
        s->event_index = 0;
        s->event_cycle ^= 1u;
    }
    Controller* c = s->owner;
    const uint32_t intr = (c->rtsoff & ~0x1Fu) + 0x20u;
    const uint64_t erdp = reinterpret_cast<uint64_t>(&s->event_ring[s->event_index]);
    write64(c->mmio, intr + 0x18u, erdp | 8u); // dequeue pointer + EHB
    return true;
}

static uint32_t event_type(const Trb& e) { return (e.control >> TRB_TYPE_SHIFT) & 0x3Fu; }
static uint8_t event_code(const Trb& e) { return static_cast<uint8_t>(e.status >> 24); }
static uint8_t event_slot(const Trb& e) { return static_cast<uint8_t>(e.control >> 24); }
static uint8_t event_endpoint(const Trb& e) { return static_cast<uint8_t>((e.control >> 16) & 0x1Fu); }

static bool wait_register(volatile uint32_t* reg, uint32_t mask, bool want_set, uint64_t polls = 5000000)
{
    if (!reg) return false;
    for (uint64_t i = 0; i < polls; ++i) {
        const bool set = ((*reg & mask) == mask);
        if (set == want_set) return true;
        if ((i & 0xFFFu) == 0) pause_cpu();
    }
    return false;
}

static bool take_legacy_ownership(volatile uint8_t* mmio, uint32_t hcc1, uint64_t bar_size)
{
    // xHCI HCCPARAMS1.ECP is a DWORD offset from the capability base.
    // Firmware normally leaves no active BIOS ownership, but reset is unsafe
    // until the OS-owned handshake has completed where the capability exists.
    uint32_t offset = ((hcc1 >> 16) & 0xFFFFu) * 4u;
    if (offset == 0)
        return true;
    if (bar_size < 4 || offset > bar_size || bar_size - offset < 8)
        return false;

    for (unsigned n = 0; n < 256; ++n) {
        const uint32_t header = read32(mmio, offset);
        if (header == 0 || header == 0xFFFFFFFFu)
            return true; // no further extended capabilities
        const uint8_t id = static_cast<uint8_t>(header & 0xFFu);
        const uint8_t next = static_cast<uint8_t>((header >> 8) & 0xFFu);
        if (id == 1u) { // USB Legacy Support capability
            uint32_t sem = read32(mmio, offset);
            if (sem & (1u << 16)) {
                write32(mmio, offset, sem | (1u << 24)); // OS Owned Semaphore
                bool handed_off = false;
                for (uint32_t wait = 0; wait < 10000000u; ++wait) {
                    sem = read32(mmio, offset);
                    if ((sem & (1u << 16)) == 0) { handed_off = true; break; }
                    if ((wait & 0xFFFu) == 0) pause_cpu();
                }
                if (!handed_off)
                    return false;
            }
            // Disable legacy SMIs and acknowledge any latched SMI status bits.
            write32(mmio, offset + 4u, 0xFFFF0000u);
            return true;
        }
        if (next == 0)
            break;
        const uint32_t next_offset = static_cast<uint32_t>(next) * 4u;
        if (next_offset <= offset || next_offset > 0x10000u ||
            next_offset > bar_size || bar_size - next_offset < 4)
            return false; // malformed/cyclic or out-of-BAR capability chain
        offset = next_offset;
    }
    return true; // no legacy capability in this controller
}

static bool wait_command(ControllerState* s, Trb* command, Trb* completion)
{
    const uint64_t target = reinterpret_cast<uint64_t>(command) & ~0xFULL;
    for (uint64_t i = 0; i < 12000000ULL; ++i) {
        Trb e{};
        if (ring_pop_event(s, &e)) {
            if (event_type(e) == 33u && ((e.parameter & ~0xFULL) == target)) {
                if (completion) *completion = e;
                return event_code(e) == 1u;
            }
        } else if ((i & 0xFFFu) == 0) pause_cpu();
    }
    return false;
}

static bool submit_command(ControllerState* s, uint64_t parameter, uint32_t status,
                           uint32_t control, Trb* completion)
{
    Trb input{parameter, status, control | (TRB_CYCLE)};
    Trb* cmd = ring_push(&s->command_ring, input);
    if (!cmd) return false;
    write32(s->owner->mmio, s->owner->dboff & ~3u, 0);
    return wait_command(s, cmd, completion);
}

static uint8_t* ctx_ptr(uint8_t* base, uint8_t ctx_bytes, unsigned context_index)
{
    return base ? base + static_cast<size_t>(ctx_bytes) * context_index : nullptr;
}
static uint32_t* ctx_dwords(uint8_t* p) { return reinterpret_cast<uint32_t*>(p); }
static uint64_t* ctx_qwords(uint8_t* p) { return reinterpret_cast<uint64_t*>(p); }

static bool wait_transfer(ControllerState* s, uint8_t slot_id, uint8_t dci,
                          Trb* expected, uint32_t* actual)
{
    const uint64_t target = reinterpret_cast<uint64_t>(expected) & ~0xFULL;
    for (uint64_t i = 0; i < 12000000ULL; ++i) {
        Trb e{};
        if (ring_pop_event(s, &e)) {
            if (event_type(e) != 32u || event_slot(e) != slot_id || event_endpoint(e) != dci)
                continue;
            if ((e.parameter & ~0xFULL) != target)
                continue;
            if (actual) *actual = e.status & 0x00FFFFFFu;
            return event_code(e) == 1u || event_code(e) == 13u; // Success or Short Packet
        } else if ((i & 0xFFFu) == 0) pause_cpu();
    }
    return false;
}

static bool control_transfer(ControllerState* s, SlotRuntime* slot,
                             uint8_t request_type, uint8_t request,
                             uint16_t value, uint16_t index,
                             void* data, uint16_t length, uint32_t* received = nullptr)
{
    if (!slot || !slot->addressed || !slot->ep0_ring.trbs) return false;
    uint8_t setup[8] = {
        request_type, request,
        static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(index), static_cast<uint8_t>(index >> 8),
        static_cast<uint8_t>(length), static_cast<uint8_t>(length >> 8)
    };
    uint32_t trt = length == 0 ? 0u : ((request_type & 0x80u) ? 3u : 2u);
    uint64_t immediate = 0;
    memcpy(&immediate, setup, sizeof(setup));
    Trb setup_trb{immediate, 8u, (2u << TRB_TYPE_SHIFT) | TRB_IDT | TRB_CHAIN | (trt << 16)};
    Trb* setup_ptr = ring_push(&slot->ep0_ring, setup_trb);
    if (!setup_ptr) return false;
    if (length != 0) {
        if (!data) return false;
        Trb data_trb{reinterpret_cast<uint64_t>(data), length,
                     (3u << TRB_TYPE_SHIFT) | TRB_CHAIN | ((request_type & 0x80u) ? TRB_DIR_IN : 0u)};
        if (!ring_push(&slot->ep0_ring, data_trb)) return false;
    }
    Trb status_trb{0, 0, (4u << TRB_TYPE_SHIFT) | TRB_IOC |
                   ((length != 0 && (request_type & 0x80u) == 0) ? TRB_DIR_IN : 0u)};
    Trb* status_ptr = ring_push(&slot->ep0_ring, status_trb);
    if (!status_ptr) return false;
    const uint32_t db = (slot->slot_id * 4u) + 0u;
    write32(s->owner->mmio, (s->owner->dboff & ~3u) + db, 1u);
    uint32_t residual = length;
    if (!wait_transfer(s, slot->slot_id, 1, status_ptr, &residual)) return false;
    if (received) *received = length >= residual ? static_cast<uint32_t>(length - residual) : 0;
    return true;
}

static bool enable_slot(ControllerState* s, uint8_t* out_slot)
{
    Trb event{};
    if (!submit_command(s, 0, 0, (9u << TRB_TYPE_SHIFT), &event)) return false;
    const uint8_t id = event_slot(event);
    if (id == 0 || id > s->max_slots || id > MAX_SLOTS) return false;
    *out_slot = id;
    return true;
}

static bool address_device(ControllerState* s, SlotRuntime* slot,
                           uint8_t root_port, uint32_t route_string,
                           uint8_t route_depth, uint8_t speed,
                           uint8_t tt_hub_slot_id, uint8_t tt_port_number,
                           uint8_t tt_think_time)
{
    slot->speed = speed;
    slot->root_port = root_port;
    slot->route_string = route_string & 0x000FFFFFu;
    slot->route_depth = route_depth;
    slot->tt_hub_slot_id = (speed <= 2u) ? tt_hub_slot_id : 0;
    slot->tt_port_number = slot->tt_hub_slot_id ? tt_port_number : 0;
    slot->tt_think_time = slot->tt_hub_slot_id ? (tt_think_time & 0x3u) : 0;
    slot->ep0_packet = (speed == 1u) ? 8u : (speed == 2u ? 8u : (speed == 3u ? 64u : 512u));
    slot->context_bytes = s->context_bytes;
    slot->input_context = static_cast<uint8_t*>(dma::alloc(static_cast<size_t>(s->context_bytes) * 33u, 4096));
    slot->output_context = static_cast<uint8_t*>(dma::alloc(static_cast<size_t>(s->context_bytes) * 32u, 4096));
    if (!slot->input_context || !slot->output_context || !alloc_ring(&slot->ep0_ring, XFER_RING_TRBS)) return false;
    memset(slot->input_context, 0, static_cast<size_t>(s->context_bytes) * 33u);
    memset(slot->output_context, 0, static_cast<size_t>(s->context_bytes) * 32u);
    s->dcbaa[slot->slot_id] = reinterpret_cast<uint64_t>(slot->output_context);

    uint8_t* input_control = ctx_ptr(slot->input_context, s->context_bytes, 0);
    uint32_t* icc = ctx_dwords(input_control);
    icc[1] = 0x3u; // Add Slot Context + EP0 Context

    uint8_t* sc = ctx_ptr(slot->input_context, s->context_bytes, 1);
    uint32_t* sw = ctx_dwords(sc);
    sw[0] = (slot->route_string & 0x000FFFFFu) |
            (static_cast<uint32_t>(speed & 0xFu) << 20) |
            (slot->tt_hub_slot_id ? (1u << 25) : 0u) |
            (1u << 27); // route string, speed, TT flag, one endpoint context
    sw[1] = static_cast<uint32_t>(root_port) << 16; // root hub port number
    sw[2] = static_cast<uint32_t>(slot->tt_hub_slot_id) |
            (static_cast<uint32_t>(slot->tt_port_number) << 8) |
            (static_cast<uint32_t>(slot->tt_think_time & 0x3u) << 16);

    uint8_t* ep0 = ctx_ptr(slot->input_context, s->context_bytes, 2);
    uint32_t* ew = ctx_dwords(ep0);
    ew[1] = (3u << 1) | (4u << 3) | (static_cast<uint32_t>(slot->ep0_packet) << 16);
    ctx_qwords(ep0)[1] = reinterpret_cast<uint64_t>(slot->ep0_ring.trbs) | 1u;
    ew[4] = 8u;

    Trb completion{};
    if (!submit_command(s, reinterpret_cast<uint64_t>(slot->input_context), 0,
                        (11u << TRB_TYPE_SHIFT) | (static_cast<uint32_t>(slot->slot_id) << 24),
                        &completion)) return false;
    slot->addressed = true;

    // Fetch the device descriptor header. This also validates that control EP0
    // is operational before any configuration descriptors are parsed.
    uint8_t devdesc[8]{};
    uint32_t got = 0;
    if (!control_transfer(s, slot, 0x80, 6, 0x0100, 0, devdesc, sizeof(devdesc), &got) || got < 8)
        return false;
    const uint8_t advertised_mps = devdesc[7];
    if (advertised_mps && advertised_mps != slot->ep0_packet && speed < 4u) {
        // Evaluate Context updates EP0's max packet size after the first 8 bytes.
        memset(slot->input_context, 0, static_cast<size_t>(s->context_bytes) * 33u);
        memcpy(ctx_ptr(slot->input_context, s->context_bytes, 1),
               ctx_ptr(slot->output_context, s->context_bytes, 0), s->context_bytes);
        memcpy(ctx_ptr(slot->input_context, s->context_bytes, 2),
               ctx_ptr(slot->output_context, s->context_bytes, 1), s->context_bytes);
        ctx_dwords(ctx_ptr(slot->input_context, s->context_bytes, 0))[1] = 0x3u;
        uint32_t* out_ep0 = ctx_dwords(ctx_ptr(slot->input_context, s->context_bytes, 2));
        out_ep0[1] = (out_ep0[1] & 0x0000FFFFu) | (static_cast<uint32_t>(advertised_mps) << 16);
        slot->ep0_packet = advertised_mps;
        Trb ev{};
        if (!submit_command(s, reinterpret_cast<uint64_t>(slot->input_context), 0,
                            (13u << TRB_TYPE_SHIFT) | (static_cast<uint32_t>(slot->slot_id) << 24), &ev))
            return false;
    }
    return true;
}

static bool configure_hid_endpoint(ControllerState* s, SlotRuntime* slot,
                                   uint8_t iface, uint8_t protocol,
                                   uint8_t ep_addr, uint16_t mps, uint8_t interval)
{
    const bool keyboard = protocol == 1;
    const bool mouse = protocol == 2;
    if (keyboard && mps < 8) return false;
    if (mouse && mps < 3) return false;
    if (mps > 64) mps = 64;

    // Request boot protocol. A device which rejects SET_PROTOCOL may still
    // continue only if it already emits boot-protocol reports.
    (void)control_transfer(s, slot, 0x21, 0x0B, 0, iface, nullptr, 0);
    const uint8_t endpoint_number = ep_addr & 0x0Fu;
    const uint8_t dci = static_cast<uint8_t>(endpoint_number * 2u + 1u);
    if (endpoint_number == 0 || dci > 31) return false;

    HidEndpoint* ep = nullptr;
    for (unsigned i = 0; i < MAX_HID_PER_SLOT; ++i) {
        if (!slot->hid[i].active) { ep = &slot->hid[i]; break; }
    }
    if (!ep || s->hid_count >= MAX_HID_TOTAL) return false;
    memset(ep, 0, sizeof(*ep));
    if (!alloc_ring(&ep->ring, XFER_RING_TRBS)) return false;
    ep->report = static_cast<uint8_t*>(dma::alloc(mps, 64));
    if (!ep->report) { dma::free(ep->ring.trbs); ep->ring = {}; return false; }
    memset(ep->report, 0, mps);
    ep->slot_id = slot->slot_id;
    ep->dci = dci;
    ep->interface_number = iface;
    ep->max_packet = mps;
    ep->keyboard = keyboard;
    ep->mouse = mouse;

    // Preserve configured context entries and add only this endpoint.
    memset(slot->input_context, 0, static_cast<size_t>(s->context_bytes) * 33u);
    memcpy(ctx_ptr(slot->input_context, s->context_bytes, 1),
           ctx_ptr(slot->output_context, s->context_bytes, 0), s->context_bytes);
    for (unsigned d = 1; d <= 31; ++d) {
        memcpy(ctx_ptr(slot->input_context, s->context_bytes, d + 1),
               ctx_ptr(slot->output_context, s->context_bytes, d), s->context_bytes);
    }
    uint32_t* icc = ctx_dwords(ctx_ptr(slot->input_context, s->context_bytes, 0));
    icc[1] = (1u << 0) | (1u << dci);
    uint8_t* sc = ctx_ptr(slot->input_context, s->context_bytes, 1);
    uint32_t* scw = ctx_dwords(sc);
    uint32_t old_entries = (scw[0] >> 27) & 0x1Fu;
    if (dci > old_entries) old_entries = dci;
    scw[0] = (scw[0] & ~(0x1Fu << 27)) | (old_entries << 27);

    uint8_t* ec = ctx_ptr(slot->input_context, s->context_bytes, dci + 1);
    uint32_t* ecw = ctx_dwords(ec);
    uint32_t xhci_interval = interval ? interval : 1u;
    if (slot->speed <= 2) {
        const uint32_t units = xhci_interval * 8u;
        xhci_interval = 0;
        while ((1u << xhci_interval) < units && xhci_interval < 15u) ++xhci_interval;
    } else {
        if (xhci_interval > 16) xhci_interval = 16;
        if (xhci_interval) --xhci_interval;
    }
    ecw[0] = (xhci_interval & 0xFFu) << 16;
    ecw[1] = (3u << 1) | (7u << 3) | (static_cast<uint32_t>(mps) << 16);
    ctx_qwords(ec)[1] = reinterpret_cast<uint64_t>(ep->ring.trbs) | 1u;
    ecw[4] = mps;

    Trb ev{};
    if (!submit_command(s, reinterpret_cast<uint64_t>(slot->input_context), 0,
                        (12u << TRB_TYPE_SHIFT) | (static_cast<uint32_t>(slot->slot_id) << 24), &ev)) {
        dma::free(ep->report);
        dma::free(ep->ring.trbs);
        memset(ep, 0, sizeof(*ep));
        return false;
    }
    ep->active = true;
    ++s->hid_count;
    return true;
}

static bool parse_hid_interfaces(ControllerState* s, SlotRuntime* slot,
                                 const uint8_t* cfg, size_t cfg_len)
{
    if (!cfg || cfg_len < 9 || cfg[1] != 2) return false;
    const uint8_t config_value = cfg[5];
    if (!control_transfer(s, slot, 0x00, 9, config_value, 0, nullptr, 0)) return false;

    bool any = false;
    uint8_t iface = 0xFF;
    uint8_t protocol = 0;
    uint8_t subclass = 0;
    uint8_t configured_interfaces[256]{};
    for (size_t off = cfg[0]; off + 2 <= cfg_len;) {
        const uint8_t len = cfg[off];
        const uint8_t type = cfg[off + 1];
        if (len < 2 || off + len > cfg_len) return any;
        if (type == 4 && len >= 9) {
            iface = cfg[off + 2];
            subclass = cfg[off + 6];
            protocol = cfg[off + 7];
        } else if (type == 5 && len >= 7 && iface != 0xFF &&
                   (cfg[off + 2] & 0x80u) && (cfg[off + 3] & 3u) == 3u &&
                   subclass == 1 && (protocol == 1 || protocol == 2) &&
                   configured_interfaces[iface] == 0) {
            const uint16_t raw_mps = static_cast<uint16_t>(
                cfg[off + 4] | (static_cast<uint16_t>(cfg[off + 5]) << 8));
            const uint16_t mps = raw_mps & 0x07FFu;
            if (mps && configure_hid_endpoint(s, slot, iface, protocol,
                                               cfg[off + 2], mps, cfg[off + 6])) {
                configured_interfaces[iface] = 1;
                any = true;
            }
        }
        off += len;
    }
    return any;
}

static void disable_slot(ControllerState* s, SlotRuntime* slot)
{
    if (!s || !slot || !slot->slot_id) return;
    Trb ignored{};
    const uint8_t id = slot->slot_id;
    (void)submit_command(s, 0, 0,
        (10u << TRB_TYPE_SHIFT) | (static_cast<uint32_t>(id) << 24), &ignored);
    if (id <= s->max_slots) s->dcbaa[id] = 0;
    for (unsigned i = 0; i < MAX_HID_PER_SLOT; ++i) {
        HidEndpoint* ep = &slot->hid[i];
        if (ep->report) dma::free(ep->report);
        if (ep->ring.trbs) dma::free(ep->ring.trbs);
    }
    if (slot->ep0_ring.trbs) dma::free(slot->ep0_ring.trbs);
    if (slot->input_context) dma::free(slot->input_context);
    if (slot->output_context) dma::free(slot->output_context);
    memset(slot, 0, sizeof(*slot));
}

static bool configure_hub_slot(ControllerState* s, SlotRuntime* slot,
                               uint8_t ports, uint8_t think_time)
{
    if (!s || !slot || !slot->addressed || ports == 0)
        return false;
    memset(slot->input_context, 0, static_cast<size_t>(s->context_bytes) * 33u);
    memcpy(ctx_ptr(slot->input_context, s->context_bytes, 1),
           ctx_ptr(slot->output_context, s->context_bytes, 0), s->context_bytes);
    uint32_t* control = ctx_dwords(ctx_ptr(slot->input_context, s->context_bytes, 0));
    control[1] = 1u; // Add Slot Context
    uint32_t* context = ctx_dwords(ctx_ptr(slot->input_context, s->context_bytes, 1));
    context[0] |= (1u << 26); // Hub bit
    context[1] = (context[1] & 0x00FFFFFFu) | (static_cast<uint32_t>(ports) << 24);
    uint32_t* tt = ctx_dwords(ctx_ptr(slot->input_context, s->context_bytes, 1));
    tt[2] = (tt[2] & ~(0x3u << 16)) |
            (slot->speed == 3u ? (static_cast<uint32_t>(think_time & 0x3u) << 16) : 0u);
    Trb completion{};
    if (!submit_command(s, reinterpret_cast<uint64_t>(slot->input_context), 0,
                        (13u << TRB_TYPE_SHIFT) |
                        (static_cast<uint32_t>(slot->slot_id) << 24), &completion))
        return false;
    slot->is_hub = true;
    slot->hub_port_count = ports;
    slot->hub_tt_think_time = slot->speed == 3u ? static_cast<uint8_t>(think_time & 0x3u) : 0;
    return true;
}

static bool hub_port_status(ControllerState* s, SlotRuntime* hub_slot, uint8_t port,
                            usb::hub::PortStatus* out)
{
    uint8_t raw[4]{};
    uint32_t got = 0;
    if (!control_transfer(s, hub_slot, 0xA3, 0, 0, port, raw, sizeof(raw), &got) || got < 4)
        return false;
    return usb::hub::decode_port_status(raw, sizeof(raw), out);
}

static void hub_clear_port_changes(ControllerState* s, SlotRuntime* hub_slot,
                                   uint8_t port, uint16_t change)
{
    // USB 2 hub C_PORT_* feature selectors are 16 through 20.
    static const uint16_t status_change_bits[] = {1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4};
    for (uint16_t i = 0; i < 5; ++i) {
        if (change & status_change_bits[i])
            (void)control_transfer(s, hub_slot, 0x23, 1, static_cast<uint16_t>(16u + i), port, nullptr, 0);
    }
}

static bool reset_hub_port(ControllerState* s, SlotRuntime* hub_slot, uint8_t port,
                           uint8_t* child_speed)
{
    if (!s || !hub_slot || !child_speed || port == 0)
        return false;
    usb::hub::PortStatus ps{};
    if (!hub_port_status(s, hub_slot, port, &ps) || !ps.connected)
        return false;
    hub_clear_port_changes(s, hub_slot, port, ps.change);
    // SET_FEATURE(PORT_RESET) to the hub's port recipient.
    if (!control_transfer(s, hub_slot, 0x23, 3, 4, port, nullptr, 0))
        return false;
    for (uint32_t i = 0; i < 2000000u; ++i) {
        if (!hub_port_status(s, hub_slot, port, &ps)) {
            pause_cpu();
            continue;
        }
        if (!ps.connected)
            return false;
        if (!ps.resetting && ps.enabled) {
            hub_clear_port_changes(s, hub_slot, port, ps.change);
            *child_speed = ps.xhci_speed_id;
            return true;
        }
        if ((i & 0x3FFu) == 0)
            pause_cpu();
    }
    return false;
}

static bool enumerate_device(ControllerState* s, uint8_t root_port,
                             const usb::hub::Route& route, uint8_t speed,
                             uint8_t tt_hub_slot_id, uint8_t tt_port_number,
                             uint8_t tt_think_time);

static bool enumerate_hub_ports(ControllerState* s, SlotRuntime* hub_slot,
                                uint8_t power_good_2ms)
{
    if (!s || !hub_slot || !hub_slot->is_hub)
        return false;

    // Power every declared port. USB 2 hub descriptors express power-good time
    // in 2 ms units; the bounded delay is conservative on common x86 systems.
    for (uint16_t port = 1; port <= hub_slot->hub_port_count; ++port) {
        (void)control_transfer(s, hub_slot, 0x23, 3, 8, port, nullptr, 0);
    }
    const uint64_t power_good_ms = static_cast<uint64_t>(power_good_2ms ? power_good_2ms : 1u) * 2u;
    bool delayed = false;
    if (hpet_timer::available())
        delayed = hpet_timer::busy_wait_ns(power_good_ms * 1000000ULL, 50000000ULL);
    if (!delayed) {
        // Fallback for platforms without a usable HPET. Keep the work bounded;
        // the status/reset handshake below still rejects an unready port.
        const uint64_t delay_loops = power_good_ms * 250000ULL;
        for (volatile uint64_t d = 0; d < delay_loops; ++d)
            pause_cpu();
    }

    bool any_child = false;
    for (uint16_t port = 1; port <= hub_slot->hub_port_count && s->hid_count < MAX_HID_TOTAL; ++port) {
        if (port > 15) // route strings have four bits for each downstream hub-port number
            continue;
        usb::hub::PortStatus ps{};
        if (!hub_port_status(s, hub_slot, static_cast<uint8_t>(port), &ps))
            continue;
        hub_clear_port_changes(s, hub_slot, static_cast<uint8_t>(port), ps.change);
        if (!ps.connected)
            continue;
        uint8_t child_speed = 0;
        if (!reset_hub_port(s, hub_slot, static_cast<uint8_t>(port), &child_speed))
            continue;
        usb::hub::Route child_route{};
        const usb::hub::Route parent_route{hub_slot->route_string, hub_slot->route_depth};
        if (!usb::hub::append_route(parent_route, static_cast<uint8_t>(port), &child_route))
            continue;
        uint8_t tt_slot = 0, tt_port = 0, tt_time = 0;
        if (child_speed <= 2u) {
            if (hub_slot->speed == 3u) {
                tt_slot = hub_slot->slot_id;
                tt_port = static_cast<uint8_t>(port);
                tt_time = hub_slot->hub_tt_think_time;
            } else if (hub_slot->tt_hub_slot_id != 0) {
                tt_slot = hub_slot->tt_hub_slot_id;
                tt_port = hub_slot->tt_port_number;
                tt_time = hub_slot->tt_think_time;
            }
        }
        if (enumerate_device(s, hub_slot->root_port, child_route, child_speed,
                             tt_slot, tt_port, tt_time))
            any_child = true;
    }
    return any_child;
}

static bool enumerate_device(ControllerState* s, uint8_t root_port,
                             const usb::hub::Route& route, uint8_t speed,
                             uint8_t tt_hub_slot_id, uint8_t tt_port_number,
                             uint8_t tt_think_time)
{
    if (!s || root_port == 0 || speed == 0 || route.depth > 5)
        return false;
    uint8_t slot_id = 0;
    if (!enable_slot(s, &slot_id)) return false;
    SlotRuntime* slot = &s->slots[slot_id];
    memset(slot, 0, sizeof(*slot));
    slot->slot_id = slot_id;
    if (!address_device(s, slot, root_port, route.string, route.depth, speed,
                        tt_hub_slot_id, tt_port_number, tt_think_time)) {
        disable_slot(s, slot);
        return false;
    }

    uint8_t devdesc[18]{};
    uint32_t got = 0;
    if (!control_transfer(s, slot, 0x80, 6, 0x0100, 0, devdesc, sizeof(devdesc), &got) || got < sizeof(devdesc)) {
        disable_slot(s, slot);
        return false;
    }

    uint8_t cfg_head[9]{};
    got = 0;
    if (!control_transfer(s, slot, 0x80, 6, 0x0200, 0, cfg_head, sizeof(cfg_head), &got) || got < 9) {
        disable_slot(s, slot);
        return false;
    }
    const uint16_t total = static_cast<uint16_t>(cfg_head[2] | (static_cast<uint16_t>(cfg_head[3]) << 8));
    if (total < 9 || total > 1024) { disable_slot(s, slot); return false; }
    uint8_t* cfg = static_cast<uint8_t*>(dma::alloc(total, 64));
    if (!cfg) { disable_slot(s, slot); return false; }
    memset(cfg, 0, total);
    got = 0;
    const bool read_ok = control_transfer(s, slot, 0x80, 6, 0x0200, 0, cfg, total, &got) && got >= 9;
    bool has_hub_interface = false;
    if (read_ok) {
        for (size_t off = 0; off + 2 <= got;) {
            const uint8_t len = cfg[off];
            if (len < 2 || off + len > got) break;
            if (cfg[off + 1] == 4 && len >= 9 && cfg[off + 5] == 9)
                has_hub_interface = true;
            off += len;
        }
    }

    bool retained = false;
    if (read_ok && (devdesc[4] == 9 || has_hub_interface)) {
        const uint8_t config_value = cfg[5];
        uint8_t hub_desc[64]{};
        uint32_t hub_got = 0;
        uint8_t hub_ports = 0, power_good = 0;
        if (control_transfer(s, slot, 0x00, 9, config_value, 0, nullptr, 0) &&
            control_transfer(s, slot, 0xA0, 6, 0x2900, 0, hub_desc, sizeof(hub_desc), &hub_got) &&
            usb::hub::validate_hub_descriptor(hub_desc, hub_got, &hub_ports, &power_good) &&
            configure_hub_slot(s, slot, hub_ports,
                static_cast<uint8_t>(((hub_desc[3] | (static_cast<uint16_t>(hub_desc[4]) << 8)) >> 5) & 0x3u))) {
            retained = true;
            (void)enumerate_hub_ports(s, slot, power_good);
        }
    } else if (read_ok) {
        size_t off = 0;
        while (off + 2 <= got) {
            const uint8_t len = cfg[off];
            if (len < 2 || off + len > got) break;
            if (cfg[off + 1] == 2) {
                retained = parse_hid_interfaces(s, slot, cfg + off, got - off);
                break;
            }
            off += len;
        }
    }
    dma::free(cfg);
    if (!retained) {
        disable_slot(s, slot);
        return false;
    }
    return true;
}

static bool enumerate_port(ControllerState* s, uint8_t port)
{
    if (!s || !s->owner) return false;
    const uint32_t status = read_port_status(s->owner, port);
    if ((status & PORTSC_CCS) == 0) return false;
    if (!reset_port(s->owner, port)) return false;
    const uint32_t after = read_port_status(s->owner, port);
    const uint8_t speed = static_cast<uint8_t>((after >> 10) & 0x0Fu);
    if (speed == 0) return false;
    const usb::hub::Route root_route{0, 0};
    return enumerate_device(s, port, root_route, speed, 0, 0, 0);
}

static void release_controller_allocations(ControllerState* s)
{
    if (!s) return;
    for (unsigned slot_id = 1; slot_id <= MAX_SLOTS; ++slot_id) {
        SlotRuntime* slot = &s->slots[slot_id];
        for (unsigned i = 0; i < MAX_HID_PER_SLOT; ++i) {
            if (slot->hid[i].report) dma::free(slot->hid[i].report);
            if (slot->hid[i].ring.trbs) dma::free(slot->hid[i].ring.trbs);
        }
        if (slot->ep0_ring.trbs) dma::free(slot->ep0_ring.trbs);
        if (slot->input_context) dma::free(slot->input_context);
        if (slot->output_context) dma::free(slot->output_context);
    }
    for (uint32_t i = 0; i < s->scratch_pages_allocated; ++i)
        if (s->scratch_pages && s->scratch_pages[i]) dma::free(s->scratch_pages[i]);
    if (s->scratch_pages) dma::free(s->scratch_pages);
    if (s->scratch_array) dma::free(s->scratch_array);
    if (s->dcbaa) dma::free(s->dcbaa);
    if (s->event_ring) dma::free(s->event_ring);
    if (s->erst) dma::free(s->erst);
    if (s->command_ring.trbs) dma::free(s->command_ring.trbs);
    memset(s, 0, sizeof(*s));
}

static bool configure_controller(Controller* c, ControllerState* s)
{
    if (!c || !c->mmio || !s) return false;
    if (!reset(c)) return false;
    uint32_t hcs1 = read32(c->mmio, 0x04);
    uint32_t hcs2 = read32(c->mmio, 0x08);
    const uint32_t advertised_slots = hcs1 & 0xFFu;
    s->max_slots = static_cast<uint8_t>(advertised_slots > MAX_SLOTS ? MAX_SLOTS : advertised_slots);
    if (!s->max_slots) return false;
    s->context_bytes = c->context_bytes;
    s->max_ports = c->max_ports;

    if ((read32(c->mmio, c->cap_length + REG_PAGESIZE) & 1u) == 0)
        return false; // this implementation allocates 4 KiB-aligned pages

    s->dcbaa = static_cast<uint64_t*>(dma::alloc(sizeof(uint64_t) * (static_cast<size_t>(s->max_slots) + 1u), 64));
    s->event_ring = static_cast<Trb*>(dma::alloc(sizeof(Trb) * EVT_RING_TRBS, 4096));
    s->erst = static_cast<ErstEntry*>(dma::alloc(sizeof(ErstEntry), 64));
    s->scratch_array = nullptr;
    if (!s->dcbaa || !s->event_ring || !s->erst || !alloc_ring(&s->command_ring, CMD_RING_TRBS)) return false;
    memset(s->dcbaa, 0, sizeof(uint64_t) * (static_cast<size_t>(s->max_slots) + 1u));
    memset(s->event_ring, 0, sizeof(Trb) * EVT_RING_TRBS);
    memset(s->erst, 0, sizeof(ErstEntry));
    s->event_index = 0;
    s->event_cycle = 1;

    // HCSPARAMS2 encodes Scratchpad Buffers Hi in bits 25:21 and
    // Scratchpad Buffers Lo in bits 31:27. The Hi field is the upper part.
    const uint32_t scratch_count = (((hcs2 >> 21) & 0x1Fu) << 5) |
                                  ((hcs2 >> 27) & 0x1Fu);
    if (scratch_count) {
        s->scratch_array = static_cast<uint64_t*>(dma::alloc(sizeof(uint64_t) * scratch_count, 4096));
        s->scratch_pages = static_cast<void**>(dma::alloc(sizeof(void*) * scratch_count, 4096));
        if (!s->scratch_array || !s->scratch_pages) return false;
        memset(s->scratch_array, 0, sizeof(uint64_t) * scratch_count);
        memset(s->scratch_pages, 0, sizeof(void*) * scratch_count);
        for (uint32_t i = 0; i < scratch_count; ++i) {
            void* page = dma::alloc(4096, 4096);
            if (!page) return false;
            memset(page, 0, 4096);
            s->scratch_pages[i] = page;
            ++s->scratch_pages_allocated;
            s->scratch_array[i] = reinterpret_cast<uint64_t>(page);
        }
        s->dcbaa[0] = reinterpret_cast<uint64_t>(s->scratch_array);
    }

    s->erst[0].base = reinterpret_cast<uint64_t>(s->event_ring);
    s->erst[0].size = EVT_RING_TRBS;
    write64(c->mmio, c->cap_length + REG_DCBAAP, reinterpret_cast<uint64_t>(s->dcbaa));
    write64(c->mmio, c->cap_length + REG_CRCR, reinterpret_cast<uint64_t>(s->command_ring.trbs) | 1u);
    write32(c->mmio, c->cap_length + REG_CONFIG, s->max_slots);

    const uint32_t intr = (c->rtsoff & ~0x1Fu) + 0x20u;
    write32(c->mmio, intr + 0x00u, 0); // polling mode, no MSI interrupt
    write32(c->mmio, intr + 0x08u, 1);
    write64(c->mmio, intr + 0x10u, reinterpret_cast<uint64_t>(s->erst));
    write64(c->mmio, intr + 0x18u, reinterpret_cast<uint64_t>(s->event_ring) | 8u);

    // Clear interrupt enable and run the controller only after the ring setup is valid.
    uint32_t cmd = read32(c->mmio, c->cap_length + REG_USBCMD);
    cmd &= ~USBCMD_INTE;
    write32(c->mmio, c->cap_length + REG_USBCMD, cmd);
    s->initialized = true;
    s->owner = c;
    c->max_slots = s->max_slots;
    c->private_state = s;
    if (!run(c)) {
        s->initialized = false;
        c->private_state = nullptr;
        return false;
    }
    return true;
}

static uint8_t usage_to_set1(uint8_t usage, bool* extended)
{
    if (extended) *extended = false;
    static const uint8_t letters[26] = {
        0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,
        0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C
    };
    if (usage >= 0x04 && usage <= 0x1D) return letters[usage - 0x04];
    if (usage >= 0x1E && usage <= 0x27) {
        static const uint8_t digits[10] = {0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B};
        return digits[usage - 0x1E];
    }
    switch (usage) {
        case 0x28: return 0x1C; case 0x29: return 0x01; case 0x2A: return 0x0E;
        case 0x2B: return 0x0F; case 0x2C: return 0x39; case 0x2D: return 0x0C;
        case 0x2E: return 0x0D; case 0x2F: return 0x1A; case 0x30: return 0x1B;
        case 0x31: return 0x2B; case 0x33: return 0x27; case 0x34: return 0x28;
        case 0x35: return 0x29; case 0x36: return 0x33; case 0x37: return 0x34;
        case 0x38: return 0x35; case 0x39: return 0x3A; case 0x3A: return 0x3B;
        case 0x3B: return 0x3C; case 0x3C: return 0x3D; case 0x3D: return 0x3E;
        case 0x3E: return 0x3F; case 0x3F: return 0x40; case 0x40: return 0x41;
        case 0x41: return 0x42; case 0x42: return 0x43; case 0x43: return 0x44;
        case 0x44: return 0x57; case 0x45: return 0x58; case 0x4A: if (extended) *extended = true; return 0x47;
        case 0x4B: if (extended) *extended = true; return 0x4B; case 0x4C: if (extended) *extended = true; return 0x53;
        case 0x4D: if (extended) *extended = true; return 0x4D; case 0x4E: if (extended) *extended = true; return 0x4F;
        case 0x4F: if (extended) *extended = true; return 0x4D; case 0x50: if (extended) *extended = true; return 0x4B;
        case 0x51: if (extended) *extended = true; return 0x50; case 0x52: if (extended) *extended = true; return 0x48;
        case 0xE0: return 0x1D; case 0xE1: return 0x2A; case 0xE2: return 0x38;
        case 0xE3: if (extended) *extended = true; return 0x5B; case 0xE4: if (extended) *extended = true; return 0x1D;
        case 0xE5: return 0x36; case 0xE6: if (extended) *extended = true; return 0x38;
        case 0xE7: if (extended) *extended = true; return 0x5C; default: return 0;
    }
}

static void dispatch_hid_report(HidEndpoint* ep, uint32_t len)
{
    if (!ep || !ep->report) return;
    hid::InputEvent events[20]{};
    size_t count = 0;
    if (ep->keyboard) {
        if (!hid::decode_boot_keyboard(ep->report, len, &ep->keyboard_state.state,
                                       events, 20, &count)) return;
        for (size_t i = 0; i < count; ++i) {
            if (events[i].type != hid::EventType::KeyDown && events[i].type != hid::EventType::KeyUp) continue;
            bool ext = false;
            const uint8_t sc = usage_to_set1(static_cast<uint8_t>(events[i].code), &ext);
            if (sc) blockos::input::push_keyboard(sc, events[i].type == hid::EventType::KeyDown, ext);
        }
    } else if (ep->mouse) {
        if (!hid::decode_boot_mouse(ep->report, len, &ep->mouse_state.state,
                                    events, 20, &count)) return;
        for (size_t i = 0; i < count; ++i) {
            if (events[i].type == hid::EventType::PointerMove || events[i].type == hid::EventType::Wheel ||
                events[i].type == hid::EventType::ButtonDown || events[i].type == hid::EventType::ButtonUp) {
                blockos::input::push_mouse(events[i].dx, -events[i].dy, events[i].wheel,
                                           ep->mouse_state.state.buttons);
            }
        }
    }
}

static void arm_interrupt(HidEndpoint* ep)
{
    if (!ep || !ep->active || ep->pending || !ep->report) return;
    Trb normal{reinterpret_cast<uint64_t>(ep->report), ep->max_packet,
               (1u << TRB_TYPE_SHIFT) | TRB_IOC};
    Trb* trb = ring_push(&ep->ring, normal);
    if (!trb) return;
    ep->pending = true;
    Controller* c = g_state.owner;
    if (!c) { ep->pending = false; return; }
    write32(c->mmio, (c->dboff & ~3u) + ep->slot_id * 4u, ep->dci);
}

static void poll_events(ControllerState* s)
{
    for (unsigned budget = 0; budget < 128; ++budget) {
        Trb e{};
        if (!ring_pop_event(s, &e)) break;
        if (event_type(e) != 32u) continue;
        const uint8_t slot_id = event_slot(e);
        const uint8_t dci = event_endpoint(e);
        if (slot_id == 0 || slot_id > s->max_slots) continue;
        SlotRuntime* slot = &s->slots[slot_id];
        for (unsigned i = 0; i < MAX_HID_PER_SLOT; ++i) {
            HidEndpoint* ep = &slot->hid[i];
            if (!ep->active || !ep->pending || ep->dci != dci) continue;
            ep->pending = false;
            if (event_code(e) == 1u || event_code(e) == 13u) {
                const uint32_t residual = e.status & 0x00FFFFFFu;
                const uint32_t actual = residual <= ep->max_packet ? ep->max_packet - residual : 0;
                dispatch_hid_report(ep, actual);
            }
            arm_interrupt(ep);
            break;
        }
    }
}

} // namespace

bool probe(Controller* out)
{
    if (!out) return false;
    pcie::Device devices[256]{};
    const size_t found = pcie::enumerate(devices, 256);
    for (size_t i = 0; i < found && i < 256; ++i) {
        const pcie::Device& d = devices[i];
        if (d.class_code != 0x0C || d.subclass != 0x03 || d.prog_if != 0x30) continue;
        pcie::Bar bar{};
        if (!pcie::read_bar(d.bus, d.slot, d.func, 0, &bar) || bar.is_io || !bar.base) continue;
        if (!pcie::enable_memory_io(d.bus, d.slot, d.func, true)) continue;
        volatile uint8_t* mmio = reinterpret_cast<volatile uint8_t*>(static_cast<uintptr_t>(bar.base));
        const uint32_t cap0 = read32(mmio, 0x00);
        const uint8_t cap_len = static_cast<uint8_t>(cap0 & 0xFFu);
        const uint32_t hcs1 = read32(mmio, 0x04);
        const uint32_t hcc1 = read32(mmio, 0x10);
        const uint8_t ports = static_cast<uint8_t>((hcs1 >> 24) & 0xFFu);
        const uint8_t slots = static_cast<uint8_t>(hcs1 & 0xFFu);
        if (bar.size < 0x20 || cap_len < 0x20 || ports == 0 || slots == 0 ||
            read32(mmio, 0x00) == 0xFFFFFFFFu) continue;
        if (!take_legacy_ownership(mmio, hcc1, bar.size)) continue;
        out->bus = d.bus; out->slot = d.slot; out->func = d.func;
        out->mmio = mmio; out->cap_length = cap_len;
        out->version = (cap0 >> 16) & 0xFFFFu;
        out->dboff = read32(mmio, 0x14);
        out->rtsoff = read32(mmio, 0x18);
        out->max_ports = ports;
        out->context_bytes = (hcc1 & (1u << 2)) ? 64u : 32u;
        out->max_slots = slots > MAX_SLOTS ? MAX_SLOTS : slots;
        out->private_state = nullptr;
        return true;
    }
    return false;
}

bool stop(Controller* c)
{
    if (!c || !c->mmio) return false;
    volatile uint32_t* cmd = reinterpret_cast<volatile uint32_t*>(c->mmio + c->cap_length + REG_USBCMD);
    volatile uint32_t* sts = reinterpret_cast<volatile uint32_t*>(c->mmio + c->cap_length + REG_USBSTS);
    *cmd &= ~USBCMD_RS;
    return wait_register(sts, USBSTS_HCH, true);
}

bool reset(Controller* c)
{
    if (!c || !c->mmio || !stop(c)) return false;
    volatile uint32_t* cmd = reinterpret_cast<volatile uint32_t*>(c->mmio + c->cap_length + REG_USBCMD);
    volatile uint32_t* sts = reinterpret_cast<volatile uint32_t*>(c->mmio + c->cap_length + REG_USBSTS);
    *cmd |= USBCMD_HCRST;
    if (!wait_register(cmd, USBCMD_HCRST, false)) return false;
    return wait_register(sts, USBSTS_CNR, false);
}

bool run(Controller* c)
{
    if (!c || !c->mmio) return false;
    volatile uint32_t* cmd = reinterpret_cast<volatile uint32_t*>(c->mmio + c->cap_length + REG_USBCMD);
    volatile uint32_t* sts = reinterpret_cast<volatile uint32_t*>(c->mmio + c->cap_length + REG_USBSTS);
    *cmd |= USBCMD_RS;
    return wait_register(sts, USBSTS_HCH, false);
}

uint32_t read_port_status(const Controller* c, unsigned port)
{
    if (!c || !c->mmio || port == 0 || port > c->max_ports) return 0;
    return read32(c->mmio, c->cap_length + 0x400u + static_cast<uint32_t>((port - 1u) * 0x10u));
}

bool reset_port(const Controller* c, unsigned port)
{
    if (!c || !c->mmio || port == 0 || port > c->max_ports) return false;
    const uint32_t off = c->cap_length + 0x400u + static_cast<uint32_t>((port - 1u) * 0x10u);
    uint32_t st = read32(c->mmio, off);
    if (!(st & PORTSC_CCS)) return false;
    if (!(st & PORTSC_PP)) write32(c->mmio, off, (st & ~PORTSC_CHANGE) | PORTSC_PP);
    for (volatile uint32_t i = 0; i < 500000u; ++i) pause_cpu();
    st = read32(c->mmio, off);
    const uint8_t speed = static_cast<uint8_t>((st >> 10) & 0xFu);
    // xHCI PORTSC.PR is bit 4 (bit 9 is Port Power, not reset).
    const uint32_t reset_bit = speed >= 4 ? PORTSC_WPR : PORTSC_PR;
    write32(c->mmio, off, (st & ~PORTSC_CHANGE) | reset_bit | PORTSC_PP);
    for (uint64_t i = 0; i < 4000000ULL; ++i) {
        const uint32_t value = read32(c->mmio, off);
        if ((value & reset_bit) == 0 && (value & PORTSC_CCS)) {
            for (volatile uint32_t d = 0; d < 100000u; ++d) pause_cpu();
            return (read32(c->mmio, off) & PORTSC_CCS) != 0;
        }
        if ((i & 0xFFFu) == 0) pause_cpu();
    }
    return false;
}

bool initialize(Controller* c)
{
    if (!c || !c->mmio || g_state.initialized) return g_state.initialized;
    memset(&g_state, 0, sizeof(g_state));
    if (!configure_controller(c, &g_state)) {
        // Never release DMA structures while a controller might still own them.
        if (g_state.owner == c) (void)stop(c);
        release_controller_allocations(&g_state);
        c->private_state = nullptr;
        return false;
    }
    // Root-port scanning is deliberately best-effort: non-HID devices and
    // unsupported hubs do not prevent the controller from working.
    for (unsigned port = 1; port <= c->max_ports && port <= 255 && g_state.hid_count < MAX_HID_TOTAL; ++port)
        (void)enumerate_port(&g_state, static_cast<uint8_t>(port));
    for (unsigned slot = 1; slot <= g_state.max_slots; ++slot)
        for (unsigned i = 0; i < MAX_HID_PER_SLOT; ++i)
            arm_interrupt(&g_state.slots[slot].hid[i]);
    return true;
}

void poll_input()
{
    if (!g_state.initialized || !g_state.owner) return;
    poll_events(&g_state);
}

size_t hid_endpoint_count()
{
    return g_state.hid_count;
}

} // namespace usb::xhci
