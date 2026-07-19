#pragma once
#include <stdint.h>
#include <stddef.h>

// Maximálisan egyidejűleg kezelhető hardvereszközök száma
#define MAX_DEVICES 16

// Eszközkategóriák definíciói
enum DeviceType {
    DEV_TYPE_UNKNOWN,
    DEV_TYPE_STORAGE,   // VirtIO-Blk, Merevlemezek
    DEV_TYPE_NETWORK,   // VirtIO-Net, Hálózati kártyák
    DEV_TYPE_GRAPHICS,  // UEFI Framebuffer, Videókártyák
    DEV_TYPE_INPUT      // PS/2 Egér, Billentyűzet
};

// Egy konkrét eszköz hardveres leíró struktúrája
struct DeviceDescriptor {
    uint32_t   id;
    char       name[32];
    DeviceType type;
    uint64_t   io_base_addr; // MMIO fizikai báziscím a memóriában
    uint8_t    irq_vector;   // CPU megszakítási vektor száma
    bool       is_ready;
};

class DeviceManager {
private:
    DeviceDescriptor device_list[MAX_DEVICES];
    uint32_t         total_devices;

    void local_strcpy(char* dest, const char* src, size_t max_len);

public:
    DeviceManager();

    // Új hardver eszköz regisztrálása a rendszermagba
    bool register_device(const char* name, DeviceType type, uint64_t io_base, uint8_t irq);
    
    // Eszköz keresése kategória alapján
    DeviceDescriptor* find_device_by_type(DeviceType type);
    
    // Hardverek inicializálása és diagnosztikai jelentés kiírása a GUI-ra
    void initialize_all_hardware();
    void show_device_status_report();
};

extern DeviceManager hardware_center;
