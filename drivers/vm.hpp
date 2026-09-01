#pragma once
#include <stdint.h>
#include <stddef.h>
namespace vm {
constexpr uint64_t P=4096; constexpr uint64_t PRESENT=1, WRITE=2, USER=4, NX=1ull<<63;
struct Vma {uint64_t start,end,flags;Vma* next;}; struct SharedPage {uint64_t phys;uint32_t refs;}; struct CachePage {uint64_t file_id,index,phys;uint32_t refs;bool dirty;};
bool map(uint64_t pml4,uint64_t va,uint64_t pa,uint64_t flags); bool map_anonymous(uint64_t pml4,uint64_t va,uint64_t pages,uint64_t flags); bool cow_clone(uint64_t parent,uint64_t child,uint64_t start,uint64_t pages); bool mmap_file(uint64_t pml4,uint64_t va,uint64_t phys,uint64_t len,uint64_t flags);
}
