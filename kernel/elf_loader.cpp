#include "elf_loader.hpp"
#include "arch/86_64x/paging.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace {
struct Ehdr{unsigned char id[16];uint16_t type,machine;uint32_t version;uint64_t entry,phoff,shoff;uint32_t flags;uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;};
struct Phdr{uint32_t type,flags;uint64_t offset,vaddr,paddr,filesz,memsz,align;};
constexpr uint32_t PT_LOAD=1,PT_INTERP=3; constexpr uint16_t ET_EXEC=2,ET_DYN=3,EM_X86_64=62;
constexpr uint32_t PF_X=1,PF_W=2,PF_R=4; constexpr uint64_t U=4,W=2,NX=1ULL<<63;
constexpr uint64_t PAGE=0x1000,USER_TOP=0x00007ffffffff000ULL,STACK_TOP=0x00007ffffff00000ULL,STACK_PAGES=32;
static bool add_ok(uint64_t a,uint64_t b,uint64_t* out){if(a+b<a)return false;*out=a+b;return true;}
}

bool elf_loader::load_elf64_from_mem(const void* buf,size_t size,uint64_t* entry_out,uint64_t* pml4_out,uint64_t* stack_out){
    if(!buf||size<sizeof(Ehdr)||!entry_out||!pml4_out)return false;
    const Ehdr* e=(const Ehdr*)buf;
    if(e->id[0]!=0x7f||e->id[1]!='E'||e->id[2]!='L'||e->id[3]!='F'||e->id[4]!=2||e->id[5]!=1)return false;
    if(e->machine!=EM_X86_64||(e->type!=ET_EXEC&&e->type!=ET_DYN)||e->version!=1)return false;
    if(e->ehsize<sizeof(Ehdr)||e->phentsize!=sizeof(Phdr)||e->phnum==0||e->phnum>128)return false;
    uint64_t phend=0;if(!add_ok(e->phoff,(uint64_t)e->phnum*sizeof(Phdr),&phend)||phend>size)return false;
    const Phdr* ph=(const Phdr*)((const uint8_t*)buf+e->phoff);
    uint64_t pml4=paging::clone_current_pml4();if(!pml4)return false;
    bool loaded=false;
    for(uint16_t i=0;i<e->phnum;i++){
        if(ph[i].type==PT_INTERP)return false;
        if(ph[i].type!=PT_LOAD)continue;
        const Phdr& p=ph[i]; if(p.memsz<p.filesz||p.align && (p.align&(p.align-1)))return false;
        uint64_t fend=0;if(!add_ok(p.offset,p.filesz,&fend)||fend>size)return false;
        uint64_t vend=0;if(!add_ok(p.vaddr,p.memsz,&vend)||vend>USER_TOP)return false;
        if(p.vaddr<0x1000 || vend<=p.vaddr)return false;
        uint64_t base=p.vaddr&~(PAGE-1),end=(vend+PAGE-1)&~(PAGE-1); if(end<vend)return false;
        uint64_t flags=U; if(p.flags&PF_W)flags|=W; if(!(p.flags&PF_X))flags|=NX;
        for(uint64_t va=base;va<end;va+=PAGE){
            void* page=paging::alloc_page();if(!page)return false;
            if(!paging::map_4k(pml4,va,(uint64_t)(uintptr_t)page,flags))return false;
            uint64_t page_lo=va,page_hi=va+PAGE;
            uint64_t src_lo=p.vaddr,src_hi=p.vaddr+p.filesz;
            uint64_t lo=page_lo>src_lo?page_lo:src_lo,hi=page_hi<src_hi?page_hi:src_hi;
            if(hi>lo){size_t n=(size_t)(hi-lo);memcpy((uint8_t*)page+(lo-va),(const uint8_t*)buf+p.offset+(lo-p.vaddr),n);}
        }
        loaded=true;
    }
    if(!loaded)return false;
    uint64_t stack_base=STACK_TOP-STACK_PAGES*PAGE;
    if(!paging::map_user_range(pml4,stack_base,STACK_PAGES*PAGE,U|W|NX))return false;
    if(e->entry<0x1000||e->entry>=USER_TOP)return false;
    *entry_out=e->entry;*pml4_out=pml4;if(stack_out)*stack_out=STACK_TOP-16;
    return true;
}
