#pragma once

#include <stdint.h>
#include <stddef.h>

namespace acpi
{

struct Rsdp
{
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt;
    uint32_t length;
    uint64_t xsdt;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed));

struct SdtHeader
{
    char     sig[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct GenericAddress
{
    uint8_t  address_space;
    uint8_t  bit_width;
    uint8_t  bit_offset;
    uint8_t  access_size;
    uint64_t address;
} __attribute__((packed));

struct MadtEntryHeader
{
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct Madt
{
    SdtHeader header;
    uint32_t  local_apic_address;
    uint32_t  flags;
} __attribute__((packed));

struct MadtLocalApic
{
    MadtEntryHeader header;
    uint8_t  processor_id;
    uint8_t  apic_id;
    uint32_t flags;
} __attribute__((packed));

struct MadtIoApic
{
    MadtEntryHeader header;
    uint8_t  ioapic_id;
    uint8_t  reserved;
    uint32_t ioapic_address;
    uint32_t global_system_interrupt_base;
} __attribute__((packed));

struct McfgAllocation
{
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} __attribute__((packed));

struct Mcfg
{
    SdtHeader header;
    uint8_t reserved[8];
} __attribute__((packed));

bool checksum_ok(const void* address, uint32_t length);

const SdtHeader*
find_table(const Rsdp* rsdp, const char signature[4]);

const Madt*
find_madt(const Rsdp* rsdp);

const Mcfg*
find_mcfg(const Rsdp* rsdp);

bool parse_madt(
    const Rsdp* rsdp,
    uint32_t* lapic_address,
    uint32_t* cpu_count,
    uint32_t* ioapic_count);

bool parse_mcfg(
    const Rsdp* rsdp,
    uint64_t* ecam_base,
    uint16_t* segment,
    uint8_t* start_bus,
    uint8_t* end_bus);

}
