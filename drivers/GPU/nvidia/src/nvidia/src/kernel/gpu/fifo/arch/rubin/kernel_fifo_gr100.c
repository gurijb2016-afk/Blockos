/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "kernel/gpu/fifo/kernel_fifo.h"
#include "kernel/gpu/mem_mgr/virt_mem_allocator.h"
#include "gpu/mem_mgr/mem_mgr.h"

#include "published/rubin/gr100/dev_fault.h"
#include "published/rubin/gr100/hwproject.h"

/**
 * @brief Converts a subid/clientid into a client string
 *
 * @param[in] pGpu
 * @param[in] pKernelFifo
 * @param[in] clientId

 * @returns a string (always non-null)
 */
const char*
kfifoGetClientIdStringCheck_GR100
(
    OBJGPU     *pGpu,
    KernelFifo *pKernelFifo,
    NvU32       clientId
)
{
    switch (clientId)
    {
        case NV_PFAULT_CLIENT_HUB_NVENC0:
            return "HUBCLIENT_NVENC0";
        case NV_PFAULT_CLIENT_HUB_MFC:
            return "HUBCLIENT_MFC";
        case NV_PFAULT_CLIENT_HUB_UNUSED:
            return "HUBCLIENT_UNUSED";
        case NV_PFAULT_CLIENT_HUB_XAL:
            return "HUBCLIENT_XAL";
        default:
            return "UNRECOGNIZED_CLIENT";

    }
}

/**
 * @brief Converts a subid/clientid into a client string.
 *        This common function may be used by subsequent chips
 *
 * @param[in] pGpu
 * @param[in] pKernelFifo
 * @param[in] pMmuExceptData

 * @returns a string (always non-null)
 */
const char*
kfifoGetClientIdStringCommon_GR100
(
    OBJGPU                  *pGpu,
    KernelFifo              *pKernelFifo,
    FIFO_MMU_EXCEPTION_DATA *pMmuExceptInfo
)
{
    if (pMmuExceptInfo->bGpc)
    {
        switch (pMmuExceptInfo->clientId)
        {
            case NV_PFAULT_CLIENT_GPC_T1_0:
                return "GPCCLIENT_T1_0";
            case NV_PFAULT_CLIENT_GPC_T1_1:
                return "GPCCLIENT_T1_1";
            case NV_PFAULT_CLIENT_GPC_T1_2:
                return "GPCCLIENT_T1_2";
            case NV_PFAULT_CLIENT_GPC_T1_3:
                return "GPCCLIENT_T1_3";
            case NV_PFAULT_CLIENT_GPC_T1_4:
                return "GPCCLIENT_T1_4";
            case NV_PFAULT_CLIENT_GPC_T1_5:
                return "GPCCLIENT_T1_5";
            case NV_PFAULT_CLIENT_GPC_T1_6:
                return "GPCCLIENT_T1_6";
            case NV_PFAULT_CLIENT_GPC_T1_7:
                return "GPCCLIENT_T1_7";
            case NV_PFAULT_CLIENT_GPC_T1_8:
                return "GPCCLIENT_T1_8";
            case NV_PFAULT_CLIENT_GPC_T1_9:
                return "GPCCLIENT_T1_9";
            case NV_PFAULT_CLIENT_GPC_T1_10:
                return "GPCCLIENT_T1_10";
            case NV_PFAULT_CLIENT_GPC_T1_11:
                return "GPCCLIENT_T1_11";
            case NV_PFAULT_CLIENT_GPC_T1_12:
                return "GPCCLIENT_T1_12";
            case NV_PFAULT_CLIENT_GPC_T1_13:
                return "GPCCLIENT_T1_13";
            case NV_PFAULT_CLIENT_GPC_T1_14:
                return "GPCCLIENT_T1_14";
            case NV_PFAULT_CLIENT_GPC_T1_15:
                return "GPCCLIENT_T1_15";
            case NV_PFAULT_CLIENT_GPC_T1_16:
                return "GPCCLIENT_T1_16";
            case NV_PFAULT_CLIENT_GPC_T1_17:
                return "GPCCLIENT_T1_17";
            case NV_PFAULT_CLIENT_GPC_T1_18:
                return "GPCCLIENT_T1_18";
            case NV_PFAULT_CLIENT_GPC_T1_19:
                return "GPCCLIENT_T1_19";
            case NV_PFAULT_CLIENT_GPC_T1_20:
                return "GPCCLIENT_T1_20";
            case NV_PFAULT_CLIENT_GPC_T1_21:
                return "GPCCLIENT_T1_21";
            case NV_PFAULT_CLIENT_GPC_T1_22:
                return "GPCCLIENT_T1_22";
            case NV_PFAULT_CLIENT_GPC_T1_23:
                return "GPCCLIENT_T1_23";
            case NV_PFAULT_CLIENT_GPC_T1_24:
                return "GPCCLIENT_T1_24";
            case NV_PFAULT_CLIENT_GPC_T1_25:
                return "GPCCLIENT_T1_25";
            case NV_PFAULT_CLIENT_GPC_T1_26:
                return "GPCCLIENT_T1_26";
            case NV_PFAULT_CLIENT_GPC_T1_27:
                return "GPCCLIENT_T1_27";
            case NV_PFAULT_CLIENT_GPC_T1_28:
                return "GPCCLIENT_T1_28";
            case NV_PFAULT_CLIENT_GPC_T1_29:
                return "GPCCLIENT_T1_29";
            case NV_PFAULT_CLIENT_GPC_PE_0:
                return "GPCCLIENT_PE_0";
            case NV_PFAULT_CLIENT_GPC_PE_1:
                return "GPCCLIENT_PE_1";
            case NV_PFAULT_CLIENT_GPC_PE_2:
                return "GPCCLIENT_PE_2";
            case NV_PFAULT_CLIENT_GPC_PE_3:
                return "GPCCLIENT_PE_3";
            case NV_PFAULT_CLIENT_GPC_PE_4:
                return "GPCCLIENT_PE_4";
            case NV_PFAULT_CLIENT_GPC_PE_5:
                return "GPCCLIENT_PE_5";
            case NV_PFAULT_CLIENT_GPC_PE_6:
                return "GPCCLIENT_PE_6";
            case NV_PFAULT_CLIENT_GPC_PE_7:
                return "GPCCLIENT_PE_7";
            case NV_PFAULT_CLIENT_GPC_RAST:
                return "GPCCLIENT_RAST";
            case NV_PFAULT_CLIENT_GPC_GCC:
                return "GPCCLIENT_GCC";
            case NV_PFAULT_CLIENT_GPC_GPCCS:
                return "GPCCLIENT_GPCCS";
            case NV_PFAULT_CLIENT_GPC_PROP_0:
                return "GPCCLIENT_PROP_0";
            case NV_PFAULT_CLIENT_GPC_PROP_1:
                return "GPCCLIENT_PROP_1";
            case NV_PFAULT_CLIENT_GPC_TPCCS_0:
                return "GPCCLIENT_TPCCS_0";
            case NV_PFAULT_CLIENT_GPC_TPCCS_1:
                return "GPCCLIENT_TPCCS_1";
            case NV_PFAULT_CLIENT_GPC_TPCCS_2:
                return "GPCCLIENT_TPCCS_2";
            case NV_PFAULT_CLIENT_GPC_TPCCS_3:
                return "GPCCLIENT_TPCCS_3";
            case NV_PFAULT_CLIENT_GPC_TPCCS_4:
                return "GPCCLIENT_TPCCS_4";
            case NV_PFAULT_CLIENT_GPC_TPCCS_5:
                return "GPCCLIENT_TPCCS_5";
            case NV_PFAULT_CLIENT_GPC_TPCCS_6:
                return "GPCCLIENT_TPCCS_6";
            case NV_PFAULT_CLIENT_GPC_TPCCS_7:
                return "GPCCLIENT_TPCCS_7";
            case NV_PFAULT_CLIENT_GPC_PE_8:
                return "GPCCLIENT_PE_8";
            case NV_PFAULT_CLIENT_GPC_TPCCS_8:
                return "GPCCLIENT_TPCCS_8";
            case NV_PFAULT_CLIENT_GPC_ROP_0:
                return "GPCCLIENT_ROP_0";
            case NV_PFAULT_CLIENT_GPC_ROP_1:
                return "GPCCLIENT_ROP_1";
            case NV_PFAULT_CLIENT_GPC_GPM:
                return "GPCCLIENT_GPM";
            default:
                return "UNRECOGNIZED_CLIENT";
        }
    }
    else
    {

        //
        // Some of the cases below use the gpcId to determine which dielet the
        // client is on. Since these strings are not generated we have to
        // hardcode the number of engines per dielet. These asserts are to
        // prevent a scenario where the values diverge from HW.
        //
        NV_ASSERT(NV_SCAL_LITTER_NUM_NVDECS_PER_DIELET == 4);
        NV_ASSERT(NV_SCAL_LITTER_NUM_NVJPGS_PER_DIELET == 4);
        NV_ASSERT(NV_SCAL_LITTER_NUM_OFAS_PER_DIELET == 1);

        switch (pMmuExceptInfo->clientId)
        {
            case NV_PFAULT_CLIENT_HUB_VIP:
                return "HUBCLIENT_VIP";
            case NV_PFAULT_CLIENT_HUB_CE0:
                return "HUBCLIENT_CE0";
            case NV_PFAULT_CLIENT_HUB_CE1:
                return "HUBCLIENT_CE1";
            case NV_PFAULT_CLIENT_HUB_CE2:
                return "HUBCLIENT_CE2";
            case NV_PFAULT_CLIENT_HUB_CE3:
                return "HUBCLIENT_CE3";
            case NV_PFAULT_CLIENT_HUB_DNISO:
                return "HUBCLIENT_DNISO";
            case NV_PFAULT_CLIENT_HUB_FE:
                return "HUBCLIENT_FE";
            case NV_PFAULT_CLIENT_HUB_FECS:
                return "HUBCLIENT_FECS";
            case NV_PFAULT_CLIENT_HUB_ISO:
                return "HUBCLIENT_ISO";
            case NV_PFAULT_CLIENT_HUB_MMU:
                return "HUBCLIENT_MMU";
            case NV_PFAULT_CLIENT_HUB_NVDEC0:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_NVDEC0";
                    case 1:
                        return "HUBCLIENT_NVDEC4";
                }
            }
            case NV_PFAULT_CLIENT_HUB_NVDEC1:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_NVDEC1";
                    case 1:
                        return "HUBCLIENT_NVDEC5";
                }
            }
            case NV_PFAULT_CLIENT_HUB_NVDEC2:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_NVDEC2";
                    case 1:
                        return "HUBCLIENT_NVDEC6";
                }
            }
            case NV_PFAULT_CLIENT_HUB_NVDEC3:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_NVDEC3";
                    case 1:
                        return "HUBCLIENT_NVDEC7";
                }
            }
            case NV_PFAULT_CLIENT_HUB_NVENC1:
                return "HUBCLIENT_NVENC1";
            case NV_PFAULT_CLIENT_HUB_NISO:
                return "HUBCLIENT_NISO";
            case NV_PFAULT_CLIENT_HUB_P2P:
                return "HUBCLIENT_P2P";
            case NV_PFAULT_CLIENT_HUB_PD:
                return "HUBCLIENT_PD";
            case NV_PFAULT_CLIENT_HUB_PERF0:
                return "HUBCLIENT_PERF";
            case NV_PFAULT_CLIENT_HUB_PMU:
                return "HUBCLIENT_PMU";
            case NV_PFAULT_CLIENT_HUB_RASTERTWOD:
                return "HUBCLIENT_RASTERTWOD";
            case NV_PFAULT_CLIENT_HUB_SCC:
                return "HUBCLIENT_SCC";
            case NV_PFAULT_CLIENT_HUB_SCC_NB:
                return "HUBCLIENT_SCC_NB";
            case NV_PFAULT_CLIENT_HUB_SEC:
                return "HUBCLIENT_SEC";
            case NV_PFAULT_CLIENT_HUB_SSYNC:
                return "HUBCLIENT_SSYNC";
            case NV_PFAULT_CLIENT_HUB_XV:
                return "HUBCLIENT_XV";
            case NV_PFAULT_CLIENT_HUB_MMU_NB:
                return "HUBCLIENT_MMU_NB";
            case NV_PFAULT_CLIENT_HUB_DFALCON:
                return "HUBCLIENT_DFALCON";
            case NV_PFAULT_CLIENT_HUB_SKED:
                return "HUBCLIENT_SKED";
            case NV_PFAULT_CLIENT_HUB_DONT_CARE:
                return "HUBCLIENT_DONT_CARE";
            case NV_PFAULT_CLIENT_HUB_HSCE0:
                return "HUBCLIENT_HSCE0";
            case NV_PFAULT_CLIENT_HUB_HSCE1:
                return "HUBCLIENT_HSCE1";
            case NV_PFAULT_CLIENT_HUB_HSCE2:
                return "HUBCLIENT_HSCE2";
            case NV_PFAULT_CLIENT_HUB_HSCE3:
                return "HUBCLIENT_HSCE3";
            case NV_PFAULT_CLIENT_HUB_HSCE4:
                return "HUBCLIENT_HSCE4";
            case NV_PFAULT_CLIENT_HUB_HSCE5:
                return "HUBCLIENT_HSCE5";
            case NV_PFAULT_CLIENT_HUB_HSCE6:
                return "HUBCLIENT_HSCE6";
            case NV_PFAULT_CLIENT_HUB_HSCE7:
                return "HUBCLIENT_HSCE7";
            case NV_PFAULT_CLIENT_HUB_HSHUB:
                return "HUBCLIENT_HSHUB";
            case NV_PFAULT_CLIENT_HUB_PTP_X0:
                return "HUBCLIENT_PTP_X0";
            case NV_PFAULT_CLIENT_HUB_PTP_X1:
                return "HUBCLIENT_PTP_X1";
            case NV_PFAULT_CLIENT_HUB_PTP_X2:
                return "HUBCLIENT_PTP_X2";
            case NV_PFAULT_CLIENT_HUB_PTP_X3:
                return "HUBCLIENT_PTP_X3";
            case NV_PFAULT_CLIENT_HUB_PTP_X4:
                return "HUBCLIENT_PTP_X4";
            case NV_PFAULT_CLIENT_HUB_PTP_X5:
                return "HUBCLIENT_PTP_X5";
            case NV_PFAULT_CLIENT_HUB_NVENC2:
                return "HUBCLIENT_NVENC2";
            case NV_PFAULT_CLIENT_HUB_VPR_SCRUBBER0:
                return "HUBCLIENT_VPR_SCRUBBER0";
            case NV_PFAULT_CLIENT_HUB_VPR_SCRUBBER1:
                return "HUBCLIENT_VPR_SCRUBBER1";
            case NV_PFAULT_CLIENT_HUB_FBFALCON:
                return "HUBCLIENT_FBFALCON";
            case NV_PFAULT_CLIENT_HUB_CE_SHIM:
                return "HUBCLIENT_CE_SHIM";
            case NV_PFAULT_CLIENT_HUB_GSP:
                return "HUBCLIENT_GSP";
            case NV_PFAULT_CLIENT_HUB_FSP:
                return "HUBCLIENT_FSP";
            case NV_PFAULT_CLIENT_HUB_NVJPG0:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_NVJPG0";
                    case 1:
                        return "HUBCLIENT_NVJPG4";
                }
            }
            case NV_PFAULT_CLIENT_HUB_NVJPG1:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_NVJPG1";
                    case 1:
                        return "HUBCLIENT_NVJPG5";
                }
            }
            case NV_PFAULT_CLIENT_HUB_NVJPG2:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_NVJPG2";
                    case 1:
                        return "HUBCLIENT_NVJPG6";
                }
            }
            case NV_PFAULT_CLIENT_HUB_NVJPG3:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_NVJPG3";
                    case 1:
                        return "HUBCLIENT_NVJPG7";
                }
            }
            case NV_PFAULT_CLIENT_HUB_OFA0:
            {
                switch(pMmuExceptInfo->gpcId)
                {
                    case 0:
                        return "HUBCLIENT_OFA0";
                    case 1:
                        return "HUBCLIENT_OFA1";
                }
            }
            case NV_PFAULT_CLIENT_HUB_FE1:
                return "HUBCLIENT_FE1";
            case NV_PFAULT_CLIENT_HUB_FE2:
                return "HUBCLIENT_FE2";
            case NV_PFAULT_CLIENT_HUB_FE3:
                return "HUBCLIENT_FE3";
            case NV_PFAULT_CLIENT_HUB_FECS1:
                return "HUBCLIENT_FECS1";
            case NV_PFAULT_CLIENT_HUB_FECS2:
                return "HUBCLIENT_FECS2";
            case NV_PFAULT_CLIENT_HUB_FECS3:
                return "HUBCLIENT_FECS3";
            case NV_PFAULT_CLIENT_HUB_SKED1:
                return "HUBCLIENT_SKED1";
            case NV_PFAULT_CLIENT_HUB_SKED2:
                return "HUBCLIENT_SKED2";
            case NV_PFAULT_CLIENT_HUB_SKED3:
                return "HUBCLIENT_SKED3";
            case NV_PFAULT_CLIENT_HUB_ESC:
                return "HUBCLIENT_ESC";
            default:
                return kfifoGetClientIdStringCheck_HAL(pGpu, pKernelFifo, pMmuExceptInfo->clientId);
        }
    }
}

