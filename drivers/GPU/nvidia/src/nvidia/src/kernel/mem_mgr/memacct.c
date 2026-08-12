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

#include "mem_mgr/memacct.h"
#include "gpu/mem_mgr/mem_mgr.h"
#include "rmapi/client.h"
#include "gpu/gpu.h"

#define MEMACCT_DEBUG_LOG_LEVEL LEVEL_SILENT

typedef struct ClientGroupLimits
{
    NvLength SoftLimit;
    NvLength HardLimit;
    NvLength Available;
} ClientGroupLimits;

// map from cgroup id to the limits/stats for that specific group
MAKE_MAP(ClientGroupMapType, ClientGroupLimits);

typedef struct GpuRegion
{
    ClientGroupMapType clientGroupMap;
    void *osRegion;
} GpuRegion;

// map region id (gpuId) to region's group limit map
MAKE_MAP(GpuRegionMapType, GpuRegion);

static struct {
    GpuRegionMapType GpuRegionMap;
    NvCgroupImpl impl;
    PORT_MUTEX *mutex;
} g_memacct;

struct MemoryCharge
{
    NvU32 gpuId;
    ClientGroupID cligrp;
    NvLength size;
    void *osPool;
    int refCount;
};

static NV_STATUS memacctTryChargeOs(void *osRegion, NvLength size, MemoryCharge **ppCharge)
{
    *ppCharge = NULL;

    void *pPool;
    NV_STATUS status = osMemacctTryCharge(osRegion, size, &pPool);
    if (status != NV_OK)
    {
        NV_PRINTF(LEVEL_NOTICE, "charge %llu osRegion 0x%016llx rejected\n", size, (NvU64)osRegion);
        return NV_ERR_RESOURCE_ACCOUNTING_HARD_LIMIT_EXCEEDED;
    }

    NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "charge %llu for osRegion 0x%016llx pool 0x%016llx ok\n", size, (NvU64)osRegion, (NvU64)pPool);

    MemoryCharge *pCharge = portMemAllocNonPaged(sizeof *pCharge);
    if (pCharge == NULL)
    {
        NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "release %llu for pool 0x%016llx (charge structure allocation failure)\n",
            size, (NvU64)pPool);
        osMemacctReleaseCharge(pPool, size);
        return NV_ERR_NO_MEMORY;
    }

    pCharge->size = size;
    pCharge->osPool = pPool;
    pCharge->refCount = 1;
    *ppCharge = pCharge;
    return NV_OK;
}

static NV_STATUS memacctTryChargeInternalLocked(GpuRegion *pRegion, ClientGroupID cligrp, NvU32 gpuId, NvLength size, MemoryCharge *pCharge)
{
    ClientGroupLimits *pLimits = mapFind(&pRegion->clientGroupMap, (NvU64)cligrp);
    if (pLimits == NULL)
    {
        NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "region %u no limits set for group %016llx\n", gpuId,
            (NvU64)cligrp);
        return NV_ERR_INVALID_LIMIT;
    }

    if (size > pLimits->Available)
    {
        NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "region %u group %016llx request %llu over limit, available %llu/%llu\n",
            gpuId, (NvU64)cligrp, size, pLimits->Available, pLimits->HardLimit);
        return NV_ERR_RESOURCE_ACCOUNTING_HARD_LIMIT_EXCEEDED;
    }

    pCharge->size = size;
    pCharge->gpuId = gpuId;
    pCharge->cligrp = cligrp;
    pCharge->refCount = 1;
    pLimits->Available -= size;

    NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "region %u group %016llx charged %llu total %llu/%llu\n", gpuId,
        (NvU64)cligrp, size, pLimits->HardLimit - pLimits->Available, pLimits->HardLimit);

    return NV_OK;
}

void memacctIncrementChargeRefCount(MemoryCharge *pCharge)
{
    if (pCharge == NULL)
        return;

    pCharge->refCount++;
}

NV_STATUS memacctTryCharge(ClientGroupID cligrp, NvU32 gpuId, NvLength size, MemoryCharge **ppCharge)
{
    if (g_memacct.impl == CGROUP_IMPL_NONE)
        return NV_OK;

    NV_STATUS status;
    NvBool doFree = NV_FALSE;

    // attempt to pre-allocate charge structure outside the lock for the fallback/misc cgroup case
    if (g_memacct.impl == CGROUP_IMPL_FALLBACK)
    {
        *ppCharge = portMemAllocNonPaged(sizeof **ppCharge);
        if (*ppCharge == NULL)
        {
            NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "region %u group %016llx request %llu charge structure allocation failure\n",
                gpuId, (NvU64)cligrp, size);
            return NV_ERR_NO_MEMORY;
        }
    }

    portSyncMutexAcquire(g_memacct.mutex);
    GpuRegion *pRegion = mapFind(&g_memacct.GpuRegionMap, gpuId);
    if (pRegion == NULL)
    {
        NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "region %u not found\n", gpuId);
        *ppCharge = NULL;
        status = NV_OK;
        goto done;
    }

    if (g_memacct.impl == CGROUP_IMPL_OS)
    {
        void *osRegion = pRegion->osRegion;
        portSyncMutexRelease(g_memacct.mutex);
        return memacctTryChargeOs(osRegion, size, ppCharge);
    }
    else
    {
        status = memacctTryChargeInternalLocked(pRegion, cligrp, gpuId, size, *ppCharge);
        if (status != NV_OK)
        {
            doFree = NV_TRUE;
            // not really an error case, just need to clean up the pre-allocation
            if (status == NV_ERR_INVALID_LIMIT)
                status = NV_OK;
        }
    }
done:
    portSyncMutexRelease(g_memacct.mutex);
    if (doFree)
    {
        portMemFree(*ppCharge);
        *ppCharge = NULL;
    }
    return status;
}

static void memacctReleaseChargeInternal(MemoryCharge *pCharge)
{
    portSyncMutexAcquire(g_memacct.mutex);
    GpuRegion *pRegion = mapFind(&g_memacct.GpuRegionMap, pCharge->gpuId);
    if (pRegion == NULL)
        goto done;

    ClientGroupLimits *pLimits = mapFind(&pRegion->clientGroupMap, (NvU64)pCharge->cligrp);
    if (pLimits == NULL)
        goto done;

    pLimits->Available += pCharge->size;
    NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "region %u group %016llx released %llu total %llu/%llu\n",
        pCharge->gpuId, (NvU64)pCharge->cligrp, pCharge->size,
        pLimits->HardLimit - pLimits->Available, pLimits->HardLimit);
done:
    portSyncMutexRelease(g_memacct.mutex);
}

void memacctReleaseCharge(MemoryCharge *pCharge)
{
    if (g_memacct.impl == CGROUP_IMPL_NONE)
        return;

    if (pCharge == NULL)
        return;

    if (--pCharge->refCount)
        return;

    if (g_memacct.impl == CGROUP_IMPL_OS)
    {
        NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "release %llu for pool 0x%016llx\n", pCharge->size,
            (NvU64)pCharge->osPool);
        osMemacctReleaseCharge(pCharge->osPool, pCharge->size);
    }
    else
    {
        memacctReleaseChargeInternal(pCharge);
    }

    portMemFree(pCharge);
}

NV_STATUS memacctGetLimits(ClientGroupID cligrp, NvU32 gpuId, NvLength *pSoftLimit, NvLength *pHardLimit, NvLength *pCurrent)
{
    if (g_memacct.impl != CGROUP_IMPL_FALLBACK)
        return NV_ERR_NOT_SUPPORTED;

    NV_STATUS status;

    portSyncMutexAcquire(g_memacct.mutex);
    GpuRegion *pRegion = mapFind(&g_memacct.GpuRegionMap, gpuId);
    if (pRegion == NULL)
    {
        status = NV_ERR_INVALID_INDEX;
        goto cleanup;
    }

    ClientGroupLimits *pLimits = mapFind(&pRegion->clientGroupMap, (NvU64)cligrp);
    if (pLimits == NULL)
    {
        status = NV_ERR_INVALID_INDEX;
        goto cleanup;
    }

    *pSoftLimit = pLimits->SoftLimit;
    *pHardLimit = pLimits->HardLimit;
    *pCurrent = pLimits->HardLimit - pLimits->Available;
    status = NV_OK;

cleanup:
    portSyncMutexRelease(g_memacct.mutex);
    return status;
}

NV_STATUS memacctSetLimits(ClientGroupID cligrp, NvU32 gpuId, NvLength softlimit, NvLength hardlimit)
{
    if (g_memacct.impl != CGROUP_IMPL_FALLBACK)
        return NV_ERR_NOT_SUPPORTED;

    NV_STATUS status;

    portSyncMutexAcquire(g_memacct.mutex);
    GpuRegion *pRegion = mapFind(&g_memacct.GpuRegionMap, gpuId);
    if (pRegion == NULL)
    {
        status = NV_ERR_INVALID_INDEX;
        goto cleanup;
    }

    ClientGroupLimits *pLimits = mapFind(&pRegion->clientGroupMap, (NvU64)cligrp);
    if (pLimits != NULL)
    {
        NvLength used = pLimits->HardLimit - pLimits->Available;
        if (used > hardlimit)
        {
            NV_PRINTF(LEVEL_ERROR, "region %u group %016llx set limit error - requested limit %llu is lower than current allocation %llu\n",
                gpuId, (NvU64)cligrp, hardlimit, used);

            status = NV_ERR_INVALID_LIMIT;
            goto cleanup;
        }
        pLimits->Available = hardlimit - used;
    }
    else
    {
        pLimits = mapInsertNew(&pRegion->clientGroupMap, (NvU64)cligrp);
        pLimits->Available = hardlimit;
    }

    pLimits->SoftLimit = softlimit;
    pLimits->HardLimit = hardlimit;
    status = NV_OK;

    NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "region %u group %016llx setting limit %llu\n", gpuId,
        (NvU64)cligrp, hardlimit);

cleanup:
    portSyncMutexRelease(g_memacct.mutex);
    return status;
}

static void memacctOneTimeSetup(void)
{
    g_memacct.impl = osCgroupImplementation();
    if ((g_memacct.mutex == NULL) && (g_memacct.impl != CGROUP_IMPL_NONE))
    {
        PORT_MEM_ALLOCATOR *allocator = portMemAllocatorGetGlobalNonPaged();
        mapInit(&g_memacct.GpuRegionMap, allocator);
        g_memacct.mutex = portSyncMutexCreate(allocator);
    }
}

NV_STATUS memacctInitGpuInfo(OBJGPU *pGpu)
{
    memacctOneTimeSetup();

    if (g_memacct.impl == CGROUP_IMPL_NONE)
        return NV_OK;

    NV_ASSERT_OR_RETURN(gpumgrIsSafeToReadGpuInfo(), NV_ERR_INVALID_LOCK_STATE);
    NvU64 id = pGpu->gpuId;

    MemoryManager *pMemoryManager = GPU_GET_MEMORY_MANAGER(pGpu);
    NvU64 size = memmgrGetUsableMemSizeMB(pGpu, pMemoryManager) * 1024 * 1024;

    NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "creating new memacct region %llu size %llu\n", id, size);

    void *osRegion = NULL;
    if (g_memacct.impl == CGROUP_IMPL_OS)
    {
        osRegion = osCgroupRegisterRegion(pGpu, size);
    }

    portSyncMutexAcquire(g_memacct.mutex);

    GpuRegion *pRegion = mapInsertNew(&g_memacct.GpuRegionMap, id);
    if (g_memacct.impl == CGROUP_IMPL_OS)
    {
        pRegion->osRegion = osRegion;
    }
    else
    {
        mapInit(&pRegion->clientGroupMap, portMemAllocatorGetGlobalNonPaged());
    }

    portSyncMutexRelease(g_memacct.mutex);
    return NV_OK;
}

void memacctRemoveGpu(OBJGPU *pGpu)
{
    if (g_memacct.impl == CGROUP_IMPL_NONE)
        return;

    if (pGpu == NULL)
        return;

    void *osRegion = NULL;

    NV_ASSERT_OR_RETURN_VOID(gpumgrIsSafeToReadGpuInfo());

    portSyncMutexAcquire(g_memacct.mutex);
    GpuRegion *pRegion = mapFind(&g_memacct.GpuRegionMap, pGpu->gpuId);
    if (pRegion == NULL)
        goto cleanup;

    NV_PRINTF(MEMACCT_DEBUG_LOG_LEVEL, "removing memacct region %u\n", pGpu->gpuId);

    if (g_memacct.impl == CGROUP_IMPL_OS)
        osRegion = pRegion->osRegion;
    else
        mapClear(&pRegion->clientGroupMap);
    mapRemove(&g_memacct.GpuRegionMap, pRegion);

cleanup:
    portSyncMutexRelease(g_memacct.mutex);
    if (osRegion)
        osCgroupUnregisterRegion(osRegion);

    if (mapCount(&g_memacct.GpuRegionMap) == 0)
    {
        mapClear(&g_memacct.GpuRegionMap);
        portSyncMutexDestroy(g_memacct.mutex);
        g_memacct.mutex = NULL;
    }
    return;
}
