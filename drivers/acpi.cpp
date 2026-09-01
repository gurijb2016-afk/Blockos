#include "acpi.hpp"

namespace acpi
{

static bool signature_equal(
    const char* a,
    const char* b)
{
    return a[0] == b[0] &&
           a[1] == b[1] &&
           a[2] == b[2] &&
           a[3] == b[3];
}

bool checksum_ok(
    const void* address,
    uint32_t length)
{
    if (address == nullptr || length == 0)
        return false;

    const uint8_t* bytes =
        static_cast<const uint8_t*>(address);

    uint8_t sum = 0;

    for (uint32_t i = 0; i < length; ++i)
        sum = static_cast<uint8_t>(sum + bytes[i]);

    return sum == 0;
}

static bool rsdp_checksum_ok(
    const Rsdp* r)
{
    if (r == nullptr)
        return false;

    for (int i = 0; i < 8; ++i)
    {
        if (r->signature[i] != "RSD PTR "[i])
            return false;
    }

    if (!checksum_ok(r, 20))
        return false;

    if (r->revision >= 2)
    {
        if (r->length < sizeof(Rsdp))
            return false;

        if (!checksum_ok(r, r->length))
            return false;
    }

    return true;
}

const SdtHeader*
find_table(
    const Rsdp* r,
    const char signature[4])
{
    if (r == nullptr || signature == nullptr)
        return nullptr;

    if (!rsdp_checksum_ok(r))
        return nullptr;

    /*
     * ACPI 2.0+
     *
     * XSDT contains 64-bit physical addresses.
     */
    if (r->revision >= 2 && r->xsdt != 0)
    {
        const SdtHeader* xsdt =
            reinterpret_cast<const SdtHeader*>(
                static_cast<uintptr_t>(r->xsdt));

        if (xsdt->length < sizeof(SdtHeader))
            return nullptr;

        if (!checksum_ok(xsdt, xsdt->length))
            return nullptr;

        const uint32_t bytes =
            xsdt->length -
            static_cast<uint32_t>(sizeof(SdtHeader));

        const uint64_t count =
            bytes / sizeof(uint64_t);

        const uint64_t* entries =
            reinterpret_cast<const uint64_t*>(
                reinterpret_cast<const uint8_t*>(xsdt) +
                sizeof(SdtHeader));

        for (uint64_t i = 0; i < count; ++i)
        {
            const uint64_t address = entries[i];

            if (address == 0)
                continue;

            const SdtHeader* table =
                reinterpret_cast<const SdtHeader*>(
                    static_cast<uintptr_t>(address));

            if (table == nullptr)
                continue;

            if (table->length < sizeof(SdtHeader))
                continue;

            if (!signature_equal(table->sig, signature))
                continue;

            if (!checksum_ok(table, table->length))
                continue;

            return table;
        }

        return nullptr;
    }

    /*
     * ACPI 1.0 fallback.
     *
     * RSDT contains 32-bit physical addresses.
     */
    if (r->rsdt != 0)
    {
        const SdtHeader* rsdt =
            reinterpret_cast<const SdtHeader*>(
                static_cast<uintptr_t>(r->rsdt));

        if (rsdt->length < sizeof(SdtHeader))
            return nullptr;

        if (!checksum_ok(rsdt, rsdt->length))
            return nullptr;

        const uint32_t bytes =
            rsdt->length -
            static_cast<uint32_t>(sizeof(SdtHeader));

        const uint32_t count =
            bytes / sizeof(uint32_t);

        const uint32_t* entries =
            reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const uint8_t*>(rsdt) +
                sizeof(SdtHeader));

        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t address = entries[i];

            if (address == 0)
                continue;

            const SdtHeader* table =
                reinterpret_cast<const SdtHeader*>(
                    static_cast<uintptr_t>(address));

            if (table == nullptr)
                continue;

            if (table->length < sizeof(SdtHeader))
                continue;

            if (!signature_equal(table->sig, signature))
                continue;

            if (!checksum_ok(table, table->length))
                continue;

            return table;
        }
    }

    return nullptr;
}

const Madt*
find_madt(const Rsdp* rsdp)
{
    return reinterpret_cast<const Madt*>(
        find_table(rsdp, "APIC"));
}

const Mcfg*
find_mcfg(const Rsdp* rsdp)
{
    return reinterpret_cast<const Mcfg*>(
        find_table(rsdp, "MCFG"));
}

bool parse_madt(
    const Rsdp* rsdp,
    uint32_t* lapic_address,
    uint32_t* cpu_count,
    uint32_t* ioapic_count)
{
    if (lapic_address == nullptr ||
        cpu_count == nullptr ||
        ioapic_count == nullptr)
        return false;

    *lapic_address = 0;
    *cpu_count = 0;
    *ioapic_count = 0;

    const Madt* madt = find_madt(rsdp);

    if (madt == nullptr)
        return false;

    if (madt->header.length < sizeof(Madt))
        return false;

    *lapic_address =
        madt->local_apic_address;

    const uint8_t* start =
        reinterpret_cast<const uint8_t*>(madt) +
        sizeof(Madt);

    const uint8_t* end =
        reinterpret_cast<const uint8_t*>(madt) +
        madt->header.length;

    const uint8_t* p = start;

    while (p + sizeof(MadtEntryHeader) <= end)
    {
        const MadtEntryHeader* entry =
            reinterpret_cast<const MadtEntryHeader*>(p);

        if (entry->length < sizeof(MadtEntryHeader))
            break;

        if (p + entry->length > end)
            break;

        switch (entry->type)
        {
            case 0:
            {
                if (entry->length >=
                    sizeof(MadtLocalApic))
                {
                    const MadtLocalApic* cpu =
                        reinterpret_cast<const MadtLocalApic*>(
                            p);

                    /*
                     * ACPI 0x1 means processor enabled.
                     * ACPI 0x2 means online-capable.
                     */
                    if ((cpu->flags & 0x3U) != 0)
                        ++(*cpu_count);
                }

                break;
            }

            case 1:
                ++(*ioapic_count);
                break;

            default:
                break;
        }

        p += entry->length;
    }

    return true;
}

bool parse_mcfg(
    const Rsdp* rsdp,
    uint64_t* ecam_base,
    uint16_t* segment,
    uint8_t* start_bus,
    uint8_t* end_bus)
{
    if (ecam_base == nullptr ||
        segment == nullptr ||
        start_bus == nullptr ||
        end_bus == nullptr)
        return false;

    *ecam_base = 0;
    *segment = 0;
    *start_bus = 0;
    *end_bus = 0;

    const Mcfg* mcfg = find_mcfg(rsdp);

    if (mcfg == nullptr)
        return false;

    if (mcfg->header.length < sizeof(Mcfg))
        return false;

    const uint32_t allocation_bytes =
        mcfg->header.length -
        static_cast<uint32_t>(sizeof(Mcfg));

    const uint32_t count =
        allocation_bytes /
        static_cast<uint32_t>(sizeof(McfgAllocation));

    if (count == 0)
        return false;

    const McfgAllocation* allocations =
        reinterpret_cast<const McfgAllocation*>(
            reinterpret_cast<const uint8_t*>(mcfg) +
            sizeof(Mcfg));

    const McfgAllocation& first =
        allocations[0];

    if (first.base_address == 0)
        return false;

    *ecam_base =
        first.base_address;

    *segment =
        first.segment_group;

    *start_bus =
        first.start_bus;

    *end_bus =
        first.end_bus;

    return true;
}

}
