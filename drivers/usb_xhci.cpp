#include "usb_xhci.hpp"
#include "pcie.hpp"

namespace usb::xhci
{

static inline uint32_t read32(
    volatile uint8_t* base,
    uint32_t offset)
{
    volatile uint32_t* reg =
        reinterpret_cast<volatile uint32_t*>(
            base + offset);

    return *reg;
}

static inline void write32(
    volatile uint8_t* base,
    uint32_t offset,
    uint32_t value)
{
    volatile uint32_t* reg =
        reinterpret_cast<volatile uint32_t*>(
            base + offset);

    *reg = value;
}

static bool wait_clear(
    volatile uint32_t* reg,
    uint32_t mask)
{
    for (volatile uint32_t i = 0;
         i < 1000000U;
         ++i)
    {
        if ((*reg & mask) == 0)
            return true;
    }

    return false;
}

bool probe(Controller* out)
{
    if (out == nullptr)
        return false;

    pcie::Device device{};

    if (!pcie::find_class(
            0x0C,
            0x03,
            &device))
    {
        return false;
    }

    /*
     * xHCI programming interface = 0x30.
     */
    if (device.prog_if != 0x30)
        return false;

    pcie::Bar bar{};

    if (!pcie::read_bar(
            device.bus,
            device.slot,
            device.func,
            0,
            &bar))
    {
        return false;
    }

    if (bar.is_io)
        return false;

    if (bar.base == 0)
        return false;

    if (!pcie::enable_memory_io(
            device.bus,
            device.slot,
            device.func,
            true))
    {
        return false;
    }

    volatile uint8_t* mmio =
        reinterpret_cast<volatile uint8_t*>(
            static_cast<uintptr_t>(
                bar.base));

    /*
     * xHCI capability registers.
     *
     * CAPLENGTH @ 0x00 byte 0
     * HCIVERSION @ 0x02
     * HCSPARAMS1 @ 0x04
     * HCCPARAMS1 @ 0x10
     * DBOFF @ 0x14
     * RTSOFF @ 0x18
     */
    const uint32_t cap0 =
        read32(mmio, 0x00);

    const uint8_t cap_length =
        static_cast<uint8_t>(
            cap0 & 0xFFU);

    const uint16_t version =
        static_cast<uint16_t>(
            (cap0 >> 16) & 0xFFFFU);

    const uint32_t hcs1 =
        read32(mmio, 0x04);

    const uint32_t hcc1 =
        read32(mmio, 0x10);

    const uint32_t dboff =
        read32(mmio, 0x14);

    const uint32_t rtsoff =
        read32(mmio, 0x18);

    /*
     * HCSPARAMS1 MaxPorts = bits 31:24.
     */
    const uint8_t max_ports =
        static_cast<uint8_t>(
            (hcs1 >> 24) & 0xFFU);

    /*
     * HCCPARAMS1 CSZ bit 2:
     * 0 = 32-byte contexts
     * 1 = 64-byte contexts
     */
    const uint8_t context_bytes =
        (hcc1 & (1U << 2)) != 0
            ? 64
            : 32;

    out->bus = device.bus;
    out->slot = device.slot;
    out->func = device.func;
    out->mmio = mmio;
    out->cap_length =
        static_cast<uint32_t>(
            cap_length);
    out->version =
        static_cast<uint32_t>(
            version);
    out->dboff = dboff;
    out->rtsoff = rtsoff;
    out->max_ports = max_ports;
    out->context_bytes = context_bytes;

    return true;
}

bool stop(Controller* controller)
{
    if (controller == nullptr)
        return false;

    if (controller->mmio == nullptr)
        return false;

    volatile uint32_t* op =
        reinterpret_cast<volatile uint32_t*>(
            controller->mmio +
            controller->cap_length);

    /*
     * USBCMD.RUN/RS bit 0.
     */
    *op &= ~1U;

    return wait_clear(
        op,
        1U);
}

bool reset(Controller* controller)
{
    if (controller == nullptr)
        return false;

    if (controller->mmio == nullptr)
        return false;

    if (!stop(controller))
        return false;

    volatile uint32_t* op =
        reinterpret_cast<volatile uint32_t*>(
            controller->mmio +
            controller->cap_length);

    /*
     * USBCMD.HCRST bit 1.
     */
    *op |= 2U;

    return wait_clear(
        op,
        2U);
}

bool run(Controller* controller)
{
    if (controller == nullptr)
        return false;

    if (controller->mmio == nullptr)
        return false;

    volatile uint32_t* op =
        reinterpret_cast<volatile uint32_t*>(
            controller->mmio +
            controller->cap_length);

    /*
     * USBCMD.RUN/RS.
     */
    *op |= 1U;

    return true;
}

uint32_t read_port_status(
    const Controller* controller,
    unsigned port)
{
    if (controller == nullptr)
        return 0;

    if (controller->mmio == nullptr)
        return 0;

    if (port == 0)
        return 0;

    if (port > controller->max_ports)
        return 0;

    /*
     * PORTSC array starts at operational + 0x400.
     * Each PORTSC is 0x10 bytes apart.
     */
    const uint32_t offset =
        controller->cap_length +
        0x400U +
        static_cast<uint32_t>(
            (port - 1U) * 0x10U);

    return read32(
        controller->mmio,
        offset);
}

bool reset_port(
    const Controller* controller,
    unsigned port)
{
    if (controller == nullptr)
        return false;

    if (controller->mmio == nullptr)
        return false;

    if (port == 0)
        return false;

    if (port > controller->max_ports)
        return false;

    const uint32_t offset =
        controller->cap_length +
        0x400U +
        static_cast<uint32_t>(
            (port - 1U) * 0x10U);

    volatile uint32_t* portsc =
        reinterpret_cast<volatile uint32_t*>(
            controller->mmio + offset);

    const uint32_t status =
        *portsc;

    /*
     * Current Connect Status (CCS), bit 0.
     */
    if ((status & 1U) == 0)
        return false;

    /*
     * USB3 ports use PRC/PLC state handling,
     * but the basic reset sequence begins with PR.
     */
    *portsc =
        (status & ~0x000001FFU) |
        0x00000200U;

    for (volatile uint32_t i = 0;
         i < 1000000U;
         ++i)
    {
        const uint32_t value =
            *portsc;

        /*
         * Port Reset (PR) clears when reset completes.
         */
        if ((value & 0x00000200U) == 0)
            return true;
    }

    return false;
}

}
