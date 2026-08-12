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

#ifndef __DRV_EVENT_H__
#define __DRV_EVENT_H__

/* ------------------------ Includes --------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#define NV_DRV_EVENT_TEXT_STRING_MAX        ((NvU8)~0 - sizeof(struct drvEventRecord))

#define NV_DRV_EVENT_FLAGS_TRUNCATED        0:0
#define NV_DRV_EVENT_FLAGS_TRUNCATED_NO       0
#define NV_DRV_EVENT_FLAGS_TRUNCATED_YES      1

#define NSM_TYPE_INTERNAL_DRIVER_MESSAGE    0x3
#define NSM_TYPE_INTERNAL_RESET_REQUIRED    0x4

/* ------------------------ Data types ------------------------------------- */

/**
 * An event is generated when a driver error message (Xid) has been issued.
 * This is the layout of this event's payload.
 **/
typedef struct drvEventRecord
{
    NvU8    flags;          //!< Bit 0:    Message text string truncated.
                            //!< Bits 7-1: Reserved
    NvU8    reserved[3];    //!< Reserved, MBZ
    NvU32   reason;         //!< Driver message type (Xid);
    NvU32   seqNumber;      //!< Sequence number
    NvU32   timestampLo;    //!< Epoch ns timestamp low 32 bits
    NvU32   timestampHi;    //!< Epoch ns timestamp high 32 bits
    /*
     * Some Windows builds may include this file, and older Microsoft
     * compilers won't accept a zero size array in this context:
     * "C4200 nonstandard extension used: zero-sized array in struct/union",
     * despite this being a legitimate C99.
     */
    NvU8    text[];         //<! non-terminated, up to NV_DRV_EVENT_TEXT_STRING_MAX
} drvEventRecord;

/**
 * An event is generated when the GPU is marked or unmarked for reset.
 * This is the layout of this event's payload.
 **/
typedef struct drvResetRequired
{
    NvU8    set;        //!< FALSE: unmark for reset
                        //!< TRUE:  mark for reset
} drvResetRequired;

#endif /* __DRV_EVENT_H__ */
