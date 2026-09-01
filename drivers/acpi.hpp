#pragma once
#include <stdint.h>
#include <stddef.h>
namespace acpi {
#pragma pack(push,1)
struct SdtHeader {char sig[4];uint32_t length;uint8_t revision;uint8_t checksum;char oemid[6];char oem_table_id[8];uint32_t oem_revision;uint32_t creator_id;uint32_t creator_revision;};
struct Rsdp {char sig[8];uint8_t checksum;char oemid[6];uint8_t revision;uint32_t rsdt;uint32_t length;uint64_t xsdt;uint8_t ext_checksum;uint8_t reserved[3];};
struct MadtEntry {uint8_t type;uint8_t length;};
struct MadtLocalApic {uint8_t type,length;uint8_t acpi_id;uint8_t apic_id;uint32_t flags;};
struct MadtIoApic {uint8_t type,length;uint8_t id;uint8_t reserved;uint32_t address;uint32_t gsi_base;};
#pragma pack(pop)
bool checksum_ok(const void*,size_t);const SdtHeader* find_table(const Rsdp*,const char[4]);
size_t enumerate_cpus(const Rsdp*,uint32_t* apic_ids,size_t cap);const MadtIoApic* first_ioapic(const Rsdp*);
}
