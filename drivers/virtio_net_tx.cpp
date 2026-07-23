#include "virtio_net_tx.hpp"
#include "virtio_net_driver.hpp"
#include "virtqueue_ops.hpp"
#include "virtio_service.hpp"
#include <efi.h>
#include <efilib.h>

// Walk the used ring and recycle any buffers referenced there. Useful after heavy stress
// or when we suspect descriptors were left uncleared.
void virtio_net::force_reclaim_all() {
    if (!g_ready) return;
    uint32_t id; uint32_t len;
    // Repeatedly try to drain used entries
    int safety = 0;
    while (virtqueue_ops::try_dequeue_used(&g_tx_vq, &id, &len)) {
        if (id >= g_tx_vq.size) continue;
        uint64_t addr = g_tx_vq.desc[id].addr;
        void* buf = (void*)(UINTN)addr;
        virtqueue_ops::set_descriptor(&g_tx_vq, id, 0, 0, 0, 0);
        // Try to recycle
        tx_pool_push(buf);
        if (++safety > 1024) break;
    }

    // Also scan our slot table and forcibly recycle any outstanding buffers
    uint64_t now = virtio_service::now_ticks();
    for (uint32_t i = 0; i < g_tx_vq.size && i < (uint32_t)(sizeof(g_tx_slots)/sizeof(g_tx_slots[0])); ++i) {
        if (g_tx_slots[i].buf != NULL) {
            CHAR16 bufmsg[128];
            UnicodeSPrint(bufmsg, sizeof(bufmsg), L"virtio-net: force_reclaim_all recycling slot %u\n", i);
            Print(bufmsg);
            tx_pool_push(g_tx_slots[i].buf);
            g_tx_slots[i].buf = NULL;
            g_tx_slots[i].submit_tick = 0;
            virtqueue_ops::set_descriptor(&g_tx_vq, i, 0, 0, 0, 0);
        }
    }
}
