#include "pcie.hpp"
#include "pci.hpp"

namespace pcie
{

static uint16_t read_command(
    uint8_t bus,
    uint8_t slot,
    uint8_t func)
{
    return pci_cfg_read16(
        bus,
        slot,
        func,
        0x04);
}

size_t enumerate(
    Device* out,
    size_t capacity)
{
    size_t count = 0;

    for (unsigned bus = 0; bus < 256; ++bus)
    {
        for (unsigned slot = 0; slot < 32; ++slot)
        {
            const uint8_t bus8 =
                static_cast<uint8_t>(bus);

            const uint8_t slot8 =
                static_cast<uint8_t>(slot);

            const uint8_t header_type =
                pci_cfg_read8(
                    bus8,
                    slot8,
                    0,
                    0x0E);

            const unsigned function_count =
                (header_type & 0x80U) != 0
                    ? 8U
                    : 1U;

            for (unsigned func = 0;
                 func < function_count;
                 ++func)
            {
                const uint8_t func8 =
                    static_cast<uint8_t>(func);

                if (!pci_device_exists(
                        bus8,
                        slot8,
                        func8))
                {
                    continue;
                }

                if (out != nullptr &&
                    count < capacity)
                {
                    Device& d = out[count];

                    d.bus = bus8;
                    d.slot = slot8;
                    d.func = func8;

                    d.vendor =
                        pci_cfg_read16(
                            bus8,
                            slot8,
                            func8,
                            0x00);

                    d.device =
                        pci_cfg_read16(
                            bus8,
                            slot8,
                            func8,
                            0x02);

                    d.class_code =
                        pci_cfg_read8(
                            bus8,
                            slot8,
                            func8,
                            0x0B);

                    d.subclass =
                        pci_cfg_read8(
                            bus8,
                            slot8,
                            func8,
                            0x0A);

                    d.prog_if =
                        pci_cfg_read8(
                            bus8,
                            slot8,
                            func8,
                            0x09);

                    d.header_type =
                        pci_cfg_read8(
                            bus8,
                            slot8,
                            func8,
                            0x0E);

                    d.irq_line =
                        pci_cfg_read8(
                            bus8,
                            slot8,
                            func8,
                            0x3C);

                    d.irq_pin =
                        pci_cfg_read8(
                            bus8,
                            slot8,
                            func8,
                            0x3D);
                }

                ++count;
            }
        }
    }

    return count;
}

bool read_bar(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    unsigned index,
    Bar* out)
{
    if (out == nullptr)
        return false;

    if (index >= 6)
        return false;

    const uint8_t offset =
        static_cast<uint8_t>(
            0x10U + index * 4U);

    const uint32_t original_low =
        pci_cfg_read32(
            bus,
            slot,
            func,
            offset);

    if (original_low == 0)
    {
        out->base = 0;
        out->size = 0;
        out->is_io = false;
        out->is_64 = false;
        out->prefetchable = false;
        return true;
    }

    const bool io_space =
        (original_low & 0x1U) != 0;

    if (io_space)
    {
        pci_cfg_write32(
            bus,
            slot,
            func,
            offset,
            0xFFFFFFFFU);

        const uint32_t mask =
            pci_cfg_read32(
                bus,
                slot,
                func,
                offset);

        pci_cfg_write32(
            bus,
            slot,
            func,
            offset,
            original_low);

        const uint32_t base =
            original_low & ~0x3U;

        const uint32_t size_mask =
            mask & ~0x3U;

        out->base = base;

        out->size =
            size_mask != 0
                ? static_cast<uint64_t>(
                      (~size_mask) + 1U)
                : 0;

        out->is_io = true;
        out->is_64 = false;
        out->prefetchable = false;

        return true;
    }

    const uint8_t type =
        static_cast<uint8_t>(
            (original_low >> 1) & 0x3U);

    const bool is_64 =
        type == 0x2;

    const bool prefetchable =
        (original_low & 0x8U) != 0;

    uint32_t original_high = 0;
    uint32_t mask_low = 0;
    uint32_t mask_high = 0;

    if (is_64)
    {
        original_high =
            pci_cfg_read32(
                bus,
                slot,
                func,
                static_cast<uint8_t>(
                    offset + 4U));
    }

    pci_cfg_write32(
        bus,
        slot,
        func,
        offset,
        0xFFFFFFFFU);

    mask_low =
        pci_cfg_read32(
            bus,
            slot,
            func,
            offset);

    pci_cfg_write32(
        bus,
        slot,
        func,
        offset,
        original_low);

    if (is_64)
    {
        pci_cfg_write32(
            bus,
            slot,
            func,
            static_cast<uint8_t>(
                offset + 4U),
            0xFFFFFFFFU);

        mask_high =
            pci_cfg_read32(
                bus,
                slot,
                func,
                static_cast<uint8_t>(
                    offset + 4U));

        pci_cfg_write32(
            bus,
            slot,
            func,
            static_cast<uint8_t>(
                offset + 4U),
            original_high);
    }

    const uint64_t base =
        (static_cast<uint64_t>(
             original_high) << 32) |
        (static_cast<uint64_t>(
             original_low) & ~0xFULL);

    const uint64_t mask =
        (static_cast<uint64_t>(
             mask_high) << 32) |
        (static_cast<uint64_t>(
             mask_low) & ~0xFULL);

    out->base = base;

    out->size =
        mask != 0
            ? (~mask) + 1ULL
            : 0;

    out->is_io = false;
    out->is_64 = is_64;
    out->prefetchable = prefetchable;

    return true;
}

bool enable_memory_io(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    bool bus_master)
{
    uint16_t value =
        read_command(
            bus,
            slot,
            func);

    value |= 0x0001U; // I/O space
    value |= 0x0002U; // memory space

    if (bus_master)
        value |= 0x0004U;

    pci_cfg_write16(
        bus,
        slot,
        func,
        0x04,
        value);

    return true;
}

bool find_class(
    uint8_t class_code,
    uint8_t subclass,
    Device* out)
{
    if (out == nullptr)
        return false;

    for (unsigned bus = 0; bus < 256; ++bus)
    {
        for (unsigned slot = 0; slot < 32; ++slot)
        {
            const uint8_t bus8 =
                static_cast<uint8_t>(bus);

            const uint8_t slot8 =
                static_cast<uint8_t>(slot);

            const uint8_t header_type =
                pci_cfg_read8(
                    bus8,
                    slot8,
                    0,
                    0x0E);

            const unsigned function_count =
                (header_type & 0x80U) != 0
                    ? 8U
                    : 1U;

            for (unsigned func = 0;
                 func < function_count;
                 ++func)
            {
                const uint8_t func8 =
                    static_cast<uint8_t>(func);

                if (!pci_device_exists(
                        bus8,
                        slot8,
                        func8))
                {
                    continue;
                }

                const uint8_t found_class =
                    pci_cfg_read8(
                        bus8,
                        slot8,
                        func8,
                        0x0B);

                const uint8_t found_subclass =
                    pci_cfg_read8(
                        bus8,
                        slot8,
                        func8,
                        0x0A);

                if (found_class != class_code ||
                    found_subclass != subclass)
                {
                    continue;
                }

                out->bus = bus8;
                out->slot = slot8;
                out->func = func8;

                out->vendor =
                    pci_cfg_read16(
                        bus8,
                        slot8,
                        func8,
                        0x00);

                out->device =
                    pci_cfg_read16(
                        bus8,
                        slot8,
                        func8,
                        0x02);

                out->class_code = found_class;
                out->subclass = found_subclass;

                out->prog_if =
                    pci_cfg_read8(
                        bus8,
                        slot8,
                        func8,
                        0x09);

                out->header_type =
                    pci_cfg_read8(
                        bus8,
                        slot8,
                        func8,
                        0x0E);

                out->irq_line =
                    pci_cfg_read8(
                        bus8,
                        slot8,
                        func8,
                        0x3C);

                out->irq_pin =
                    pci_cfg_read8(
                        bus8,
                        slot8,
                        func8,
                        0x3D);

                return true;
            }
        }
    }

    return false;
}

}
