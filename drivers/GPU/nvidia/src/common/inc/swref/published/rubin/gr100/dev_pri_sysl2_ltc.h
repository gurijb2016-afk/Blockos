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

#ifndef GR100_DEV_PRI_SYSL2_LTC_H
#define GR100_DEV_PRI_SYSL2_LTC_H

#define NV_PSYSL2LTC_LTSS_CBC_NUM_ACTIVE_LTCS                                  0x0000027c /* RW-4R */
#define NV_PSYSL2LTC_LTSS_CBC_NUM_ACTIVE_LTCS_SYS_ALL_FORCE_VOL                     21:21 /* RWIVF */
#define NV_PSYSL2LTC_LTSS_CBC_NUM_ACTIVE_LTCS_SYS_ALL_FORCE_VOL_INIT                  0x0 /* RWI-V */
#define NV_PSYSL2LTC_LTSS_CBC_NUM_ACTIVE_LTCS_SYS_ALL_FORCE_VOL_DISABLE               0x0 /* RW--V */
#define NV_PSYSL2LTC_LTSS_CBC_NUM_ACTIVE_LTCS_SYS_ALL_FORCE_VOL_ENABLE                0x1 /* RW--V */

#endif // GR100_DEV_PRI_SYSL2_LTC_H
