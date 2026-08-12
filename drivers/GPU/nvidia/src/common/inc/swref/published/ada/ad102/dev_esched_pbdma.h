/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES
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

#ifndef __ad102_dev_pbdma_zero__
#define __ad102_dev_pbdma_zero__
#define NV_PBDMA       0x000007ff:0x00000000 /* RW--D */
#define NV_PBDMA_SET_CHANNEL_INFO                            0x0fc /* RW-4R */
#define NV_PBDMA_SET_CHANNEL_INFO_ASYNC_CE_PREFETCH_ENABLE          1:1 /*       */
#define NV_PBDMA_SET_CHANNEL_INFO_ASYNC_CE_PREFETCH_ENABLE_FALSE    0x0 /*       */
#define NV_PBDMA_SET_CHANNEL_INFO_ASYNC_CE_PREFETCH_ENABLE_TRUE     0x1 /*       */
#endif // __tu102_dev_pbdma_zero__
