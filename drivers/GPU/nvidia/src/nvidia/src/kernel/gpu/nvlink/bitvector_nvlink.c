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

#define NVOC_KERNEL_NVLINK_H_PRIVATE_ACCESS_ALLOWED
#include "kernel/gpu/nvlink/bitvector_nvlink.h"

/*!
 * @copydoc convertMaskToBitVector
 */
NV_STATUS
convertMaskToBitVector(NvU64 inputLinkMask, NVLINK_BIT_VECTOR *pLocalLinkMask)
{
    NV_STATUS status;

    if (pLocalLinkMask == NULL)
    {
        return NV_ERR_INVALID_ARGUMENT;
    }

    status = bitVectorClrAll(pLocalLinkMask);
    if (status != NV_OK)
    {
        return status;
    }

    status = bitVectorFromRaw(pLocalLinkMask, &inputLinkMask, sizeof(inputLinkMask));
    if (status != NV_OK)
    {
        return status;
    }

    return NV_OK;
}

/*!
 * @copydoc convertBitVectorToLinkMask32
 */
NV_STATUS
convertBitVectorToLinkMask32(NVLINK_BIT_VECTOR *pBitVector, NvU32 *linkMask)
{
    NV_STATUS status;
    NvU64 tmpLinkMask = 0;

    status = bitVectorToRaw(pBitVector, &tmpLinkMask, sizeof(tmpLinkMask));
    if (status != NV_OK)
    {
        return status;
    }

    *linkMask = (NvU32) tmpLinkMask;

    return NV_OK;
}

/*!
 * @copydoc convertBitVectorToLinkMasks
 */
NV_STATUS
convertBitVectorToLinkMasks(NVLINK_BIT_VECTOR *pLocalLinkMask,
                            void *pOutputLinkMask1, NvU32 outputLinkMask1Size,
                            NV2080_CTRL_NVLINK_LINK_MASK *pOutputLinkMask2)
{
    NV_STATUS status;

    if (pLocalLinkMask == NULL)
    {
        return NV_ERR_INVALID_ARGUMENT;
    }

    if (pOutputLinkMask1 != NULL)
    {
        if (outputLinkMask1Size == sizeof(NvU32))
        {
            status = convertBitVectorToLinkMask32(pLocalLinkMask, pOutputLinkMask1);
            if (status != NV_OK)
            {
                return status;
            }
        }
        else if (outputLinkMask1Size == sizeof(NvU64))
        {
            status = bitVectorToRaw(pLocalLinkMask, pOutputLinkMask1, outputLinkMask1Size);
            if (status != NV_OK)
            {
                return status;
            }
        }
        else
        {
            return NV_ERR_INVALID_ARGUMENT;
        }
    }

    if (pOutputLinkMask2 != NULL)
    {
        status = bitVectorToRaw(pLocalLinkMask, pOutputLinkMask2->masks, sizeof(pOutputLinkMask2->masks));
        if (status != NV_OK)
        {
            return status;
        }

        pOutputLinkMask2->lenMasks = sizeof(pOutputLinkMask2->masks)/sizeof(NvU64);
    }

    return NV_OK;
}

/*!
 * @copydoc convertLinkMasksToBitVector
 */
NV_STATUS
convertLinkMasksToBitVector(const void *pLinkMask1, NvU32 linkMask1Size,
                            const NV2080_CTRL_NVLINK_LINK_MASK *pLinkMask2,
                            NVLINK_BIT_VECTOR *pOutputBitVector)
{
    NV_STATUS status;
    NVLINK_BIT_VECTOR localLinkMask1;
    NVLINK_BIT_VECTOR localLinkMask2;

    // Return invalid args if output NVLINK_BIT_VECTOR is not specified
    if (pOutputBitVector == NULL)
    {
        return NV_ERR_INVALID_ARGUMENT;
    }

    status = bitVectorClrAll(pOutputBitVector);
    if (status != NV_OK)
    {
        return status;
    }

    status = bitVectorClrAll(&localLinkMask1);
    if (status != NV_OK)
    {
        return status;
    }

    status = bitVectorClrAll(&localLinkMask2);
    if (status != NV_OK)
    {
        return status;
    }

    // If pLinkMask1 is provided, convert it to bitvector
    if (pLinkMask1 != NULL)
    {
        // Validate size of the mask to be NvU32 or NvU64
        NV_CHECK_OR_RETURN(LEVEL_ERROR, (linkMask1Size == sizeof(NvU32) || linkMask1Size == sizeof(NvU64)), NV_ERR_INVALID_ARGUMENT);
        NvU64 tmpLinkMask = 0;
        portMemCopy(&tmpLinkMask, sizeof(tmpLinkMask), pLinkMask1, linkMask1Size);
        status = bitVectorFromRaw(&localLinkMask1, &tmpLinkMask, sizeof(tmpLinkMask));
        if (status != NV_OK)
        {
            return status;
        }
    }

    // If pLinkMask2 is provided, convert it to bitvector
    if (pLinkMask2 != NULL)
    {
        status = bitVectorFromRaw(&localLinkMask2, &pLinkMask2->masks, sizeof(pLinkMask2->masks));
        if (status != NV_OK)
        {
            return status;
        }
    }

    // Return the OR of the 2 converted bitvectors from pLinkMask1 and pLinkMask2
    return bitVectorOr(pOutputBitVector, &localLinkMask1, &localLinkMask2);
}

NV_STATUS
bitVectorGetSliceAtOffsetUnaligned_IMPL
(
    NV_BITVECTOR *pBitVector,
    NvU16         bitVectorLast,
    NvU32         offset,
    NvU32         size,
    NvU64        *pSlice
)
{
    NV_STATUS status;
    NvU64 tmpSlice = 0;

    if (pBitVector == NULL || pSlice == NULL)
    {
        return NV_ERR_INVALID_ARGUMENT;
    }

    // Get slice into aligned temporary variable
    status = bitVectorGetSlice_IMPL(pBitVector, bitVectorLast,
                                    rangeMake(offset, offset + size - 1),
                                    &tmpSlice);
    if (status != NV_OK)
    {
        return status;
    }

    // Copy to potentially unaligned destination
    portMemCopy(pSlice, sizeof(*pSlice), &tmpSlice, sizeof(tmpSlice));

    return NV_OK;
}

NV_STATUS
nvlinkConvertSplitMasksToBitVector_IMPL
(
    NvU32         numMasks,
    NvU64         mask0,
    NvU64         mask1,
    NvU64         mask2,
    NvU64         mask3,
    NV_BITVECTOR *pOutBitVector,
    NvU16         bitVectorLast
)
{
    NV_STATUS status;
    NvU64 masks[4] = { mask0, mask1, mask2, mask3 };
    NV_ASSERT_OR_RETURN(pOutBitVector != NULL, NV_ERR_INVALID_ARGUMENT);
    NV_ASSERT_OR_RETURN((numMasks > 0) && (numMasks <= NV_ARRAY_ELEMENTS(masks)),
                        NV_ERR_INVALID_ARGUMENT);
    status = bitVectorClrAll_IMPL(pOutBitVector, bitVectorLast);
    if (status != NV_OK)
        return status;
    return bitVectorFromRaw_IMPL(pOutBitVector, bitVectorLast,
                                 masks, numMasks * sizeof(NvU64));
}
NV_STATUS
nvlinkConvertBitVectorToSplitMasks_IMPL
(
    const NV_BITVECTOR *pInBitVector,
    NvU16               bitVectorLast,
    NvU32               numMasks,
    NvU64              *pMask0,
    NvU64              *pMask1,
    NvU64              *pMask2,
    NvU64              *pMask3
)
{
    NV_STATUS status;
    NvU64 masks[4] = { 0 };
    NV_ASSERT_OR_RETURN(pInBitVector != NULL, NV_ERR_INVALID_ARGUMENT);
    NV_ASSERT_OR_RETURN((numMasks > 0) && (numMasks <= NV_ARRAY_ELEMENTS(masks)),
                        NV_ERR_INVALID_ARGUMENT);
    status = bitVectorToRaw_IMPL(pInBitVector, bitVectorLast,
                                 masks, numMasks * sizeof(NvU64));
    if (status != NV_OK)
        return status;
    if (pMask0 != NULL)
        portMemCopy(pMask0, sizeof(*pMask0), &masks[0], sizeof(masks[0]));
    if (pMask1 != NULL)
        portMemCopy(pMask1, sizeof(*pMask1), &masks[1], sizeof(masks[1]));
    if (pMask2 != NULL)
        portMemCopy(pMask2, sizeof(*pMask2), &masks[2], sizeof(masks[2]));
    if (pMask3 != NULL)
        portMemCopy(pMask3, sizeof(*pMask3), &masks[3], sizeof(masks[3]));
    return NV_OK;
}
