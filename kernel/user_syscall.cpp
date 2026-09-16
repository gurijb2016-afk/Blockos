#include "preempt.hpp"
#include "syscall/syscall_numbers.hpp"
#include "process.hpp"
#include "arch/86_64x/paging.hpp"
#include "vfs.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
extern "C" int64_t blockos_tty_read(void*,size_t); extern "C" int64_t blockos_tty_write(const void*,size_t);
extern "C" uint64_t timer_uptime_ms();
extern "C" void blockos_user_return(); extern "C" uint64_t blockos_user_saved_rsp;
namespace {constexpr int64_t EINVAL=-22,EBADF=-9,ENOMEM=-12,EFAULT=-14,ENOENT=-2,ENOSYS=-38; constexpr uint64_t PAGE=0x1000; constexpr uint64_t MAP_ANONYMOUS=0x20; struct FD{bool used;const uint8_t* data;uint32_t size;uint64_t off;}; static FD fds[32];
static bool userstr(uint64_t p){if(!p||!paging::is_user_range(p,1,false))return false;for(size_t i=0;i<4096;i++){uint64_t a=p+i;if(!paging::is_user_range(a,1,false))return false;if(*(volatile char*)a==0)return true;}return false;}
static int allocfd(){for(int i=3;i<32;i++)if(!fds[i].used){fds[i].used=true;return i;}return -1;}

/*
 * mmap, now with real file-backed support (MAP_PRIVATE-style eager copy,
 * not lazy/shared - matches how the rest of BlockOS's VFS works, since
 * vfs::read_file already hands back a fully-resident in-memory buffer for
 * the whole file rather than something paged in on demand).
 *
 * addr/len/prot behave exactly as before. flags/fd/offset are read from
 * r10/r8/r9 - the same registers real x86-64 Linux syscalls use for mmap's
 * 4th/5th/6th arguments, so any caller that already follows that
 * convention (as ld.so now does) works correctly. Any OLDER caller that
 * only ever set rdi/rsi/rdx (3-arg mmap) leaves r10/r8/r9 as whatever was
 * already in those registers - to stay safe for those callers, a request
 * is treated as file-backed ONLY when flags explicitly clears
 * MAP_ANONYMOUS *and* fd names a small, currently-open descriptor;
 * anything else silently falls back to the original anonymous behavior,
 * so no existing 3-arg caller changes behavior.
 */
static int64_t do_mmap(uint64_t addr,uint64_t len,uint64_t prot,uint64_t flags,uint64_t fd_raw,uint64_t offset){
    auto* p=process::current();
    if(!p||!len) return EINVAL;
    if(addr==0){addr=(p->mmap_next+PAGE-1)&~(PAGE-1);p->mmap_next=addr+((len+PAGE-1)&~(PAGE-1));}
    else addr&=~(PAGE-1);
    if(addr<0x10000||addr+len<addr||addr+len>0x00007ffff0000000ULL) return EINVAL;

    uint64_t pflags=4; if(prot&2)pflags|=2; if(!(prot&4))pflags|=1ULL<<63;
    if(!paging::map_user_range(p->pml4,addr,(size_t)len,pflags)) return ENOMEM;

    int64_t fd=(int64_t)fd_raw;
    bool file_backed = !(flags&MAP_ANONYMOUS) && fd>=0 && fd<32 && fds[(size_t)fd].used;

    // Zero explicitly rather than assume the page allocator already zeroes
    // fresh pages - correctness here matters more than the small extra cost.
    memset((void*)(uintptr_t)addr,0,(size_t)len);

    if (file_backed) {
        FD& f2 = fds[(size_t)fd];
        uint64_t avail = offset < f2.size ? f2.size - offset : 0;
        uint64_t n = len < avail ? len : avail;
        if (n) memcpy((void*)(uintptr_t)addr, f2.data + offset, (size_t)n);
        // Bytes beyond the file's remaining length stay zero, matching
        // normal MAP_PRIVATE-past-EOF semantics.
    }

    return (int64_t)addr;
}
}
extern "C" void blockos_syscall_dispatch_frame(BlockOSSyscallFrame* f){if(!f)return;int64_t r=ENOSYS;auto* p=process::current();switch(f->rax){case blockos::syscall::SYS_write: if((f->rdi>2)||!paging::is_user_range(f->rsi,(size_t)f->rdx,false)){r=EFAULT;break;} r=blockos_tty_write((const void*)(uintptr_t)f->rsi,(size_t)f->rdx);break;case blockos::syscall::SYS_read: if(f->rdi<3){if(!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;} r=blockos_tty_read((void*)(uintptr_t)f->rsi,(size_t)f->rdx);break;} if(f->rdi>=32||!fds[f->rdi].used||!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;} {FD& fd=fds[f->rdi];uint64_t avail=fd.off<fd.size?fd.size-fd.off:0;size_t n=(size_t)((uint64_t)f->rdx<avail?(uint64_t)f->rdx:avail);if(n)memcpy((void*)(uintptr_t)f->rsi,fd.data+fd.off,n);fd.off+=n;r=(int64_t)n;} break;case blockos::syscall::SYS_getpid:r=p?p->pid:-1;break;case blockos::syscall::SYS_exit:case blockos::syscall::SYS_exit_group: if(preempt::on_exit(f))return; if(p)p->state=process::State::TERMINATED;f->rax=0;f->rip=(uint64_t)(uintptr_t)&blockos_user_return;f->cs=0x08;f->ss=0x10;f->rsp=blockos_user_saved_rsp;f->rflags|=0x200;return;case blockos::syscall::SYS_clock_gettime:{if(!paging::is_user_range(f->rsi,16,true)){r=EFAULT;break;}uint64_t ms=timer_uptime_ms();struct TS{int64_t sec,nsec;}*ts=(TS*)(uintptr_t)f->rsi;ts->sec=(int64_t)(ms/1000);ts->nsec=(int64_t)((ms%1000)*1000000);r=0;break;}case blockos::syscall::SYS_mmap:r=do_mmap(f->rdi,f->rsi,f->rdx,f->r10,f->r8,f->r9);break;case blockos::syscall::SYS_brk:{if(!p){r=ENOMEM;break;}uint64_t want=f->rdi;if(!want){r=p->brk_current;break;}if(want<p->brk_base||want>0x0000000400000000ULL){r=EINVAL;break;}uint64_t old=(p->brk_current+PAGE-1)&~(PAGE-1),nw=(want+PAGE-1)&~(PAGE-1);if(nw>old&&!paging::map_user_range(p->pml4,old,(size_t)(nw-old),6|(1ULL<<63))){r=ENOMEM;break;}p->brk_current=want;r=want;break;}case blockos::syscall::SYS_openat:{if(!userstr(f->rsi)){r=EFAULT;break;}char path[256];size_t n=0;while(n+1<sizeof(path)){path[n]=*(const char*)(uintptr_t)(f->rsi+n);if(!path[n])break;n++;}path[sizeof(path)-1]=0;uint32_t sz=0;const uint8_t* d=vfs::read_file(path,&sz);if(!d){r=ENOENT;break;}int fd=allocfd();if(fd<0){r=ENOMEM;break;}fds[fd]={true,d,sz,0};r=fd;break;}case blockos::syscall::SYS_readv:case blockos::syscall::SYS_writev:r=ENOSYS;break;case blockos::syscall::SYS_close:if(f->rdi>=32||!fds[f->rdi].used){r=EBADF;break;}fds[f->rdi]={};r=0;break;case blockos::syscall::SYS_lseek:if(f->rdi>=32||!fds[f->rdi].used){r=EBADF;break;}if(f->rdx==0)fds[f->rdi].off=f->rsi;else if(f->rdx==1)fds[f->rdi].off+=f->rsi;else if(f->rdx==2)fds[f->rdi].off=fds[f->rdi].size+f->rsi;else {r=EINVAL;break;}r=fds[f->rdi].off;break;case blockos::syscall::SYS_fstat:{if(f->rdi>=32||!fds[f->rdi].used||!paging::is_user_range(f->rsi,128,true)){r=EBADF;break;}memset((void*)(uintptr_t)f->rsi,0,128);uint64_t* q=(uint64_t*)(uintptr_t)f->rsi;q[0]=1;q[1]=1;q[2]=0100444;q[7]=fds[f->rdi].size;r=0;break;}default:r=ENOSYS;break;}f->rax=(uint64_t)r;}
