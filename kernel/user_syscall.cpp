#include "preempt.hpp"
#include "tls.hpp"
#include "syscall/syscall_numbers.hpp"
#include "process.hpp"
#include "arch/86_64x/paging.hpp"
#include "vfs.hpp"
#include "elf_loader.hpp"
#include "input_bridge.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* Address-space reclamation guard. A space may be freed only when no other
 * process still uses it (vfork children share their parent's space). Shared
 * spaces are left alone and counted, so the fault report shows the cases. */
uint64_t g_pml4_skips = 0;
uint64_t g_alias_hits = 0;
uint64_t g_reaps = 0;
uint64_t g_alias_pids = 0;
static bool pml4_used_elsewhere(uint64_t root, const process::Process* self) {
    root &= 0x000ffffffffff000ULL;
    for (size_t i = 0; i < process::slot_count(); i++) {
        auto* x = process::slot_at(i);
        if (!x || x == self || x->state == process::State::EMPTY) continue;
        if ((x->pml4 & 0x000ffffffffff000ULL) == root) return true;
    }
    return false;
}

extern "C" int64_t blockos_tty_read(void*,size_t);
extern "C" int64_t blockos_tty_ioctl(uint64_t,void*);
extern "C" int64_t blockos_tty_write(const void*,size_t);
extern "C" uint64_t timer_uptime_ms();
extern "C" void blockos_user_return();
extern "C" uint64_t blockos_user_saved_cr3;
extern "C" uint64_t blockos_user_saved_rsp;

namespace {
constexpr int64_t EPERM=-1,ESRCH=-3,EINTR=-4,EBADF=-9,EAGAIN=-11,EMFILE=-24,EFAULT=-14,EINVAL=-22,ENOSYS=-38,ENOTCONN=-107,EADDRINUSE=-98,ENOENT=-2,ENOTSOCK=-88,EMSGSIZE=-90,ENOMEM=-12,ESPIPE=-29,ECHILD=-10,EEXIST=-17,ENOTDIR=-20,EISDIR=-21,ENAMETOOLONG=-36,ENOSPC=-28,ENOTEMPTY=-39,EACCES=-13,ENOTTY=-25,EIO=-5;
constexpr uint64_t PAGE=0x1000;
constexpr uint64_t MAP_ANONYMOUS=0x20;
constexpr uint64_t MAP_FIXED=0x10;
constexpr int AF_UNIX=1;
constexpr int SOCK_STREAM=1;
constexpr int FUTEX_WAIT=0;
constexpr int FUTEX_WAKE=1;
constexpr int ARCH_SET_FS=0x1002;
constexpr int ARCH_GET_FS=0x1003;
constexpr int F_DUPFD=0;
constexpr int F_GETFD=1;
constexpr int F_SETFD=2;
constexpr int F_GETFL=3;
constexpr int F_SETFL=4;
constexpr uint64_t FD_CLOEXEC=1;
constexpr uint64_t FD_NONBLOCK=04000;
constexpr int O_WRONLY=1, O_RDWR=2, O_CREAT=0100, O_EXCL=0200, O_TRUNC=01000, O_APPEND=02000, O_NONBLOCK=04000, O_DIRECTORY=0200000, O_CLOEXEC=02000000;
constexpr int WNOHANG=1;
constexpr int TIOCGWINSZ=0x5413;
constexpr uint32_t POLLIN=0x001, POLLOUT=0x004, POLLERR=0x008, POLLHUP=0x010;

struct UnixSocket {
    bool used;
    bool listener;
    bool connected;
    bool closed;
    char path[108];
    int peer;
    int pending[16];
    uint8_t pending_count;
    uint8_t rx[65536];
    uint32_t rx_head;
    uint32_t rx_size;
};
static UnixSocket unix_socks[256];
static char directory_paths[128][256];
static uint8_t directory_path_used[128];
static vfs::DeviceNodeInfo device_infos[64];
static uint8_t device_info_used[64];
static const vfs::DeviceNodeInfo* store_device_info(const vfs::DeviceNodeInfo& in){for(size_t i=0;i<64;i++)if(!device_info_used[i]){device_info_used[i]=1;device_infos[i]=in;return &device_infos[i];}return nullptr;}
static const char* store_directory_path(const char* path){for(size_t i=0;i<128;i++)if(!directory_path_used[i]){directory_path_used[i]=1;strncpy(directory_paths[i],path,255);directory_paths[i][255]=0;return directory_paths[i];}return nullptr;}

struct FutexWaiter { bool used; uint64_t addr; uint64_t pid; };
static FutexWaiter futex_waiters[128];

struct EpollWatch { bool used; int fd; uint32_t events; uint64_t data; };
struct EpollObj { bool used; EpollWatch watch[64]; };
static EpollObj epolls[64];

static bool userstr(uint64_t p,size_t maxlen=4096){
    if(!p||!paging::is_user_range(p,1,false))return false;
    for(size_t i=0;i<maxlen;i++){
        uint64_t a=p+i;
        if(!paging::is_user_range(a,1,false))return false;
        if(*(volatile char*)a==0)return true;
    }
    return false;
}

static bool copy_user_string(uint64_t up,char* out,size_t cap){
    if(!out||cap<1||!userstr(up,cap-1)) return false;
    for(size_t i=0;i+1<cap;i++){
        char c=*(const char*)(uintptr_t)(up+i); out[i]=c;
        if(!c){return true;}
    }
    out[cap-1]=0; return false;
}

static process::Process* owner_process(){
    process::Process* p=process::current();
    return p && p->fd_owner ? p->fd_owner : p;
}

static process::RuntimeFd* fdtable(){
    process::Process* p=owner_process();
    return p ? p->fds : nullptr;
}

static int alloc_fd(uint16_t start=0){
    auto* fs=fdtable(); if(!fs) return -1;
    for(int i=start;i<(int)process::MAX_RUNTIME_FDS;i++) if(!fs[i].used){
        fs[i]={true,process::RuntimeFd::None,0,0,nullptr,0,0}; return i;
    }
    return -1;
}

/* Canonical lookup key for a sockaddr_un path. Path names are kept as-is;
 * abstract names (sun_path[0]==NUL, as Xtrans/Xlib use for the "local"
 * transport) are stored as "@" + name so they can be matched by strcmp. */
static bool unix_key_from_addr(uint64_t addr, uint64_t len, char* key, size_t cap){
    if(len<3||cap<2) return false;
    const uint8_t* b=(const uint8_t*)(uintptr_t)addr+2; size_t n=(size_t)len-2;
    if(b[0]==0){
        if(n>cap-1) return false;
        key[0]='@'; size_t m=n-1; if(m>cap-2) return false;
        memcpy(key+1,b+1,m); key[1+m]=0; return m>0;
    }
    size_t m=0; while(m<n && b[m]) m++;
    if(m+1>cap) return false;
    memcpy(key,b,m); key[m]=0; return true;
}

static void socket_free(int idx){
    const int N=(int)(sizeof(unix_socks)/sizeof(unix_socks[0]));
    if(idx<0||idx>=N) return;
    int pe=unix_socks[idx].peer;
    if(pe>=0&&pe<N&&unix_socks[pe].used&&unix_socks[pe].peer==idx) unix_socks[pe].peer=-1;
    unix_socks[idx]={};
}

/* Socket objects have no reference count. Instead, a socket is still in use
 * while any fd of any live process refers to it (fork copies fd tables, so
 * the count is taken over all processes). Returns how many such fds exist,
 * skipping the fd (except_p, except_fd) when given. */
static int socket_other_refs(int si, process::Process* except_p, int except_fd){
    int n=0;
    for(size_t i=0;;i++){
        process::Process* q=process::slot_at(i);
        if(!q) break;
        if(q->state==process::State::EMPTY) continue;
        for(int fd=0;fd<(int)process::MAX_RUNTIME_FDS;fd++){
            if(q==except_p && fd==except_fd) continue;
            const auto& d=q->fds[fd];
            if(d.used && d.kind==process::RuntimeFd::UnixSocket && (int)d.object-1==si) n++;
        }
    }
    return n;
}

/* Closes one unix-socket fd entry: frees the socket only when no other fd
 * refers to it, and then marks the peer as closed (EOF for its reader). */
/* Wakes every task asleep in a read on unix socket idx (data arrived or the peer closed). */
static void wake_socket_writers(int idx){
    if(idx<0) return;
    for(size_t i=0;i<process::slot_count();++i){
        process::Process* q=process::slot_at(i);
        if(q&&q->state==process::State::BLOCKED&&q->wait_sock_plus1==(((uint32_t)idx+1)|0x80000000u)){ q->wait_sock_plus1=0; q->state=process::State::READY; }
    }
}
static void wake_socket_readers(int idx){
    if(idx<0) return;
    for(size_t i=0;i<process::slot_count();++i){
        process::Process* q=process::slot_at(i);
        if(q&&q->state==process::State::BLOCKED&&q->wait_sock_plus1==(uint32_t)idx+1){ q->wait_sock_plus1=0; q->state=process::State::READY; }
    }
}
/* Unix-style permission check for a regular file: root passes; otherwise owner, group or other bits apply. */
static bool perm_allows(process::Process* p,const char* path,bool want_r,bool want_w){
    if(!p||p->euid==0) return true;
    uint32_t mode=0644u,ou=0,og=0; vfs::get_mode(path,&mode); vfs::get_owner(path,&ou,&og);
    uint32_t bits = (p->euid==ou) ? ((mode>>6)&7u) : (p->egid==og) ? ((mode>>3)&7u) : (mode&7u);
    if(want_r&&!(bits&4u)) return false;
    if(want_w&&!(bits&2u)) return false;
    return true;
}
static void socket_release_fd(int si, process::Process* except_p, int except_fd){
    if(si<0) return;
    if(socket_other_refs(si, except_p, except_fd)!=0) return;
    int pe=unix_socks[si].peer;
    if(pe>=0 && pe<(int)(sizeof(unix_socks)/sizeof(unix_socks[0]))) { unix_socks[pe].closed=true; wake_socket_readers(pe); }
    socket_free(si);
}

/* Process exit: drops every unix-socket fd the process still holds. */
static void release_process_fds(process::Process* q){
    if(!q) return;
    for(int fd=0;fd<(int)process::MAX_RUNTIME_FDS;fd++){
        auto& d=q->fds[fd];
        if(!d.used || d.kind!=process::RuntimeFd::UnixSocket) continue;
        int si=(int)d.object-1;
        d={};
        socket_release_fd(si, nullptr, -1);
    }
}

static int socket_obj_alloc(){
    for(int i=0;i<(int)(sizeof(unix_socks)/sizeof(unix_socks[0]));i++) if(!unix_socks[i].used){
        unix_socks[i]={}; unix_socks[i].used=true; unix_socks[i].peer=-1; return i;
    }
    return -1;
}

static int fd_socket_index(int fd){
    auto* fs=fdtable(); if(!fs||fd<0||fd>=(int)process::MAX_RUNTIME_FDS||!fs[fd].used||fs[fd].kind!=process::RuntimeFd::UnixSocket) return -1;
    return (int)fs[fd].object-1;
}

static bool socket_write_idx(int idx,const uint8_t* src,size_t n){
    if(idx<0||idx>=256||!unix_socks[idx].used||unix_socks[idx].peer<0)return false;
    UnixSocket& p=unix_socks[unix_socks[idx].peer];
    if(!p.used)return false;
    if(n>sizeof(p.rx)-p.rx_size)return false;
    for(size_t i=0;i<n;i++) p.rx[(p.rx_head+p.rx_size+i)%sizeof(p.rx)]=src[i];
    p.rx_size+=(uint32_t)n;
    wake_socket_readers(unix_socks[idx].peer);
    return true;
}

/* Partial write, like a pipe: stores as much as fits and returns the count (0 when full). */
static size_t socket_write_partial(int idx,const uint8_t* src,size_t n){
    if(idx<0||idx>=256||!unix_socks[idx].used||unix_socks[idx].peer<0)return 0;
    UnixSocket& p=unix_socks[unix_socks[idx].peer];
    if(!p.used)return 0;
    size_t fr=sizeof(p.rx)-p.rx_size; if(n>fr)n=fr;
    if(!n)return 0;
    for(size_t i=0;i<n;i++) p.rx[(p.rx_head+p.rx_size+i)%sizeof(p.rx)]=src[i];
    p.rx_size+=(uint32_t)n;
    wake_socket_readers(unix_socks[idx].peer);
    return n;
}

static size_t socket_read_idx(int idx,uint8_t* dst,size_t n){
    if(idx<0||idx>=256||!unix_socks[idx].used)return 0;
    UnixSocket& s=unix_socks[idx];
    size_t k=n<s.rx_size?n:s.rx_size;
    for(size_t i=0;i<k;i++) dst[i]=s.rx[(s.rx_head+i)%sizeof(s.rx)];
    s.rx_head=(s.rx_head+(uint32_t)k)%sizeof(s.rx); s.rx_size-=(uint32_t)k;
    if(k) wake_socket_writers(idx);
    return k;
}

static bool resolve_path_plain(process::Process* p, const char* in, char* out, size_t cap);

/* Resolves a path and follows symbolic links (at most 8 hops). */
static bool resolve_path_for_process(process::Process* p, const char* in, char* out, size_t cap) {
    if(!resolve_path_plain(p,in,out,cap)) return false;
    char cur[256];
    if(strlen(out)>=sizeof(cur)) return false;
    memcpy(cur,out,strlen(out)+1);
    for(int hop=0;hop<8;hop++){
        const char* t=vfs::read_symlink(cur);
        if(!t) break;
        char next[256];
        size_t tl=strlen(t);
        if(t[0]=='/'){
            if(tl>=sizeof(next)) return false;
            memcpy(next,t,tl+1);
        } else {
            /* relative target: relative to the directory that holds the link */
            size_t last=0; for(size_t i=0;cur[i];i++) if(cur[i]=='/') last=i;
            size_t dl=last==0?1:last;
            if(dl+1+tl>=sizeof(next)) return false;
            if(last==0) next[0]='/'; else memcpy(next,cur,dl);
            next[dl]='/';
            memcpy(next+dl+1,t,tl+1);
        }
        if(strlen(next)>=sizeof(cur)) return false;
        memcpy(cur,next,strlen(next)+1);
        if(hop==7) return false; /* too many links */
    }
    if(strlen(cur)>=cap) return false;
    memcpy(out,cur,strlen(cur)+1);
    return true;
}

/* Collapses "." and ".." components of an absolute path, in place. */
static void normalize_path(char* s){
    char tmp[256]; size_t o=1; tmp[0]='/'; tmp[1]=0;
    const char* p=s;
    while(*p){
        while(*p=='/') p++;
        if(!*p) break;
        const char* q=p; while(*q && *q!='/') q++;
        size_t len=(size_t)(q-p);
        if(len==1 && p[0]=='.'){ }
        else if(len==2 && p[0]=='.' && p[1]=='.'){
            if(o>1){ while(o>1 && tmp[o-1]!='/') o--; if(o>1) o--; tmp[o]=0; }
        } else {
            if(o>1) tmp[o++]='/';
            if(o+len>=sizeof(tmp)) return;
            memcpy(tmp+o,p,len); o+=len; tmp[o]=0;
        }
        p=q;
    }
    size_t n=strlen(tmp);
    memcpy(s,tmp,n+1);
}

static bool resolve_path_plain_raw(process::Process* p, const char* in, char* out, size_t cap) {
    if (!in || !out || cap < 2) return false;
    if (in[0] == '/') {
        size_t n = strlen(in);
        if (n >= cap) return false;
        memcpy(out, in, n + 1);
        return true;
    }
    const char* cwd = process::cwd(p);
    size_t a = strlen(cwd);
    size_t b = strlen(in);
    if (a + 1 + b + 1 > cap) return false;
    memcpy(out, cwd, a);
    if (a == 0 || out[a - 1] != '/') out[a++] = '/';
    memcpy(out + a, in, b + 1);
    return true;
}

static bool resolve_path_plain(process::Process* p, const char* in, char* out, size_t cap) {
    if (!resolve_path_plain_raw(p, in, out, cap)) return false;
    normalize_path(out);
    return true;
}

static int copy_user_vec(uint64_t up, char out[][256], int max_count) {
    if (!up) return 0;
    int n = 0;
    for (;;) {
        if (n >= max_count) return -1;
        if (!paging::is_user_range(up + (uint64_t)n * 8, 8, false)) return -1;
        uint64_t ptr = *reinterpret_cast<const uint64_t*>(static_cast<uintptr_t>(up + (uint64_t)n * 8));
        if (!ptr) break;
        if (!copy_user_string(ptr, out[n], 256)) return -1;
        ++n;
    }
    return n;
}

extern "C" void blockos_display_claim(void);
static int64_t do_mmap(uint64_t addr,uint64_t len,uint64_t prot,uint64_t flags,uint64_t fd_raw,uint64_t offset){
    auto* p=process::current(); if(!p||!len)return EINVAL;
    uint64_t rounded=(len+PAGE-1)&~(PAGE-1); if(rounded<len)return EINVAL;
    if(addr==0){addr=(p->mmap_next+PAGE-1)&~(PAGE-1);p->mmap_next=addr+rounded;}
    else { addr&=~(PAGE-1); if(!(flags&MAP_FIXED) && addr<p->mmap_next) addr=(p->mmap_next+PAGE-1)&~(PAGE-1); }
    if(addr<0x10000||addr+rounded<addr||addr+rounded>0x00007ffff0000000ULL)return EINVAL;
    if(flags&MAP_FIXED){ for(uint64_t va=addr;va<addr+rounded;va+=PAGE) paging::unmap_4k(p->pml4,va); }
    uint64_t pflags=4; if(prot&2)pflags|=2; if(!(prot&4))pflags|=1ULL<<63;
    if(!paging::map_user_range(p->pml4,addr,(size_t)rounded,pflags|4))return ENOMEM;
    memset((void*)(uintptr_t)addr,0,(size_t)rounded);
    int64_t fd=(int64_t)fd_raw;
    auto* fs=fdtable();
    bool device_backed=!(flags&MAP_ANONYMOUS)&&fd>=0&&fd<(int64_t)process::MAX_RUNTIME_FDS&&fs&&fs[fd].used&&fs[fd].kind==process::RuntimeFd::Device;
    if(device_backed){
        const auto* di=reinterpret_cast<const vfs::DeviceNodeInfo*>(fs[fd].data);
        if(!di || offset>=di->size || rounded>di->size-offset){return EINVAL;}
        if(di->type==vfs::DEVICE_GPU) blockos_display_claim();
        for(uint64_t va=addr,po=offset;va<addr+rounded;va+=PAGE,po+=PAGE){
            if(!paging::map_4k(p->pml4,va,di->base+po,((prot&2)?2:0)|4|((prot&4)?0:(1ULL<<63)))) return ENOMEM;
        }
        return (int64_t)addr;
    }
    bool file_backed=!(flags&MAP_ANONYMOUS)&&fd>=0&&fd<(int64_t)process::MAX_RUNTIME_FDS&&fs&&fs[fd].used&&fs[fd].kind==process::RuntimeFd::File;
    if(file_backed){ auto &f=fs[fd]; uint64_t avail=offset<f.size?f.size-offset:0; uint64_t n=rounded<avail?rounded:avail; if(n)memcpy((void*)(uintptr_t)addr,f.data+offset,(size_t)n); }
    return (int64_t)addr;
}

static bool wake_futex(uint64_t addr,uint32_t count){
    uint32_t woken=0;
    for(auto& w:futex_waiters){
        if(!w.used||w.addr!=addr)continue;
        process::Process* p=process::get(w.pid);
        w.used=false;
        if(p&&p->state==process::State::BLOCKED){p->state=process::State::READY; if(++woken>=count)break;}
    }
    return true;
}

static bool fd_readable(int fd){
    auto* fs=fdtable(); if(!fs||fd<0||fd>=(int)process::MAX_RUNTIME_FDS||!fs[fd].used)return false;
    if(fs[fd].kind==process::RuntimeFd::Tty) return true;
    if(fs[fd].kind==process::RuntimeFd::File) return fs[fd].off<fs[fd].size;
    if(fs[fd].kind==process::RuntimeFd::Device) return false;
    if(fs[fd].kind==process::RuntimeFd::UnixSocket){int i=fd_socket_index(fd);if(i<0)return false;if(unix_socks[i].listener)return unix_socks[i].pending_count>0;return unix_socks[i].rx_size>0||unix_socks[i].closed;}
    return false;
}
static bool fd_writable(int fd){
    auto* fs=fdtable(); if(!fs||fd<0||fd>=(int)process::MAX_RUNTIME_FDS||!fs[fd].used)return false;
    if(fs[fd].kind==process::RuntimeFd::Tty||fs[fd].kind==process::RuntimeFd::File||fs[fd].kind==process::RuntimeFd::Device)return true;
    if(fs[fd].kind==process::RuntimeFd::UnixSocket){int i=fd_socket_index(fd);return i>=0&&unix_socks[i].peer>=0;}
    return false;
}

static bool prepare_execve(BlockOSSyscallFrame* f) {
    auto* p=process::current(); if(!p) return false;
    char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))) return false;
    if(path[0] != '/') { char abs[256]; if(!resolve_path_for_process(p,path,abs,sizeof(abs))) return false; memcpy(path,abs,sizeof(path)); }
    uint32_t sz=0; const uint8_t* main_buf=vfs::read_file(path,&sz); if(!main_buf||sz==0) return false;

    /* static: exec vectors are large and prepare_execve never yields while using them.
     * pkgsrc bootstrap exports more than 16 variables, so allow up to 128. */
    static char argv[128][256], envp[128][256];
    int argc=copy_user_vec(f->rsi,argv,128); int envc=copy_user_vec(f->rdx,envp,128);
    if(argc<0||envc<0) return false;
    static const char* argv_ptrs[128]; static const char* envp_ptrs[128];
    if(argc==0){ strncpy(argv[0],path,sizeof(argv[0])-1); argv[0][sizeof(argv[0])-1]=0; argc=1; }
    for(int i=0;i<argc;i++) argv_ptrs[i]=argv[i];
    for(int i=0;i<envc;i++) envp_ptrs[i]=envp[i];

    uint64_t new_pml4=paging::create_user_pml4(); if(!new_pml4) return false;
    elf_loader::LoadResult main_image{};
    if(!elf_loader::load_elf64_into(new_pml4,main_buf,sz,0,&main_image)) return false;
    constexpr uint64_t INTERP_BASE=0x0000700000000000ULL;
    uint64_t start_entry=main_image.entry, start_rsp=0, real_entry=main_image.entry;
    if(main_image.has_interp){
        const char* selected_interp=main_image.interp_path;
        if(strcmp(selected_interp,"/lib64/ld-linux-x86-64.so.2")==0 || strcmp(selected_interp,"/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2")==0 || strcmp(selected_interp,"/lib/ld-linux-x86-64.so.2")==0) selected_interp="/system/lib/ld.so";
        uint32_t isz=0; const uint8_t* ib=vfs::read_file(selected_interp,&isz); if(!ib||isz==0) return false;
        elf_loader::LoadResult interp{}; if(!elf_loader::load_elf64_into(new_pml4,ib,isz,INTERP_BASE,&interp)) return false;
        if(interp.has_interp) return false;
        start_entry=interp.entry;
        start_rsp=elf_loader::build_initial_stack(new_pml4,0x00007ffffff00000ULL,32,argv_ptrs,argc,envp_ptrs,envc,main_image,interp.load_bias);
        if(!start_rsp) return false;
        real_entry=main_image.entry;
    } else {
        uint64_t stack_base=0x00007ffffff00000ULL-32*0x1000ULL;
        if(!paging::map_user_range(new_pml4,stack_base,32*0x1000ULL,4|2|(1ULL<<63))) return false;
        const char* const empty_env[1]={nullptr};
        start_rsp=elf_loader::build_initial_stack(new_pml4,0x00007ffffff00000ULL,32,argv_ptrs,argc,envp_ptrs,envc,main_image,0);
        if(!start_rsp){ (void)empty_env; return false; }
    }

    for(size_t i=3;i<process::MAX_RUNTIME_FDS;i++) if(p->fd_owner->fds[i].used && (p->fd_owner->fds[i].flags & 1u)) p->fd_owner->fds[i]={};
    if(p->pml4_owned && p->pml4 && p->pml4 != new_pml4 && !p->pml4_pending_free) p->pml4_pending_free=p->pml4;
    p->pml4=new_pml4; p->pml4_owned=true; p->entry=start_entry; p->stack=start_rsp; p->has_interp=main_image.has_interp; p->real_entry=real_entry; p->fs_base=0; p->brk_current=p->brk_base; p->brk_mapped=0; p->mmap_next=0x0000000100000000ULL;
    size_t pn=strlen(path); if(pn>=sizeof(p->name)) pn=sizeof(p->name)-1; memcpy(p->name,path,pn); p->name[pn]=0;
    if (p->vfork_child) { if(auto* parent=process::get(p->vfork_parent_pid)){ if(parent->state==process::State::BLOCKED) parent->state=process::State::READY; } p->vfork_child=false; p->vfork_parent_pid=0; }
    paging::switch_pml4(new_pml4);
    f->rax=0; f->rip=start_entry; f->rsp=start_rsp; f->cs=0x1b; f->ss=0x23; f->rflags=0x202;
    return true;
}
}

extern "C" void blockos_kernel_poll_input();
extern "C" void blockos_syscall_dispatch_frame(BlockOSSyscallFrame* f){
    if(!f)return;
    /* Poll hardware AND route the events to the tty; poll_hardware() alone
     * leaves keystrokes in the input queue where read(0) never sees them. */
    blockos_kernel_poll_input();
    int64_t r=ENOSYS; auto* p=process::current(); auto* fs=fdtable();
    /* A space replaced by exec is freed here: this syscall runs on the new space. */
    if(p&&p->pml4_pending_free){ if(pml4_used_elsewhere(p->pml4_pending_free,p)) g_pml4_skips++; else paging::destroy_user_space(p->pml4_pending_free); p->pml4_pending_free=0; }
    switch(f->rax){
    case blockos::syscall::SYS_write:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        if(f->rdx==0){r=0;break;}
        if(!paging::is_user_range(f->rsi,(size_t)f->rdx,false)){r=EFAULT;break;}
        auto& d=fs[f->rdi];
        if(d.kind==process::RuntimeFd::Null){ r=f->rdx; }
        else if(d.kind==process::RuntimeFd::Tty){ r=blockos_tty_write((const void*)(uintptr_t)f->rsi,(size_t)f->rdx); }
        else if(d.kind==process::RuntimeFd::File){ if((d.flags&3u)==0){r=EBADF;break;} uint64_t noff=d.off; if(!vfs::write_file_at(d.path,d.off,(const uint8_t*)(uintptr_t)f->rsi,(uint32_t)f->rdx,(d.flags&O_APPEND)!=0,&noff)){r=ENOSPC;break;} uint32_t ns=0; d.data=vfs::read_file(d.path,&ns); d.size=ns; d.off=noff; r=f->rdx; }
        else if(d.kind==process::RuntimeFd::UnixSocket){
            int si=fd_socket_index((int)f->rdi);
            if(si<0){r=ENOTCONN;break;}
            /* A blocking writer waits until the whole request (up to one buffer) fits, so
             * stdio-style callers that do not loop on short writes lose nothing. */
            const size_t cap=sizeof(unix_socks[0].rx);
            size_t want=(size_t)f->rdx<cap?(size_t)f->rdx:cap;
            int wpeer=unix_socks[si].peer;
            size_t freeb=(wpeer>=0&&wpeer<256)?cap-unix_socks[wpeer].rx_size:0;
            if(!(d.flags&FD_NONBLOCK) && wpeer>=0 && freeb<want){
                f->rax=blockos::syscall::SYS_write; f->rip-=2;
                if(p){ p->wait_sock_plus1=(((uint32_t)wpeer+1)|0x80000000u); if(preempt::block_from_syscall(f)) return; p->wait_sock_plus1=0; }
                if(preempt::yield_from_syscall(f)) return;
                f->rip+=2; r=EAGAIN; break;
            }
            size_t wn=socket_write_partial(si,(const uint8_t*)(uintptr_t)f->rsi,(size_t)f->rdx);
            if(wn==0){ r=EAGAIN; }
            else r=(int64_t)wn;
        }
        else r=EBADF; break;
    }
    case blockos::syscall::SYS_chmod:{
        /* chmod(path, mode): sets the permission bits; ownership checks are not enforced yet. */
        char raw[256],path[256]; if(!copy_user_string(f->rdi,raw,sizeof(raw))){r=EFAULT;break;}
        if(!resolve_path_for_process(p,raw,path,sizeof(path))){r=ENAMETOOLONG;break;}
        if(!vfs::exists(path)){r=ENOENT;break;}
        /* Only root or the file's owner may change its mode. */
        if(p&&p->euid!=0){ uint32_t ou=0,og=0; vfs::get_owner(path,&ou,&og); if(p->euid!=ou){r=EPERM;break;} }
        r=vfs::set_mode(path,(uint32_t)f->rsi)?0:EIO; break;
    }
    case blockos::syscall::SYS_symlink:{
        /* symlink(target, linkpath): the link itself is not followed. */
        char target[256],raw[256],link[256];
        if(!copy_user_string(f->rdi,target,sizeof(target))){r=EFAULT;break;}
        if(!copy_user_string(f->rsi,raw,sizeof(raw))){r=EFAULT;break;}
        if(!resolve_path_plain(p,raw,link,sizeof(link))){r=ENAMETOOLONG;break;}
        if(vfs::exists(link)){r=EEXIST;break;}
        r=vfs::create_symlink(target,link)?0:ENOSPC; break;
    }
    case blockos::syscall::SYS_pread64:{
        /* pread64(fd, buf, count, offset): like read, at an explicit offset; the fd offset is not changed. */
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        if(!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;}
        auto& d=fs[f->rdi];
        if(d.kind!=process::RuntimeFd::File){r=ESPIPE;break;}
        if((d.flags&3u)==1u){r=EBADF;break;}
        uint64_t off=(uint64_t)f->r10; uint64_t avail=off<d.size?d.size-off:0;
        size_t n=(size_t)((uint64_t)f->rdx<avail?(uint64_t)f->rdx:avail);
        if(n) memcpy((void*)(uintptr_t)f->rsi,d.data+off,n);
        r=(int64_t)n; break;
    }
    case blockos::syscall::SYS_pwrite64:{
        /* pwrite64(fd, buf, count, offset): like write, at an explicit offset; the fd offset is not changed. */
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        if(!paging::is_user_range(f->rsi,(size_t)f->rdx,false)){r=EFAULT;break;}
        auto& d=fs[f->rdi];
        if(d.kind!=process::RuntimeFd::File){r=ESPIPE;break;}
        if((d.flags&3u)==0){r=EBADF;break;}
        uint64_t noff=(uint64_t)f->r10;
        if(!vfs::write_file_at(d.path,(uint64_t)f->r10,(const uint8_t*)(uintptr_t)f->rsi,(uint32_t)f->rdx,false,&noff)){r=ENOSPC;break;}
        uint32_t ns=0; d.data=vfs::read_file(d.path,&ns); d.size=ns;
        r=(int64_t)f->rdx; break;
    }
    case blockos::syscall::SYS_read:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        if(f->rdx==0){r=0;break;} /* zero-length read: no buffer check (POSIX) */
        if(!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;}
        auto& d=fs[f->rdi];
        if(d.kind==process::RuntimeFd::Null){r=0;break;}
        if(d.kind==process::RuntimeFd::Random){
            static uint64_t rng=0;
            if(!rng) rng=__builtin_ia32_rdtsc()|1;
            uint8_t* ub=(uint8_t*)(uintptr_t)f->rsi;
            for(size_t k=0;k<(size_t)f->rdx;k++){ rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; ub[k]=(uint8_t)(rng>>24); }
            r=f->rdx; break;
        }
        if(d.kind==process::RuntimeFd::Tty) r=blockos_tty_read((void*)(uintptr_t)f->rsi,(size_t)f->rdx);
        else if(d.kind==process::RuntimeFd::File){uint64_t avail=d.off<d.size?d.size-d.off:0;size_t n=(size_t)((uint64_t)f->rdx<avail?(uint64_t)f->rdx:avail);if(n)memcpy((void*)(uintptr_t)f->rsi,d.data+d.off,n);d.off+=n;r=n;}
        else if(d.kind==process::RuntimeFd::UnixSocket){int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTCONN;break;}if(unix_socks[si].rx_size==0&&!unix_socks[si].closed&&unix_socks[si].peer>=0&&f->rdx>0){ /* open but empty: not EOF. A blocking fd gives up the CPU and re-runs this read later. */ if(!(d.flags&FD_NONBLOCK)){ f->rax=0; f->rip-=2; if(p){ p->wait_sock_plus1=(uint32_t)si+1; if(preempt::block_from_syscall(f)) return; p->wait_sock_plus1=0; } if(preempt::yield_from_syscall(f)) return; f->rip+=2; } r=EAGAIN;break;} r=(int64_t)socket_read_idx(si,(uint8_t*)(uintptr_t)f->rsi,(size_t)f->rdx);}
        else if(d.kind==process::RuntimeFd::Device){
            const auto* di=reinterpret_cast<const vfs::DeviceNodeInfo*>(d.data);
            if(!di || di->type!=vfs::DEVICE_INPUT){ r=ENOTTY; break; }
            size_t cap=(size_t)f->rdx/sizeof(blockos::input::Event);
            if(cap==0){ r=0; break; }
            blockos::input::Event tmp[32];
            size_t want=cap<32?cap:32;
            size_t n=blockos::input::read(tmp,want);
            if(n==0 && !(d.flags&FD_NONBLOCK)){ r=EAGAIN; break; }
            if(n) memcpy((void*)(uintptr_t)f->rsi,tmp,n*sizeof(tmp[0]));
            r=(int64_t)(n*sizeof(tmp[0]));
        }
        else r=EBADF; break;
    }
    case blockos::syscall::SYS_close:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        if(fs[f->rdi].kind==process::RuntimeFd::UnixSocket){int si=fd_socket_index((int)f->rdi); fs[f->rdi]={}; socket_release_fd(si,p,(int)f->rdi);} else fs[f->rdi]={}; r=0; break;
    }
    case blockos::syscall::SYS_lseek:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used||fs[f->rdi].kind!=process::RuntimeFd::File){r=ESPIPE;break;}
        auto& d=fs[f->rdi]; int64_t base=(f->rdx==0?0:(f->rdx==1?(int64_t)d.off:(f->rdx==2?(int64_t)d.size:0))); if(f->rdx>2){r=EINVAL;break;} int64_t n=base+(int64_t)f->rsi;if(n<0){r=EINVAL;break;} d.off=(uint64_t)n;r=n;break;
    }
    case blockos::syscall::SYS_fstat:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used||!paging::is_user_range(f->rsi,128,true)){r=EBADF;break;} memset((void*)(uintptr_t)f->rsi,0,128); uint64_t* q=(uint64_t*)(uintptr_t)f->rsi; q[0]=1;q[1]=1;uint32_t fm=0644u; if(fs[f->rdi].path[0]) vfs::get_mode(fs[f->rdi].path,&fm); q[2]=((fs[f->rdi].kind==process::RuntimeFd::Directory)?0040000u:((fs[f->rdi].kind==process::RuntimeFd::File)?0100000u:0140000u))|(fm&07777u);q[7]=fs[f->rdi].size;r=0;break;
    }
    case blockos::syscall::SYS_openat:{
        char raw[256], path[256]; if(!copy_user_string(f->rsi,raw,sizeof(raw))){r=EFAULT;break;} if(!resolve_path_for_process(p,raw,path,sizeof(path))){r=ENAMETOOLONG;break;}
        if(strcmp(path,"/dev/null")==0){ int fd=alloc_fd(); if(fd<0){r=EMFILE;break;} fs=fdtable(); uint32_t ff=(uint32_t)f->rdx; if(f->rdx&O_CLOEXEC) ff|=FD_CLOEXEC; fs[fd]={true,process::RuntimeFd::Null,ff,0,nullptr,0,0,{}}; strncpy(fs[fd].path,path,sizeof(fs[fd].path)-1); r=fd; break; }
        if((strcmp(path,"/dev/urandom")==0||strcmp(path,"/dev/random")==0)){ int fd=alloc_fd(); if(fd<0){r=EMFILE;break;} fs=fdtable(); uint32_t ff=(uint32_t)f->rdx; if(f->rdx&O_CLOEXEC) ff|=FD_CLOEXEC; fs[fd]={true,process::RuntimeFd::Random,ff,0,nullptr,0,0,{}}; strncpy(fs[fd].path,path,sizeof(fs[fd].path)-1); r=fd; break; }
        if(vfs::is_directory(path)){ int fd=alloc_fd(); if(fd<0){r=EMFILE;break;} fs=fdtable(); fs[fd]={true,process::RuntimeFd::Directory,(uint32_t)f->rdx,0,(const uint8_t*)store_directory_path(path),0,0,{}}; strncpy(fs[fd].path,path,sizeof(fs[fd].path)-1); r=fd; break; }
        if(vfs::exists(path)&&!vfs::is_directory(path)){
            int acc=(int)(f->rdx&3); bool wantw=(acc==1||acc==2)||((f->rdx&O_TRUNC)!=0); bool wantr=(acc==0||acc==2);
            if(!perm_allows(p,path,wantr,wantw)){r=EACCES;break;}
        }
        uint32_t sz=0; const uint8_t* data=vfs::read_file(path,&sz); bool exists=vfs::exists(path);
        if(exists && (f->rdx&O_CREAT) && (f->rdx&O_EXCL)){r=EEXIST;break;} /* O_CREAT|O_EXCL: must not exist (lock files) */
        if(!exists && (f->rdx&O_CREAT)){ if(!vfs::create_file(path,nullptr,0)){r=EEXIST;break;} vfs::set_owner(path,p?p->euid:0,p?p->egid:0); vfs::set_mode(path,(uint32_t)(f->r10&07777u&~0022u)); exists=true; data=vfs::read_file(path,&sz); }
        if(exists && !vfs::is_directory(path) && !vfs::is_device(path)){ int fd=alloc_fd(); if(fd<0){r=EMFILE;break;} if(f->rdx&O_TRUNC){ if(!vfs::truncate_file(path,0)){fs[fd]={};r=EIO;break;} data=vfs::read_file(path,&sz);} uint32_t ff=(uint32_t)f->rdx; if(f->rdx&O_CLOEXEC) ff|=FD_CLOEXEC; fs=fdtable(); fs[fd]={true,process::RuntimeFd::File,ff,0,data,sz,(f->rdx&O_APPEND)?sz:0,{}}; strncpy(fs[fd].path,path,sizeof(fs[fd].path)-1); fs[fd].path[sizeof(fs[fd].path)-1]=0; r=fd; break; }
        if(vfs::is_device(path)){int fd=alloc_fd(); if(fd<0){r=EMFILE;break;} vfs::DeviceNodeInfo di{}; if(!vfs::get_device_info(path,&di)){fs[fd]={};r=ENOENT;break;} const auto* saved=store_device_info(di); if(!saved){fs[fd]={};r=ENOMEM;break;} fs[fd]={true,process::RuntimeFd::Device,(uint32_t)f->rdx,0,(const uint8_t*)saved,di.size,0,{}}; strncpy(fs[fd].path,path,sizeof(fs[fd].path)-1); r=fd; break;}
        r=ENOENT; break;
    }
    case blockos::syscall::SYS_execve:
        if(prepare_execve(f)) return;
        /* Exists but is not a loadable image: ENOEXEC lets the shell run it as a script. */
        r=ENOENT;
        { auto* xp=process::current(); char ep[256];
          if(xp && copy_user_string(f->rdi,ep,sizeof(ep))){
            char ab[256]; const char* pth=ep;
            if(ep[0]!='/'){ pth = resolve_path_for_process(xp,ep,ab,sizeof(ab)) ? ab : nullptr; }
            uint32_t esz=0; if(pth && vfs::read_file(pth,&esz) && esz) r=-8; /* ENOEXEC */
          } }
        break;
    case blockos::syscall::SYS_getcwd:{
        char* out=(char*)(uintptr_t)f->rdi; size_t cap=(size_t)f->rsi; const char* c=process::cwd(p); size_t n=strlen(c); if(!cap||n+1>cap||!paging::is_user_range(f->rdi,n+1,true)){r=EINVAL;break;} memcpy(out,c,n+1); r=(int64_t)(n+1); break; }
    case blockos::syscall::SYS_chdir:{
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;} char abs[256]; if(!resolve_path_for_process(p,path,abs,sizeof(abs))||!vfs::is_directory(abs)){r=ENOTDIR;break;} process::set_cwd(p,abs); r=0; break; }
    case blockos::syscall::SYS_mkdir:{
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;} char abs[256]; if(!resolve_path_for_process(p,path,abs,sizeof(abs))){r=ENAMETOOLONG;break;} if(vfs::create_directory(abs)){ vfs::set_owner(abs,p?p->euid:0,p?p->egid:0); r=0; } else r=-EEXIST; break; }
    case blockos::syscall::SYS_unlink:{
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;} char abs[256]; if(!resolve_path_for_process(p,path,abs,sizeof(abs))){r=ENAMETOOLONG;break;} r=vfs::remove_file(abs)?0:ENOENT; break; }
    case blockos::syscall::SYS_rename:{
        char a[256],b[256],aa[256],bb[256]; if(!copy_user_string(f->rdi,a,sizeof(a))||!copy_user_string(f->rsi,b,sizeof(b))){r=EFAULT;break;} if(!resolve_path_for_process(p,a,aa,sizeof(aa))||!resolve_path_for_process(p,b,bb,sizeof(bb))){r=ENAMETOOLONG;break;} r=vfs::rename_path(aa,bb)?0:ENOENT; break; }
    case blockos::syscall::SYS_getdents64:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used||fs[f->rdi].kind!=process::RuntimeFd::Directory){r=ENOTDIR;break;}
        if(!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;}
        auto& d=fs[f->rdi]; const char* dir=(const char*)(uintptr_t)d.data; size_t count=vfs::directory_entry_count(dir); size_t idx=(size_t)d.off; size_t used=0;
        while(idx<count){ const char* nm=vfs::directory_entry_name(dir,idx); if(!nm) break; size_t nl=strlen(nm); size_t reclen=(24+nl+1+7)&~(size_t)7; if(used+reclen>(size_t)f->rdx) break; uint8_t* out=(uint8_t*)(uintptr_t)(f->rsi+used); memset(out,0,reclen); uint64_t ino=idx+2; memcpy(out,&ino,8); int64_t off=(int64_t)(idx+1); memcpy(out+8,&off,8); uint16_t rr=(uint16_t)reclen; memcpy(out+16,&rr,2); char child[256]; size_t dl=strlen(dir); if(dl==1 && dir[0]=='/') snprintf(child,sizeof(child),"/%s",nm); else snprintf(child,sizeof(child),"%s/%s",dir,nm); out[18]=vfs::is_directory(child)?4:8; memcpy(out+19,nm,nl+1); used+=reclen; ++idx; } d.off=idx; r=(int64_t)used; break; }
    case blockos::syscall::SYS_mmap:r=do_mmap(f->rdi,f->rsi,f->rdx,f->r10,f->r8,f->r9);break;
    case blockos::syscall::SYS_munmap:{if(!p||!f->rsi){r=EINVAL;break;}uint64_t len=(f->rsi+PAGE-1)&~(PAGE-1),addr=f->rdi&~(PAGE-1);for(uint64_t va=addr;va<addr+len;va+=PAGE)paging::unmap_4k(p->pml4,va);r=0;break;}
    case blockos::syscall::SYS_mprotect:{if(!p||!f->rsi){r=EINVAL;break;}uint64_t len=(f->rsi+PAGE-1)&~(PAGE-1);uint64_t pf=4;if(f->rdx&2)pf|=2;if(!(f->rdx&4))pf|=1ULL<<63;r=paging::protect_user_range(p->pml4,f->rdi&~(PAGE-1),(size_t)len,pf)?0:ENOMEM;break;}
    case blockos::syscall::SYS_brk:{if(!p){r=ENOMEM;break;}uint64_t want=f->rdi;if(!want){r=p->brk_current;break;}if(want<p->brk_base||want>0x0000000400000000ULL){r=EINVAL;break;}uint64_t old=(p->brk_current+PAGE-1)&~(PAGE-1),nw=(want+PAGE-1)&~(PAGE-1);/* Pages up to brk_mapped are still mapped (brk never unmaps on shrink): map only the new tail, never remap live pages. */uint64_t from=old>p->brk_mapped?old:p->brk_mapped;if(nw>from&&!paging::map_user_range(p->pml4,from,(size_t)(nw-from),6|(1ULL<<63))){r=ENOMEM;break;}if(nw>p->brk_mapped)p->brk_mapped=nw;p->brk_current=want;r=want;break;}
    case blockos::syscall::SYS_getuid:r=p?(int64_t)p->uid:0;break;
    case blockos::syscall::SYS_geteuid:r=p?(int64_t)p->euid:0;break;
    case blockos::syscall::SYS_getgid:r=p?(int64_t)p->gid:0;break;
    case blockos::syscall::SYS_getegid:r=p?(int64_t)p->egid:0;break;
    case blockos::syscall::SYS_setuid:{ if(!p){r=EPERM;break;} uint32_t v=(uint32_t)f->rdi; if(p->euid==0){p->uid=p->euid=v;} else if(v==p->uid){p->euid=v;} else {r=EPERM;break;} r=0; break; }
    case blockos::syscall::SYS_seteuid:{ if(!p){r=EPERM;break;} uint32_t v=(uint32_t)f->rdi; if(p->euid==0||v==p->uid||v==p->euid){p->euid=v;} else {r=EPERM;break;} r=0; break; }
    case blockos::syscall::SYS_setgid:{ if(!p){r=EPERM;break;} uint32_t v=(uint32_t)f->rdi; if(p->euid==0){p->gid=p->egid=v;} else if(v==p->gid){p->egid=v;} else {r=EPERM;break;} r=0; break; }
    case blockos::syscall::SYS_setegid:{ if(!p){r=EPERM;break;} uint32_t v=(uint32_t)f->rdi; if(p->euid==0||v==p->gid||v==p->egid){p->egid=v;} else {r=EPERM;break;} r=0; break; }
    case blockos::syscall::SYS_setgroups:
    case blockos::syscall::SYS_capget:
    case blockos::syscall::SYS_capset:r=0;break;
    case blockos::syscall::SYS_getpid:r=p?p->pid:-1;break;
    case blockos::syscall::SYS_getppid:r=p?(int64_t)p->parent_pid:0;break;
    case blockos::syscall::SYS_gettid:r=p?(int64_t)p->tid:-1;break;
    case blockos::syscall::SYS_set_tid_address: if(!p||!paging::is_user_range(f->rdi,4,true)){r=EFAULT;break;}p->clear_tid=f->rdi;r=(int64_t)p->tid;break;
    case blockos::syscall::SYS_arch_prctl:{if(!p) {r=EINVAL;break;} if(f->rdi==ARCH_SET_FS){if(f->rsi>0x00007ffffffff000ULL){r=EINVAL;break;}p->fs_base=f->rsi; tls::write_fs_base(f->rsi); r=0;}else if(f->rdi==ARCH_GET_FS){if(!paging::is_user_range(f->rsi,8,true)){r=EFAULT;break;}*(uint64_t*)(uintptr_t)f->rsi=p->fs_base;r=0;}else r=EINVAL;break;}
    case blockos::syscall::SYS_nanosleep:{
        if(!paging::is_user_range(f->rdi,16,false)){r=EFAULT;break;}
        const int64_t sec=*(const int64_t*)(uintptr_t)f->rdi; const int64_t nsec=*(const int64_t*)(uintptr_t)(f->rdi+8);
        if(sec<0||nsec<0||nsec>=1000000000){r=EINVAL;break;}
        uint64_t deadline=timer_uptime_ms()+(uint64_t)sec*1000ull+(uint64_t)(nsec/1000000);
        r=0;
        if(preempt::block_until_from_syscall(f,deadline)) return;
        break;
    }
    case blockos::syscall::SYS_clone:{
        if(!p){r=EINVAL;break;}uint64_t child_sp=f->rsi; if(child_sp && (child_sp&0xF))child_sp-=8; if(!child_sp||!paging::is_user_range(child_sp-8,8,true)){r=EFAULT;break;}
        process::Process* child=nullptr;for(size_t i=0;i<process::slot_count();i++){auto* x=process::slot_at(i);if(x&&x->state==process::State::EMPTY){child=x;break;}}
        if(!child){r=ENOMEM;break;}memset(child,0,sizeof(*child)); child->pid=child->tid=process::count()+1000; while(process::get(child->pid))child->pid++; child->pml4=p->pml4; p->pml4_owned=false; child->pml4_owned=false; child->fd_owner=p->fd_owner?p->fd_owner:p; child->parent_pid=p->tid; child->is_thread=true; child->fs_base=f->r8?f->r8:p->fs_base; child->brk_base=p->brk_base; child->brk_current=p->brk_current; child->brk_mapped=p->brk_mapped; child->mmap_next=p->mmap_next; child->entry=p->entry; child->stack=child_sp; child->saved_frame=*reinterpret_cast<TrapFrame*>(f); child->saved_frame.rax=0; child->saved_frame.rdi=f->rdx; child->saved_frame.rsp=child_sp; child->state=process::State::READY; child->frame_valid=true; r=(int64_t)child->tid; break;
    }
    case blockos::syscall::SYS_sched_yield:{r=0;f->rax=0;if(preempt::yield_from_syscall(f))return;break;}
    case blockos::syscall::SYS_futex:{
        uint64_t uaddr=f->rdi;uint32_t op=(uint32_t)f->rsi&0x7f;uint32_t val=(uint32_t)f->rdx;
        if(!paging::is_user_range(uaddr,4,(op==FUTEX_WAIT))){r=EFAULT;break;}
        if(op==FUTEX_WAKE){wake_futex(uaddr,val?val:0xffffffffu);r=0;break;}
        if(op==FUTEX_WAIT){if(*(volatile uint32_t*)(uintptr_t)uaddr!=val){r=EAGAIN;break;}int slot=-1;for(int i=0;i<(int)(sizeof(futex_waiters)/sizeof(futex_waiters[0]));i++)if(!futex_waiters[i].used){slot=i;break;}if(slot<0){r=ENOMEM;break;}if(f->r10){r=EAGAIN;break;}futex_waiters[slot]={true,uaddr,p->tid};r=0;if(!preempt::block_from_syscall(f)){futex_waiters[slot].used=false;r=EAGAIN;break;}return;}
        r=ENOSYS;break;
    }
    case blockos::syscall::SYS_exit:
    case blockos::syscall::SYS_exit_group:{ 
        release_process_fds(p);
        if(p&&p->vfork_child){ if(auto* parent=process::get(p->vfork_parent_pid)){ if(parent->state==process::State::BLOCKED) parent->state=process::State::READY; } p->vfork_child=false; p->vfork_parent_pid=0; }
        if(p){p->exit_code=f->rdi; if(auto* parent=process::get(p->parent_pid)){ if(parent->state==process::State::BLOCKED && (parent->wait_target_pid==0 || parent->wait_target_pid==p->pid)){ parent->saved_frame.rax=p->pid; if(parent->wait_status_ptr && parent->pml4){ /* the parent's stack lives in the parent's tables; fork gave us private copies, so write through the parent's pml4 */ uint64_t my_cr3=paging::read_cr3(); paging::switch_pml4(parent->pml4); if(paging::is_user_range(parent->wait_status_ptr,4,true)) *reinterpret_cast<int*>(static_cast<uintptr_t>(parent->wait_status_ptr))=(int)(p->exit_code<<8); paging::switch_pml4(my_cr3); } parent->wait_target_pid=0; parent->wait_status_ptr=0; parent->state=process::State::READY; } }if(p->clear_tid&&paging::is_user_range(p->clear_tid,4,true)){*(uint32_t*)(uintptr_t)p->clear_tid=0;wake_futex(p->clear_tid,1);}p->state=process::State::TERMINATED;}
        if(preempt::on_exit(f))return; f->rax=0;f->rip=(uint64_t)(uintptr_t)&blockos_user_return;f->cs=0x08;f->ss=0x10;f->rsp=blockos_user_saved_rsp;f->rflags|=0x200;return;
    }
    case blockos::syscall::SYS_getrandom:{
        if(!paging::is_user_range(f->rdi,(size_t)f->rsi,true)){r=EFAULT;break;} uint64_t x=timer_uptime_ms()^(uint64_t)(uintptr_t)p^(uint64_t)f->rip; for(size_t i=0;i<(size_t)f->rsi;i++){x^=x<<13;x^=x>>7;x^=x<<17;((uint8_t*)(uintptr_t)f->rdi)[i]=(uint8_t)x;} r=(int64_t)f->rsi; break; }
    case blockos::syscall::SYS_pipe2:{
        if(!paging::is_user_range(f->rdi,8,true)){r=EFAULT;break;} int sfds[2]; int sa=socket_obj_alloc(), sb=socket_obj_alloc(); if(sa<0||sb<0){if(sa>=0)socket_free(sa);if(sb>=0)socket_free(sb);r=ENOMEM;break;} unix_socks[sa].peer=sb;unix_socks[sb].peer=sa; int a=alloc_fd(),b=alloc_fd(); if(a<0||b<0){if(a>=0)fs[a]={};if(b>=0)fs[b]={};socket_free(sa);socket_free(sb);r=EMFILE;break;} uint32_t pf=(f->rsi&(O_CLOEXEC))?FD_CLOEXEC:0; if(f->rsi&O_NONBLOCK)pf|=FD_NONBLOCK; fs=fdtable();fs[a]={true,process::RuntimeFd::UnixSocket,pf,(uint32_t)(sa+1),nullptr,0,0,{}};fs[b]={true,process::RuntimeFd::UnixSocket,pf,(uint32_t)(sb+1),nullptr,0,0,{}};sfds[0]=a;sfds[1]=b;memcpy((void*)(uintptr_t)f->rdi,sfds,8);r=0;break; }
    case blockos::syscall::SYS_readv:{
        struct IOV{void* base;size_t len;}; if(!paging::is_user_range(f->rsi,(size_t)f->rdx*sizeof(IOV),false)){r=EFAULT;break;} int64_t total=0; const IOV* v=(const IOV*)(uintptr_t)f->rsi; for(size_t i=0;i<(size_t)f->rdx;i++){if(!v[i].base||!paging::is_user_range((uint64_t)(uintptr_t)v[i].base,v[i].len,true)){r=EFAULT;break;} int64_t n=blockos_tty_read(v[i].base,v[i].len); if(n<0)break; total+=n;if((size_t)n<v[i].len)break;} if(r==ENOSYS)r=total; break; }
    case blockos::syscall::SYS_writev:{
        struct IOV{const void* base;size_t len;}; if(!paging::is_user_range(f->rsi,(size_t)f->rdx*sizeof(IOV),false)){r=EFAULT;break;} int64_t total=0; const IOV* v=(const IOV*)(uintptr_t)f->rsi; for(size_t i=0;i<(size_t)f->rdx;i++){if(!v[i].base||!paging::is_user_range((uint64_t)(uintptr_t)v[i].base,v[i].len,false)){r=EFAULT;break;} int64_t n=blockos_tty_write(v[i].base,v[i].len); if(n<0)break; total+=n;} if(r==ENOSYS)r=total; break; }
    case blockos::syscall::SYS_ioctl:{
        /* Only tty termios GET/SET are implemented; other requests stay no-ops. */
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        if(fs[f->rdi].kind==process::RuntimeFd::Tty && (f->rsi==0x424F5303ULL||f->rsi==0x424F5304ULL)){
            const bool get = (f->rsi==0x424F5303ULL);
            if(!paging::is_user_range(f->rdx,36,get)){r=EFAULT;break;}
            r=blockos_tty_ioctl(f->rsi,(void*)(uintptr_t)f->rdx);
            break;
        }
        r=0;break;
    }
    case blockos::syscall::SYS_rt_sigaction:{
        if(!p || f->rdi==0 || f->rdi>64 || f->r10!=8){r=EINVAL;break;}
        const uint64_t idx=f->rdi-1;
        struct SigAction { uint64_t handler; uint64_t flags; uint64_t restorer; uint64_t mask; };
        if(f->rsi && !paging::is_user_range(f->rsi,sizeof(SigAction),false)){r=EFAULT;break;}
        if(f->rdx && !paging::is_user_range(f->rdx,sizeof(SigAction),true)){r=EFAULT;break;}
        if(f->rdx){ SigAction* old=(SigAction*)(uintptr_t)f->rdx; old->handler=p->signal_handlers[idx]; old->flags=p->signal_flags[idx]; old->restorer=0; old->mask=0; }
        if(f->rsi){ const SigAction* neu=(const SigAction*)(uintptr_t)f->rsi; p->signal_handlers[idx]=neu->handler; p->signal_flags[idx]=neu->flags; }
        r=0;break;
    }
    case blockos::syscall::SYS_rt_sigprocmask:{
        if(!p || f->r10!=8){r=EINVAL;break;}
        if(f->rdx && !paging::is_user_range(f->rdx,8,true)){r=EFAULT;break;}
        if(f->rsi && !paging::is_user_range(f->rsi,8,false)){r=EFAULT;break;}
        if(f->rdx)*(uint64_t*)(uintptr_t)f->rdx=p->signal_mask;
        if(f->rsi){uint64_t m=*(const uint64_t*)(uintptr_t)f->rsi;switch((int)f->rdi){case 0:p->signal_mask|=m;break;case 1:p->signal_mask&=~m;break;case 2:p->signal_mask=m;break;default:r=EINVAL;break;}}
        if(r==ENOSYS)r=0;
        break;
    }
    case blockos::syscall::SYS_rt_sigreturn:r=0;break;
    case blockos::syscall::SYS_kill:
    case blockos::syscall::SYS_tgkill:r=0;break;
    case blockos::syscall::SYS_setpgid:{
        if(!p){r=ESRCH;break;}
        uint64_t pid=f->rdi?f->rdi:p->pid, pgid=f->rsi;
        if(!pgid) pgid=pid;
        auto* x=process::get(pid); if(!x){r=ESRCH;break;}
        x->pgid=pgid; r=0; break;
    }
    case blockos::syscall::SYS_getpgid:{
        if(!p){r=ESRCH;break;}
        uint64_t pid=f->rdi?f->rdi:p->pid; auto* x=process::get(pid);
        if(!x){r=ESRCH;break;}
        r=(int64_t)x->pgid; break;
    }
    case blockos::syscall::SYS_setsid:{
        if(!p){r=ESRCH;break;}
        p->sid=p->pid; p->pgid=p->pid; r=(int64_t)p->sid; break;
    }
    case blockos::syscall::SYS_set_robust_list:
    case blockos::syscall::SYS_rseq:
    case blockos::syscall::SYS_prctl:
        r=0;break;
    case blockos::syscall::SYS_statx:{
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;} char abs[256];if(!resolve_path_for_process(p,path,abs,sizeof(abs))){r=ENAMETOOLONG;break;} uint32_t sz=0; const uint8_t*d=vfs::read_file(abs,&sz); if(!d&&!vfs::is_directory(abs)){r=ENOENT;break;} if(!paging::is_user_range(f->r9,256,true)){r=EFAULT;break;} memset((void*)(uintptr_t)f->r9,0,256); ((uint64_t*)(uintptr_t)f->r9)[0]=1; ((uint64_t*)(uintptr_t)f->r9)[2]=vfs::is_directory(abs)?0040755:0100444; ((uint64_t*)(uintptr_t)f->r9)[11]=sz; r=0;break; }
    case blockos::syscall::SYS_socket:{int domain=(int)f->rdi,type=(int)(f->rsi&0xffff),proto=(int)f->rdx;if(domain!=AF_UNIX||(type&0xf)!=SOCK_STREAM){r=ENOSYS;break;}int si=socket_obj_alloc();if(si<0){r=ENOMEM;break;}int fd=alloc_fd();if(fd<0){socket_free(si);r=ENOMEM;break;}fs=fdtable();fs[fd]={true,process::RuntimeFd::UnixSocket,0,(uint32_t)(si+1),nullptr,0,0};r=fd;break;}
    case blockos::syscall::SYS_bind:{int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}if(f->rdx<3||!paging::is_user_range(f->rsi,(size_t)f->rdx,false)){r=EFAULT;break;}char key[sizeof(unix_socks[0].path)]; if(!unix_key_from_addr(f->rsi,f->rdx,key,sizeof(key))){r=EINVAL;break;}bool collision=false;for(auto& s:unix_socks)if(s.used&&s.listener&&strcmp(s.path,key)==0){collision=true;break;}if(collision){r=EADDRINUSE;break;}strncpy(unix_socks[si].path,key,sizeof(unix_socks[si].path)-1);unix_socks[si].path[sizeof(unix_socks[si].path)-1]=0;r=0;break;}
    case blockos::syscall::SYS_listen:{int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}unix_socks[si].listener=true;r=0;break;}
    case blockos::syscall::SYS_connect:{int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}if(!paging::is_user_range(f->rsi,(size_t)f->rdx,false)||f->rdx<3){r=EFAULT;break;}char sp[sizeof(unix_socks[0].path)]; if(!unix_key_from_addr(f->rsi,f->rdx,sp,sizeof(sp))){r=EINVAL;break;}int li=-1;for(int i=0;i<256;i++)if(unix_socks[i].used&&unix_socks[i].listener&&strcmp(unix_socks[i].path,sp)==0){li=i;break;}if(li<0){r=ENOENT;break;}int peer=socket_obj_alloc();if(peer<0||unix_socks[li].pending_count>=16){r=ENOMEM;break;}unix_socks[si].peer=peer;unix_socks[si].connected=true;unix_socks[peer].peer=si;unix_socks[peer].connected=true;unix_socks[li].pending[unix_socks[li].pending_count++]=peer;r=0;break;}
    case blockos::syscall::SYS_accept:
    case blockos::syscall::SYS_accept4:{int li=fd_socket_index((int)f->rdi);if(li<0||!unix_socks[li].listener){r=ENOTSOCK;break;}if(unix_socks[li].pending_count==0){r=EAGAIN;break;}int si=unix_socks[li].pending[0];for(int i=1;i<unix_socks[li].pending_count;i++)unix_socks[li].pending[i-1]=unix_socks[li].pending[i];--unix_socks[li].pending_count;int fd=alloc_fd();if(fd<0){r=ENOMEM;break;}fs=fdtable();fs[fd]={true,process::RuntimeFd::UnixSocket,(uint16_t)f->r10,(uint32_t)(si+1),nullptr,0,0};r=fd;break;}
    case blockos::syscall::SYS_sendto:{int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}if(!paging::is_user_range(f->rsi,(size_t)f->rdx,false)){r=EFAULT;break;}if(!socket_write_idx(si,(const uint8_t*)(uintptr_t)f->rsi,(size_t)f->rdx)){r=EAGAIN;break;}r=f->rdx;break;}
    case blockos::syscall::SYS_recvfrom:{int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}if(!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;}if(unix_socks[si].rx_size==0&&!unix_socks[si].closed&&unix_socks[si].peer>=0&&f->rdx>0){r=EAGAIN;break;} /* open but empty: not EOF */ r=(int64_t)socket_read_idx(si,(uint8_t*)(uintptr_t)f->rsi,(size_t)f->rdx);break;}
    case blockos::syscall::SYS_getsockname:
    case blockos::syscall::SYS_getpeername:{
        /* sockaddr_un layout: u16 family (AF_UNIX=1) + path; *len in/out. */
        int si=fd_socket_index((int)f->rdi);
        if(si<0){r=ENOTSOCK;break;}
        if(!paging::is_user_range(f->rdx,4,true)){r=EFAULT;break;}
        uint32_t cap=*reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(f->rdx));
        const char* path=unix_socks[si].path;
        if(f->rax==blockos::syscall::SYS_getpeername){
            int pi=unix_socks[si].peer;
            if(pi<0||!unix_socks[si].connected){r=ENOTCONN;break;}
            path=unix_socks[pi].path;
        }
        uint32_t need=2u+(uint32_t)strlen(path)+1u;
        uint32_t n=cap<need?cap:need;
        if(n>0){
            if(!paging::is_user_range(f->rsi,n,true)){r=EFAULT;break;}
            uint8_t* o=reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(f->rsi));
            if(n>=1)o[0]=1;
            if(n>=2)o[1]=0;
            for(uint32_t i=2;i<n;i++)o[i]=(uint8_t)path[i-2];
        }
        *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(f->rdx))=need;
        r=0; break;}
    case blockos::syscall::SYS_shutdown:{r=0;break;}
    case blockos::syscall::SYS_setsockopt: case blockos::syscall::SYS_getsockopt:{r=0;break;}
    case blockos::syscall::SYS_socketpair:{
        if((int)f->rdi!=AF_UNIX||(int)(f->rsi&0xffff)!=SOCK_STREAM||!paging::is_user_range(f->r9,8,true)){r=ENOSYS;break;}int a=socket_obj_alloc(),b=socket_obj_alloc();if(a<0||b<0){if(a>=0)socket_free(a);if(b>=0)socket_free(b);r=ENOMEM;break;}unix_socks[a].peer=b;unix_socks[b].peer=a;unix_socks[a].connected=unix_socks[b].connected=true;int fda=alloc_fd(),fdb=alloc_fd();if(fda<0||fdb<0){r=ENOMEM;break;}fs=fdtable();fs[fda]={true,process::RuntimeFd::UnixSocket,0,(uint32_t)(a+1),nullptr,0,0};fs[fdb]={true,process::RuntimeFd::UnixSocket,0,(uint32_t)(b+1),nullptr,0,0};int* up=(int*)(uintptr_t)f->r9;up[0]=fda;up[1]=fdb;r=0;break;}
    case blockos::syscall::SYS_sendmsg:{
        int si=fd_socket_index((int)f->rdi);if(si<0||!paging::is_user_range(f->rsi,56,false)){r=ENOTSOCK;break;}struct IOV{void*base;size_t len;};struct MSG{void*name;uint32_t namelen;uint32_t pad;IOV*iov;size_t iovlen;void*control;size_t controllen;int flags;};auto*m=(MSG*)(uintptr_t)f->rsi;size_t total=0;for(size_t i=0;i<m->iovlen&&i<64;i++){if(!paging::is_user_range((uint64_t)(uintptr_t)m->iov[i].base,m->iov[i].len,false)){r=EFAULT;goto sendmsg_done;}if(!socket_write_idx(si,(const uint8_t*)m->iov[i].base,m->iov[i].len)){r=EAGAIN;goto sendmsg_done;}total+=m->iov[i].len;}r=(int64_t)total;sendmsg_done:break;}
    case blockos::syscall::SYS_recvmsg:{
        int si=fd_socket_index((int)f->rdi);if(si<0||!paging::is_user_range(f->rsi,56,true)){r=ENOTSOCK;break;}struct IOV{void*base;size_t len;};struct MSG{void*name;uint32_t namelen;uint32_t pad;IOV*iov;size_t iovlen;void*control;size_t controllen;int flags;};auto*m=(MSG*)(uintptr_t)f->rsi;size_t total=0;m->controllen=0;m->flags=0;if(unix_socks[si].rx_size==0&&!unix_socks[si].closed&&unix_socks[si].peer>=0&&m->iovlen>0){r=EAGAIN;break;} /* open but empty: not EOF */for(size_t i=0;i<m->iovlen&&i<64;i++){if(!paging::is_user_range((uint64_t)(uintptr_t)m->iov[i].base,m->iov[i].len,true)){r=EFAULT;goto recvmsg_done;}size_t n=socket_read_idx(si,(uint8_t*)m->iov[i].base,m->iov[i].len);total+=n;if(n<m->iov[i].len)break;}r=(int64_t)total;recvmsg_done:break;}
    case blockos::syscall::SYS_poll:{
        struct Pollfd{int fd;short events;short revents;};if(!paging::is_user_range(f->rdi,(size_t)f->rsi*sizeof(Pollfd),true)){r=EFAULT;break;}auto* pfd=(Pollfd*)(uintptr_t)f->rdi;int ready=0;for(uint64_t i=0;i<f->rsi;i++){pfd[i].revents=0;if((pfd[i].events&POLLIN)&&fd_readable(pfd[i].fd))pfd[i].revents|=POLLIN;if((pfd[i].events&POLLOUT)&&fd_writable(pfd[i].fd))pfd[i].revents|=POLLOUT;if(pfd[i].revents)ready++;}r=ready;break;}
    case blockos::syscall::SYS_epoll_create1:{int ei=-1;for(int i=0;i<64;i++)if(!epolls[i].used){epolls[i]={};epolls[i].used=true;ei=i;break;}if(ei<0){r=ENOMEM;break;}int fd=alloc_fd();if(fd<0){epolls[ei]={};r=ENOMEM;break;}fs=fdtable();fs[fd]={true,process::RuntimeFd::File,0,(uint32_t)(0x80000000u|(uint32_t)ei),nullptr,0,0};r=fd;break;}
    case blockos::syscall::SYS_epoll_ctl:{if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}uint32_t obj=fs[f->rdi].object; if(!(obj&0x80000000u)){r=EINVAL;break;}EpollObj& ep=epolls[obj&0x7fffffffu];struct Ev{uint32_t events;uint32_t pad;uint64_t data;};if(!paging::is_user_range(f->r9,16,false)){r=EFAULT;break;}auto* ev=(Ev*)(uintptr_t)f->r9;int idx=-1;for(int i=0;i<64;i++)if(ep.watch[i].used&&ep.watch[i].fd==(int)f->rdx){idx=i;break;}int op=(int)f->rsi;if(op==1){for(int i=0;i<64;i++)if(!ep.watch[i].used){ep.watch[i]={true,(int)f->rdx,ev->events,ev->data};r=0;break;}if(r!=0)r=ENOMEM;}else if(op==2){if(idx>=0){ep.watch[idx]={};r=0;}else r=ENOENT;}else if(op==3){if(idx>=0){ep.watch[idx].events=ev->events;ep.watch[idx].data=ev->data;r=0;}else r=ENOENT;}else r=EINVAL;break;}
    case blockos::syscall::SYS_epoll_wait:{if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}uint32_t obj=fs[f->rdi].object;if(!(obj&0x80000000u)){r=EINVAL;break;}EpollObj& ep=epolls[obj&0x7fffffffu];struct Ev{uint32_t events;uint32_t pad;uint64_t data;};if(!paging::is_user_range(f->rsi,(size_t)f->rdx*sizeof(Ev),true)){r=EFAULT;break;}auto*out=(Ev*)(uintptr_t)f->rsi;int n=0;for(auto&w:ep.watch){if(!w.used||n>=(int)f->rdx)continue;uint32_t ev=0;if((w.events&POLLIN)&&fd_readable(w.fd))ev|=POLLIN;if((w.events&POLLOUT)&&fd_writable(w.fd))ev|=POLLOUT;if(ev){out[n++]={ev,0,w.data};}}r=n;break;}
    case blockos::syscall::SYS_fcntl:{if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}auto&d=fs[f->rdi];switch(f->rsi){case F_GETFD:r=(d.flags&FD_CLOEXEC)?FD_CLOEXEC:0;break;case F_SETFD:d.flags=(uint16_t)((d.flags&~FD_CLOEXEC)|((uint16_t)f->rdx&FD_CLOEXEC));r=0;break;case F_GETFL:r=d.flags;break;case F_SETFL:d.flags=(uint16_t)f->rdx;r=0;break;case F_DUPFD:{int n=alloc_fd((uint16_t)f->rdx);if(n<0){r=EMFILE;break;}fs[n]=d;r=n;break;}case 1030 /* F_DUPFD_CLOEXEC */:{int n=alloc_fd((uint16_t)f->rdx);if(n<0){r=EMFILE;break;}fs[n]=d;fs[n].flags=(uint16_t)(fs[n].flags|FD_CLOEXEC);r=n;break;}default:r=EINVAL;break;}break;}
    case blockos::syscall::SYS_dup:{if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}int n=alloc_fd();if(n<0){r=ENOMEM;break;}fs[n]=fs[f->rdi];r=n;break;}
    case blockos::syscall::SYS_dup2:{if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used||f->rsi>=process::MAX_RUNTIME_FDS){r=EBADF;break;}if(f->rdi==f->rsi){r=f->rsi;break;} if(fs[f->rsi].used&&fs[f->rsi].kind==process::RuntimeFd::UnixSocket){int oi=(int)fs[f->rsi].object-1; fs[f->rsi]={}; socket_release_fd(oi,p,(int)f->rsi);} fs[f->rsi]=fs[f->rdi];r=f->rsi;break;}
    case blockos::syscall::SYS_clock_gettime:{if(!paging::is_user_range(f->rsi,16,true)){r=EFAULT;break;}uint64_t ms=timer_uptime_ms();struct TS{int64_t sec,nsec;}*ts=(TS*)(uintptr_t)f->rsi;ts->sec=(int64_t)(ms/1000);ts->nsec=(int64_t)((ms%1000)*1000000);r=0;break;}
    case blockos::syscall::SYS_wait4:{
        if(!p){r=ECHILD;break;} r=ECHILD; int64_t wanted=(int64_t)f->rdi; if(wanted<=0) wanted=0; /* pid -1 / 0: any child */ bool nohang=(f->r10&WNOHANG)!=0; bool have_child=false;
        for(size_t i=0;i<process::slot_count();i++){auto*x=process::slot_at(i);if(!x||x->state==process::State::EMPTY||x->parent_pid!=p->tid)continue;have_child=true;if(x->state!=process::State::TERMINATED)continue;if(wanted>0 && (int64_t)x->pid!=wanted)continue;{ /* File offsets are per-process copies here; a child that wrote through an inherited fd
         * hands its offset back when it is reaped, like a shared open file. */
  auto* cf=process::fds(x); auto* pf=process::fds(p);
  if(cf&&pf) for(size_t i=0;i<process::MAX_RUNTIME_FDS;i++){
    if(cf[i].used&&pf[i].used&&cf[i].kind==process::RuntimeFd::File&&pf[i].kind==process::RuntimeFd::File&&strcmp(cf[i].path,pf[i].path)==0) pf[i].off=cf[i].off;
  } }
if(x->pml4_owned&&x->pml4&&x->pml4!=(paging::read_cr3()&0x000ffffffffff000ULL)){ if(pml4_used_elsewhere(x->pml4,x)) g_pml4_skips++; else paging::destroy_user_space(x->pml4); x->pml4_owned=false; x->pml4=0; }
        r=x->pid;if(f->rdx&&paging::is_user_range(f->rdx,4,true))*reinterpret_cast<int*>(static_cast<uintptr_t>(f->rdx))=(int)(x->exit_code<<8);x->state=process::State::EMPTY;break;}
        if(r==ECHILD){ if(!have_child){r=ECHILD;break;} if(nohang){r=0;break;} p->wait_target_pid=wanted; p->wait_status_ptr=f->rdx; f->rax=0; p->state=process::State::BLOCKED; if(preempt::block_from_syscall(f)) return; return; }
        break; }
    case blockos::syscall::SYS_fsync: { r=0; break; }
    case blockos::syscall::SYS_ftruncate: {
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used||fs[f->rdi].kind!=process::RuntimeFd::File){r=EBADF;break;}
        auto& d=fs[f->rdi]; if(f->rsi>0xffffffffULL||!vfs::truncate_file(d.path,(uint32_t)f->rsi)){r=EINVAL;break;}
        uint32_t ns=0; d.data=vfs::read_file(d.path,&ns); d.size=ns; if(d.off>ns)d.off=ns; r=0; break;
    }
    case blockos::syscall::SYS_readlink: {
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;}
        const char* target = (p && strcmp(path,"/proc/self/exe")==0) ? p->name : nullptr;
        if(!target){r=EINVAL;break;} if(!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;}
        size_t n=strlen(target); if(n>(size_t)f->rdx)n=(size_t)f->rdx; memcpy((void*)(uintptr_t)f->rsi,target,n); r=(int64_t)n; break;
    }
    case blockos::syscall::SYS_rmdir: {
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;} r=vfs::remove_file(path)?0:ENOTEMPTY; break;
    }
    case blockos::syscall::SYS_fork:
    case blockos::syscall::SYS_vfork: {
        if(!p){r=EFAULT;break;}
        bool vf = (f->rax==blockos::syscall::SYS_vfork);
        auto* child=process::fork_like(p,*reinterpret_cast<TrapFrame*>(f),vf);
        if(!child){r=ENOMEM;break;}
        f->rax=child->pid;
        /* fork: the child has its own copy, so the parent keeps running.
         * vfork: the parent waits until the child execs or exits. */
        if(!vf) return;
        p->state=process::State::BLOCKED;
        if(preempt::block_from_syscall(f)) return;
        return;
    }
    case blockos::syscall::SYS_dup3: {
        if(f->rdi>=process::MAX_RUNTIME_FDS||f->rsi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        if(f->rdi==f->rsi){r=EINVAL;break;} if(fs[f->rsi].used&&fs[f->rsi].kind==process::RuntimeFd::UnixSocket){int oi=(int)fs[f->rsi].object-1; fs[f->rsi]={}; socket_release_fd(oi,p,(int)f->rsi);} fs[f->rsi]=fs[f->rdi]; fs[f->rsi].flags=(fs[f->rsi].flags & ~FD_CLOEXEC)|((f->rdx&O_CLOEXEC)?FD_CLOEXEC:0); r=f->rsi; break;
    }
    case blockos::syscall::SYS_madvise: { r=0; break; }
    case blockos::syscall::SYS_gettimeofday: {
        if(!paging::is_user_range(f->rdi,16,true)){r=EFAULT;break;} struct TV{int64_t sec,usec;}*tv=(TV*)(uintptr_t)f->rdi; uint64_t ms=timer_uptime_ms(); tv->sec=(int64_t)(ms/1000); tv->usec=(int64_t)((ms%1000)*1000); r=0; break;
    }
    case blockos::syscall::SYS_uname: {
        if(!paging::is_user_range(f->rdi,390,true)){r=EFAULT;break;} char* q=(char*)(uintptr_t)f->rdi; memset(q,0,390); const char* a[]={"BlockOS","blockos","1.0","BlockOS x86_64","x86_64"}; for(int i=0;i<5;i++)strncpy(q+i*65,a[i],64); r=0; break;
    }
    case blockos::syscall::SYS_getrlimit:
    case blockos::syscall::SYS_setrlimit: {
        if(!paging::is_user_range(f->rsi,16,f->rax==blockos::syscall::SYS_getrlimit)){r=EFAULT;break;}
        if(f->rax==blockos::syscall::SYS_getrlimit){uint64_t* q=(uint64_t*)(uintptr_t)f->rsi;q[0]=0x7fffffffffffffffULL;q[1]=0x7fffffffffffffffULL;} r=0; break;
    }
    case blockos::syscall::SYS_sched_getaffinity: { if(!paging::is_user_range(f->r9,8,true)){r=EFAULT;break;} *(uint64_t*)(uintptr_t)f->r9=1ULL; r=8; break; }
    case blockos::syscall::SYS_access:{char path[256];if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;}uint32_t sz=0;r=(vfs::read_file(path,&sz)||vfs::is_directory(path)||vfs::is_device(path))?0:ENOENT;break;}
    case blockos::syscall::SYS_stat: case blockos::syscall::SYS_lstat:{
        char raw[256],path[256];if(!copy_user_string(f->rdi,raw,sizeof(raw))){r=EFAULT;break;}
        if(!resolve_path_for_process(p,raw,path,sizeof(path))){r=ENAMETOOLONG;break;}
        if(!paging::is_user_range(f->rsi,128,true)){r=EFAULT;break;}
        uint32_t sz=0;const uint8_t*d=vfs::read_file(path,&sz);bool dir=vfs::is_directory(path);bool dev=vfs::is_device(path);
        if(!d&&!dir&&!dev){r=ENOENT;break;}
        uint32_t pm=dir?0755u:(dev?0666u:0644u); vfs::get_mode(path,&pm);
        /* The image format carries no exec bits: a regular file that is clearly a program (ELF or #! script) reports execute permission so shell tests such as [ -x ] work. */
        if(!dir&&!dev&&d&&sz>=4&&(pm&0111u)==0&&((d[0]==0x7f&&d[1]=='E'&&d[2]=='L'&&d[3]=='F')||(d[0]=='#'&&d[1]=='!')))pm|=0111u;
        memset((void*)(uintptr_t)f->rsi,0,128);uint64_t*q=(uint64_t*)(uintptr_t)f->rsi;q[0]=1;q[1]=1;q[2]=(dev?0140000u:(dir?0040000u:0100000u))|(pm&07777u);q[7]=d?(uint64_t)sz:0;r=0;break;
    }
    default:r=ENOSYS;break;
    }
    f->rax=(uint64_t)r;
}
