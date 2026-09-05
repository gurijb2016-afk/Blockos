#include "process.hpp"
#include "elf_loader.hpp"
#include "arch/86_64x/paging.hpp"
#include "allocator.hpp"
#include <string.h>
extern "C" void blockos_user_enter(uint64_t entry,uint64_t stack,uint64_t pml4);
extern "C" uint64_t blockos_user_saved_rsp;
extern "C" void blockos_user_return();
namespace process {
static constexpr size_t MAX_PROCESS=128; static Process ps[MAX_PROCESS]; static uint64_t next_pid=1; static Process* cur=nullptr;
static Process* free_slot(){for(auto& p:ps)if(p.state==State::EMPTY)return &p;return nullptr;}
void init(){memset(ps,0,sizeof(ps));for(auto& p:ps)p.state=State::EMPTY;next_pid=1;cur=nullptr;}
Process* create(const void* elf,size_t size){Process* p=free_slot();if(!p||!elf||!size)return nullptr;uint64_t e=0,cr3=0,st=0;if(!elf_loader::load_elf64_from_mem(elf,size,&e,&cr3,&st)){p->state=State::EMPTY;return nullptr;}memset(&p->context,0,sizeof(p->context));p->pid=next_pid++;p->pml4=cr3;p->entry=e;p->stack=st;p->task_id=0;p->brk_base=0x0000000200000000ULL;p->brk_current=p->brk_base;p->mmap_next=0x0000000100000000ULL;p->state=State::READY;return p;}
int run(Process* p){if(!p||p->state!=State::READY)return -1;cur=p;p->state=State::RUNNING;uint64_t old= paging::read_cr3();(void)old;blockos_user_enter(p->entry,p->stack,p->pml4);if(p->state==State::RUNNING)p->state=State::TERMINATED;cur=nullptr;return 0;}
bool terminate(Process* p){if(!p||p->state==State::EMPTY)return false;p->state=State::TERMINATED;if(cur==p)cur=nullptr;return true;}
Process* current(){return cur;} Process* get(uint64_t pid){if(!pid)return nullptr;for(auto& p:ps)if(p.state!=State::EMPTY&&p.pid==pid)return &p;return nullptr;} size_t count(){size_t n=0;for(auto& p:ps)if(p.state!=State::EMPTY)n++;return n;}
}
