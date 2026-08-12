/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "core/core.h"
#include "gpu/gpu.h"
#include "gpu/mem_mgr/mem_mgr.h"
#include "gpu/mem_sys/kern_mem_sys.h"

#include "published/rubin/gr100/hwproject.h"

NvBool
memmgrComprMappingSupported_GR100
(
    MemoryManager    *pMemoryManager,
    NV_ADDRESS_SPACE  addrSpace
)
{
    OBJGPU             *pGpu                = ENG_GET_GPU(pMemoryManager);
    KernelMemorySystem *pKernelMemorySystem = GPU_GET_KERNEL_MEMORY_SYSTEM(pGpu);
    const MEMORY_SYSTEM_STATIC_CONFIG *pMemorySystemConfig =
        kmemsysGetStaticConfig(pGpu, pKernelMemorySystem);

    if (pMemoryManager->bSkipCompressionCheck)
    {
        return NV_TRUE;
    }

    if (pMemorySystemConfig->bDisableCompbitBacking)
    {
        return NV_FALSE;
    }

    return memmgrComprSupported(pMemoryManager, addrSpace);
}

NvBool
memmgrIsFlaSysmemSupported_GR100(OBJGPU *pGpu, MemoryManager *pMemoryManager)
{
    // TODO : Bug 4847324: Check if serial ATS is supported when allowing SYSMEM access.
    return NV_TRUE;
}

NvU8
memmgrGetLocalizedOffset_GR100
(
    OBJGPU *pGpu,
    MemoryManager *pMemoryManager
)
{
    return NV_LOCALIZATION_MODE_BIT_IN_ADDRESS_OFFSET;
}
