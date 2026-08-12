/*
 * SPDX-FileCopyrightText: Copyright (c) 1993-2015 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/* this file contains dma push buffer inlined routines */

#ifndef __NVKMS_DMA_H__
#define __NVKMS_DMA_H__

#include <nvctassert.h>

#include "nvkms-types.h"
#include "nvkms-utils.h"

/* declare prototypes: */
void nvDmaKickoffEvo(NVEvoChannelPtr);

void nvEvoMakeRoom(NVEvoChannelPtr pChannel, NvU32 count);
void nvWriteEvoCoreNotifier(const NVDispEvoRec *, NvU32 offset, NvU32 value);

NvBool nvEvoIsCoreNotifierComplete(NVDispEvoPtr pDispEvo,
                                   NvU32 offset, NvU32 done_base_bit,
                                   NvU32 done_extent_bit,
                                   NvU32 done_false_value);
void nvEvoWaitForCoreNotifier(const NVDispEvoRec *pDispEvo, NvU32 offset,
                              NvU32 done_base_bit,
                              NvU32 done_extent_bit, NvU32 done_false_value);
void nvEvoSetSubdeviceMask(NVEvoChannelPtr pChannel, NvU32 mask);

NvU32 nvEvoReadCRC32Notifier(volatile NvU32 *pCRC32Notifier,
                             NvU32 entry_stride,
                             NvU32 entry_count,
                             NvU32 status_offset,
                             NvU32 field_count,
                             NvU32 flag_count,
                             const CRC32NotifierEntryRec *field_info,
                             const CRC32NotifierEntryFlags *flag_info);
void nvEvoResetCRC32Notifier(volatile NvU32 *pCRC32Notifier,
                             NvU32 offset,
                             NvU32 reset_base_bit,
                             NvU32 reset_value);
NvBool nvEvoWaitForCRC32Notifier(const NVDevEvoPtr pDevEvo,
                                 volatile NvU32 *pCRC32Notifier,
                                 NvU32 offset,
                                 NvU32 done_base_bit,
                                 NvU32 done_extent_bit,
                                 NvU32 done_value);

static inline void nvDmaStorePioMethod(
    void *pBase, NvU32 offset, NvU32 value)
{
    NvU32 *ptr = ((NvU32 *)pBase) + (offset/sizeof(NvU32));

    /*
     * Use gcc built-in atomic store to ensure the write happens exactly once
     * and to ensure ordering.  We can use the weaker "relaxed" model because we
     * separately use appropriate fencing on anything that needs to preceed this
     * write.
     */
    __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

static inline NvU32 nvDmaLoadPioMethod(
    const void *pBase, NvU32 offset)
{
    const NvU32 *ptr = ((const NvU32 *)pBase) + (offset/sizeof(NvU32));

    /*
     * Use gcc built-in atomic load to ensure the read happens exactly once and
     * to ensure ordering.  We use the "acquire" model to ensure anything after
     * this read doesn't get reordered earlier than this read.  (E.g., we don't
     * want any writes to the pushbuffer that are waiting on GET to advance to
     * get reordered before this read, potentially clobbering the pushbuffer
     * before it's been read.)
     */
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

static inline void nvDmaSetEvoMethodData(
    NVEvoChannelPtr pChannel,
    const NvU32 data)
{
    *(pChannel->pb.buffer) = data;
    pChannel->pb.buffer++;
}

static inline void nvDmaSetEvoMethodDataU64(
    NVEvoChannelPtr pChannel,
    const NvU64 data)
{
    nvDmaSetEvoMethodData(pChannel, NvU64_HI32(data));
    nvDmaSetEvoMethodData(pChannel, NvU64_LO32(data));
}


/*
 * Update the state tracked in updateState to indicate that pChannel has
 * pending methods and requires an update/kickoff.
 */
static inline void nvUpdateUpdateState(NVDevEvoPtr pDevEvo,
                                       NVEvoUpdateState *updateState,
                                       const NVEvoChannel *pChannel)
{
    updateState->subdev[0].channelMask |= pChannel->channelMask;
}

/*
 * Update the state tracked in updateState to indicate that pChannel has
 * pending WindowImmediate methods.
 */
static inline void nvWinImmChannelUpdateState(NVDevEvoPtr pDevEvo,
                                              NVEvoUpdateState *updateState,
                                              const NVEvoChannel *pChannel)
{
    updateState->subdev[0].winImmChannelMask |= pChannel->channelMask;
}

/*
 * Update the state tracked in updateState to prevent pChannel from
 * interlocking with the core channel on the next UPDATE.
 */
static inline
void nvDisableCoreInterlockUpdateState(NVDevEvoPtr pDevEvo,
                                       NVEvoUpdateState *updateState,
                                       const NVEvoChannel *pChannel)
{
    updateState->subdev[0].noCoreInterlockMask |= pChannel->channelMask;
}

// These macros verify that the values used in the methods fit
// into the defined ranges.
#define ASSERT_DRF_NUM(d, r, f, n) \
    nvAssert(!(~DRF_MASK(NV ## d ## r ## f) & (n)))

// From resman nv50/dev_disp.h
#define NV_UDISP_DMA_OPCODE                                   31:29 /* RWXUF */
#define NV_UDISP_DMA_OPCODE_METHOD                       0x00000000 /* RW--V */
#define NV_UDISP_DMA_METHOD_COUNT                             27:18 /* RWXUF */
#define NV_UDISP_DMA_METHOD_OFFSET                             15:2 /* RWXUF */

// Start an EVO method.
static inline void nvDmaSetStartEvoMethod(
    NVEvoChannelPtr pChannel,
    NvU32 method,
    NvU32 count)
{
    NVDmaBufferEvoPtr p = &pChannel->pb;

    // We add 1 to the count for the method header.
    const NvU32 countPlusHeader = count + 1;

    const NvU32 methodDwords = method >> 2;

    nvAssert((method & 0x3) == 0);

    ASSERT_DRF_NUM(_UDISP, _DMA, _METHOD_COUNT,  count);
    ASSERT_DRF_NUM(_UDISP, _DMA, _METHOD_OFFSET, methodDwords);

    if (p->fifo_free_count <= countPlusHeader) {
        nvEvoMakeRoom(pChannel, countPlusHeader);
    }

    nvDmaSetEvoMethodData(pChannel,
        DRF_DEF(_UDISP, _DMA, _OPCODE,        _METHOD) |
        DRF_NUM(_UDISP, _DMA, _METHOD_COUNT,  count) |
        DRF_NUM(_UDISP, _DMA, _METHOD_OFFSET, methodDwords));

    p->fifo_free_count -= countPlusHeader;
}

static inline NvBool nvIsUpdateStateEmpty(const NVDevEvoRec *pDevEvo,
                                          const NVEvoUpdateState *updateState)
{
    NvU32 sd;
    for (sd = 0; sd < pDevEvo->numSubDevices; sd++) {
        if (updateState->subdev[sd].channelMask != 0x0) {
            return FALSE;
        }
    }
    return TRUE;
}

NvBool nvEvoPollForEmptyChannel(NVEvoChannelPtr pChannel, NvU32 sd,
                                NvU64 *pStartTime, const NvU32 timeout);

#endif /* __NVKMS_DMA_H__ */
