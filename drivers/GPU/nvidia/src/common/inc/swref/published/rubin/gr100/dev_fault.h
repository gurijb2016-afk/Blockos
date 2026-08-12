/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES
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

#ifndef __gr100_dev_fault_h__
#define __gr100_dev_fault_h__

#define NV_PFAULT_CLIENT                       14:8 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_0        0x00000000 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_1        0x00000001 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_2        0x00000002 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_3        0x00000003 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_4        0x00000004 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_5        0x00000005 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_6        0x00000006 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_7        0x00000007 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_8        0x00000008 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_9        0x00000009 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_10       0x0000000A /*       */
#define NV_PFAULT_CLIENT_GPC_T1_11       0x0000000B /*       */
#define NV_PFAULT_CLIENT_GPC_T1_12       0x0000000C /*       */
#define NV_PFAULT_CLIENT_GPC_T1_13       0x0000000D /*       */
#define NV_PFAULT_CLIENT_GPC_T1_14       0x0000000E /*       */
#define NV_PFAULT_CLIENT_GPC_T1_15       0x0000000F /*       */
#define NV_PFAULT_CLIENT_GPC_T1_16       0x00000010 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_17       0x00000011 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_18       0x00000012 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_19       0x00000013 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_20       0x00000014 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_21       0x00000015 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_22       0x00000016 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_23       0x00000017 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_24       0x00000018 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_25       0x00000019 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_26       0x0000001A /*       */
#define NV_PFAULT_CLIENT_GPC_T1_27       0x0000001B /*       */
#define NV_PFAULT_CLIENT_GPC_T1_28       0x0000001C /*       */
#define NV_PFAULT_CLIENT_GPC_T1_29       0x0000001D /*       */
#define NV_PFAULT_CLIENT_GPC_T1_30       0x0000001E /*       */
#define NV_PFAULT_CLIENT_GPC_T1_31       0x0000001F /*       */
#define NV_PFAULT_CLIENT_GPC_T1_32       0x00000020 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_33       0x00000021 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_34       0x00000022 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_35       0x00000023 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_36       0x00000024 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_37       0x00000025 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_38       0x00000026 /*       */
#define NV_PFAULT_CLIENT_GPC_T1_39       0x00000027 /*       */
#define NV_PFAULT_CLIENT_GPC_RAST        0x00000039 /*       */
#define NV_PFAULT_CLIENT_GPC_GCC         0x0000003A /*       */
#define NV_PFAULT_CLIENT_GPC_GPCCS       0x0000003B /*       */
#define NV_PFAULT_CLIENT_GPC_ROP_0       0x0000003C /*       */
#define NV_PFAULT_CLIENT_GPC_ROP_1       0x0000003D /*       */
#define NV_PFAULT_CLIENT_GPC_ROP_2       0x0000003E /*       */
#define NV_PFAULT_CLIENT_GPC_ROP_3       0x0000003F /*       */
#define NV_PFAULT_CLIENT_GPC_PE_0        0x00000040 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_1        0x00000041 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_2        0x00000042 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_3        0x00000043 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_4        0x00000044 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_5        0x00000045 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_6        0x00000046 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_7        0x00000047 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_8        0x00000048 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_9        0x00000049 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_10       0x0000004A /*       */
#define NV_PFAULT_CLIENT_GPC_PE_11       0x0000004B /*       */
#define NV_PFAULT_CLIENT_GPC_PE_12       0x0000004C /*       */
#define NV_PFAULT_CLIENT_GPC_PE_13       0x0000004D /*       */
#define NV_PFAULT_CLIENT_GPC_PE_14       0x0000004E /*       */
#define NV_PFAULT_CLIENT_GPC_PE_15       0x0000004F /*       */
#define NV_PFAULT_CLIENT_GPC_PE_16       0x00000050 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_17       0x00000051 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_18       0x00000052 /*       */
#define NV_PFAULT_CLIENT_GPC_PE_19       0x00000053 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_0     0x00000060 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_1     0x00000061 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_2     0x00000062 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_3     0x00000063 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_4     0x00000064 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_5     0x00000065 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_6     0x00000066 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_7     0x00000067 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_8     0x00000068 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_9     0x00000069 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_10    0x0000006A /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_11    0x0000006B /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_12    0x0000006C /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_13    0x0000006D /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_14    0x0000006E /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_15    0x0000006F /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_16    0x00000070 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_17    0x00000071 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_18    0x00000072 /*       */
#define NV_PFAULT_CLIENT_GPC_TPCCS_19    0x00000073 /*       */
#define NV_PFAULT_CLIENT_GPC_PROP_0      0x0000007C /*       */
#define NV_PFAULT_CLIENT_GPC_PROP_1      0x0000007D /*       */
#define NV_PFAULT_CLIENT_GPC_PROP_2      0x0000007E /*       */
#define NV_PFAULT_CLIENT_GPC_PROP_3      0x0000007F /*       */
#define NV_PFAULT_CLIENT_GPC_GPM          0x0000007B /*       */
#define NV_PFAULT_CLIENT_HUB_VIP         0x00000000 /*       */
#define NV_PFAULT_CLIENT_HUB_CE0         0x00000001 /*       */
#define NV_PFAULT_CLIENT_HUB_CE1         0x00000002 /*       */
#define NV_PFAULT_CLIENT_HUB_DNISO       0x00000003 /*       */
#define NV_PFAULT_CLIENT_HUB_DISPNISO    0x00000003 /*       */
#define NV_PFAULT_CLIENT_HUB_FE0         0x00000004 /*       */
#define NV_PFAULT_CLIENT_HUB_FE          0x00000004 /*       */
#define NV_PFAULT_CLIENT_HUB_FECS0       0x00000005 /*       */
#define NV_PFAULT_CLIENT_HUB_FECS        0x00000005 /*       */
#define NV_PFAULT_CLIENT_HUB_MFC         0x00000006 /*       */
#define NV_PFAULT_CLIENT_HUB_UNUSED      0x00000007 /*       */
#define NV_PFAULT_CLIENT_HUB_XAL         0x00000008 /*       */
#define NV_PFAULT_CLIENT_HUB_ISO         0x00000009 /*       */
#define NV_PFAULT_CLIENT_HUB_MMU         0x0000000A /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC0      0x0000000B /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC       0x0000000B /*       */
#define NV_PFAULT_CLIENT_HUB_CE3         0x0000000C /*       */
#define NV_PFAULT_CLIENT_HUB_NVENC1      0x0000000D /*       */
#define NV_PFAULT_CLIENT_HUB_NISO        0x0000000E /*       */
#define NV_PFAULT_CLIENT_HUB_ACTRS       0x0000000E /*       */
#define NV_PFAULT_CLIENT_HUB_P2P         0x0000000F /*       */
#define NV_PFAULT_CLIENT_HUB_PD          0x00000010 /*       */
#define NV_PFAULT_CLIENT_HUB_PD0         0x00000010 /*       */
#define NV_PFAULT_CLIENT_HUB_PERF0       0x00000011 /*       */
#define NV_PFAULT_CLIENT_HUB_PERF        0x00000011 /*       */
#define NV_PFAULT_CLIENT_HUB_PMU         0x00000012 /*       */
#define NV_PFAULT_CLIENT_HUB_RASTERTWOD  0x00000013 /*       */
#define NV_PFAULT_CLIENT_HUB_RASTERTWOD0 0x00000013 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC         0x00000014 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC0        0x00000014 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC_NB      0x00000015 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC_NB0     0x00000015 /*       */
#define NV_PFAULT_CLIENT_HUB_SEC         0x00000016 /*       */
#define NV_PFAULT_CLIENT_HUB_SSYNC       0x00000017 /*       */
#define NV_PFAULT_CLIENT_HUB_SSYNC0      0x00000017 /*       */
#define NV_PFAULT_CLIENT_HUB_GRCOPY      0x00000018 /*       */
#define NV_PFAULT_CLIENT_HUB_CE2         0x00000018 /*       */
#define NV_PFAULT_CLIENT_HUB_XV          0x00000019 /*       */
#define NV_PFAULT_CLIENT_HUB_MMU_NB      0x0000001A /*       */
#define NV_PFAULT_CLIENT_HUB_NVENC0      0x0000001B /*       */
#define NV_PFAULT_CLIENT_HUB_NVENC       0x0000001B /*       */
#define NV_PFAULT_CLIENT_HUB_DFALCON     0x0000001C /*       */
#define NV_PFAULT_CLIENT_HUB_SKED0       0x0000001D /*       */
#define NV_PFAULT_CLIENT_HUB_SKED        0x0000001D /*       */
#define NV_PFAULT_CLIENT_HUB_PD1         0x0000001E /*       */
#define NV_PFAULT_CLIENT_HUB_DONT_CARE   0x0000001F /*       */
#define NV_PFAULT_CLIENT_HUB_HSCE0       0x00000020 /*       */
#define NV_PFAULT_CLIENT_HUB_HSCE1       0x00000021 /*       */
#define NV_PFAULT_CLIENT_HUB_HSCE2       0x00000022 /*       */
#define NV_PFAULT_CLIENT_HUB_HSCE3       0x00000023 /*       */
#define NV_PFAULT_CLIENT_HUB_HSCE4       0x00000024 /*       */
#define NV_PFAULT_CLIENT_HUB_HSCE5       0x00000025 /*       */
#define NV_PFAULT_CLIENT_HUB_HSCE6       0x00000026 /*       */
#define NV_PFAULT_CLIENT_HUB_HSCE7       0x00000027 /*       */
#define NV_PFAULT_CLIENT_HUB_SSYNC1      0x00000028 /*       */
#define NV_PFAULT_CLIENT_HUB_SSYNC2      0x00000029 /*       */
#define NV_PFAULT_CLIENT_HUB_HSHUB       0x0000002A /*       */
#define NV_PFAULT_CLIENT_HUB_PTP_X0      0x0000002B /*       */
#define NV_PFAULT_CLIENT_HUB_PTP_X1      0x0000002C /*       */
#define NV_PFAULT_CLIENT_HUB_PTP_X2      0x0000002D /*       */
#define NV_PFAULT_CLIENT_HUB_PTP_X3      0x0000002E /*       */
#define NV_PFAULT_CLIENT_HUB_PTP_X4      0x0000002F /*       */
#define NV_PFAULT_CLIENT_HUB_PTP_X5      0x00000030 /*       */
#define NV_PFAULT_CLIENT_HUB_PTP_X6      0x00000031 /*       */
#define NV_PFAULT_CLIENT_HUB_PTP_X7      0x00000032 /*       */
#define NV_PFAULT_CLIENT_HUB_NVENC2      0x00000033 /*       */
#define NV_PFAULT_CLIENT_HUB_VPR_SCRUBBER0 0x00000034 /*       */
#define NV_PFAULT_CLIENT_HUB_VPR_SCRUBBER1 0x00000035 /*       */
#define NV_PFAULT_CLIENT_HUB_SSYNC3      0x00000036 /*       */
#define NV_PFAULT_CLIENT_HUB_FBFALCON    0x00000037 /*       */
#define NV_PFAULT_CLIENT_HUB_CE_SHIM     0x00000038 /*       */
#define NV_PFAULT_CLIENT_HUB_CE_SHIM0    0x00000038 /*       */
#define NV_PFAULT_CLIENT_HUB_GSP         0x00000039 /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC1      0x0000003A /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC2      0x0000003B /*       */
#define NV_PFAULT_CLIENT_HUB_NVJPG0      0x0000003C /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC3      0x0000003D /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC4      0x0000003E /*       */
#define NV_PFAULT_CLIENT_HUB_OFA0        0x0000003F /*       */
#define NV_PFAULT_CLIENT_HUB_SCC1        0x00000040 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC_NB1     0x00000041 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC2        0x00000042 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC_NB2     0x00000043 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC3        0x00000044 /*       */
#define NV_PFAULT_CLIENT_HUB_SCC_NB3     0x00000045 /*       */
#define NV_PFAULT_CLIENT_HUB_RASTERTWOD1 0x00000046 /*       */
#define NV_PFAULT_CLIENT_HUB_RASTERTWOD2 0x00000047 /*       */
#define NV_PFAULT_CLIENT_HUB_RASTERTWOD3 0x00000048 /*       */
#define NV_PFAULT_CLIENT_HUB_GSPLITE1    0x00000049 /*       */
#define NV_PFAULT_CLIENT_HUB_GSPLITE2    0x0000004A /*       */
#define NV_PFAULT_CLIENT_HUB_GSPLITE3    0x0000004B /*       */
#define NV_PFAULT_CLIENT_HUB_PD2         0x0000004C /*       */
#define NV_PFAULT_CLIENT_HUB_PD3         0x0000004D /*       */
#define NV_PFAULT_CLIENT_HUB_FE1         0x0000004E /*       */
#define NV_PFAULT_CLIENT_HUB_FE2         0x0000004F /*       */
#define NV_PFAULT_CLIENT_HUB_FE3         0x00000050 /*       */
#define NV_PFAULT_CLIENT_HUB_FE4         0x00000051 /*       */
#define NV_PFAULT_CLIENT_HUB_FE5         0x00000052 /*       */
#define NV_PFAULT_CLIENT_HUB_FE6         0x00000053 /*       */
#define NV_PFAULT_CLIENT_HUB_FE7         0x00000054 /*       */
#define NV_PFAULT_CLIENT_HUB_FECS1       0x00000055 /*       */
#define NV_PFAULT_CLIENT_HUB_FECS2       0x00000056 /*       */
#define NV_PFAULT_CLIENT_HUB_FECS3       0x00000057 /*       */
#define NV_PFAULT_CLIENT_HUB_FECS4       0x00000058 /*       */
#define NV_PFAULT_CLIENT_HUB_FECS5       0x00000059 /*       */
#define NV_PFAULT_CLIENT_HUB_FECS6       0x0000005A /*       */
#define NV_PFAULT_CLIENT_HUB_FECS7       0x0000005B /*       */
#define NV_PFAULT_CLIENT_HUB_SKED1       0x0000005C /*       */
#define NV_PFAULT_CLIENT_HUB_SKED2       0x0000005D /*       */
#define NV_PFAULT_CLIENT_HUB_SKED3       0x0000005E /*       */
#define NV_PFAULT_CLIENT_HUB_SKED4       0x0000005F /*       */
#define NV_PFAULT_CLIENT_HUB_SKED5       0x00000060 /*       */
#define NV_PFAULT_CLIENT_HUB_SKED6       0x00000061 /*       */
#define NV_PFAULT_CLIENT_HUB_SKED7       0x00000062 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC          0x00000063 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC0         0x00000063 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC1         0x00000064 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC2         0x00000065 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC3         0x00000066 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC4         0x00000067 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC5         0x00000068 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC6         0x00000069 /*       */
#define NV_PFAULT_CLIENT_HUB_ESC7         0x0000006a /*       */
#define NV_PFAULT_CLIENT_HUB_ESC8         0x0000006b /*       */
#define NV_PFAULT_CLIENT_HUB_ESC9         0x0000006c /*       */
#define NV_PFAULT_CLIENT_HUB_ESC10        0x0000006d /*       */
#define NV_PFAULT_CLIENT_HUB_ESC11        0x0000006e /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC5      0x0000006F /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC6      0x00000070 /*       */
#define NV_PFAULT_CLIENT_HUB_NVDEC7      0x00000071 /*       */
#define NV_PFAULT_CLIENT_HUB_NVJPG1      0x00000072 /*       */
#define NV_PFAULT_CLIENT_HUB_NVJPG2      0x00000073 /*       */
#define NV_PFAULT_CLIENT_HUB_NVJPG3      0x00000074 /*       */
#define NV_PFAULT_CLIENT_HUB_NVJPG4      0x00000075 /*       */
#define NV_PFAULT_CLIENT_HUB_NVJPG5      0x00000076 /*       */
#define NV_PFAULT_CLIENT_HUB_NVJPG6      0x00000077 /*       */
#define NV_PFAULT_CLIENT_HUB_NVJPG7      0x00000078 /*       */
#define NV_PFAULT_CLIENT_HUB_FSP         0x00000079 /*       */
#define NV_PFAULT_CLIENT_HUB_BSI         0x0000007A /*       */
#define NV_PFAULT_CLIENT_HUB_GSPLITE       0x0000007B /*       */
#define NV_PFAULT_CLIENT_HUB_GSPLITE0      0x0000007B /*       */
#define NV_PFAULT_CLIENT_HUB_VPR_SCRUBBER2 0x0000007C /*       */
#define NV_PFAULT_CLIENT_HUB_VPR_SCRUBBER3 0x0000007D /*       */
#define NV_PFAULT_CLIENT_HUB_VPR_SCRUBBER4 0x0000007E /*       */
#define NV_PFAULT_CLIENT_HUB_NVENC3        0x0000007F /*       */


#endif // __gr100_dev_fault_h__
