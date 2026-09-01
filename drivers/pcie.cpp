#include "pcie.hpp"
#include "pci.hpp"

namespace pcie {
static uint16_t command(uint8_t b,uint8_t s,uint8_t f){return pci_cfg_read16(b,s,f,0x04);}

size_t enumerate(Device* out, size_t capacity){
    size_t n=0;
    for(unsigned b=0;b<256;b++) for(unsigned s=0;s<32;s++){
        uint8_t h0=pci_cfg_read8((uint8_t)b,(uint8_t)s,0,0x0E);
        unsigned fnmax=(h0&0x80)?8:1;
        for(unsigned f=0;f<fnmax;f++) if(pci_device_exists((uint8_t)b,(uint8_t)s,(uint8_t)f)){
            if(n<capacity){
                Device& d=out[n]; d.bus=b; d.slot=s; d.func=f;
                d.vendor=pci_cfg_read16(d.bus,d.slot,d.func,0x00);
                d.device=pci_cfg_read16(d.bus,d.slot,d.func,0x02);
                d.class_code=pci_cfg_read8(d.bus,d.slot,d.func,0x0B);
                d.subclass=pci_cfg_read8(d.bus,d.slot,d.func,0x0A);
                d.prog_if=pci_cfg_read8(d.bus,d.slot,d.func,0x09);
                d.header_type=pci_cfg_read8(d.bus,d.slot,d.func,0x0E);
                d.irq_line=pci_cfg_read8(d.bus,d.slot,d.func,0x3C);
                d.irq_pin=pci_cfg_read8(d.bus,d.slot,d.func,0x3D);
            }
            n++;
        }
    }
    return n;
}

bool read_bar(uint8_t b,uint8_t s,uint8_t f,unsigned i,Bar* out){
    if(!out || i>=6) return false;
    uint8_t off=static_cast<uint8_t>(0x10+i*4);
    uint32_t lo=pci_cfg_read32(b,s,f,off);
    if(lo==0) { *out={}; return true; }
    bool io=(lo&1)!=0;
    if(io){
        uint32_t saved=lo;
        pci_cfg_write32(b,s,f,off,0xFFFFFFFFu);
        uint32_t mask=pci_cfg_read32(b,s,f,off);
        pci_cfg_write32(b,s,f,off,saved);
        uint32_t base=lo&~0x3u;
        uint32_t m=mask&~0x3u;
        out->base=base; out->size=m?uint64_t(~m+1):0; out->is_io=true; out->is_64=false; out->prefetchable=false;
        return true;
    }
    bool is64=((lo>>1)&3)==2;
    bool prefetch=(lo&8)!=0;
    uint64_t saved=lo;
    uint64_t mask=0;
    pci_cfg_write32(b,s,f,off,0xFFFFFFFFu);
    uint32_t mlo=pci_cfg_read32(b,s,f,off);
    pci_cfg_write32(b,s,f,off,(uint32_t)saved);
    uint32_t hi=0, mhi=0;
    if(is64){
        hi=pci_cfg_read32(b,s,f,off+4);
        uint32_t hiSaved=hi;
        pci_cfg_write32(b,s,f,off+4,0xFFFFFFFFu);
        mhi=pci_cfg_read32(b,s,f,off+4);
        pci_cfg_write32(b,s,f,off+4,hiSaved);
    }
    mask=(uint64_t(mhi)<<32)|(mlo&~0xFULL);
    uint64_t base=(uint64_t(hi)<<32)|(saved&~0xFULL);
    out->base=base; out->size=mask?uint64_t(~mask+1):0; out->is_io=false; out->is_64=is64; out->prefetchable=prefetch;
    return true;
}

bool enable_memory_io(uint8_t b,uint8_t s,uint8_t f,bool bus_master){
    uint16_t c=command(b,s,f); c|=0x2|0x1; if(bus_master)c|=0x4; pci_cfg_write16(b,s,f,0x04,c); return true;
}

bool find_class(uint8_t cc,uint8_t sc,Device* out){
    if(!out) return false; Device tmp[1]; for(unsigned b=0;b<256;b++) for(unsigned s=0;s<32;s++){uint8_t h=pci_cfg_read8(b,s,0,0x0E);unsigned fm=(h&0x80)?8:1;for(unsigned f=0;f<fm;f++) if(pci_device_exists(b,s,f)&&pci_cfg_read8(b,s,f,0x0B)==cc&&pci_cfg_read8(b,s,f,0x0A)==sc){tmp[0].bus=b;tmp[0].slot=s;tmp[0].func=f;tmp[0].vendor=pci_cfg_read16(b,s,f,0);tmp[0].device=pci_cfg_read16(b,s,f,2);tmp[0].class_code=cc;tmp[0].subclass=sc;tmp[0].prog_if=pci_cfg_read8(b,s,f,9);tmp[0].header_type=pci_cfg_read8(b,s,f,0xE);tmp[0].irq_line=pci_cfg_read8(b,s,f,0x3C);tmp[0].irq_pin=pci_cfg_read8(b,s,f,0x3D);*out=tmp[0];return true;}} return false;
}
}
