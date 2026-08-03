#include "virtio_input.hpp"

#include "virtio_common.hpp"
#include "dma.hpp"

#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <string.h>

namespace virtio_input {

static VirtioInputEvent* g_event_buffer = nullptr;

bool init() {
    Print((CHAR16*)L"virtio-input: initializing\n");

    /*
     * TODO:
     * Később itt történik:
     *  - VirtIO input device keresése
     *  - virtqueue létrehozása
     *  - event buffer DMA-val történő lefoglalása
     */

    g_event_buffer = static_cast<VirtioInputEvent*>(
        dma::alloc(sizeof(VirtioInputEvent), 4096)
    );

    if (!g_event_buffer) {
        Print((CHAR16*)L"virtio-input: DMA allocation failed\n");
        return false;
    }

    memset(g_event_buffer, 0, sizeof(VirtioInputEvent));

    Print((CHAR16*)L"virtio-input: initialized\n");

    return true;
}

bool poll(VirtioInputEvent* event) {
    if (!event) {
        return false;
    }

    if (!g_event_buffer) {
        return false;
    }

    /*
     * TODO:
     * Itt kell majd ellenőrizni a VirtIO input
     * virtqueue-t és átvenni a következő eseményt.
     */

    return false;
}

int8_t read_byte_nonblocking() {
    VirtioInputEvent event;

    if (!poll(&event)) {
        return -1;
    }

    if (event.value < -128 || event.value > 127) {
        return -1;
    }

    return static_cast<int8_t>(event.value);
}

} // namespace virtio_input
