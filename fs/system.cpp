#include "fs/vfs.hpp"
#include "kernel/scheduler.hpp"
#include "kernel/spinlock.hpp"
#include "drivers/pci.hpp"

namespace Blockos {

class SystemFileSystem : public VFS::FileSystem {
private:
    Spinlock m_lock;

    // 1. /system/services dinamikus generálása a systemd_graph állapota alapján [source: 2]
    static size_t read_services_status(char* buf, size_t max) {
        size_t idx = 0;
        const char* header = "--- Blockos Systemd Active Services ---\n";
        memcpy(buf + idx, header, strlen(header));
        idx += strlen(header);

        // Itt a systemd_graph.cpp osztályodból dinamikusan kiolvashatod a szolgáltatásokat [source: 2]
        // Példa makett kiíratás:
        const char* s1 = "[ACTIVE]  udevd.service\n[ACTIVE]  knetworkd.service\n[STANDBY] gui_compositor.service\n";
        size_t s1_len = strlen(s1);
        if (idx + s1_len < max) {
            memcpy(buf + idx, s1, s1_len);
            idx += s1_len;
        }
        return idx;
    }

    // 2. /system/pci_devices topológia kiírása a pci.cpp drivered alapján [source: 2]
    static size_t read_pci_tree(char* buf, size_t max) {
        size_t idx = 0;
        const char* header = "--- PCI Bus Topology ---\n";
        memcpy(buf + idx, header, strlen(header));
        idx += strlen(header);

        // Itt meghívhatod a te saját PCI alrendszeredet [source: 2]
        // pl. for(int i=0; i < PCI::get_device_count(); ++i) { ... }
        const char* mock_pci = "Bus 00 Slot 01.0: VirtIO Network Card (Vendor: 0x1AF4 Device: 0x1000)\n"
                               "Bus 00 Slot 02.0: VirtIO Block Device (Vendor: 0x1AF4 Device: 0x1001)\n"
                               "Bus 00 Slot 03.0: Standard VGA Controller\n";
        size_t pci_len = strlen(mock_pci);
        if (idx + pci_len < max) {
            memcpy(buf + idx, mock_pci, pci_len);
            idx += pci_len;
        }
        return idx;
    }

public:
    SystemFileSystem() {}

    // VFS lookup: Amikor a rendszer megnyit egy virtuális csomópontot a /system alatt [source: 2]
    virtual VFS::Node* lookup(const char* path) override {
        ScopedLock guard(m_lock);

        if (strcmp(path, "/services") == 0) {
            return new VFS::Node("services", false, [](char* b, size_t m) { return read_services_status(b, m); });
        }
        if (strcmp(path, "/pci_devices") == 0) {
            return new VFS::Node("pci_devices", false, [](char* b, size_t m) { return read_pci_tree(b, m); });
        }

        return nullptr;
    }

    // Lehetővé tesszük, hogy a felhasználói space (Ring 3) írni is tudjon a /system fájljaiba [source: 2]
    // Ez a Linux-szintű konfigurációs interfész alapja (pl. echo "restart" > /system/services)
    virtual size_t write(VFS::Node* node, const char* buffer, size_t len) override {
        ScopedLock guard(m_lock);

        if (strcmp(node->get_name(), "services") == 0) {
            if (strncmp(buffer, "reload", 6) == 0) {
                // Itt közvetlenül kiküldhetsz egy eseményt a systemd_parser.cpp-nek [source: 2]
                // SystemdParser::reload_graph();
                return len;
            }
        }
        return -1; // Ismeretlen parancs vagy írásvédett konfiguráció
    }

    virtual size_t readdir(VFS::Node* dir_node, char* buffer, size_t max_entries) override {
        ScopedLock guard(m_lock);
        size_t count = 0;

        const char* entries[] = { "services", "pci_devices" };
        for (const char* name : entries) {
            if (count >= max_entries) return count;
            memcpy(buffer + (count * 32), name, strlen(name) + 1);
            count++;
        }
        return count;
    }
};

void init_system_fs() {
    VFS::register_filesystem("sysfs", [](int disk_id) -> VFS::FileSystem* {
        return new SystemFileSystem();
    });
}

} // namespace Blockos
