#include "proc.hpp"

#include <stdint.h>
#include <stddef.h>

#include "../kernel/allocator.hpp"
#include "../arch/86_64x/irq.hpp"
#include "../kernel/sysmem.hpp"
#include "vfs.hpp"

namespace blockos::proc
{
namespace
{

constexpr size_t PROC_PATH_MAX = 256;
constexpr size_t PROC_BUFFER_MAX = 8192;

static const char* const top_files[] = {
    "cmdline",
    "cpuinfo",
    "devices",
    "filesystems",
    "loadavg",
    "meminfo",
    "mounts",
    "partitions",
    "stat",
    "swaps",
    "uptime",
    "version"
};

static constexpr size_t TOP_FILE_COUNT = sizeof(top_files) / sizeof(top_files[0]);

static size_t len(const char* s)
{
    if (!s) return 0;
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static bool eq(const char* a, const char* b)
{
    if (!a || !b) return false;
    size_t na = len(a);
    size_t nb = len(b);
    if (na != nb) return false;
    for (size_t i = 0; i < na; ++i)
        if (a[i] != b[i]) return false;
    return true;
}

static bool prefix(const char* s, const char* p)
{
    if (!s || !p) return false;
    size_t n = len(p);
    for (size_t i = 0; i < n; ++i)
        if (s[i] != p[i]) return false;
    return true;
}

static void copy_text(char* dst, size_t cap, const char* src)
{
    if (!dst || !cap) return;
    size_t i = 0;
    if (src) {
        while (i + 1 < cap && src[i]) {
            dst[i] = src[i];
            ++i;
        }
    }
    dst[i] = 0;
}

static bool append_char(char* out, size_t cap, size_t& pos, char c)
{
    if (!out || pos + 1 >= cap) return false;
    out[pos++] = c;
    out[pos] = 0;
    return true;
}

static bool append_text(char* out, size_t cap, size_t& pos, const char* s)
{
    if (!out || !s) return false;
    for (size_t i = 0; s[i]; ++i)
        if (!append_char(out, cap, pos, s[i])) return false;
    return true;
}

static bool append_u64(char* out, size_t cap, size_t& pos, uint64_t value)
{
    char tmp[32];
    size_t n = 0;
    if (value == 0) tmp[n++] = '0';
    else {
        while (value && n < sizeof(tmp)) {
            tmp[n++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
    }
    while (n) {
        if (!append_char(out, cap, pos, tmp[--n])) return false;
    }
    return true;
}

static bool append_s64(char* out, size_t cap, size_t& pos, int64_t value)
{
    if (value < 0) {
        if (!append_char(out, cap, pos, '-')) return false;
        value = -value;
    }
    return append_u64(out, cap, pos, static_cast<uint64_t>(value));
}

static bool append_kb(char* out, size_t cap, size_t& pos, uint64_t bytes)
{
    return append_u64(out, cap, pos, bytes / 1024) &&
           append_text(out, cap, pos, " kB\n");
}

static size_t finish(char* out, size_t cap, size_t pos)
{
    if (out && cap) out[pos < cap ? pos : cap - 1] = 0;
    return pos;
}

static char state_char(const process::Process* p)
{
    if (!p) return 'S';
    switch (p->state) {
        case process::State::RUNNING: return 'R';
        case process::State::READY: return 'R';
        case process::State::BLOCKED: return 'S';
        case process::State::TERMINATED: return 'Z';
        default: return 'S';
    }
}

static const char* process_basename(const process::Process* p)
{
    if (!p || !p->name[0]) return "BlockOS";
    const char* last = p->name;
    for (const char* s = p->name; *s; ++s)
        if (*s == '/') last = s + 1;
    return *last ? last : "BlockOS";
}

static void process_path(const process::Process* p, char* out, size_t cap)
{
    if (!p || !p->name[0]) copy_text(out, cap, "/bin/app");
    else copy_text(out, cap, p->name);
}

static bool parse_pid_dir(const char* path, uint64_t* pid)
{
    if (!path || !pid) return false;
    if (path[0] != '/') return false;

    size_t i = 1;
    if (path[i] == 0) return false;
    uint64_t value = 0;
    bool any = false;
    while (path[i] >= '0' && path[i] <= '9') {
        any = true;
        value = value * 10 + static_cast<uint64_t>(path[i] - '0');
        ++i;
    }
    if (!any || path[i] != '/' || path[i + 1] != 0) return false;
    *pid = value;
    return true;
}

static bool split_process_path(const char* path, uint64_t* pid, const char** leaf)
{
    if (!path || !pid || !leaf) return false;
    if (path[0] != '/') return false;

    size_t i = 1;
    if (path[i] == 0) return false;
    uint64_t value = 0;
    bool any = false;
    while (path[i] >= '0' && path[i] <= '9') {
        any = true;
        value = value * 10 + static_cast<uint64_t>(path[i] - '0');
        ++i;
    }
    if (!any || path[i] != '/') return false;
    ++i;
    if (!path[i]) return false;

    *pid = value;
    *leaf = path + i;
    return true;
}

static process::Process* process_from_path(const char* path)
{
    uint64_t pid = 0;
    const char* leaf = nullptr;
    if (!split_process_path(path, &pid, &leaf)) return nullptr;
    (void)leaf;
    return process::get(pid);
}

static size_t render_meminfo(char* out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = 0;
    size_t pos = 0;
    const auto& sys = sysmem::get_record();
    const auto& heap = allocator::get_record();
    const uint64_t heap_free = heap.total > heap.used ? heap.total - heap.used : 0;

    const uint64_t total = sys.total;
    const uint64_t free_b = sys.free;
    const uint64_t available = sys.free + sys.reclaimable;

    append_text(out, cap, pos, "MemTotal:       "); append_kb(out, cap, pos, total);
    append_text(out, cap, pos, "MemFree:        "); append_kb(out, cap, pos, free_b);
    append_text(out, cap, pos, "MemAvailable:   "); append_kb(out, cap, pos, available > total ? total : available);
    append_text(out, cap, pos, "Buffers:        0 kB\n");
    append_text(out, cap, pos, "Cached:         0 kB\n");
    append_text(out, cap, pos, "SwapCached:     0 kB\n");
    append_text(out, cap, pos, "Active:         0 kB\n");
    append_text(out, cap, pos, "Inactive:       0 kB\n");
    append_text(out, cap, pos, "Active(anon):   0 kB\n");
    append_text(out, cap, pos, "Inactive(anon): 0 kB\n");
    append_text(out, cap, pos, "Active(file):   0 kB\n");
    append_text(out, cap, pos, "Inactive(file): 0 kB\n");
    append_text(out, cap, pos, "Unevictable:    0 kB\n");
    append_text(out, cap, pos, "Mlocked:        0 kB\n");
    append_text(out, cap, pos, "SwapTotal:      0 kB\n");
    append_text(out, cap, pos, "SwapFree:       0 kB\n");
    append_text(out, cap, pos, "Dirty:          0 kB\n");
    append_text(out, cap, pos, "Writeback:      0 kB\n");
    append_text(out, cap, pos, "AnonPages:      0 kB\n");
    append_text(out, cap, pos, "Mapped:         0 kB\n");
    append_text(out, cap, pos, "Slab:           "); append_kb(out, cap, pos, heap.used);
    append_text(out, cap, pos, "SReclaimable:   "); append_kb(out, cap, pos, sys.reclaimable);
    append_text(out, cap, pos, "SUnreclaim:     "); append_kb(out, cap, pos, heap_free);
    append_text(out, cap, pos, "PageTables:     0 kB\n");
    append_text(out, cap, pos, "KernelStack:    0 kB\n");
    append_text(out, cap, pos, "CommitLimit:    "); append_kb(out, cap, pos, total);
    append_text(out, cap, pos, "Committed_AS:   0 kB\n");
    return finish(out, cap, pos);
}

static size_t render_uptime(char* out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = 0;
    size_t pos = 0;
    uint64_t ms = timer_uptime_ms();
    uint64_t sec = ms / 1000;
    uint64_t frac = (ms % 1000) / 10;
    append_u64(out, cap, pos, sec);
    append_char(out, cap, pos, '.');
    if (frac < 10) append_char(out, cap, pos, '0');
    append_u64(out, cap, pos, frac);
    append_text(out, cap, pos, " ");
    append_u64(out, cap, pos, sec);
    append_text(out, cap, pos, "\n");
    return finish(out, cap, pos);
}

static size_t render_version(char* out, size_t cap)
{
    const char* s = "BlockOS version 1.0.0 x86_64\n";
    size_t n = len(s);
    if (!out || !cap) return 0;
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = s[i];
    out[n] = 0;
    return n;
}

static size_t render_cmdline(char* out, size_t cap)
{
    const char* s = "BOOT_IMAGE=BlockOS";
    if (!out || !cap) return 0;
    out[0] = 0;
    size_t pos = 0;
    append_text(out, cap, pos, s);
    append_char(out, cap, pos, '\n');
    return pos;
}

static size_t render_filesystems(char* out, size_t cap)
{
    const char* s =
        "nodev\tproc\n"
        "nodev\tsysfs\n"
        "nodev\ttmpfs\n"
        "nodev\tdevpts\n"
        "ext2\n"
        "ext3\n"
        "ext4\n"
        "vfat\n"
        "exfat\n"
        "iso9660\n"
        "ntfs\n"
        "udf\n";
    if (!out || !cap) return 0;
    size_t n = len(s);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = s[i];
    out[n] = 0;
    return n;
}

static size_t render_mounts(char* out, size_t cap)
{
    const char* s =
        "rootfs / ext4 rw 0 0\n"
        "proc /proc proc rw 0 0\n"
        "sysfs /system sysfs rw 0 0\n";
    if (!out || !cap) return 0;
    size_t n = len(s);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = s[i];
    out[n] = 0;
    return n;
}

static size_t render_stat(char* out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = 0;
    size_t pos = 0;
    uint64_t ms = timer_uptime_ms();
    uint64_t jiffies = ms / 10;
    size_t nr = process::count();
    size_t running = 0;
    uint64_t last_pid = 0;
    for (size_t i = 0; i < process::slot_count(); ++i) {
        process::Process* p = process::slot_at(i);
        if (!p || p->state == process::State::EMPTY) continue;
        if (p->state == process::State::READY || p->state == process::State::RUNNING) ++running;
        if (p->pid > last_pid) last_pid = p->pid;
    }
    append_text(out, cap, pos, "cpu  ");
    append_u64(out, cap, pos, 0);
    append_text(out, cap, pos, " 0 ");
    append_u64(out, cap, pos, jiffies);
    append_text(out, cap, pos, " 0 0 0 0 0 0 0\n");
    append_text(out, cap, pos, "intr 0\ncontext 0\nprocesses ");
    append_u64(out, cap, pos, nr);
    append_text(out, cap, pos, "\nprocs_running ");
    append_u64(out, cap, pos, running);
    append_text(out, cap, pos, "\nprocs_blocked ");
    append_u64(out, cap, pos, nr >= running ? nr - running : 0);
    append_text(out, cap, pos, "\nbtime 0\n\n");
    append_text(out, cap, pos, "last_pid ");
    append_u64(out, cap, pos, last_pid);
    append_char(out, cap, pos, '\n');
    return finish(out, cap, pos);
}

static size_t render_loadavg(char* out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = 0;
    size_t pos = 0;
    size_t running = 0;
    size_t total = 0;
    uint64_t last_pid = 0;
    for (size_t i = 0; i < process::slot_count(); ++i) {
        process::Process* p = process::slot_at(i);
        if (!p || p->state == process::State::EMPTY) continue;
        ++total;
        if (p->state == process::State::READY || p->state == process::State::RUNNING) ++running;
        if (p->pid > last_pid) last_pid = p->pid;
    }
    uint64_t hundred = running * 100;
    append_u64(out, cap, pos, hundred / 100);
    append_char(out, cap, pos, '.');
    if ((hundred % 100) < 10) append_char(out, cap, pos, '0');
    append_u64(out, cap, pos, hundred % 100);
    append_text(out, cap, pos, " 0.00 0.00 ");
    append_u64(out, cap, pos, running);
    append_char(out, cap, pos, '/');
    append_u64(out, cap, pos, total);
    append_char(out, cap, pos, ' ');
    append_u64(out, cap, pos, last_pid);
    append_char(out, cap, pos, '\n');
    return pos;
}

static size_t render_devices(char* out, size_t cap)
{
    const char* s =
        "Character devices:\n"
        "  4 tty\n"
        " 10 misc\n"
        "Block devices:\n"
        "  8 disk\n";
    if (!out || !cap) return 0;
    size_t n = len(s);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = s[i];
    out[n] = 0;
    return n;
}

static size_t render_partitions(char* out, size_t cap)
{
    const char* s = "major minor  #blocks  name\n\n";
    if (!out || !cap) return 0;
    size_t n = len(s);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = s[i];
    out[n] = 0;
    return n;
}

static size_t render_swaps(char* out, size_t cap)
{
    const char* s = "Filename\t\t\tType\t\tSize\tUsed\tPriority\n";
    if (!out || !cap) return 0;
    size_t n = len(s);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = s[i];
    out[n] = 0;
    return n;
}

static size_t render_cpuinfo(char* out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = 0;
    size_t pos = 0;
    append_text(out, cap, pos, "processor\t: 0\nmodel name\t: BlockOS x86_64\n");
    append_text(out, cap, pos, "cpu family\t: 6\nmodel\t\t: 0\nstepping\t: 0\n\n");
    return finish(out, cap, pos);
}

static size_t render_process_status(const process::Process* p, char* out, size_t cap)
{
    if (!p || !out || !cap) return 0;
    out[0] = 0;
    size_t pos = 0;
    const char* name = process_basename(p);
    append_text(out, cap, pos, "Name:\t"); append_text(out, cap, pos, name); append_char(out, cap, pos, '\n');
    append_text(out, cap, pos, "State:\t"); append_char(out, cap, pos, state_char(p)); append_text(out, cap, pos, " (BlockOS)\n");
    append_text(out, cap, pos, "Tgid:\t"); append_u64(out, cap, pos, p->fd_owner ? p->fd_owner->pid : p->pid); append_char(out, cap, pos, '\n');
    append_text(out, cap, pos, "Pid:\t"); append_u64(out, cap, pos, p->pid); append_char(out, cap, pos, '\n');
    append_text(out, cap, pos, "PPid:\t"); append_u64(out, cap, pos, p->parent_pid); append_char(out, cap, pos, '\n');
    append_text(out, cap, pos, "TracerPid:\t0\nUid:\t0\t0\t0\t0\nGid:\t0\t0\t0\t0\n");
    append_text(out, cap, pos, "Threads:\t");
    size_t threads = 1;
    process::Process* owner = p->fd_owner ? p->fd_owner : const_cast<process::Process*>(p);
    for (size_t i = 0; i < process::slot_count(); ++i) {
        process::Process* q = process::slot_at(i);
        if (!q || q == p || q->state == process::State::EMPTY) continue;
        if ((q->fd_owner ? q->fd_owner : q) == owner) ++threads;
    }
    append_u64(out, cap, pos, threads); append_char(out, cap, pos, '\n');
    append_text(out, cap, pos, "VmSize:\t"); append_kb(out, cap, pos, p->brk_current >= p->brk_base ? p->brk_current - p->brk_base : 0);
    append_text(out, cap, pos, "VmRSS:\t0 kB\nVmData:\t0 kB\nVmStk:\t128 kB\nVmExe:\t0 kB\nThreadsMax:\t0\n");
    return finish(out, cap, pos);
}

static size_t render_process_stat(const process::Process* p, char* out, size_t cap)
{
    if (!p || !out || !cap) return 0;
    out[0] = 0;
    size_t pos = 0;
    const char* name = process_basename(p);
    append_u64(out, cap, pos, p->pid);
    append_text(out, cap, pos, " (");
    append_text(out, cap, pos, name);
    append_text(out, cap, pos, ") ");
    append_char(out, cap, pos, state_char(p));
    append_char(out, cap, pos, ' ');
    append_u64(out, cap, pos, p->parent_pid);
    append_text(out, cap, pos, " 0 0 0 0 0 0 0 0 0 0 0 ");
    append_u64(out, cap, pos, timer_uptime_ms() / 10);
    append_text(out, cap, pos, " 0 0 0 0 0 1 0 0 0 ");
    append_u64(out, cap, pos, p->brk_current >= p->brk_base ? p->brk_current - p->brk_base : 0);
    append_text(out, cap, pos, " 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n");
    return finish(out, cap, pos);
}

static size_t render_process_statm(const process::Process* p, char* out, size_t cap)
{
    if (!p || !out || !cap) return 0;
    out[0] = 0;
    size_t pos = 0;
    uint64_t size_pages = p->brk_current >= p->brk_base ? (p->brk_current - p->brk_base + 4095) / 4096 : 0;
    append_u64(out, cap, pos, size_pages);
    append_text(out, cap, pos, " 0 0 0 0 0 0\n");
    return pos;
}

static size_t render_process_cmdline(const process::Process* p, char* out, size_t cap)
{
    if (!p || !out || !cap) return 0;
    char path[PROC_PATH_MAX];
    process_path(p, path, sizeof(path));
    size_t n = len(path);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = path[i];
    out[n] = 0;
    return n + 1; /* Linux cmdline is NUL separated. */
}

static size_t render_process_comm(const process::Process* p, char* out, size_t cap)
{
    if (!p || !out || !cap) return 0;
    const char* name = process_basename(p);
    size_t n = len(name);
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = name[i];
    out[n++] = '\n';
    out[n] = 0;
    return n;
}

static size_t render_process_path(const process::Process* p, char* out, size_t cap)
{
    if (!p || !out || !cap) return 0;
    char path[PROC_PATH_MAX];
    process_path(p, path, sizeof(path));
    size_t n = len(path);
    if (n + 1 >= cap) n = cap - 2;
    for (size_t i = 0; i < n; ++i) out[i] = path[i];
    out[n++] = '\n';
    out[n] = 0;
    return n;
}

static size_t render_process_cwd(const process::Process* p, char* out, size_t cap)
{
    if (!p || !out || !cap) return 0;
    const char* cwd = process::cwd(const_cast<process::Process*>(p));
    size_t n = len(cwd);
    if (n + 1 >= cap) n = cap - 2;
    for (size_t i = 0; i < n; ++i) out[i] = cwd[i];
    out[n++] = '\n';
    out[n] = 0;
    return n;
}

static size_t render_leaf(const char* path, process::Process* current, char* out, size_t cap)
{
    if (!path || !out || !cap) return 0;
    out[0] = 0;

    const char* p = path;
    if (prefix(p, "/proc/")) p += 6;
    else if (prefix(p, "proc/")) p += 5;

    if (eq(p, "self/status")) {
        if (!current) current = process::current();
        return render_process_status(current, out, cap);
    }

    if (eq(p, "self/stat")) {
        if (!current) current = process::current();
        return render_process_stat(current, out, cap);
    }

    if (eq(p, "self/statm")) {
        if (!current) current = process::current();
        return render_process_statm(current, out, cap);
    }

    if (eq(p, "self/cmdline")) {
        if (!current) current = process::current();
        return render_process_cmdline(current, out, cap);
    }

    if (eq(p, "self/comm")) {
        if (!current) current = process::current();
        return render_process_comm(current, out, cap);
    }

    if (eq(p, "self/cwd") || eq(p, "self/exe")) {
        if (!current) current = process::current();
        return eq(p, "self/cwd") ? render_process_cwd(current, out, cap) : render_process_path(current, out, cap);
    }

    uint64_t pid = 0;
    const char* leaf = nullptr;
    if (split_process_path(path, &pid, &leaf)) {
        process::Process* proc = process::get(pid);
        if (!proc) return 0;
        if (eq(leaf, "status")) return render_process_status(proc, out, cap);
        if (eq(leaf, "stat")) return render_process_stat(proc, out, cap);
        if (eq(leaf, "statm")) return render_process_statm(proc, out, cap);
        if (eq(leaf, "cmdline")) return render_process_cmdline(proc, out, cap);
        if (eq(leaf, "comm")) return render_process_comm(proc, out, cap);
        if (eq(leaf, "cwd")) return render_process_cwd(proc, out, cap);
        if (eq(leaf, "exe")) return render_process_path(proc, out, cap);
        return 0;
    }

    if (eq(p, "meminfo")) return render_meminfo(out, cap);
    if (eq(p, "uptime")) return render_uptime(out, cap);
    if (eq(p, "version")) return render_version(out, cap);
    if (eq(p, "cmdline")) return render_cmdline(out, cap);
    if (eq(p, "filesystems")) return render_filesystems(out, cap);
    if (eq(p, "mounts")) return render_mounts(out, cap);
    if (eq(p, "stat")) return render_stat(out, cap);
    if (eq(p, "loadavg")) return render_loadavg(out, cap);
    if (eq(p, "cpuinfo")) return render_cpuinfo(out, cap);
    if (eq(p, "devices")) return render_devices(out, cap);
    if (eq(p, "partitions")) return render_partitions(out, cap);
    if (eq(p, "swaps")) return render_swaps(out, cap);
    return 0;
}

static bool create_dir(const char* path)
{
    if (vfs::is_directory(path)) return true;
    return vfs::create_directory(path);
}

static bool write_proc_file(const char* path, process::Process* current)
{
    char data[PROC_BUFFER_MAX];
    size_t n = render_leaf(path, current, data, sizeof(data));
    return vfs::write_file(path, reinterpret_cast<const uint8_t*>(data), static_cast<uint32_t>(n));
}

static bool ensure_process_tree(process::Process* p)
{
    if (!p || p->state == process::State::EMPTY) return false;
    char base[64];
    size_t pos = 0;
    base[pos++] = '/'; base[pos++] = 'p'; base[pos++] = 'r'; base[pos++] = 'o'; base[pos++] = 'c'; base[pos++] = '/';
    char digits[32]; size_t dn = 0; uint64_t v = p->pid;
    if (v == 0) digits[dn++] = '0';
    while (v) { digits[dn++] = static_cast<char>('0' + v % 10); v /= 10; }
    while (dn) base[pos++] = digits[--dn];
    base[pos] = 0;

    if (!create_dir(base)) return false;

    const char* files[] = {"status", "stat", "statm", "cmdline", "comm", "cwd", "exe"};
    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); ++i) {
        char path[PROC_PATH_MAX];
        copy_text(path, sizeof(path), base);
        size_t n = len(path);
        path[n++] = '/';
        copy_text(path + n, sizeof(path) - n, files[i]);
        if (!write_proc_file(path, p)) return false;
    }
    create_dir("/proc/self");
    return true;
}

} // namespace

size_t read(const char* name, char* buffer, size_t max_size)
{
    if (!name || !buffer || max_size == 0) return 0;
    if (name[0] == '/') return render_leaf(name, process::current(), buffer, max_size);

    char path[PROC_PATH_MAX];
    path[0] = '/'; path[1] = 'p'; path[2] = 'r'; path[3] = 'o'; path[4] = 'c'; path[5] = '/';
    copy_text(path + 6, sizeof(path) - 6, name);
    return render_leaf(path, process::current(), buffer, max_size);
}

bool exists(const char* name)
{
    if (!name) return false;
    char buf[64];
    return read(name, buf, sizeof(buf)) != 0 ||
           eq(name, "proc") || eq(name, "/proc") || eq(name, "self") || eq(name, "/proc/self");
}

size_t count()
{
    return TOP_FILE_COUNT;
}

const char* name_at(size_t index)
{
    return index < TOP_FILE_COUNT ? top_files[index] : nullptr;
}

bool is_proc_path(const char* path)
{
    if (!path) return false;
    return eq(path, "/proc") || prefix(path, "/proc/");
}

bool is_directory_path(const char* path, process::Process* current)
{
    if (!path) return false;
    if (eq(path, "/proc") || eq(path, "/proc/self")) return true;

    uint64_t only_pid = 0;
    if (parse_pid_dir(path, &only_pid))
        return process::get(only_pid) != nullptr;

    uint64_t pid = 0;
    const char* leaf = nullptr;
    if (split_process_path(path, &pid, &leaf)) {
        if (!eq(leaf, "fd")) return false;
        return process::get(pid) != nullptr;
    }

    if (eq(path, "/proc/self/fd")) return current != nullptr || process::current() != nullptr;
    return false;
}

size_t directory_entry_count(const char* path, process::Process* current)
{
    if (!path) return 0;
    if (eq(path, "/proc") ) {
        size_t n = TOP_FILE_COUNT + 1; /* self */
        for (size_t i = 0; i < process::slot_count(); ++i) {
            process::Process* p = process::slot_at(i);
            if (p && p->state != process::State::EMPTY) ++n;
        }
        return n;
    }
    if (eq(path, "/proc/self")) return 8; /* 7 files + fd */
    uint64_t only_pid = 0;
    if (parse_pid_dir(path, &only_pid))
        return process::get(only_pid) ? 8 : 0; /* 7 files + fd */

    uint64_t pid = 0;
    const char* leaf = nullptr;
    if (split_process_path(path, &pid, &leaf) && eq(leaf, "fd")) {
        process::Process* p = process::get(pid);
        if (!p) return 0;
        return process::MAX_RUNTIME_FDS;
    }
    if (eq(path, "/proc/self/fd")) {
        process::Process* p = current ? current : process::current();
        return p ? process::MAX_RUNTIME_FDS : 0;
    }
    return 0;
}

const char* directory_entry_name(const char* path, size_t index, process::Process* current)
{
    static char name[32];
    static char files[] = "status";
    (void)files;
    if (!path) return nullptr;

    if (eq(path, "/proc")) {
        if (index < TOP_FILE_COUNT) return top_files[index];
        if (index == TOP_FILE_COUNT) return "self";
        size_t skip = TOP_FILE_COUNT + 1;
        for (size_t i = 0; i < process::slot_count(); ++i) {
            process::Process* p = process::slot_at(i);
            if (!p || p->state == process::State::EMPTY) continue;
            if (index == skip) {
                size_t n = 0; uint64_t v = p->pid;
                char rev[24]; size_t rn = 0;
                do { rev[rn++] = static_cast<char>('0' + (v % 10)); v /= 10; } while (v && rn < sizeof(rev));
                while (rn) name[n++] = rev[--rn];
                name[n] = 0;
                return name;
            }
            ++skip;
        }
        return nullptr;
    }

    if (eq(path, "/proc/self")) {
        static const char* const self_entries[] = {"status", "stat", "statm", "cmdline", "comm", "cwd", "exe", "fd"};
        return index < sizeof(self_entries)/sizeof(self_entries[0]) ? self_entries[index] : nullptr;
    }

    uint64_t only_pid = 0;
    if (parse_pid_dir(path, &only_pid)) {
        static const char* const pid_entries[] = {"status", "stat", "statm", "cmdline", "comm", "cwd", "exe", "fd"};
        return process::get(only_pid) && index < sizeof(pid_entries)/sizeof(pid_entries[0]) ? pid_entries[index] : nullptr;
    }

    uint64_t pid = 0;
    const char* leaf = nullptr;
    if (split_process_path(path, &pid, &leaf) && eq(leaf, "fd")) {
        if (index >= process::MAX_RUNTIME_FDS) return nullptr;
        size_t n = 0; uint64_t v = index;
        char rev[24]; size_t rn = 0;
        do { rev[rn++] = static_cast<char>('0' + (v % 10)); v /= 10; } while (v && rn < sizeof(rev));
        while (rn) name[n++] = rev[--rn];
        name[n] = 0;
        return name;
    }
    if (eq(path, "/proc/self/fd")) {
        (void)current;
        if (index >= process::MAX_RUNTIME_FDS) return nullptr;
        size_t n = 0; uint64_t v = index;
        char rev[24]; size_t rn = 0;
        do { rev[rn++] = static_cast<char>('0' + (v % 10)); v /= 10; } while (v && rn < sizeof(rev));
        while (rn) name[n++] = rev[--rn];
        name[n] = 0;
        return name;
    }

    return nullptr;
}

bool refresh(process::Process* current)
{
    if (!current) current = process::current();
    if (!create_dir("/proc")) return false;

    char data[PROC_BUFFER_MAX];
    const char* files[] = {"cmdline","cpuinfo","devices","filesystems","loadavg","meminfo","mounts","partitions","stat","swaps","uptime","version"};
    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); ++i) {
        char path[64];
        path[0]='/'; path[1]='p'; path[2]='r'; path[3]='o'; path[4]='c'; path[5]='/';
        copy_text(path + 6, sizeof(path) - 6, files[i]);
        size_t n = render_leaf(path, current, data, sizeof(data));
        if (!vfs::write_file(path, reinterpret_cast<const uint8_t*>(data), static_cast<uint32_t>(n))) return false;
    }

    create_dir("/proc/self");
    if (current) {
        const char* names[] = {"status","stat","statm","cmdline","comm","cwd","exe"};
        for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i) {
            char path[64];
            copy_text(path, sizeof(path), "/proc/self/");
            size_t n = len(path);
            copy_text(path + n, sizeof(path) - n, names[i]);
            size_t used = render_leaf(path, current, data, sizeof(data));
            if (!vfs::write_file(path, reinterpret_cast<const uint8_t*>(data), static_cast<uint32_t>(used))) return false;
        }
    }

    for (size_t i = 0; i < process::slot_count(); ++i) {
        process::Process* p = process::slot_at(i);
        if (p && p->state != process::State::EMPTY)
            if (!ensure_process_tree(p)) return false;
    }
    return true;
}

void init()
{
    (void)refresh(process::current());
}

bool test()
{
    char buffer[256];
    return read("meminfo", buffer, sizeof(buffer)) > 0;
}

} // namespace blockos::proc

void init_proc_fs()
{
    blockos::proc::init();
}
