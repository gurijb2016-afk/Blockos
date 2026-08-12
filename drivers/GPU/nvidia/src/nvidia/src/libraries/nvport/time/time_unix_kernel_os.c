/*
 * SPDX-FileCopyrightText: Copyright (c) 2016-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * @brief TIME module implementation for Unix kernelmode
 */

#include "nvport/nvport.h"

#if !PORT_IS_KERNEL_BUILD
#error "This file can only be compiled as part of the kernel build."
#endif
#if !NVOS_IS_UNIX
#error "This file can only be compiled on Unix."
#endif

#include "os-interface.h"

NvU32 portTimeGetSeconds(void)
{
    return (NvU32)(portTimeGetMicroseconds() / 1000000);
}

NvU64 portTimeGetMilliseconds(void)
{
    return portTimeGetMicroseconds() / 1000;
}

NvU64 portTimeGetMicroseconds(void)
{
    NvU32 sec, usec;
    os_get_system_time(&sec, &usec);

    return (NvU64)sec * 1000000 + usec;
}

NvU64 portTimeGetNanoseconds(void)
{
    return portTimeGetMicroseconds() * 1000ULL;
}

NvU64 portTimeGetUptimeMicroseconds(void)
{
    return portTimeGetUptimeNanoseconds() / 1000;
}

NvU64 portTimeGetUptimeNanoseconds(void)
{
    return os_get_monotonic_time_ns();
}

NvU64 portTimeGetUptimeNanosecondsHighPrecision(void)
{
    return os_get_monotonic_time_ns_hr();
}

NvU64 portTimeGetUptimeResolution(void)
{
    return os_get_monotonic_tick_resolution_ns();
}
