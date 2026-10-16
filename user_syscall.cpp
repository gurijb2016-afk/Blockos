#include "preempt.hpp"
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

extern "C" int64_t blockos_tty_read(void*,size_t);
extern "C" int64_t blockos_tty_write(const void*,size_t);
extern "C" uint64_t timer_uptime_ms();
extern "C" void blockos_user_return();
extern "C" uint64_t blockos_user_saved_rsp;

namespace {
constexpr int64_t EPERM=-1,ESRCH=-3,EINTR=-4,EBADF=-9,EAGAIN=-11,EMFILE=-24,EFAULT=-14,EINVAL=-22,ENOSYS=-38,ENOTCONN=-107,EADDRINUSE=-98,ENOENT=-2,ENOTSOCK=-88,EMSGSIZE=-90,ENOMEM=-12,ESPIPE=-29,ECHILD=-10,EEXIST=-17,ENOTDIR=-20,EISDIR=-21,ENAMETOOLONG=-36,ENOSPC=-28,ENOTEMPTY=-39,ENOTTY=-25,EIO=-5,EACCES=-13,EPROTONOSUPPORT=-93,EISCONN=-106,EAFNOSUPPORT=-97,ENOPROTOOPT=-92;
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
constexpr uint32_t FD_CLOEXEC=0x80000000u;
constexpr uint64_t FD_NONBLOCK=04000;
constexpr int O_WRONLY=1, O_RDWR=2, O_CREAT=0100, O_EXCL=0200, O_TRUNC=01000, O_APPEND=02000, O_NONBLOCK=04000, O_DIRECTORY=0200000, O_CLOEXEC=02000000;
constexpr int WNOHANG=1;
constexpr int AT_FDCWD=-100;
constexpr int AT_SYMLINK_NOFOLLOW=0x100;
constexpr uint64_t TIOCGWINSZ=0x424F5301UL;
constexpr uint64_t TIOCSWINSZ=0x424F5302UL;
constexpr uint64_t TIOCGETA=0x424F5303UL;
constexpr uint64_t TIOCSETA=0x424F5304UL;
constexpr uint64_t TIOCGPGRP=0x424F5305UL;
constexpr uint64_t TIOCSPGRP=0x424F5306UL;
constexpr uint64_t TCFLSH=0x424F5307UL;
constexpr uint64_t FIONBIO=0x424F5308UL;
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
    int so_reuseaddr;
    int so_keepalive;
    int so_rcvbuf;
    int so_sndbuf;
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

static int alloc_fd(uint16_t start=3){
    auto* fs=fdtable(); if(!fs) return -1;
    for(int i=start;i<(int)process::MAX_RUNTIME_FDS;i++) if(!fs[i].used){
        fs[i]={true,process::RuntimeFd::None,0,0,nullptr,0,0,{}}; return i;
    }
    return -1;
}

static bool wait_selector_matches(const process::Process* parent, const process::Process* child) {
    if (!parent || !child) return false;
    const int64_t selector = (int64_t)parent->wait_target_pid;
    if (selector == -1) return true;
    if (selector > 0) return child->pid == (uint64_t)selector;
    if (selector == 0) return child->pgid == parent->pgid;
    const uint64_t group = (uint64_t)(-(selector + 1)) + 1;
    return child->pgid == group;
}

static void socket_free(int idx){ if(idx>=0&&idx<(int)(sizeof(unix_socks)/sizeof(unix_socks[0]))) unix_socks[idx]={}; }

/* Does any live process/thread still reference this open kernel object? */
static bool descriptor_object_referenced(const process::RuntimeFd& old) {
    for (size_t pi = 0; pi < process::slot_count(); ++pi) {
        process::Process* task = process::slot_at(pi);
        if (!task || task->state == process::State::EMPTY || task->state == process::State::TERMINATED)
            continue;
        process::RuntimeFd* table = process::fds(task);
        if (!table) continue;
        for (size_t i = 0; i < process::MAX_RUNTIME_FDS; ++i) {
            const process::RuntimeFd& candidate = table[i];
            if (!candidate.used) continue;
            if (old.kind == process::RuntimeFd::UnixSocket &&
                candidate.kind == old.kind && candidate.object == old.object) return true;
            if ((old.kind == process::RuntimeFd::Directory || old.kind == process::RuntimeFd::Device) &&
                candidate.kind == old.kind && candidate.data == old.data) return true;
            if (old.kind == process::RuntimeFd::File && (old.object & 0x80000000u) &&
                candidate.kind == old.kind && candidate.object == old.object) return true;
        }
    }
    return false;
}

/* Release one descriptor without accidentally killing shared dup()/fork handles. */
static void release_fd(int fd) {
    auto* table = fdtable();
    if (!table || fd < 0 || fd >= (int)process::MAX_RUNTIME_FDS || !table[fd].used) return;
    const process::RuntimeFd old = table[fd];
    table[fd] = {};

    if (old.kind == process::RuntimeFd::UnixSocket && old.object > 0) {
        const int si = (int)old.object - 1;
        if (!descriptor_object_referenced(old) && si >= 0 && si < 256 && unix_socks[si].used) {
            const int peer = unix_socks[si].peer;
            if (peer >= 0 && peer < 256 && unix_socks[peer].used) {
                unix_socks[peer].closed = true;
                unix_socks[peer].peer = -1;
            }
            socket_free(si);
        }
    } else if (old.kind == process::RuntimeFd::Directory) {
        if (!descriptor_object_referenced(old)) {
            for (size_t i = 0; i < 128; ++i)
                if (directory_path_used[i] && old.data == (const uint8_t*)directory_paths[i]) {
                    directory_path_used[i] = 0; directory_paths[i][0] = 0; break;
                }
        }
    } else if (old.kind == process::RuntimeFd::Device) {
        if (!descriptor_object_referenced(old)) {
            for (size_t i = 0; i < 64; ++i)
                if (device_info_used[i] && old.data == (const uint8_t*)&device_infos[i]) {
                    device_info_used[i] = 0; memset(&device_infos[i], 0, sizeof(device_infos[i])); break;
                }
        }
    } else if (old.kind == process::RuntimeFd::File && (old.object & 0x80000000u)) {
        if (!descriptor_object_referenced(old)) {
            const uint32_t ei = old.object & 0x7fffffffu;
            if (ei < 64) epolls[ei] = {};
        }
    }
}

static bool owner_has_other_live(process::Process* exiting) {
    if (!exiting) return false;
    process::Process* owner = exiting->fd_owner ? exiting->fd_owner : exiting;
    for (size_t i = 0; i < process::slot_count(); ++i) {
        process::Process* task = process::slot_at(i);
        if (!task || task == exiting || task->state == process::State::EMPTY || task->state == process::State::TERMINATED)
            continue;
        process::Process* task_owner = task->fd_owner ? task->fd_owner : task;
        if (task_owner == owner) return true;
    }
    return false;
}

/* Close a shared fd table only when the last live member of its thread group exits. */
static void close_owner_fds_if_last(process::Process* exiting) {
    if (!exiting || owner_has_other_live(exiting)) return;
    process::Process* owner = exiting->fd_owner ? exiting->fd_owner : exiting;
    for (size_t i = 0; i < process::MAX_RUNTIME_FDS; ++i)
        if (owner->fds[i].used) release_fd((int)i);
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
    return true;
}

static size_t socket_read_idx(int idx,uint8_t* dst,size_t n){
    if(idx<0||idx>=256||!unix_socks[idx].used)return 0;
    UnixSocket& s=unix_socks[idx];
    size_t k=n<s.rx_size?n:s.rx_size;
    for(size_t i=0;i<k;i++) dst[i]=s.rx[(s.rx_head+i)%sizeof(s.rx)];
    s.rx_head=(s.rx_head+(uint32_t)k)%sizeof(s.rx); s.rx_size-=(uint32_t)k;
    return k;
}

static bool resolve_path_for_process(process::Process* p, const char* in, char* out, size_t cap) {
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

static bool resolve_at_path(process::Process* p, const process::RuntimeFd* fs,
                            int dirfd, const char* in, char* out, size_t cap) {
    if (!in || !out || cap < 2) return false;
    if (in[0] == '/') return resolve_path_for_process(p, in, out, cap);
    if (dirfd == AT_FDCWD) return resolve_path_for_process(p, in, out, cap);
    if (!fs || dirfd < 0 || dirfd >= (int)process::MAX_RUNTIME_FDS ||
        !fs[dirfd].used || fs[dirfd].kind != process::RuntimeFd::Directory)
        return false;
    const char* base = fs[dirfd].path;
    if (!base[0]) base = (const char*)fs[dirfd].data;
    if (!base || !base[0]) return false;
    size_t a = strlen(base), b = strlen(in);
    bool slash = a == 0 || base[a-1] != '/';
    if (a + (slash ? 1u : 0u) + b + 1 > cap) return false;
    memcpy(out, base, a);
    if (slash) out[a++] = '/';
    memcpy(out + a, in, b + 1);
    return true;
}

static int copy_user_vec(uint64_t up, char out[][256], int max_count) {
    if (!up) return 0;
    int n = 0;
    for (;;) {
        if (n >= max_count || up > UINT64_MAX - (uint64_t)n * 8) return -1;
        if (!paging::is_user_range(up + (uint64_t)n * 8, 8, false)) return -1;
        uint64_t ptr = *reinterpret_cast<const uint64_t*>(static_cast<uintptr_t>(up + (uint64_t)n * 8));
        if (!ptr) break;
        if (!copy_user_string(ptr, out[n], 256)) return -1;
        ++n;
    }
    return n;
}

static bool cpu_has_rdrand() {
    uint32_t eax=1, ebx=0, ecx=0, edx=0;
    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) :: "memory");
    (void)ebx; (void)edx;
    return (ecx & (1u << 30)) != 0;
}

static bool hardware_random64(uint64_t* out) {
    if (!out || !cpu_has_rdrand()) return false;
    for (unsigned tries=0; tries<10; ++tries) {
        uint64_t value=0;
        unsigned char ok=0;
        asm volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(ok) :: "cc");
        if (ok) { *out=value; return true; }
    }
    return false;
}

static int64_t do_mmap(uint64_t requested_addr, uint64_t len, uint64_t prot,
                       uint64_t flags, uint64_t fd_raw, uint64_t offset) {
    process::Process* p = process::current();
    if (!p || len == 0) return EINVAL;
    if (prot & ~7ULL) return EINVAL;

    const bool anonymous = (flags & MAP_ANONYMOUS) != 0;
    const bool fixed = (flags & MAP_FIXED) != 0;
    const uint64_t map_type = flags & 3ULL;
    if ((flags & ~(MAP_ANONYMOUS | MAP_FIXED | 3ULL)) != 0 ||
        (map_type != 1 && map_type != 2)) return EINVAL;
    if (len > UINT64_MAX - (PAGE - 1)) return EINVAL;
    const uint64_t rounded = (len + PAGE - 1) & ~(PAGE - 1);
    if (rounded == 0 || offset > UINT64_MAX - rounded) return EINVAL;
    if ((int64_t)offset < 0 || (offset & (PAGE - 1))) return EINVAL;
    if (fixed && (requested_addr & (PAGE - 1))) return EINVAL;

    const int64_t fd = (int64_t)fd_raw;
    process::RuntimeFd* backing = nullptr;
    const vfs::DeviceNodeInfo* device = nullptr;
    if (!anonymous) {
        auto* table = fdtable();
        if (fd < 0 || fd >= (int64_t)process::MAX_RUNTIME_FDS || !table || !table[fd].used)
            return EBADF;
        backing = &table[fd];
        if (backing->kind == process::RuntimeFd::Device) {
            device = reinterpret_cast<const vfs::DeviceNodeInfo*>(backing->data);
            if (!device || !device->base || offset >= device->size || rounded > device->size - offset)
                return EINVAL;
            if (((uint64_t)(uintptr_t)device->base + offset) & (PAGE - 1)) return EINVAL;
        } else if (backing->kind == process::RuntimeFd::File) {
            if ((backing->flags & 3u) == O_WRONLY) return EACCES;
        } else {
            return EINVAL;
        }
    }

    uint64_t addr;
    if (fixed) {
        addr = requested_addr & ~(PAGE - 1);
    } else {
        /* Treat addr as a hint only. Use the monotonic arena so the current
         * simple allocator never silently replaces an existing mapping. */
        addr = (p->mmap_next + PAGE - 1) & ~(PAGE - 1);
    }
    if (addr < 0x10000 || addr + rounded < addr ||
        addr + rounded > 0x00007ffff0000000ULL) return EINVAL;

    if (fixed) {
        for (uint64_t va = addr; va < addr + rounded; va += PAGE)
            (void)paging::unmap_4k(p->pml4, va);
    }

    uint64_t pflags = 4; /* user page */
    if (prot & 2) pflags |= 2; /* writable */
    if (!(prot & 4)) pflags |= 1ULL << 63; /* NX unless PROT_EXEC */

    if (device) {
        uint64_t va = addr;
        uint64_t po = offset;
        for (; va < addr + rounded; va += PAGE, po += PAGE) {
            if (!paging::map_4k(p->pml4, va,
                               (uint64_t)(uintptr_t)device->base + po, pflags)) {
                for (uint64_t rollback = addr; rollback < va; rollback += PAGE)
                    (void)paging::unmap_4k(p->pml4, rollback);
                return ENOMEM;
            }
        }
    } else {
        if (!paging::map_user_range(p->pml4, addr, (size_t)rounded, pflags)) {
            for (uint64_t rollback = addr; rollback < addr + rounded; rollback += PAGE)
                (void)paging::unmap_4k(p->pml4, rollback);
            return ENOMEM;
        }
        memset((void*)(uintptr_t)addr, 0, (size_t)rounded);
        if (!anonymous && backing && backing->kind == process::RuntimeFd::File &&
            offset < backing->size && backing->data) {
            uint64_t available = backing->size - offset;
            size_t copy_size = (size_t)(available < rounded ? available : rounded);
            if (copy_size)
                memcpy((void*)(uintptr_t)addr, backing->data + offset, copy_size);
        }
    }

    if (addr + rounded > p->mmap_next) p->mmap_next = addr + rounded;
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
    if(fs[fd].kind==process::RuntimeFd::UnixSocket){int i=fd_socket_index(fd);return i>=0&&(unix_socks[i].rx_size>0||unix_socks[i].closed);}
    return false;
}
static bool fd_writable(int fd){
    auto* fs=fdtable(); if(!fs||fd<0||fd>=(int)process::MAX_RUNTIME_FDS||!fs[fd].used)return false;
    if(fs[fd].kind==process::RuntimeFd::Tty||fs[fd].kind==process::RuntimeFd::File||fs[fd].kind==process::RuntimeFd::Device)return true;
    if(fs[fd].kind==process::RuntimeFd::UnixSocket){int i=fd_socket_index(fd);return i>=0&&unix_socks[i].peer>=0;}
    return false;
}

/* Common checked I/O paths used by read/write and their vectored forms.
 * User pointers are validated before any kernel-side dereference. */
static int64_t read_fd_user(int fd, uint64_t ubuf, size_t len) {
    if (fd < 0 || fd >= (int)process::MAX_RUNTIME_FDS) return EBADF;
    auto* fs = fdtable();
    if (!fs || !fs[fd].used) return EBADF;
    if (len == 0) return 0;
    if (!paging::is_user_range(ubuf, len, true)) return EFAULT;
    auto& d = fs[fd];
    if (d.kind == process::RuntimeFd::Tty)
        return blockos_tty_read((void*)(uintptr_t)ubuf, len);
    if (d.kind == process::RuntimeFd::File) {
        if ((d.flags & 3u) == O_WRONLY) return EBADF;
        uint32_t fresh_size = 0;
        const uint8_t* fresh = vfs::read_file(d.path, &fresh_size);
        d.data = fresh; d.size = fresh ? fresh_size : 0;
        uint64_t avail = d.off < d.size ? d.size - d.off : 0;
        size_t n = (size_t)((uint64_t)len < avail ? (uint64_t)len : avail);
        if (n && d.data) memcpy((void*)(uintptr_t)ubuf, d.data + d.off, n);
        d.off += n;
        return (int64_t)n;
    }
    if (d.kind == process::RuntimeFd::UnixSocket) {
        int si = fd_socket_index(fd);
        if (si < 0) return ENOTCONN;
        if (unix_socks[si].rx_size == 0 && !unix_socks[si].closed && (d.flags & FD_NONBLOCK)) return EAGAIN;
        if (unix_socks[si].rx_size == 0 && !unix_socks[si].closed) return EAGAIN;
        return (int64_t)socket_read_idx(si, (uint8_t*)(uintptr_t)ubuf, len);
    }
    if (d.kind == process::RuntimeFd::Device) {
        const auto* di = reinterpret_cast<const vfs::DeviceNodeInfo*>(d.data);
        if (!di || di->type != vfs::DEVICE_INPUT) return ENOTTY;
        size_t cap = len / sizeof(blockos::input::Event);
        if (cap == 0) return 0;
        blockos::input::Event tmp[32];
        size_t want = cap < 32 ? cap : 32;
        size_t n = blockos::input::read(tmp, want);
        if (n == 0 && !(d.flags & FD_NONBLOCK)) return EAGAIN;
        if (n) memcpy((void*)(uintptr_t)ubuf, tmp, n * sizeof(tmp[0]));
        return (int64_t)(n * sizeof(tmp[0]));
    }
    return EBADF;
}

static int64_t write_fd_user(int fd, uint64_t ubuf, size_t len) {
    if (fd < 0 || fd >= (int)process::MAX_RUNTIME_FDS) return EBADF;
    auto* fs = fdtable();
    if (!fs || !fs[fd].used) return EBADF;
    if (len == 0) return 0;
    if (!paging::is_user_range(ubuf, len, false)) return EFAULT;
    auto& d = fs[fd];
    if (d.kind == process::RuntimeFd::Tty)
        return blockos_tty_write((const void*)(uintptr_t)ubuf, len);
    if (d.kind == process::RuntimeFd::File) {
        if ((d.flags & 3u) == 0) return EBADF;
        if (len > 0xffffffffu) return EINVAL;
        uint64_t new_off = d.off;
        if (!vfs::write_file_at(d.path, d.off, (const uint8_t*)(uintptr_t)ubuf,
                                (uint32_t)len, (d.flags & O_APPEND) != 0, &new_off))
            return ENOSPC;
        uint32_t fresh_size = 0;
        d.data = vfs::read_file(d.path, &fresh_size);
        d.size = fresh_size;
        d.off = new_off;
        return (int64_t)len;
    }
    if (d.kind == process::RuntimeFd::UnixSocket) {
        int si = fd_socket_index(fd);
        if (si < 0) return ENOTCONN;
        return socket_write_idx(si, (const uint8_t*)(uintptr_t)ubuf, len)
            ? (int64_t)len : EAGAIN;
    }
    return EBADF;
}

static bool prepare_execve(BlockOSSyscallFrame* f) {
    auto* p=process::current(); if(!p) return false;
    char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))) return false;
    if(path[0] != '/') { char abs[256]; if(!resolve_path_for_process(p,path,abs,sizeof(abs))) return false; memcpy(path,abs,sizeof(path)); }
    uint32_t sz=0; const uint8_t* main_buf=vfs::read_file(path,&sz); if(!main_buf||sz==0) return false;

    char argv[16][256], envp[16][256];
    int argc=copy_user_vec(f->rsi,argv,16); int envc=copy_user_vec(f->rdx,envp,16);
    if(argc<0||envc<0) return false;
    const char* argv_ptrs[16]; const char* envp_ptrs[16];
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

    for (size_t i=3; i<process::MAX_RUNTIME_FDS; ++i)
        if (p->fd_owner->fds[i].used && (p->fd_owner->fds[i].flags & FD_CLOEXEC)) release_fd((int)i);
    p->pml4=new_pml4; p->entry=start_entry; p->stack=start_rsp; p->has_interp=main_image.has_interp; p->real_entry=real_entry; p->fs_base=0; p->brk_current=p->brk_base; p->mmap_next=0x0000000100000000ULL;
    size_t pn=strlen(path); if(pn>=sizeof(p->name)) pn=sizeof(p->name)-1; memcpy(p->name,path,pn); p->name[pn]=0;
    if (p->vfork_child) { if(auto* parent=process::get(p->vfork_parent_pid)){ if(parent->state==process::State::BLOCKED) parent->state=process::State::READY; } p->vfork_child=false; p->vfork_parent_pid=0; }
    paging::switch_pml4(new_pml4);
    f->rax=0; f->rip=start_entry; f->rsp=start_rsp; f->cs=0x1b; f->ss=0x23; f->rflags=0x202;
    return true;
}
}

extern "C" void blockos_syscall_dispatch_frame(BlockOSSyscallFrame* f){
    if(!f)return;
    blockos::input::poll_hardware();
    int64_t r=ENOSYS; auto* p=process::current(); auto* fs=fdtable();
    switch(f->rax){
    case blockos::syscall::SYS_write:
        r = write_fd_user((int)f->rdi, f->rsi, (size_t)f->rdx);
        break;
    case blockos::syscall::SYS_read:
        r = read_fd_user((int)f->rdi, f->rsi, (size_t)f->rdx);
        break;
    case blockos::syscall::SYS_close:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        release_fd((int)f->rdi); r=0; break;
    }
    case blockos::syscall::SYS_lseek:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used||fs[f->rdi].kind!=process::RuntimeFd::File){r=ESPIPE;break;}
        auto& d=fs[f->rdi];
        if (f->rdx > 2 || d.off > (uint64_t)INT64_MAX || d.size > (uint64_t)INT64_MAX) { r=EINVAL; break; }
        int64_t base=(f->rdx==0?0:(f->rdx==1?(int64_t)d.off:(int64_t)d.size));
        int64_t delta=(int64_t)f->rsi;
        if ((delta > 0 && base > INT64_MAX-delta) || (delta < 0 && base < INT64_MIN-delta)) { r=EINVAL; break; }
        int64_t n=base+delta; if(n<0){r=EINVAL;break;} d.off=(uint64_t)n;r=n;break;
    }
    case blockos::syscall::SYS_fstat:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}if(!paging::is_user_range(f->rsi,128,true)){r=EFAULT;break;} memset((void*)(uintptr_t)f->rsi,0,128); uint64_t* q=(uint64_t*)(uintptr_t)f->rsi; q[0]=1;q[1]=1;q[2]=(fs[f->rdi].kind==process::RuntimeFd::Directory)?0040755:((fs[f->rdi].kind==process::RuntimeFd::File)?0100644:((fs[f->rdi].kind==process::RuntimeFd::Device)?0020666:0140777));q[7]=fs[f->rdi].size;r=0;break;
    }
    case blockos::syscall::SYS_openat:{
        char raw[256], path[256]; if(!copy_user_string(f->rsi,raw,sizeof(raw))){r=EFAULT;break;} if(!resolve_at_path(p,fs,(int)f->rdi,raw,path,sizeof(path))){r=EBADF;break;}
        if(vfs::is_directory(path)){ int fd=alloc_fd(); if(fd<0){r=EMFILE;break;} uint32_t ff=(uint32_t)f->rdx&~(uint32_t)O_CLOEXEC;if(f->rdx&O_CLOEXEC)ff|=FD_CLOEXEC;fs=fdtable(); const char* saved_path=store_directory_path(path); if(!saved_path){fs[fd]={};r=ENOMEM;break;} fs[fd]={true,process::RuntimeFd::Directory,ff,0,(const uint8_t*)saved_path,0,0,{}}; strncpy(fs[fd].path,path,sizeof(fs[fd].path)-1); fs[fd].path[sizeof(fs[fd].path)-1]=0; r=fd; break; }
        uint32_t sz=0; const uint8_t* data=vfs::read_file(path,&sz); bool exists=vfs::exists(path);
        if((f->rdx&O_DIRECTORY) && !vfs::is_directory(path)){r=ENOTDIR;break;}
        if(exists && (f->rdx&O_CREAT) && (f->rdx&O_EXCL)){r=EEXIST;break;}
        if(!exists && (f->rdx&O_CREAT)){ if(!vfs::create_file(path,nullptr,0)){r=ENOENT;break;} exists=true; data=vfs::read_file(path,&sz); }
        if(exists && !vfs::is_directory(path) && !vfs::is_device(path)){ int fd=alloc_fd(); if(fd<0){r=EMFILE;break;} if(f->rdx&O_TRUNC){ if(!vfs::truncate_file(path,0)){fs[fd]={};r=EIO;break;} data=vfs::read_file(path,&sz);} uint32_t ff=(uint32_t)f->rdx & ~(uint32_t)O_CLOEXEC; if(f->rdx&O_CLOEXEC) ff|=FD_CLOEXEC; fs=fdtable(); fs[fd]={true,process::RuntimeFd::File,ff,0,data,sz,(f->rdx&O_APPEND)?sz:0,{}}; strncpy(fs[fd].path,path,sizeof(fs[fd].path)-1); fs[fd].path[sizeof(fs[fd].path)-1]=0; r=fd; break; }
        if(vfs::is_device(path)){int fd=alloc_fd(); if(fd<0){r=EMFILE;break;} vfs::DeviceNodeInfo di{}; if(!vfs::get_device_info(path,&di)){fs[fd]={};r=ENOENT;break;} const auto* saved=store_device_info(di); if(!saved){fs[fd]={};r=ENOMEM;break;} uint32_t df=(uint32_t)f->rdx&~(uint32_t)O_CLOEXEC;if(f->rdx&O_CLOEXEC)df|=FD_CLOEXEC;fs[fd]={true,process::RuntimeFd::Device,df,0,(const uint8_t*)saved,di.size,0,{}}; strncpy(fs[fd].path,path,sizeof(fs[fd].path)-1);fs[fd].path[sizeof(fs[fd].path)-1]=0;r=fd; break;}
        r=ENOENT; break;
    }
    case blockos::syscall::SYS_execve:
        if (prepare_execve(f)) return;
        r=ENOENT; break;
    case blockos::syscall::SYS_getcwd:{
        char* out=(char*)(uintptr_t)f->rdi; size_t cap=(size_t)f->rsi; const char* c=process::cwd(p); size_t n=strlen(c); if(!cap||n+1>cap||!paging::is_user_range(f->rdi,n+1,true)){r=EINVAL;break;} memcpy(out,c,n+1); r=(int64_t)(n+1); break; }
    case blockos::syscall::SYS_chdir:{
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;} char abs[256]; if(!resolve_path_for_process(p,path,abs,sizeof(abs))||!vfs::is_directory(abs)){r=ENOTDIR;break;} process::set_cwd(p,abs); r=0; break; }
    case blockos::syscall::SYS_mkdir:{
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;} char abs[256]; if(!resolve_path_for_process(p,path,abs,sizeof(abs))){r=ENAMETOOLONG;break;} if(vfs::exists(abs)){r=EEXIST;break;} r=vfs::create_directory(abs)?0:ENOENT; break; }
    case blockos::syscall::SYS_unlink:{
        char path[256]; if(!copy_user_string(f->rdi,path,sizeof(path))){r=EFAULT;break;} char abs[256]; if(!resolve_path_for_process(p,path,abs,sizeof(abs))){r=ENAMETOOLONG;break;} if(vfs::is_directory(abs)){r=EISDIR;break;} r=vfs::remove_file(abs)?0:ENOENT; break; }
    case blockos::syscall::SYS_rename:{
        char a[256],b[256],aa[256],bb[256]; if(!copy_user_string(f->rdi,a,sizeof(a))||!copy_user_string(f->rsi,b,sizeof(b))){r=EFAULT;break;} if(!resolve_path_for_process(p,a,aa,sizeof(aa))||!resolve_path_for_process(p,b,bb,sizeof(bb))){r=ENAMETOOLONG;break;} if(!vfs::exists(aa)){r=ENOENT;break;} if(vfs::exists(bb)){r=EEXIST;break;} r=vfs::rename_path(aa,bb)?0:ENOENT; break; }
    case blockos::syscall::SYS_getdents64:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used||fs[f->rdi].kind!=process::RuntimeFd::Directory){r=ENOTDIR;break;}
        if(!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;}
        auto& d=fs[f->rdi]; const char* dir=(const char*)(uintptr_t)d.data; size_t count=vfs::directory_entry_count(dir); size_t idx=(size_t)d.off; size_t used=0;
        while(idx<count){ const char* nm=vfs::directory_entry_name(dir,idx); if(!nm) break; size_t nl=strlen(nm); size_t reclen=(24+nl+1+7)&~(size_t)7; if(used+reclen>(size_t)f->rdx) break; uint8_t* out=(uint8_t*)(uintptr_t)(f->rsi+used); memset(out,0,reclen); uint64_t ino=idx+2; memcpy(out,&ino,8); int64_t off=(int64_t)(idx+1); memcpy(out+8,&off,8); uint16_t rr=(uint16_t)reclen; memcpy(out+16,&rr,2); char child[256]; size_t dl=strlen(dir); if(dl==1 && dir[0]=='/') snprintf(child,sizeof(child),"/%s",nm); else snprintf(child,sizeof(child),"%s/%s",dir,nm); out[18]=vfs::is_directory(child)?4:8; memcpy(out+19,nm,nl+1); used+=reclen; ++idx; } d.off=idx; r=(int64_t)used; break; }
    case blockos::syscall::SYS_mmap:r=do_mmap(f->rdi,f->rsi,f->rdx,f->r10,f->r8,f->r9);break;
    case blockos::syscall::SYS_munmap:{
        if(!p||!f->rsi||f->rsi>UINT64_MAX-(PAGE-1)){r=EINVAL;break;}
        uint64_t len=(f->rsi+PAGE-1)&~(PAGE-1),addr=f->rdi&~(PAGE-1);
        if(addr<0x10000||addr+len<addr||addr+len>0x00007ffff0000000ULL){r=EINVAL;break;}
        for(uint64_t va=addr;va<addr+len;va+=PAGE) paging::unmap_4k(p->pml4,va);
        r=0; break;
    }
    case blockos::syscall::SYS_mprotect:{if(!p||!f->rsi||f->rsi>UINT64_MAX-(PAGE-1)){r=EINVAL;break;}uint64_t len=(f->rsi+PAGE-1)&~(PAGE-1),addr=f->rdi&~(PAGE-1);if(addr<0x10000||addr+len<addr||addr+len>0x00007ffff0000000ULL){r=EINVAL;break;}uint64_t pf=4;if(f->rdx&2)pf|=2;if(!(f->rdx&4))pf|=1ULL<<63;r=paging::protect_user_range(p->pml4,addr,(size_t)len,pf)?0:ENOMEM;break;}
    case blockos::syscall::SYS_brk:{if(!p){r=ENOMEM;break;}uint64_t want=f->rdi;if(!want){r=p->brk_current;break;}if(want<p->brk_base||want>0x0000000400000000ULL){r=EINVAL;break;}uint64_t old=(p->brk_current+PAGE-1)&~(PAGE-1),nw=(want+PAGE-1)&~(PAGE-1);if(nw>old&&!paging::map_user_range(p->pml4,old,(size_t)(nw-old),6|(1ULL<<63))){r=ENOMEM;break;}p->brk_current=want;r=want;break;}
    case blockos::syscall::SYS_getuid:r=p?(int64_t)p->uid:ESRCH;break;
    case blockos::syscall::SYS_getgid:r=p?(int64_t)p->gid:ESRCH;break;
    case blockos::syscall::SYS_geteuid:r=p?(int64_t)p->euid:ESRCH;break;
    case blockos::syscall::SYS_getegid:r=p?(int64_t)p->egid:ESRCH;break;
    case blockos::syscall::SYS_setuid:{
        if(!p){r=ESRCH;break;}uint32_t id=(uint32_t)f->rdi;
        if(p->euid!=0 && id!=p->uid && id!=p->euid){r=EPERM;break;}
        p->uid=id;p->euid=id;r=0;break;
    }
    case blockos::syscall::SYS_setgid:{
        if(!p){r=ESRCH;break;}uint32_t id=(uint32_t)f->rdi;
        if(p->euid!=0 && id!=p->gid && id!=p->egid){r=EPERM;break;}
        p->gid=id;p->egid=id;r=0;break;
    }
    case blockos::syscall::SYS_seteuid:{
        if(!p){r=ESRCH;break;}uint32_t id=(uint32_t)f->rdi;
        if(p->euid!=0 && id!=p->uid){r=EPERM;break;}p->euid=id;r=0;break;
    }
    case blockos::syscall::SYS_setegid:{
        if(!p){r=ESRCH;break;}uint32_t id=(uint32_t)f->rdi;
        if(p->euid!=0 && id!=p->gid){r=EPERM;break;}p->egid=id;r=0;break;
    }
    case blockos::syscall::SYS_setgroups:{
        if(!p){r=ESRCH;break;}if(p->euid!=0){r=EPERM;break;}
        size_t n=(size_t)f->rdi;if(n>16){r=EINVAL;break;}
        if(n && (f->rsi==0 || !paging::is_user_range(f->rsi,n*sizeof(uint32_t),false))){r=EFAULT;break;}
        for(size_t i=0;i<n;i++)p->supplementary_groups[i]=((const uint32_t*)(uintptr_t)f->rsi)[i];
        for(size_t i=n;i<16;i++)p->supplementary_groups[i]=0;
        p->supplementary_group_count=(uint32_t)n;r=0;break;
    }
    case blockos::syscall::SYS_getgroups:{
        if(!p){r=ESRCH;break;}size_t cap=(size_t)f->rdi;size_t n=p->supplementary_group_count;
        if(cap==0){r=(int64_t)n;break;}if(cap<n){r=EINVAL;break;}
        if(n && !paging::is_user_range(f->rsi,n*sizeof(uint32_t),true)){r=EFAULT;break;}
        if(n) memcpy((void*)(uintptr_t)f->rsi,p->supplementary_groups,n*sizeof(uint32_t));
        r=(int64_t)n; break;
    }
    case blockos::syscall::SYS_capget:
    case blockos::syscall::SYS_capset:r=ENOSYS;break;
    case blockos::syscall::SYS_getpid:
    case blockos::syscall::SYS_getpid_dup:r=p?(int64_t)p->pid:ESRCH;break;
    case blockos::syscall::SYS_getppid:r=p?(int64_t)p->parent_pid:0;break;
    case blockos::syscall::SYS_gettid:r=p?(int64_t)p->tid:-1;break;
    case blockos::syscall::SYS_set_tid_address: if(!p||(f->rdi&&!paging::is_user_range(f->rdi,4,true))){r=EFAULT;break;}p->clear_tid=f->rdi;r=(int64_t)p->tid;break;
    case blockos::syscall::SYS_arch_prctl:{if(!p) {r=EINVAL;break;} if(f->rdi==ARCH_SET_FS){if(f->rsi>0x00007ffffffff000ULL){r=EINVAL;break;}p->fs_base=f->rsi; r=0;}else if(f->rdi==ARCH_GET_FS){if(!paging::is_user_range(f->rsi,8,true)){r=EFAULT;break;}*(uint64_t*)(uintptr_t)f->rsi=p->fs_base;r=0;}else r=EINVAL;
        break;}
    case blockos::syscall::SYS_nanosleep:{
        if(!paging::is_user_range(f->rdi,16,false)){r=EFAULT;break;}
        const int64_t sec=*(const int64_t*)(uintptr_t)f->rdi; const int64_t nsec=*(const int64_t*)(uintptr_t)(f->rdi+8);
        if(sec<0||nsec<0||nsec>=1000000000){r=EINVAL;break;}
        uint64_t now=timer_uptime_ms();
        uint64_t ms_from_sec=(uint64_t)sec*1000ull;
        uint64_t ms_from_nsec=((uint64_t)nsec+999999ull)/1000000ull;
        if (ms_from_nsec > UINT64_MAX-now || (uint64_t)sec > (UINT64_MAX-now-ms_from_nsec)/1000ull) { r=EINVAL; break; }
        uint64_t deadline=now+ms_from_sec+ms_from_nsec;
        r=0; f->rax=0;
        if(preempt::block_until_from_syscall(f,deadline)) return;
        break;
    }
    case blockos::syscall::SYS_clone:{
        if(!p){r=EINVAL;break;}uint64_t child_sp=f->rsi; if(child_sp && (child_sp&0xF))child_sp-=8; if(!child_sp||!paging::is_user_range(child_sp-8,8,true)){r=EFAULT;break;}
        process::Process* child=nullptr;for(size_t i=0;i<process::slot_count();i++){auto* x=process::slot_at(i);if(x&&x->state==process::State::EMPTY){child=x;break;}}
        if(!child){r=ENOMEM;break;}memset(child,0,sizeof(*child)); child->pid=child->tid=process::count()+1000; while(process::get(child->pid))child->pid++; child->pml4=p->pml4; child->fd_owner=p->fd_owner?p->fd_owner:p; child->parent_pid=p->tid; child->is_thread=true; child->fs_base=f->r8?f->r8:p->fs_base; child->uid=p->uid; child->gid=p->gid; child->euid=p->euid; child->egid=p->egid; child->supplementary_group_count=p->supplementary_group_count; memcpy(child->supplementary_groups,p->supplementary_groups,sizeof(child->supplementary_groups)); child->signal_mask=p->signal_mask; memcpy(child->signal_handlers,p->signal_handlers,sizeof(child->signal_handlers)); memcpy(child->signal_flags,p->signal_flags,sizeof(child->signal_flags)); child->brk_base=p->brk_base; child->brk_current=p->brk_current; child->mmap_next=p->mmap_next; child->entry=p->entry; child->stack=child_sp; child->saved_frame=*reinterpret_cast<TrapFrame*>(f); child->saved_frame.rax=0; child->saved_frame.rdi=f->rdx; child->saved_frame.rsp=child_sp; child->state=process::State::READY; child->frame_valid=true; r=(int64_t)child->tid; break;
    }
    case blockos::syscall::SYS_sched_yield:{r=0;f->rax=0;if(preempt::yield_from_syscall(f))return;break;}
    case blockos::syscall::SYS_futex:{
        uint64_t uaddr=f->rdi;uint32_t op=(uint32_t)f->rsi&0x7f;uint32_t val=(uint32_t)f->rdx;
        if(!paging::is_user_range(uaddr,4,(op==FUTEX_WAIT))){r=EFAULT;break;}
        if(op==FUTEX_WAKE){wake_futex(uaddr,val?val:0xffffffffu);r=0;break;}
        if(op==FUTEX_WAIT){if(*(volatile uint32_t*)(uintptr_t)uaddr!=val){r=EAGAIN;break;}int slot=-1;for(int i=0;i<(int)(sizeof(futex_waiters)/sizeof(futex_waiters[0]));i++)if(!futex_waiters[i].used){slot=i;break;}if(slot<0){r=ENOMEM;break;}if(f->r10){
            if(!paging::is_user_range(f->r10,sizeof(int64_t)*2,false)){r=EFAULT;break;}
            const int64_t* ts=(const int64_t*)(uintptr_t)f->r10;
            if(ts[0]<0||ts[1]<0||ts[1]>=1000000000LL){r=EINVAL;break;}
            /* Time-limited futex waits need a timeout result wired into the waiter state;
             * do not pretend to sleep successfully when that ABI path is unavailable. */
            r=ENOSYS;break;
        }
        futex_waiters[slot]={true,uaddr,p->tid};r=0;f->rax=0;
        if(!preempt::block_from_syscall(f)){futex_waiters[slot].used=false;r=EAGAIN;break;}return;}
        r=ENOSYS;break;
    }
    case blockos::syscall::SYS_exit:
    case blockos::syscall::SYS_exit_group:{
        const bool group_exit=(f->rax==blockos::syscall::SYS_exit_group);
        if(p){
            process::Process* owner=p->fd_owner?p->fd_owner:p;
            if(group_exit){
                for(size_t i=0;i<process::slot_count();++i){
                    process::Process* task=process::slot_at(i);
                    if(!task||task==p||task->state==process::State::EMPTY||task->state==process::State::TERMINATED)continue;
                    process::Process* task_owner=task->fd_owner?task->fd_owner:task;
                    if(task_owner!=owner)continue;
                    task->exit_code=f->rdi;
                    if(task->clear_tid&&paging::is_user_range(task->clear_tid,4,true)){
                        *(uint32_t*)(uintptr_t)task->clear_tid=0; wake_futex(task->clear_tid,1);
                    }
                    task->state=process::State::TERMINATED;
                }
            }
            const bool other_group_live=owner_has_other_live(p);
            process::Process* exiting=(group_exit||p->is_thread)?owner:p;
            exiting->exit_code=f->rdi;
            if(exiting->vfork_child){
                if(auto* parent=process::get(exiting->vfork_parent_pid))
                    if(parent->state==process::State::BLOCKED)parent->state=process::State::READY;
                exiting->vfork_child=false; exiting->vfork_parent_pid=0;
            }
            /* Threads are not wait4-visible process children; exit_group reports the leader. */
            if(group_exit || !other_group_live){
                if(auto* parent=process::get(exiting->parent_pid)){
                    if(parent->state==process::State::BLOCKED&&parent->waiting_child&&wait_selector_matches(parent,exiting)){
                        parent->saved_frame.rax=exiting->pid;
                        if(parent->wait_status_ptr&&paging::is_user_range(parent->wait_status_ptr,4,true))
                            *reinterpret_cast<int*>(static_cast<uintptr_t>(parent->wait_status_ptr))=(int)(exiting->exit_code<<8);
                        parent->wait_target_pid=0; parent->wait_status_ptr=0; parent->waiting_child=false;
                        parent->state=process::State::READY;
                    }
                }
            }
            if(p->clear_tid&&paging::is_user_range(p->clear_tid,4,true)){
                *(uint32_t*)(uintptr_t)p->clear_tid=0; wake_futex(p->clear_tid,1);
            }
            close_owner_fds_if_last(p);
            p->state=process::State::TERMINATED;
        }
        if(preempt::on_exit(f))return;
        f->rax=0; f->rip=(uint64_t)(uintptr_t)&blockos_user_return; f->cs=0x08; f->ss=0x10; f->rsp=blockos_user_saved_rsp; f->rflags|=0x200; return;
    }
    case blockos::syscall::SYS_getrandom:{
        if (f->rdx & ~3ULL) { r=EINVAL; break; }
        if (f->rsi == 0) { r=0; break; }
        if (f->rsi > 33554431ULL) { r=EINVAL; break; }
        if (!paging::is_user_range(f->rdi,(size_t)f->rsi,true)) { r=EFAULT; break; }
        if (!cpu_has_rdrand()) { r=ENOSYS; break; }
        uint8_t* dst=(uint8_t*)(uintptr_t)f->rdi;
        size_t done=0;
        while (done < (size_t)f->rsi) {
            uint64_t value=0;
            if (!hardware_random64(&value)) { r=done?(int64_t)done:EAGAIN; break; }
            size_t chunk=(size_t)f->rsi-done; if (chunk>sizeof(value)) chunk=sizeof(value);
            memcpy(dst+done,&value,chunk); done+=chunk;
        }
        if (done == (size_t)f->rsi) r=(int64_t)done;
        break;
    }
    case blockos::syscall::SYS_pipe2:{
        if(f->rsi&~(uint64_t)(O_CLOEXEC|O_NONBLOCK)){r=EINVAL;break;}
        if(!paging::is_user_range(f->rdi,8,true)){r=EFAULT;break;} int sfds[2]; int sa=socket_obj_alloc(), sb=socket_obj_alloc(); if(sa<0||sb<0){if(sa>=0)socket_free(sa);if(sb>=0)socket_free(sb);r=ENOMEM;break;} unix_socks[sa].peer=sb;unix_socks[sb].peer=sa; int a=alloc_fd(),b=alloc_fd(); if(a<0||b<0){if(a>=0)fs[a]={};if(b>=0)fs[b]={};socket_free(sa);socket_free(sb);r=EMFILE;break;} uint32_t pf=(f->rsi&(O_CLOEXEC))?FD_CLOEXEC:0; if(f->rsi&O_NONBLOCK)pf|=FD_NONBLOCK; fs=fdtable();fs[a]={true,process::RuntimeFd::UnixSocket,pf,(uint32_t)(sa+1),nullptr,0,0,{}};fs[b]={true,process::RuntimeFd::UnixSocket,pf,(uint32_t)(sb+1),nullptr,0,0,{}};sfds[0]=a;sfds[1]=b;memcpy((void*)(uintptr_t)f->rdi,sfds,8);r=0;break; }
    case blockos::syscall::SYS_readv:{
        struct IOV { void* base; size_t len; };
        if(f->rdx>1024 || f->rdx>SIZE_MAX/sizeof(IOV) ||
           !paging::is_user_range(f->rsi,(size_t)f->rdx*sizeof(IOV),false)){r=EFAULT;break;}
        if(f->rdx==0){r=0;break;}
        const IOV* v=(const IOV*)(uintptr_t)f->rsi;int64_t total=0;
        for(size_t i=0;i<(size_t)f->rdx;i++){
            if(v[i].len && (!v[i].base || !paging::is_user_range((uint64_t)(uintptr_t)v[i].base,v[i].len,true))){r=total?total:EFAULT;goto readv_done;}
            int64_t n=read_fd_user((int)f->rdi,(uint64_t)(uintptr_t)v[i].base,v[i].len);
            if(n<0){r=total?total:n;goto readv_done;}
            total+=n;if((size_t)n<v[i].len)break;
        }
        r=total;
    readv_done: break;
    }
    case blockos::syscall::SYS_writev:{
        struct IOV { const void* base; size_t len; };
        if(f->rdx>1024 || f->rdx>SIZE_MAX/sizeof(IOV) ||
           !paging::is_user_range(f->rsi,(size_t)f->rdx*sizeof(IOV),false)){r=EFAULT;break;}
        if(f->rdx==0){r=0;break;}
        const IOV* v=(const IOV*)(uintptr_t)f->rsi;int64_t total=0;
        for(size_t i=0;i<(size_t)f->rdx;i++){
            if(v[i].len && (!v[i].base || !paging::is_user_range((uint64_t)(uintptr_t)v[i].base,v[i].len,false))){r=total?total:EFAULT;goto writev_done;}
            int64_t n=write_fd_user((int)f->rdi,(uint64_t)(uintptr_t)v[i].base,v[i].len);
            if(n<0){r=total?total:n;goto writev_done;}
            total+=n;if((size_t)n<v[i].len)break;
        }
        r=total;
    writev_done: break;
    }
    case blockos::syscall::SYS_ioctl:
    case blockos::syscall::SYS_ioctl_dup:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        const uint64_t req=f->rsi;const uint64_t arg=f->rdx;
        if(req==TIOCGWINSZ){
            if(!paging::is_user_range(arg,8,true)){r=EFAULT;break;}
            uint16_t* w=(uint16_t*)(uintptr_t)arg;w[0]=24;w[1]=80;w[2]=0;w[3]=0;r=0;
        }else if(req==TIOCSWINSZ){r=paging::is_user_range(arg,8,false)?0:EFAULT;
        }else if(req==TIOCGETA){r=ENOTTY;
        }else if(req==TIOCSETA||req==TIOCSPGRP||req==TCFLSH){r=0;
        }else if(req==TIOCGPGRP){if(!paging::is_user_range(arg,sizeof(int),true)){r=EFAULT;break;}*(int*)(uintptr_t)arg=p?(int)p->pgid:0;r=0;
        }else if(req==FIONBIO){if(!paging::is_user_range(arg,sizeof(int),false)){r=EFAULT;break;}int on=*(const int*)(uintptr_t)arg;if(on)fs[f->rdi].flags|=FD_NONBLOCK;else fs[f->rdi].flags&=~FD_NONBLOCK;r=0;
        }else r=ENOTTY;
        break;
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
    case blockos::syscall::SYS_rt_sigreturn:r=ENOSYS;break;
    case blockos::syscall::SYS_kill:{
        int sig=(int)f->rsi;if(sig<0||sig>64){r=EINVAL;break;}
        process::Process* target=nullptr;int64_t id=(int64_t)f->rdi;
        if(id>0)target=process::get((uint64_t)id);
        if(!target){r=ESRCH;break;}
        r=(sig==0)?0:ENOSYS;break;
    }
    case blockos::syscall::SYS_tgkill:{
        int sig=(int)f->rdx;if(sig<0||sig>64){r=EINVAL;break;}
        process::Process* target=process::get((uint64_t)f->rsi);
        uint64_t tgid=target?(target->is_thread?target->parent_pid:target->pid):0;if(!target||target->tid!=(uint64_t)f->rsi||((int64_t)f->rdi>0&&tgid!=(uint64_t)f->rdi)){r=ESRCH;break;}
        r=(sig==0)?0:ENOSYS;break;
    }
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
        r=ENOSYS;break;
    case blockos::syscall::SYS_prctl:{
        if(!p){r=ESRCH;break;}
        constexpr uint64_t PR_SET_NAME=15,PR_GET_NAME=16;
        if(f->rdi==PR_SET_NAME){
            if(!paging::is_user_range(f->rsi,16,false)){r=EFAULT;break;}
            char nm[16];memcpy(nm,(const void*)(uintptr_t)f->rsi,16);nm[15]=0;
            size_t n=strlen(nm);if(n>=sizeof(p->name))n=sizeof(p->name)-1;memcpy(p->name,nm,n);p->name[n]=0;r=0;
        }else if(f->rdi==PR_GET_NAME){
            if(!paging::is_user_range(f->rsi,16,true)){r=EFAULT;break;}
            memset((void*)(uintptr_t)f->rsi,0,16);size_t n=strlen(p->name);if(n>15)n=15;memcpy((void*)(uintptr_t)f->rsi,p->name,n);r=0;
        }else r=EINVAL;
        break;
    }
    case blockos::syscall::SYS_statx:{
        char raw[256],abs[256];
        if(!copy_user_string(f->rsi,raw,sizeof(raw))){r=EFAULT;break;}
        if(!resolve_at_path(p,fs,(int)f->rdi,raw,abs,sizeof(abs))){r=EBADF;break;}
        if((uint32_t)f->rdx & ~((uint32_t)AT_SYMLINK_NOFOLLOW)){r=EINVAL;break;}
        uint32_t sz=0;const uint8_t*d=vfs::read_file(abs,&sz);bool dir=vfs::is_directory(abs);bool dev=vfs::is_device(abs);
        if(!d&&!dir&&!dev){r=ENOENT;break;}
        /* Linux statx layout: stx_mode at byte 28, stx_ino at 32, stx_size at 40. */
        if(!paging::is_user_range(f->r8,256,true)){r=EFAULT;break;}
        uint8_t* out=(uint8_t*)(uintptr_t)f->r8;memset(out,0,256);
        uint32_t mask=0x7ffu;memcpy(out,&mask,sizeof(mask));
        uint32_t blksize=4096;memcpy(out+4,&blksize,sizeof(blksize));
        uint32_t nlink=1;memcpy(out+16,&nlink,sizeof(nlink));
        uint16_t mode=(uint16_t)(dev?0020666:(dir?0040755:0100644));memcpy(out+28,&mode,sizeof(mode));
        uint64_t ino=1,size=d?(uint64_t)sz:0,blocks=(size+511)/512;memcpy(out+32,&ino,8);memcpy(out+40,&size,8);memcpy(out+48,&blocks,8);
        r=0;break;
    }
    case blockos::syscall::SYS_socket:{
        int domain=(int)f->rdi,type=(int)f->rsi,proto=(int)f->rdx;uint32_t st=(uint32_t)type&0xf;
        if(domain!=AF_UNIX||st!=SOCK_STREAM){r=ENOSYS;break;}if(proto!=0){r=EPROTONOSUPPORT;break;}
        if(((uint32_t)type&~0x8080fu)!=0){r=EINVAL;break;}
        int si=socket_obj_alloc();if(si<0){r=ENOMEM;break;}int fd=alloc_fd();if(fd<0){socket_free(si);r=EMFILE;break;}
        uint32_t sf=0;if((type&0x80000)!=0)sf|=FD_CLOEXEC;if((type&0x800)!=0)sf|=FD_NONBLOCK;
        fs=fdtable();fs[fd]={true,process::RuntimeFd::UnixSocket,sf,(uint32_t)(si+1),nullptr,0,0,{}};r=fd;break;
    }
    case blockos::syscall::SYS_bind:{
        int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}
        if(f->rdx<3||f->rdx>110||!paging::is_user_range(f->rsi,(size_t)f->rdx,false)){r=EINVAL;break;}
        const uint8_t* sa=(const uint8_t*)(uintptr_t)f->rsi;uint16_t fam=0;memcpy(&fam,sa,2);if(fam!=AF_UNIX){r=EAFNOSUPPORT;break;}
        char sp[108];size_t cap=(size_t)f->rdx-2;if(cap>sizeof(sp)-1)cap=sizeof(sp)-1;size_t n=0;const char* up=(const char*)(sa+2);while(n<cap&&up[n]){sp[n]=up[n];n++;}sp[n]=0;
        if(n==0){r=EINVAL;break;}bool collision=false;for(auto& sock:unix_socks)if(sock.used&&sock.listener&&strcmp(sock.path,sp)==0){collision=true;break;}
        if(collision){r=EADDRINUSE;break;}strncpy(unix_socks[si].path,sp,sizeof(unix_socks[si].path)-1);unix_socks[si].path[sizeof(unix_socks[si].path)-1]=0;r=0;break;
    }
    case blockos::syscall::SYS_listen:{int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}unix_socks[si].listener=true;r=0;break;}
    case blockos::syscall::SYS_connect:{
        int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}
        if(unix_socks[si].connected){r=EISCONN;break;}
        if(f->rdx<3||f->rdx>110||!paging::is_user_range(f->rsi,(size_t)f->rdx,false)){r=EINVAL;break;}
        const uint8_t* sa=(const uint8_t*)(uintptr_t)f->rsi;uint16_t fam=0;memcpy(&fam,sa,2);if(fam!=AF_UNIX){r=EAFNOSUPPORT;break;}
        char sp[108];size_t cap=(size_t)f->rdx-2;if(cap>sizeof(sp)-1)cap=sizeof(sp)-1;size_t n=0;const char* up=(const char*)(sa+2);while(n<cap&&up[n]){sp[n]=up[n];n++;}sp[n]=0;
        if(n==0){r=EINVAL;break;}int li=-1;for(int i=0;i<256;i++)if(unix_socks[i].used&&unix_socks[i].listener&&strcmp(unix_socks[i].path,sp)==0){li=i;break;}
        if(li<0){r=ENOENT;break;}if(unix_socks[li].pending_count>=16){r=EAGAIN;break;}
        int peer=socket_obj_alloc();if(peer<0){r=ENOMEM;break;}
        unix_socks[si].peer=peer;unix_socks[si].connected=true;unix_socks[peer].peer=si;unix_socks[peer].connected=true;
        unix_socks[li].pending[unix_socks[li].pending_count++]=peer;r=0;break;
    }
    case blockos::syscall::SYS_accept:
    case blockos::syscall::SYS_accept4:{
        int li=fd_socket_index((int)f->rdi);
        if(li<0||!unix_socks[li].listener){r=ENOTSOCK;break;}
        uint32_t afd=0;
        if(f->rax==blockos::syscall::SYS_accept4){
            if(f->r10&~0x80800u){r=EINVAL;break;}
            if(f->r10&0x80000u)afd|=FD_CLOEXEC;
            if(f->r10&0x800u)afd|=FD_NONBLOCK;
        }
        if((f->rsi==0)!=(f->rdx==0)){r=EFAULT;break;}
        uint32_t addr_capacity=0;
        if(f->rsi){
            if(!paging::is_user_range(f->rdx,sizeof(uint32_t),true)){r=EFAULT;break;}
            addr_capacity=*(uint32_t*)(uintptr_t)f->rdx;
            if(addr_capacity && !paging::is_user_range(f->rsi,addr_capacity<3?addr_capacity:3,true)){r=EFAULT;break;}
        }
        if(unix_socks[li].pending_count==0){r=EAGAIN;break;}
        int fd=alloc_fd();if(fd<0){r=EMFILE;break;}
        int si=unix_socks[li].pending[0];
        for(int i=1;i<unix_socks[li].pending_count;i++)unix_socks[li].pending[i-1]=unix_socks[li].pending[i];
        --unix_socks[li].pending_count;
        if(f->rsi){uint8_t sa[3]={1,0,0};size_t n=addr_capacity<sizeof(sa)?addr_capacity:sizeof(sa);if(n)memcpy((void*)(uintptr_t)f->rsi,sa,n);*(uint32_t*)(uintptr_t)f->rdx=sizeof(sa);}
        fs=fdtable();fs[fd]={true,process::RuntimeFd::UnixSocket,afd,(uint32_t)(si+1),nullptr,0,0,{}};r=fd;break;
    }
    case blockos::syscall::SYS_sendto:{int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}if(!paging::is_user_range(f->rsi,(size_t)f->rdx,false)){r=EFAULT;break;}if(!socket_write_idx(si,(const uint8_t*)(uintptr_t)f->rsi,(size_t)f->rdx)){r=EAGAIN;break;}r=f->rdx;break;}
    case blockos::syscall::SYS_recvfrom:{int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}if(!paging::is_user_range(f->rsi,(size_t)f->rdx,true)){r=EFAULT;break;}r=(int64_t)socket_read_idx(si,(uint8_t*)(uintptr_t)f->rsi,(size_t)f->rdx);break;}
    case blockos::syscall::SYS_shutdown:{
        int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}
        int how=(int)f->rsi;if(how<0||how>2){r=EINVAL;break;}
        UnixSocket& sock=unix_socks[si];if(how==0||how==2)sock.closed=true;
        if((how==1||how==2)&&sock.peer>=0&&sock.peer<256&&unix_socks[sock.peer].used)unix_socks[sock.peer].closed=true;
        r=0;break;
    }
    case blockos::syscall::SYS_setsockopt:{
        int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}
        if(f->rsi!=1){r=ENOPROTOOPT;break;}
        if(f->r8<sizeof(int)||!paging::is_user_range(f->r10,(size_t)f->r8,false)){r=EFAULT;break;}
        int val=*(const int*)(uintptr_t)f->r10;
        switch((int)f->rdx){
        case 2:unix_socks[si].so_reuseaddr=!!val;r=0;break;
        case 9:unix_socks[si].so_keepalive=!!val;r=0;break;
        case 7:if(val<=0){r=EINVAL;break;}unix_socks[si].so_sndbuf=val;r=0;break;
        case 8:if(val<=0){r=EINVAL;break;}unix_socks[si].so_rcvbuf=val;r=0;break;
        default:r=ENOPROTOOPT;break;
        }break;
    }
    case blockos::syscall::SYS_getsockopt:{
        int si=fd_socket_index((int)f->rdi);if(si<0){r=ENOTSOCK;break;}
        if(f->rsi!=1){r=ENOPROTOOPT;break;}
        if(!paging::is_user_range(f->r8,sizeof(uint32_t),true)){r=EFAULT;break;}
        uint32_t cap=*(uint32_t*)(uintptr_t)f->r8;
        if(cap && !paging::is_user_range(f->r10,cap,true)){r=EFAULT;break;}
        int val=0;
        switch((int)f->rdx){case 2:val=unix_socks[si].so_reuseaddr;break;case 3:val=SOCK_STREAM;break;case 4:val=0;break;case 7:val=unix_socks[si].so_sndbuf?unix_socks[si].so_sndbuf:65536;break;case 8:val=unix_socks[si].so_rcvbuf?unix_socks[si].so_rcvbuf:65536;break;case 9:val=unix_socks[si].so_keepalive;break;default:r=ENOPROTOOPT;break;}
        if(r==ENOSYS){if(cap<sizeof(int)){r=EINVAL;break;}*(int*)(uintptr_t)f->r10=val;*(uint32_t*)(uintptr_t)f->r8=sizeof(int);r=0;}
        break;
    }
    case blockos::syscall::SYS_socketpair:{
        int type=(int)(f->rsi&0xffff), proto=(int)f->rdx;
        if((int)f->rdi!=AF_UNIX||(type&0xf)!=SOCK_STREAM||(type&~0x8080f)!=0||proto!=0||!paging::is_user_range(f->r10,8,true)){r=EINVAL;break;}
        int a=socket_obj_alloc(),b=socket_obj_alloc();if(a<0||b<0){if(a>=0)socket_free(a);if(b>=0)socket_free(b);r=ENOMEM;break;}
        int fda=alloc_fd(),fdb=alloc_fd();if(fda<0||fdb<0){if(fda>=0)fdtable()[fda]={};if(fdb>=0)fdtable()[fdb]={};socket_free(a);socket_free(b);r=EMFILE;break;}
        unix_socks[a].peer=b;unix_socks[b].peer=a;unix_socks[a].connected=unix_socks[b].connected=true;
        uint32_t sf=(type&0x80000)?FD_CLOEXEC:0;if(type&0x800)sf|=FD_NONBLOCK;
        fs=fdtable();fs[fda]={true,process::RuntimeFd::UnixSocket,sf,(uint32_t)(a+1),nullptr,0,0,{}};fs[fdb]={true,process::RuntimeFd::UnixSocket,sf,(uint32_t)(b+1),nullptr,0,0,{}};int* up=(int*)(uintptr_t)f->r10;up[0]=fda;up[1]=fdb;r=0;break;}
    case blockos::syscall::SYS_sendmsg:{
        if(fd_socket_index((int)f->rdi)<0){r=ENOTSOCK;break;}
        struct IOV{void*base;size_t len;};
        struct MSG{void*name;uint32_t namelen;uint32_t pad;IOV*iov;size_t iovlen;void*control;size_t controllen;int flags;};
        if(!paging::is_user_range(f->rsi,sizeof(MSG),false)){r=EFAULT;break;}
        auto*m=(MSG*)(uintptr_t)f->rsi;
        if(m->iovlen>64 || m->iovlen>SIZE_MAX/sizeof(IOV)){r=EINVAL;break;}
        if(m->iovlen&&!paging::is_user_range((uint64_t)(uintptr_t)m->iov,m->iovlen*sizeof(IOV),false)){r=EFAULT;break;}
        size_t total=0;
        for(size_t i=0;i<m->iovlen;i++){
            int64_t n=write_fd_user((int)f->rdi,(uint64_t)(uintptr_t)m->iov[i].base,m->iov[i].len);
            if(n<0){r=total?(int64_t)total:n;goto sendmsg_done;}
            total+=(size_t)n;if((size_t)n<m->iov[i].len)break;
        }
        r=(int64_t)total;
    sendmsg_done:break;
    }
    case blockos::syscall::SYS_recvmsg:{
        if(fd_socket_index((int)f->rdi)<0){r=ENOTSOCK;break;}
        struct IOV{void*base;size_t len;};
        struct MSG{void*name;uint32_t namelen;uint32_t pad;IOV*iov;size_t iovlen;void*control;size_t controllen;int flags;};
        if(!paging::is_user_range(f->rsi,sizeof(MSG),false)){r=EFAULT;break;}
        auto*m=(MSG*)(uintptr_t)f->rsi;
        if(m->iovlen>64 || m->iovlen>SIZE_MAX/sizeof(IOV)){r=EINVAL;break;}
        if(m->iovlen&&!paging::is_user_range((uint64_t)(uintptr_t)m->iov,m->iovlen*sizeof(IOV),false)){r=EFAULT;break;}
        size_t total=0;
        for(size_t i=0;i<m->iovlen;i++){
            int64_t n=read_fd_user((int)f->rdi,(uint64_t)(uintptr_t)m->iov[i].base,m->iov[i].len);
            if(n<0){r=total?(int64_t)total:n;goto recvmsg_done;}
            total+=(size_t)n;if((size_t)n<m->iov[i].len)break;
        }
        r=(int64_t)total;
    recvmsg_done:break;
    }
    case blockos::syscall::SYS_poll:
    case blockos::syscall::SYS_ppoll:{
        struct Pollfd { int fd; short events; short revents; };
        if (f->rsi > 4096 || f->rsi > SIZE_MAX / sizeof(Pollfd)) { r=EINVAL; break; }
        if (f->rsi && !paging::is_user_range(f->rdi, (size_t)f->rsi * sizeof(Pollfd), true)) { r=EFAULT; break; }
        /* ppoll's fifth argument is sigsetsize; only the 64-bit signal mask ABI is supported. */
        uint64_t oldmask = 0;
        if (f->rax == blockos::syscall::SYS_ppoll) {
            if (f->r8 != sizeof(uint64_t)) { r=EINVAL; break; }
            if (f->r10 && !p) { r=ESRCH; break; }
            if (f->r10 && !paging::is_user_range(f->r10, sizeof(uint64_t), false)) { r=EFAULT; break; }
            if (f->rdx && !paging::is_user_range(f->rdx, sizeof(int64_t)*2, false)) { r=EFAULT; break; }
            if (f->rdx) {
                const int64_t* ts=(const int64_t*)(uintptr_t)f->rdx;
                if (ts[0] < 0 || ts[1] < 0 || ts[1] >= 1000000000LL) { r=EINVAL; break; }
            }
            if (p && f->r10) { oldmask=p->signal_mask; p->signal_mask=*(const uint64_t*)(uintptr_t)f->r10; }
        }
        auto* pfd=(Pollfd*)(uintptr_t)f->rdi; int ready=0;
        for (uint64_t i=0; i<f->rsi; ++i) {
            pfd[i].revents=0;
            if (pfd[i].fd < 0) continue;
            if (!fs || pfd[i].fd >= (int)process::MAX_RUNTIME_FDS || !fs[pfd[i].fd].used) {
                pfd[i].revents=0x020; /* POLLNVAL */
            } else {
                if ((pfd[i].events & POLLIN) && fd_readable(pfd[i].fd)) pfd[i].revents|=POLLIN;
                if ((pfd[i].events & POLLOUT) && fd_writable(pfd[i].fd)) pfd[i].revents|=POLLOUT;
                if (fs[pfd[i].fd].kind==process::RuntimeFd::UnixSocket) {
                    int si=fd_socket_index(pfd[i].fd);
                    if (si>=0 && unix_socks[si].closed) pfd[i].revents|=POLLHUP;
                }
            }
            if (pfd[i].revents) ++ready;
        }
        if (f->rax == blockos::syscall::SYS_ppoll && p && f->r10) p->signal_mask=oldmask;
        /* This kernel currently reports readiness immediately; it does not yet park a task on an I/O wait queue. */
        r=ready; break;
    }
    case blockos::syscall::SYS_service:
        /* Reserved BlockOS service ABI number; not implemented as a syscall. */
        r=ENOSYS; break;
    case blockos::syscall::SYS_epoll_create1:{
        if (f->rdi & ~(uint64_t)O_CLOEXEC) { r=EINVAL; break; }
        int ei=-1;for(int i=0;i<64;i++)if(!epolls[i].used){epolls[i]={};epolls[i].used=true;ei=i;break;}
        if(ei<0){r=ENOMEM;break;} int fd=alloc_fd(); if(fd<0){epolls[ei]={};r=EMFILE;break;}
        fs=fdtable(); fs[fd]={true,process::RuntimeFd::File,(f->rdi&O_CLOEXEC)?FD_CLOEXEC:0,(uint32_t)(0x80000000u|(uint32_t)ei),nullptr,0,0,{}}; r=fd; break;
    }
    case blockos::syscall::SYS_epoll_ctl:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        uint32_t obj=fs[f->rdi].object;if(!(obj&0x80000000u)||(obj&0x7fffffffu)>=64||!epolls[obj&0x7fffffffu].used){r=EINVAL;break;}
        EpollObj& ep=epolls[obj&0x7fffffffu];int target=(int)f->rdx;int op=(int)f->rsi;
        if(target<0||target>=(int)process::MAX_RUNTIME_FDS||!fs[target].used){r=EBADF;break;}
        struct Ev{uint32_t events;uint32_t pad;uint64_t data;};Ev* ev=nullptr;
        if(op!=2){if(!paging::is_user_range(f->r10,sizeof(Ev),false)){r=EFAULT;break;}ev=(Ev*)(uintptr_t)f->r10;}
        int idx=-1;for(int i=0;i<64;i++)if(ep.watch[i].used&&ep.watch[i].fd==target){idx=i;break;}
        if(op==1){if(idx>=0){r=EEXIST;break;}int slot=-1;for(int i=0;i<64;i++)if(!ep.watch[i].used){slot=i;break;}if(slot<0){r=ENOMEM;break;}ep.watch[slot]={true,target,ev->events,ev->data};r=0;}
        else if(op==2){if(idx>=0){ep.watch[idx]={};r=0;}else r=ENOENT;}
        else if(op==3){if(idx>=0){ep.watch[idx].events=ev->events;ep.watch[idx].data=ev->data;r=0;}else r=ENOENT;}
        else r=EINVAL;
        break;}
    case blockos::syscall::SYS_epoll_wait:{if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}uint32_t obj=fs[f->rdi].object;if(!(obj&0x80000000u)||(obj&0x7fffffffu)>=64||!epolls[obj&0x7fffffffu].used){r=EINVAL;break;}if(f->rdx==0||f->rdx>64){r=EINVAL;break;}EpollObj& ep=epolls[obj&0x7fffffffu];struct Ev{uint32_t events;uint32_t pad;uint64_t data;};if(!paging::is_user_range(f->rsi,(size_t)f->rdx*sizeof(Ev),true)){r=EFAULT;break;}auto*out=(Ev*)(uintptr_t)f->rsi;int n=0;for(auto&w:ep.watch){if(!w.used||n>=(int)f->rdx)continue;uint32_t ev=0;if((w.events&POLLIN)&&fd_readable(w.fd))ev|=POLLIN;if((w.events&POLLOUT)&&fd_writable(w.fd))ev|=POLLOUT;if(ev){out[n++]={ev,0,w.data};}}r=n;break;}
    case blockos::syscall::SYS_fcntl:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        auto&d=fs[f->rdi];
        switch(f->rsi){
        case F_GETFD:r=(d.flags&FD_CLOEXEC)?1:0;break;
        case F_SETFD:d.flags=(d.flags&~FD_CLOEXEC)|((f->rdx&1)?FD_CLOEXEC:0);r=0;break;
        case F_GETFL:r=(int64_t)(d.flags&~FD_CLOEXEC);break;
        case F_SETFL:d.flags=(d.flags&~(uint32_t)(O_APPEND|O_NONBLOCK))|((uint32_t)f->rdx&(O_APPEND|O_NONBLOCK));r=0;break;
        case F_DUPFD:{if(f->rdx>=process::MAX_RUNTIME_FDS){r=EINVAL;break;}int n=alloc_fd((uint16_t)f->rdx);if(n<0){r=EMFILE;break;}fs=fdtable();fs[n]=d;fs[n].flags&=~FD_CLOEXEC;r=n;break;}
        default:r=EINVAL;break;
        }break;
    }
    case blockos::syscall::SYS_dup:{if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}int n=alloc_fd();if(n<0){r=EMFILE;break;}fs[n]=fs[f->rdi];fs[n].flags&=~FD_CLOEXEC;r=n;break;}
    case blockos::syscall::SYS_dup2:{
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used||f->rsi>=process::MAX_RUNTIME_FDS){r=EBADF;break;}
        if(f->rdi==f->rsi){r=f->rsi;break;}
        release_fd((int)f->rsi); fs=fdtable(); fs[f->rsi]=fs[f->rdi]; fs[f->rsi].flags&=~FD_CLOEXEC; r=f->rsi; break;
    }
    case blockos::syscall::SYS_clock_gettime:{if(!paging::is_user_range(f->rsi,16,true)){r=EFAULT;break;}uint64_t ms=timer_uptime_ms();struct TS{int64_t sec,nsec;}*ts=(TS*)(uintptr_t)f->rsi;ts->sec=(int64_t)(ms/1000);ts->nsec=(int64_t)((ms%1000)*1000000);r=0;break;}
    case blockos::syscall::SYS_wait4:{
        if(!p){r=ECHILD;break;}
        const int64_t selector=(int64_t)f->rdi;
        const bool nohang=(f->r10&WNOHANG)!=0;
        if(f->r10&~(uint64_t)WNOHANG){r=EINVAL;break;}
        if(f->rdx && !paging::is_user_range(f->rdx,4,true)){r=EFAULT;break;}
        bool have_matching_child=false;
        r=ECHILD;
        for(size_t i=0;i<process::slot_count();i++){
            auto*x=process::slot_at(i);
            if(!x||x->state==process::State::EMPTY||x->parent_pid!=p->pid)continue;
            bool match=false;
            if(selector==-1) match=true;
            else if(selector>0) match=x->pid==(uint64_t)selector;
            else if(selector==0) match=x->pgid==p->pgid;
            else match=x->pgid==((uint64_t)(-(selector+1))+1);
            if(!match)continue;
            have_matching_child=true;
            if(x->state!=process::State::TERMINATED)continue;
            r=(int64_t)x->pid;
            if(f->rdx)*(int*)(uintptr_t)f->rdx=(int)(x->exit_code<<8);
            x->state=process::State::EMPTY;
            break;
        }
        if(r==ECHILD){
            if(!have_matching_child){r=ECHILD;break;}
            if(nohang){r=0;break;}
            p->wait_target_pid=(uint64_t)selector;
            p->wait_status_ptr=f->rdx;
            p->waiting_child=true;
            f->rax=0;
            if(preempt::block_from_syscall(f))return;
            p->waiting_child=false; p->wait_target_pid=0; p->wait_status_ptr=0;
            r=ECHILD;
        }
        break;
    }
    case blockos::syscall::SYS_fsync: {
        if(f->rdi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used) r=EBADF;
        else if(fs[f->rdi].kind==process::RuntimeFd::UnixSocket) r=EINVAL;
        else r=0; /* Current VFS is memory-backed; no disk flush operation exists. */
        break;
    }
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
        char raw[256], path[256];
        if(!copy_user_string(f->rdi,raw,sizeof(raw))){r=EFAULT;break;}
        if(!resolve_at_path(p,fs,AT_FDCWD,raw,path,sizeof(path))){r=ENAMETOOLONG;break;}
        if(!vfs::exists(path)){r=ENOENT;break;}
        if(!vfs::is_directory(path)){r=ENOTDIR;break;}
        r=vfs::remove_directory(path)?0:ENOTEMPTY;
        break;
    }
    case blockos::syscall::SYS_fork:
    case blockos::syscall::SYS_vfork: {
        if(!p){r=EFAULT;break;}
        bool vf = (f->rax==blockos::syscall::SYS_vfork);
        auto* child=process::fork_like(p,*reinterpret_cast<TrapFrame*>(f),true);
        (void)vf;
        if(!child){r=ENOMEM;break;}
        f->rax=child->pid; r=(int64_t)child->pid;
        if(preempt::block_from_syscall(f)) return;
        break;
    }
    case blockos::syscall::SYS_dup3: {
        if(f->rdi>=process::MAX_RUNTIME_FDS||f->rsi>=process::MAX_RUNTIME_FDS||!fs||!fs[f->rdi].used){r=EBADF;break;}
        if(f->rdx & ~(uint64_t)O_CLOEXEC){r=EINVAL;break;}
        if(f->rdi==f->rsi){r=EINVAL;break;} release_fd((int)f->rsi); fs=fdtable(); fs[f->rsi]=fs[f->rdi]; fs[f->rsi].flags=(fs[f->rsi].flags & ~FD_CLOEXEC)|((f->rdx&O_CLOEXEC)?FD_CLOEXEC:0); r=f->rsi; break;
    }
    case blockos::syscall::SYS_madvise: {
        if(!p){r=ESRCH;break;}
        if(!f->rsi||f->rsi>UINT64_MAX-(PAGE-1)){r=EINVAL;break;}
        if(!paging::is_user_range(f->rdi,(size_t)f->rsi,false)){r=EFAULT;break;}
        if(f->rdx!=0&&f->rdx!=1&&f->rdx!=2&&f->rdx!=4){r=EINVAL;break;}
        r=0; break; /* Advisory only: no page-reclaim backend yet. */
    }
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
    case blockos::syscall::SYS_sched_getaffinity: {
        if(f->rsi<sizeof(uint64_t)){r=EINVAL;break;}
        if(!paging::is_user_range(f->rdx,(size_t)f->rsi,true)){r=EFAULT;break;}
        memset((void*)(uintptr_t)f->rdx,0,(size_t)f->rsi);
        *(uint64_t*)(uintptr_t)f->rdx=1ULL;
        r=(int64_t)sizeof(uint64_t);break;
    }
    case blockos::syscall::SYS_access:{char raw[256],path[256];if(!copy_user_string(f->rdi,raw,sizeof(raw))){r=EFAULT;break;}if(!resolve_at_path(p,fs,AT_FDCWD,raw,path,sizeof(path))){r=ENAMETOOLONG;break;}uint32_t sz=0;r=(vfs::read_file(path,&sz)||vfs::is_directory(path)||vfs::is_device(path))?0:ENOENT;break;}
    case blockos::syscall::SYS_stat: case blockos::syscall::SYS_lstat:{
        char raw[256],path[256];if(!copy_user_string(f->rdi,raw,sizeof(raw))){r=EFAULT;break;}
        if(!resolve_at_path(p,fs,AT_FDCWD,raw,path,sizeof(path))){r=ENAMETOOLONG;break;}
        if(!paging::is_user_range(f->rsi,128,true)){r=EFAULT;break;}
        uint32_t sz=0;const uint8_t*d=vfs::read_file(path,&sz);bool dir=vfs::is_directory(path);bool dev=vfs::is_device(path);
        if(!d&&!dir&&!dev){r=ENOENT;break;}
        memset((void*)(uintptr_t)f->rsi,0,128);uint64_t*q=(uint64_t*)(uintptr_t)f->rsi;q[0]=1;q[1]=1;q[2]=dev?0020666:(dir?0040755:0100644);q[7]=d?(uint64_t)sz:0;r=0;break;
    }
    case blockos::syscall::SYS_fstatat:{
        char raw[256],path[256];
        if(!copy_user_string(f->rsi,raw,sizeof(raw))){r=EFAULT;break;}
        if((int)f->r10 & ~AT_SYMLINK_NOFOLLOW){r=EINVAL;break;}
        if(!resolve_at_path(p,fs,(int)f->rdi,raw,path,sizeof(path))){r=EBADF;break;}
        if(!paging::is_user_range(f->rdx,128,true)){r=EFAULT;break;}
        uint32_t sz=0;const uint8_t*d=vfs::read_file(path,&sz);bool dir=vfs::is_directory(path);bool dev=vfs::is_device(path);
        if(!d&&!dir&&!dev){r=ENOENT;break;}
        uint64_t*q=(uint64_t*)(uintptr_t)f->rdx;memset(q,0,128);q[0]=1;q[1]=1;q[2]=dev?0020666:(dir?0040755:0100644);q[7]=d?(uint64_t)sz:0;r=0;break;
    }
    default:r=ENOSYS;break;
    }
    f->rax=(uint64_t)r;
}
