/*
 * SPDX-FileCopyrightText: Copyright (c) 2016 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/**
 * @file
 * @brief Time module timer object inline implementation
 */

#ifndef _NVPORT_TIME_H_
#error "This file should only be included through nvport/time.h"
#endif

#ifndef _NVPORT_TIME_TIMER_H_
#define _NVPORT_TIME_TIMER_H_

struct PORT_TIMER
{
    /// @privatesection
    NvU64 totalNanoseconds;
    NvU64 prevCount;
    NvBool isActive;
};

PORT_INLINE void portTimeTimerStart(PORT_TIMER *pTimer)
{
    pTimer->isActive = NV_TRUE;
    pTimer->prevCount = portTimeGetUptimeNanoseconds();
}

PORT_INLINE void portTimeTimerStop(PORT_TIMER *pTimer)
{
    NvU64 ns = portTimeGetUptimeNanoseconds();
    pTimer->isActive = NV_FALSE;
    pTimer->totalNanoseconds += (ns - pTimer->prevCount);
}

PORT_INLINE void portTimeTimerReset(PORT_TIMER *pTimer)
{
    pTimer->totalNanoseconds = 0;
    pTimer->prevCount = portTimeGetUptimeNanoseconds();
}

PORT_INLINE NvU64 portTimeTimerGetMilliseconds(PORT_TIMER *pTimer)
{
    return portTimeTimerGetNanoseconds(pTimer) / 1000000;
}

PORT_INLINE NvU64 portTimeTimerGetMicroseconds(PORT_TIMER *pTimer)
{
    return portTimeTimerGetNanoseconds(pTimer) / 1000;
}

PORT_INLINE NvU64 portTimeTimerGetNanoseconds(PORT_TIMER *pTimer)
{
    if (pTimer->isActive)
    {
        NvU64 ns = portTimeGetUptimeNanoseconds();
        pTimer->totalNanoseconds += (ns - pTimer->prevCount);
        pTimer->prevCount = ns;
    }
    return pTimer->totalNanoseconds;
}

#endif
