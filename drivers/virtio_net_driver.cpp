#include "virtio_net_driver.hpp"
#include "virtio_common.hpp"
#include "virtqueue.hpp"
#include "virtqueue_ops.hpp"
#include "dma.hpp"
#include "virtio_notify.hpp"
#include "virtio_service.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

// Improved TX reclaim: handle used entries robustly and avoid treating invalid ids as fatal.
void virtio_net::reclaim_tx() {
    if (!g_ready) return;
    uint32_t id; uint32_t len;
    // process all used entries currently available
    while (virtqueue_ops::try_dequeue_used(&g_tx_vq, &id, &len)) {
        // Protect against out-of-range ids
        if (id >= g_tx_vq.size) {
            // Unknown id — ignore but continue
            continue;
        }
        uint64_t addr = g_tx_vq.desc[id].addr;
        void* buf = (void*)(UINTN)addr;
        // clear descriptor safely
        virtqueue_ops::set_descriptor(&g_tx_vq, id, 0, 0, 0, 0);
        // If the slot tracked this buffer, recycle it
        if (id < (uint32_t)(sizeof(g_tx_slots)/sizeof(g_tx_slots[0])) && g_tx_slots[id].buf != NULL) {
            if (g_tx_slots[id].buf == buf) {
                tx_pool_push(buf);
                g_tx_slots[id].buf = NULL;
                g_tx_slots[id].submit_tick = 0;
            } else {
                // Buffer mismatch: recycle both just in case
                tx_pool_push(buf);
                tx_pool_push(g_tx_slots[id].buf);
                g_tx_slots[id].buf = NULL;
                g_tx_slots[id].submit_tick = 0;
            }
        } else {
            // Unknown mapping — attempt to recycle buffer
            tx_pool_push(buf);
        }
    }

    // Timeout-based reclaim for pending slots
    uint64_t now = virtio_service::now_ticks();
    const uint64_t TIMEOUT_TICKS = 20; // allow more ticks before force reclaim
    for (uint32_t i = 0; i < g_tx_vq.size && i < (uint32_t)(sizeof(g_tx_slots)/sizeof(g_tx_slots[0])); ++i) {
        if (g_tx_slots[i].buf != NULL) {
            if ((now - g_tx_slots[i].submit_tick) > TIMEOUT_TICKS) {
                // Log a diagnostic
                CHAR16 bufmsg[128];
                UnicodeSPrint(bufmsg, sizeof(bufmsg), L"virtio-net: reclaim timeout on slot %u, forcing recycle\n", i);
                Print(bufmsg);
                // Force reclaim: push buffer to pool and clear descriptor
                tx_pool_push(g_tx_slots[i].buf);
                g_tx_slots[i].buf = NULL;
                g_tx_slots[i].submit_tick = 0;
                virtqueue_ops::set_descriptor(&g_tx_vq, i, 0, 0, 0, 0);
            }
        }
    }
}
