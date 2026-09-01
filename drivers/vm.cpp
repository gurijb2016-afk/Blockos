#include "vm.hpp"
#include "paging.hpp"
namespace vm {
static inline uint64_t align_down(uint64_t x){return x&~(P-1);} static inline uint64_t align_up(uint64_t x){return (x+P-1)&~(P-1);} 
bool map(uint64_t pml4,uint64_t va,uint64_t pa,uint64_t f){return paging::map_4k(pml4,va,pa,f);}
bool map_anonymous(uint64_t pml4,uint64_t va,uint64_t pages,uint64_t f){for(uint64_t i=0;i<pages;i++){void*p=paging::alloc_page();if(!p)return false;uint64_t pa=(uint64_t)(uintptr_t)p;if(!map(pml4,va+i*P,pa,f))return false;}return true;}
bool cow_clone(uint64_t parent,uint64_t child,uint64_t start,uint64_t pages){(void)parent;(void)child;(void)start;(void)pages;return false;}
bool mmap_file(uint64_t pml4,uint64_t va,uint64_t phys,uint64_t len,uint64_t f){uint64_t s=align_down(va),off=va-s,n=align_up(off+len)/P;for(uint64_t i=0;i<n;i++)if(!map(pml4,s+i*P,phys+i*P,f))return false;return true;}
}
