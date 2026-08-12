/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "kernel/diagnostics/nocat_event_list.h"

#include "rmapi/client.h"
#include "rmapi/event_api.h"
#include "kernel/rmapi/event_buffer.h"
#include "gpu_mgr/gpu_mgr.h"
#include "nvlimits.h"
#include "class/cl90cd.h"
#include "class/cl90cdtypes.h"
#include "core/locks.h"
#include "os/os.h"

typedef struct NOCAT_EVENTBUFFER_BIND_POINT
{
    struct EventBuffer *pEventBuffer;
    NvHandle            hClient;
    NvHandle            hNotifier;
    NvHandle            hEventBuffer;
} NOCAT_EVENTBUFFER_BIND_POINT;

/*
 * Very small per-GPU list of bindpoints.
 * For simplicity and minimalism keep a single forward list per GPU.
 */
typedef struct NOCAT_EVENTBUFFER_BIND_LIST
{
    NOCAT_EVENTBUFFER_BIND_POINT *items[8];
    NvU32                         count;
} NOCAT_EVENTBUFFER_BIND_LIST;

/* Global bind lists - force single instance across all contexts */
static NOCAT_EVENTBUFFER_BIND_LIST g_nocatBindLists[NV_MAX_DEVICES];

static NOCAT_EVENTBUFFER_BIND_LIST *
_nocatGetBindList(OBJGPU *pGpu)
{
    NOCAT_EVENTBUFFER_BIND_LIST *pList = &g_nocatBindLists[pGpu->gpuInstance];
    return pList;
}

NV_STATUS nocatAddBindpoint(
    OBJGPU *pGpu,
    RsClient *pClient,
    RsResourceRef *pEventBufferRef,
    NvHandle hNotifier)
{
    NV_STATUS status = NV_OK;
    NOCAT_EVENTBUFFER_BIND_LIST *pList = _nocatGetBindList(pGpu);
    EventBuffer *pEventBuffer = dynamicCast(pEventBufferRef->pResource, EventBuffer);

    NV_ASSERT_OR_RETURN(rmapiLockIsOwner() && rmGpuLockIsOwner(), NV_ERR_INVALID_LOCK_STATE);
    NV_ASSERT_OR_RETURN(pEventBuffer != NULL, NV_ERR_INVALID_ARGUMENT);

    // Idempotent bind: if already present for this buffer, do nothing
    for (NvU32 i = 0; i < pList->count; ++i)
    {
        if (pList->items[i] != NULL && pList->items[i]->pEventBuffer == pEventBuffer)
        {
            return NV_OK;
        }
    }

    if (pList->count >= NV_ARRAY_ELEMENTS(pList->items))
    {
        NV_PRINTF(LEVEL_ERROR, "pList->count=%d\n", pList->count);
        return NV_ERR_INSUFFICIENT_RESOURCES;
    }

    NOCAT_EVENTBUFFER_BIND_POINT *pBind = portMemAllocNonPaged(sizeof(*pBind));
    NV_ASSERT_OR_RETURN(pBind != NULL, NV_ERR_INSUFFICIENT_RESOURCES);

    pBind->pEventBuffer  = pEventBuffer;
    pBind->hClient       = pClient->hClient;
    pBind->hNotifier     = hNotifier;
    pBind->hEventBuffer  = pEventBufferRef->hResource;

    // Register OS notification for NOCAT record type
    status = registerEventNotification(&pEventBuffer->pListeners,
                pClient,
                hNotifier,
                pBind->hEventBuffer,
                NV_EVENT_BUFFER_RECORD_TYPE_NOCAT_NOTIFY | NV01_EVENT_WITHOUT_EVENT_DATA,
                NV_EVENT_BUFFER_BIND,
                pEventBuffer->producerInfo.notificationHandle,
                NV_FALSE);
    if (status != NV_OK)
    {
        NV_PRINTF(LEVEL_ERROR, "nocatAddBindpoint: registerEventNotification failed: 0x%x\n", status);
        portMemFree(pBind);
        return status;
    }

    pList->items[pList->count++] = pBind;
    return status;
}

void nocatRemoveAllBindpoints(EventBuffer *pEventBuffer)
{
    GPU_MASK mask;
    NvU32 idx = 0;
    OBJGPU *pGpu;

    gpumgrGetGpuAttachInfo(NULL, &mask);
    while ((pGpu = gpumgrGetNextGpu(mask, &idx)) != NULL)
    {
        NOCAT_EVENTBUFFER_BIND_LIST *pList = _nocatGetBindList(pGpu);
        for (NvU32 i = 0; i < pList->count; )
        {
            if (pList->items[i] != NULL && pList->items[i]->pEventBuffer == pEventBuffer)
            {
                // Unregister OS notification for this bindpoint
                unregisterEventNotificationWithData(&pEventBuffer->pListeners,
                        pList->items[i]->hClient,
                        pList->items[i]->hNotifier,
                        pList->items[i]->hEventBuffer,
                        NV_TRUE,
                        pEventBuffer->producerInfo.notificationHandle);

                portMemFree(pList->items[i]);
                pList->items[i] = pList->items[pList->count - 1];
                pList->items[pList->count - 1] = NULL;
                pList->count--;
                continue;
            }
            i++;
        }
    }
}

void nocatRemoveAllBindpointsForGpu(OBJGPU *pGpu)
{
    NOCAT_EVENTBUFFER_BIND_LIST *pList = _nocatGetBindList(pGpu);
    for (NvU32 i = 0; i < pList->count; ++i)
    {
        if (pList->items[i] != NULL)
        {
            EventBuffer *pEventBuffer = pList->items[i]->pEventBuffer;
            if (pEventBuffer != NULL)
            {
                unregisterEventNotificationWithData(&pEventBuffer->pListeners,
                        pList->items[i]->hClient,
                        pList->items[i]->hNotifier,
                        pList->items[i]->hEventBuffer,
                        NV_TRUE,
                        pEventBuffer->producerInfo.notificationHandle);
            }
            portMemFree(pList->items[i]);
            pList->items[i] = NULL;
        }
    }
    pList->count = 0;
}

typedef struct NOCAT_NOTIFY_PAYLOAD
{
    NvU32 posted;
    NvU32 recType;
    NV_DECLARE_ALIGNED(NvU64 timeStamp, 8);
} NOCAT_NOTIFY_PAYLOAD;

void nocatEventBufferAddNotify(
    OBJGPU *pGpu,
    NvU32 posted,
    NvU32 recType,
    NvU64 timeStamp)
{
    NOCAT_EVENTBUFFER_BIND_LIST *pList = _nocatGetBindList(pGpu);

    /* Build inline payload */
    NOCAT_NOTIFY_PAYLOAD payload;
    payload.posted   = posted;
    payload.recType  = recType;
    payload.timeStamp = timeStamp;

    for (NvU32 i = 0; i < pList->count; ++i)
    {
        NOCAT_EVENTBUFFER_BIND_POINT *pBind = pList->items[i];
        if (pBind == NULL || pBind->pEventBuffer == NULL)
            continue;

        EVENT_BUFFER_PRODUCER_DATA ev = {0};
        NvBool bNotify = NV_FALSE;
        NvP64 notifyHandle = 0;

        ev.pPayload    = NV_PTR_TO_NvP64(&payload);
        ev.payloadSize = sizeof(payload);
        ev.pVardata    = NV_PTR_TO_NvP64(NULL);
        ev.vardataSize = 0;

        /* Use dedicated NOCAT notify record type */
        const NvU32 recordType = NV_EVENT_BUFFER_RECORD_TYPE_NOCAT_NOTIFY;

        NV_STATUS status = eventBufferAdd(pBind->pEventBuffer, &ev, recordType, &bNotify, &notifyHandle);
        if (status != NV_OK)
        {
            NV_PRINTF(LEVEL_ERROR, "nocatEventBufferAddNotify: eventBufferAdd failed: 0x%x\n", status);
        }
        else if (bNotify && notifyHandle)
        {
            osEventNotification(pGpu, pBind->pEventBuffer->pListeners, recordType, &ev, 0);
            pBind->pEventBuffer->bNotifyPending = NV_TRUE;
        }
    }
}


