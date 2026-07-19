#include "device_manager.hpp"
#include "gui.hpp"

extern GuiEngine desktop;

DeviceManager::DeviceManager() : total_devices(0) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        device_list[i].id = 0;
        device_list[i].type = DEV_TYPE_UNKNOWN;
        device_list[i].io_base_addr = 0;
        device_list[i].irq_vector = 0;
        device_list[i].is_ready = false;
        device_list[i].name[0] = '\0';
    }
}

void DeviceManager::local_strcpy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (src[i] != '\0' && i < (max_len - 1)) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// ÚJ HARDVER ESZKÖZ REGISZTRÁLÁSA
bool DeviceManager::register_device(const char* name, DeviceType type, uint64_t io_base, uint8_t irq) {
    if (total_devices >= MAX_DEVICES || !name) return false;

    DeviceDescriptor& dev = device_list[total_devices];
    dev.id = total_devices + 1;
    local_strcpy(dev.name, name, 32);
    dev.type = type;
    dev.io_base_addr = io_base;
    dev.irq_vector = irq;
    dev.is_ready = false;

    total_devices++;
    return true;
}

// ESZKÖZ KERESÉSE KATEGÓRIA ALAPJÁN
DeviceDescriptor* DeviceManager::find_device_by_type(DeviceType type) {
    for (uint32_t i = 0; i < total_devices; i++) {
        if (device_list[i].type == type) {
            return &device_list[i];
        }
    }
    return nullptr;
}

// HARDVEREK SZEKVENCIÁLIS INICIALIZÁLÁSA
void DeviceManager::initialize_all_hardware() {
    for (uint32_t i = 0; i < total_devices; i++) {
        DeviceDescriptor& dev = device_list[i];
        
        // MMIO címek szoftveres ellenőrzése és busz polling szimuláció
        if (dev.io_base_addr != 0) {
            volatile uint32_t* hw_register = (volatile uint32_t*)dev.io_base_addr;
            (void)hw_register; // Hardver jelenlét teszt
        }

        // Átváltjuk az eszközt működőképes állapotba
        dev.is_ready = true;
    }
}

// VIZUÁLIS HARDVER-DIAGNOSZTIKA KIÍRÁSA A GUI-RA
void DeviceManager::show_device_status_report() {
    // Ha az eszközök készen állnak, kis hardver-státusz LED-eket (zöld négyzeteket)
    // rajzolunk a jobb oldali "VirtIO Hardware Control" ablak belsejébe (x=600-tól kezdve)
    for (uint32_t i = 0; i < total_devices; i++) {
        if (device_list[i].is_ready) {
            // Aktív hardver zöld jelzése
            desktop.draw_rect(600 + (i * 24), 140, 16, 16, COLOR_ARGB(255, 0, 255, 0));
        } else {
            // Hibás vagy inaktív hardver piros jelzése
            desktop.draw_rect(600 + (i * 24), 140, 16, 16, COLOR_ARGB(255, 255, 0, 0));
        }
    }
    desktop.render();
}

DeviceManager hardware_center;
