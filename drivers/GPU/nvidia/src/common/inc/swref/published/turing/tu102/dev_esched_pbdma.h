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

#ifndef __tu102_dev_pbdma_zero__
#define __tu102_dev_pbdma_zero__
#define NV_PBDMA                                                         0x0001ffff:0x00000000 /* RW--D */
#define NV_PBDMA_SIGNATURE(i)                                            (0x00000010+(i)*8192) /* RW-4A */
#define NV_PBDMA_SIGNATURE__SIZE_1                                                          12 /*       */
#define NV_PBDMA_SIGNATURE_HW                                                             15:0 /* RWXUF */
#define NV_PBDMA_SIGNATURE_HW_VALID                                                 0x0000face /* RW--V */
#define NV_PBDMA_SIGNATURE_HW_HOST_CLASS_ID                                              50287 /* RW--V */
#define NV_PBDMA_SIGNATURE_SW                                                            31:16 /* RWXUF */
#define NV_PBDMA_SIGNATURE_SW_ZERO                                                  0x00000000 /* RW--V */
#define NV_PBDMA_PB_HEADER(i)                                            (0x00000084+(i)*8192) /* RW-4A */
#define NV_PBDMA_PB_HEADER__SIZE_1                                                          12 /*       */
#define NV_PBDMA_PB_HEADER_METHOD_OR_SDMASK                                               15:2 /* RWXUF */
#define NV_PBDMA_PB_HEADER_METHOD                                                         13:2 /*       */
#define NV_PBDMA_PB_HEADER_METHOD_ZERO                                              0x00000000 /*       */
#define NV_PBDMA_PB_HEADER_SDMASK                                                         15:4 /*       */
#define NV_PBDMA_PB_HEADER_SUBCHANNEL                                                    18:16 /* RWXUF */
#define NV_PBDMA_PB_HEADER_SUBCHANNEL_ZERO                                          0x00000000 /* RW--V */
#define NV_PBDMA_PB_HEADER_LEVEL                                                         20:20 /* RWXVF */
#define NV_PBDMA_PB_HEADER_LEVEL_MAIN                                               0x00000000 /* RW--V */
#define NV_PBDMA_PB_HEADER_LEVEL_SUBROUTINE                                         0x00000001 /* RW--V */
#define NV_PBDMA_PB_HEADER_FIRST                                                         22:22 /* RWXVF */
#define NV_PBDMA_PB_HEADER_FIRST_FALSE                                              0x00000000 /* RW--V */
#define NV_PBDMA_PB_HEADER_FIRST_TRUE                                               0x00000001 /* RW--V */
#define NV_PBDMA_PB_HEADER_CONDITIONAL                                                   23:23 /* RWXVF */
#define NV_PBDMA_PB_HEADER_CONDITIONAL_FALSE                                        0x00000000 /* RW--V */
#define NV_PBDMA_PB_HEADER_CONDITIONAL_TRUE                                         0x00000001 /* RW--V */
#define NV_PBDMA_PB_HEADER_FINAL                                                         24:24 /* RWXVF */
#define NV_PBDMA_PB_HEADER_FINAL_FALSE                                              0x00000000 /* RW--V */
#define NV_PBDMA_PB_HEADER_FINAL_TRUE                                               0x00000001 /* RW--V */
#define NV_PBDMA_PB_HEADER_TYPE                                                          31:29 /* RWXUF */
#define NV_PBDMA_PB_HEADER_TYPE_SSDM                                                0x00000000 /* RW--V */
#define NV_PBDMA_PB_HEADER_TYPE_INC                                                 0x00000001 /* RW--V */
#define NV_PBDMA_PB_HEADER_TYPE_STORE_SDM                                           0x00000002 /* RW--V */
#define NV_PBDMA_PB_HEADER_TYPE_NON_INC                                             0x00000003 /* RW--V */
#define NV_PBDMA_PB_HEADER_TYPE_IMMD                                                0x00000004 /* RW--V */
#define NV_PBDMA_PB_HEADER_TYPE_INC_ONCE                                            0x00000005 /* RW--V */
#define NV_PBDMA_PB_HEADER_TYPE_USE_SDM                                             0x00000006 /* RW--V */
#define NV_PBDMA_PB_HEADER_TYPE_END_SEG                                             0x00000007 /* RW--V */
#define NV_PBDMA_CONFIG(i)                                               (0x000000F4+(i)*8192) /* R--4A */
#define NV_PBDMA_CONFIG_USERD_WRITEBACK                                                  12:12 /* R--VF */
#define NV_PBDMA_CONFIG_USERD_WRITEBACK_DISABLE                                     0x00000000 /* R---V */
#define NV_PBDMA_CONFIG_USERD_WRITEBACK_ENABLE                                      0x00000001 /* R---V */
#define NV_PBDMA_CONFIG(i)                                               (0x000000F4+(i)*8192) /* R--4A */
#define NV_PBDMA_CONFIG_CE_THROTTLE_MODE                                                   5:5 /* R--VF */
#define NV_PBDMA_CONFIG_CE_THROTTLE_MODE_THROTTLE                                   0x00000000 /* R---V */
#define NV_PBDMA_CONFIG_CE_THROTTLE_MODE_NO_THROTTLE                                0x00000001 /* R---V */
#endif // __tu102_dev_pbdma_zero__
