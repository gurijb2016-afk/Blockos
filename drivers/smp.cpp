#include "smp.hpp"
namespace smp {
static Cpu cpus[64]{}; static size_t count=0; static volatile uint32_t* lapic=(volatile uint32_t*)(uintptr_t)0xFEE00000u;
static inline void mmio(uint32_t r,uint32_t v){lapic[r/4]=v;} static inline void delay(){for(volatile unsigned i=0;i<100000;i++)__asm__ __volatile__("pause");}
void init(const uint32_t*ids,size_t n){count=n>64?64:n;for(size_t i=0;i<count;i++){cpus[i].apic_id=ids[i];cpus[i].index=i;cpus[i].started=(i==0);}}
size_t cpu_count(){return count;}const Cpu*cpu(size_t i){return i<count?&cpus[i]:nullptr;}
void apic_eoi(){mmio(0xB0,0);} void apic_send_init(uint32_t id){mmio(0x310,id);mmio(0x300,0x4500);delay();} void apic_send_sipi(uint32_t id,uint8_t v){mmio(0x310,id);mmio(0x300,0x4600|v);delay();}
bool start_secondary(uint32_t id,void(*entry)()){for(size_t i=0;i<count;i++)if(cpus[i].apic_id==id&&entry){cpus[i].entry=entry;cpus[i].started=0;apic_send_init(id);apic_send_sipi(id,0x08);apic_send_sipi(id,0x08);return true;}return false;}
}
