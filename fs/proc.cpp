#include "fs/vfs.hpp"
#include "kernel/scheduler.hpp"
#include "kernel/allocator.hpp"
#include "kernel/spinlock.hpp"

// Egyszerű beépített segédfunkció az egész számok szöveggé alakításához
static size_t uint_to_str(uint64_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; return 1; }
    char tmp[24];
    int i = 0;
    while (val > 0) { tmp[i++] = (val % 10) + '0'; val /= 10; }
    size_t len = i;
    int j = 0;
    while (i > 0) { buf[j++] = tmp[--i]; }
    return len;
}

namespace Blockos {

// Definiáljuk a virtuális proc node-okat, amik futás közben generálnak adatot
typedef size_t (*ProcReadCallback)(char* buffer, size_t max_len);

class ProcFileSystem : public VFS::FileSystem {
private:
    Spinlock m_lock;

    // 1. /proc/meminfo dinamikus legenerálása a te allocator.hpp-d alapján [source: 2]
    static size_t read_meminfo(char* buf, size_t max) {
        // Feltételezve, hogy a KernelAllocator lekérhető statikus függvényekkel [source: 2]
        size_t free_mem = KernelAllocator::get_free_memory();
        size_t total_mem = KernelAllocator::get_total_memory();
        
        size_t idx = 0;
        const char* l1 = "MemTotal: "; memcpy(buf + idx, l1, strlen(l1)); idx += strlen(l1);
        idx += uint_to_str(total_mem / 1024, buf + idx);
        const char* kb = " kB\nMemFree:  "; memcpy(buf + idx, kb, strlen(kb)); idx += strlen(kb);
        idx += uint_to_str(free_mem / 1024, buf + idx);
        const char* kb2 = " kB\n"; memcpy(buf + idx, kb2, strlen(kb2)); idx += strlen(kb2);
        
        return idx;
    }

    // 2. /proc/version dinamikus legenerálása
    static size_t read_version(char* buf, size_t max) {
        const char* ver = "Blockos Kernel v1.0.0-uefi (x86_64) SMP Preempt\n";
        size_t len = strlen(ver);
        memcpy(buf, ver, len);
        return len;
    }

    // 3. /proc/[PID]/status dinamikus lekérése az ütemezőből [source: 2]
    static size_t read_task_status(uint32_t pid, char* buf, size_t max) {
        Task* task = Scheduler::get_task_by_pid(pid);
        if (!task) return 0;

        size_t idx = 0;
        const char* n = "Name:\t"; memcpy(buf + idx, n, strlen(n)); idx += strlen(n);
        memcpy(buf + idx, task->get_name(), strlen(task->get_name())); idx += strlen(task->get_name());
        
        const char* p = "\nPid:\t"; memcpy(buf + idx, p, strlen(p)); idx += strlen(p);
        idx += uint_to_str(pid, buf + idx);
        
        const char* s = "\nState:\t"; memcpy(buf + idx, s, strlen(s)); idx += strlen(s);
        const char* state_str = task->is_running() ? "R (running)" : "S (sleeping)";
        memcpy(buf + idx, state_str, strlen(state_str)); idx += strlen(state_str);
        buf[idx++] = '\n';
        
        return idx;
    }

public:
    ProcFileSystem() {}

    // VFS lookup: Amikor a rendszered megnyit egy elérési utat a /proc alatt
    virtual VFS::Node* lookup(const char* path) override {
        ScopedLock guard(m_lock);

        if (strcmp(path, "/meminfo") == 0) {
            return new VFS::Node("meminfo", false, [](char* b, size_t m) { return read_meminfo(b, m); });
        }
        if (strcmp(path, "/version") == 0) {
            return new VFS::Node("version", false, [](char* b, size_t m) { return read_version(b, m); });
        }

        // Dinamikus folyamat-mappák detektálása (pl. /proc/12/status)
        if (path[0] == '/') {
            uint32_t pid = 0;
            size_t i = 1;
            while (path[i] >= '0' && path[i] <= '9') {
                pid = pid * 10 + (path[i] - '0');
                i++;
            }

            if (pid > 0 && strcmp(path + i, "/status") == 0) {
                if (Scheduler::task_exists(pid)) {
                    return new VFS::Node("status", false, [pid](char* b, size_t m) { 
                        return read_task_status(pid, b, m); 
                    });
                }
            }
        }
        return nullptr;
    }

    // Virtuális könyvtár listázás (pl. `ls /proc` parancs futtatásakor)
    virtual size_t readdir(VFS::Node* dir_node, char* buffer, size_t max_entries) override {
        ScopedLock guard(m_lock);
        size_t count = 0;

        // Statikus bejegyzések
        const char* statics[] = { "meminfo", "version" };
        for (const char* name : statics) {
            if (count >= max_entries) return count;
            memcpy(buffer + (count * 32), name, strlen(name) + 1);
            count++;
        }

        // Aktív PID-ek lekérése a schedulertől és hozzáadása a listához [source: 2]
        uint32_t pids[128];
        size_t active_tasks = Scheduler::get_active_pids(pids, 128);
        for (size_t i = 0; i < active_tasks; ++i) {
            if (count >= max_entries) return count;
            
            char pid_str[16];
            size_t len = uint_to_str(pids[i], pid_str);
            pid_str[len] = '\0';

            memcpy(buffer + (count * 32), pid_str, len + 1);
            count++;
        }
        return count;
    }
};

void init_proc_fs() {
    VFS::register_filesystem("proc", new ProcFileSystem());
}

} // namespace Blockos
