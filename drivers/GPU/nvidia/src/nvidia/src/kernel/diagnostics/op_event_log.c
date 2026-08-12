/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "diagnostics/op_event_log.h"
#include "nvport/memory.h"

NV_STATUS
opEventLogConstruct(void)
{
    opEventLog.pSpinlock = portSyncSpinlockCreate(portMemAllocatorGetGlobalNonPaged());
    if (opEventLog.pSpinlock == NULL)
    {
        return NV_ERR_NO_MEMORY;
    }

    listInitIntrusive(&opEventLog.cperBufferList);

    return NV_OK;
}

NV_STATUS
opEventLogAppend
(
    NvU8 *pCperBytes,
    NvU32 cperBufferSize
)
{
    CperBuffer *pCperNode = NULL;
    NV_STATUS status = NV_OK;

    if (opEventLog.pSpinlock == NULL)
        return NV_ERR_INVALID_STATE;


    // Create new node outside of spinlock
    pCperNode = portMemAllocNonPaged(sizeof(CperBuffer));
    if (pCperNode == NULL)
    {
        return NV_ERR_NO_MEMORY;
    }

    pCperNode->pCperBuffer = pCperBytes;
    pCperNode->size = cperBufferSize;

    portSyncSpinlockAcquire(opEventLog.pSpinlock);
    listAppendExisting(&opEventLog.cperBufferList, pCperNode);
    portSyncSpinlockRelease(opEventLog.pSpinlock);

    return status;
}

void
opEventLogDestruct(void)
{
    CperBuffer *pCperNode = NULL;
    void *pItem;

    if (opEventLog.pSpinlock == NULL)
        return;

    while (NV_TRUE)
    {
        portSyncSpinlockAcquire(opEventLog.pSpinlock);

        pItem = listHead(&opEventLog.cperBufferList);
        if (pItem == NULL)
        {
            portSyncSpinlockRelease(opEventLog.pSpinlock);
            break;
        }

        pCperNode = (CperBuffer *)pItem;
        listRemove(&opEventLog.cperBufferList, pCperNode);

        portSyncSpinlockRelease(opEventLog.pSpinlock);

        // Note: nvport memory operations must be done outside the spinlock
        if (pCperNode->pCperBuffer != NULL)
        {
            portMemFree(pCperNode->pCperBuffer);
        }
        portMemFree(pCperNode);

    }
    listDestroy(&opEventLog.cperBufferList);

    portSyncSpinlockDestroy(opEventLog.pSpinlock);
    opEventLog.pSpinlock = NULL;
}
