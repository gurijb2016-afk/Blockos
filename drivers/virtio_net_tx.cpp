#include "virtio_net_tx.hpp"
#include "virtio_net_driver.hpp"
#include "virtio_net_internal.hpp"

#include "virtqueue_ops.hpp"
#include "virtio_service.hpp"

extern "C" {
#include <efi.h>
}
extern "C" {
#include <efilib.h>
}
#include <stdint.h>
#include <stddef.h>

// -----------------------------------------------------------------------------
// Force reclaim all TX buffers
// -----------------------------------------------------------------------------
//
// Drains the VirtIO TX used ring and recycles all tracked TX buffers.
//
// This function is intended as a recovery/safety mechanism after heavy network
// traffic or when descriptors may have remained pending.
// -----------------------------------------------------------------------------

void virtio_net::force_reclaim_all()
{
    if (!g_ready)
        return;

    uint32_t id = 0;
    uint32_t len = 0;

    // Prevent an unexpected broken used ring from looping forever.
    uint32_t safety = 0;
    constexpr uint32_t MAX_USED_ENTRIES = 1024;

    // -------------------------------------------------------------------------
    // 1. Drain completed descriptors from the used ring
    // -------------------------------------------------------------------------

    while (virtqueue_ops::try_dequeue_used(
        &g_tx_vq,
        &id,
        &len))
    {
        if (++safety > MAX_USED_ENTRIES)
            break;

        // Never access an invalid descriptor.
        if (id >= g_tx_vq.size)
            continue;

        // Save the buffer address before clearing the descriptor.
        const uint64_t addr =
            g_tx_vq.desc[id].addr;

        void* buf =
            reinterpret_cast<void*>(
                static_cast<UINTN>(addr)
            );

        // Clear descriptor.
        virtqueue_ops::set_descriptor(
            &g_tx_vq,
            id,
            0,
            0,
            0,
            0
        );

        // Recycle the completed buffer.
        if (buf != nullptr)
            tx_pool_push(buf);

        // Clear software tracking for this descriptor.
        if (id < 256) {
            g_tx_slots[id].buf = nullptr;
            g_tx_slots[id].submit_tick = 0;
        }
    }

    // -------------------------------------------------------------------------
    // 2. Force-reclaim buffers still present in the TX slot table
    // -------------------------------------------------------------------------

    const uint32_t slot_count = 256;

    uint32_t max_slots = g_tx_vq.size;

    if (max_slots > slot_count)
        max_slots = slot_count;

    for (uint32_t i = 0; i < max_slots; ++i)
    {
        void* buf = g_tx_slots[i].buf;

        if (buf == nullptr)
            continue;

        // Diagnostic message.
        CHAR16 bufmsg[128];

        UnicodeSPrint(
            bufmsg,
            sizeof(bufmsg),
            (CHAR16*)L"virtio-net: force reclaim slot %u\n",
            i
        );

        Print(bufmsg);

        // Return buffer to the TX pool.
        tx_pool_push(buf);

        // Clear software tracking.
        g_tx_slots[i].buf = nullptr;
        g_tx_slots[i].submit_tick = 0;

        // Clear descriptor.
        virtqueue_ops::set_descriptor(
            &g_tx_vq,
            i,
            0,
            0,
            0,
            0
        );
    }
}
