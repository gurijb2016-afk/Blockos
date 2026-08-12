/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES
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

#ifndef __tu102_dev_ram_h__
#define __tu102_dev_ram_h__
#define NV_PRAMIN                             0x007FFFFF:0x00700000 /* RW--M */
#define NV_RAMFC                                                    /* ----G */
#define NV_RAMFC_SIGNATURE                       (4*32+31):(4*32+0) /* RWXUF */
#define NV_RAMFC_PB_HEADER                     (33*32+31):(33*32+0) /* RWXUF */
#define NV_RAMIN_ENG_METHOD_BUFFER_ADDR_LO       (136*32+31):(136*32+0)  /* RWXUF */
#define NV_RAMIN_ENG_METHOD_BUFFER_ADDR_HI       (137*32+(((49-1)-32))):(137*32+0)  /* RWXUF */
#define NV_RAMFC_CONFIG                          (61*32+31):(61*32+0) /* RWXUF */
#endif // __tu102_dev_ram_h__
