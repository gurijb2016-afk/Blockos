#pragma once
#include <stdint.h>
#include <stddef.h>
namespace smp {
struct Cpu {uint32_t apic_id;uint32_t index;volatile uint32_t started;void(*entry)();};
void init(const uint32_t*apic_ids,size_t count);size_t cpu_count();const Cpu* cpu(size_t index);void apic_eoi();void apic_send_init(uint32_t apic_id);void apic_send_sipi(uint32_t apic_id,uint8_t vector);bool start_secondary(uint32_t apic_id,void(*entry)());
}
