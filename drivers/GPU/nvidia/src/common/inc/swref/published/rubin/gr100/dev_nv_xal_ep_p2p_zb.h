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

#ifndef __gr100_dev_nv_xal_ep_p2p_zb__h__
#define __gr100_dev_nv_xal_ep_p2p_zb__h__

#define NV_XAL_EP_P2P_ZB_WMBOX_ADDR(i)                 (0x00000024+(i)*64) /* RW-4A */
#define NV_XAL_EP_P2P_ZB_WMBOX_ADDR__SIZE_1                                    8 /*       */
#define NV_XAL_EP_P2P_ZB_WMBOX_ADDR__PRIV_LEVEL_MASK               0x00000900 /*       */
#define NV_XAL_EP_P2P_ZB_WMBOX_ADDR_DIS                                                      0:0 /* RWIUF */
#define NV_XAL_EP_P2P_ZB_WMBOX_ADDR_DIS_DISABLED                                      0x00000001 /* RWI-V */
#define NV_XAL_EP_P2P_ZB_WMBOX_ADDR_DIS_ENABLED                                       0x00000000 /* RW--V */
#define NV_XAL_EP_P2P_ZB_WMBOX_ADDR_ADDR              (47-16):1 /* RWIUF */
#define NV_XAL_EP_P2P_ZB_WMBOX_ADDR_ADDR_INIT                                         0x0003ffff /* RWI-V */

#endif // __gr100_dev_nv_xal_ep_p2p_zb__h__
