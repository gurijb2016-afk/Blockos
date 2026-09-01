#include "usb_xhci.hpp"
#include "pcie.hpp"

namespace usb::xhci {
static inline uint32_t rd32(volatile uint8_t* p,uint32_t o){return *reinterpret_cast<volatile uint32_t*>(p+o);} 
static inline void wr32(volatile uint8_t* p,uint32_t o,uint32_t v){*reinterpret_cast<volatile uint32_t*>(p+o)=v;}

bool probe(Controller* out){
    if(!out)return false; pcie::Device d{}; if(!pcie::find_class(0x0C,0x03,&d)) return false;
    uint8_t pi=pci_cfg_read8(d.bus,d.slot,d.func,0x09); if(pi!=0x30) return false;
    pcie::Bar bar{}; if(!pcie::read_bar(d.bus,d.slot,d.func,0,&bar)||bar.is_io||bar.base==0) return false; pcie::enable_memory_io(d.bus,d.slot,d.func,true);
    auto* mmio=reinterpret_cast<volatile uint8_t*>(bar.base); uint8_t cap=rd32(mmio,0)&0xFF; uint32_t hcs1=rd32(mmio,4); uint32_t hcc1=rd32(mmio,0x10); out->bus=d.bus;out->slot=d.slot;out->func=d.func;out->mmio=mmio;out->cap_length=cap;out->version=uint32_t(rd32(mmio,2)&0xFFFF);out->dboff=rd32(mmio,0x14);out->rtsoff=rd32(mmio,0x18);out->max_ports=uint8_t((hcs1>>24)&0xFF);out->context_bytes=(hcc1&(1u<<2))?64:32; return true;
}

bool stop(Controller* c){if(!c||!c->mmio)return false; volatile uint32_t* op=reinterpret_cast<volatile uint32_t*>(c->mmio+c->cap_length); *op&=~1u; for(volatile unsigned i=0;i<1000000;i++){if((*op&1)==0)return true;} return false;}
bool reset(Controller* c){if(!c)return false;if(!stop(c))return false; volatile uint32_t* op=reinterpret_cast<volatile uint32_t*>(c->mmio+c->cap_length); *op|=2u; for(volatile unsigned i=0;i<1000000;i++){if((*op&2)==0)return true;} return false;}
bool run(Controller* c){if(!c)return false; volatile uint32_t* op=reinterpret_cast<volatile uint32_t*>(c->mmio+c->cap_length); *op|=1u; return true;}
uint32_t read_port_status(const Controller* c,unsigned port){if(!c||port==0||port>c->max_ports)return 0; volatile uint32_t* p=reinterpret_cast<volatile uint32_t*>(c->mmio+c->cap_length+0x400+(port-1)*0x10); return *p;}
bool reset_port(const Controller* c,unsigned port){if(!c||port==0||port>c->max_ports)return false; volatile uint32_t* p=reinterpret_cast<volatile uint32_t*>(c->mmio+c->cap_length+0x400+(port-1)*0x10); uint32_t v=*p; if(!(v&1))return false; *p=(v&~0x80u)|0x00000200u; for(volatile unsigned i=0;i<1000000;i++){if(((*p)&0x200)==0)return true;} return false;}
}
