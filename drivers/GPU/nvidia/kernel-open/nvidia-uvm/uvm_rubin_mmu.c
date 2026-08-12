/*******************************************************************************
    Copyright (c) 2024-2025 NVIDIA Corporation

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be
        included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "uvm_types.h"
#include "uvm_global.h"
#include "uvm_hal.h"
#include "uvm_rubin_fault_buffer.h"
#include "hwref/rubin/gr100/dev_fault.h"
#include "hwref/rubin/gr100/dev_mmu.h"

static uvm_mmu_mode_hal_t rubin_mmu_mode_hal;

NvU16 uvm_hal_rubin_mmu_client_id_to_utlb_id(NvU16 client_id)
{
    switch (client_id) {
        case NV_PFAULT_CLIENT_GPC_RAST:
        case NV_PFAULT_CLIENT_GPC_GCC:
        case NV_PFAULT_CLIENT_GPC_GPCCS:
            return UVM_RUBIN_GPC_UTLB_ID_RGG;
        case NV_PFAULT_CLIENT_GPC_T1_0:
            return UVM_RUBIN_GPC_UTLB_ID_LTP0;
        case NV_PFAULT_CLIENT_GPC_T1_1:
        case NV_PFAULT_CLIENT_GPC_PE_0:
        case NV_PFAULT_CLIENT_GPC_TPCCS_0:
            return UVM_RUBIN_GPC_UTLB_ID_LTP1;
        case NV_PFAULT_CLIENT_GPC_T1_2:
            return UVM_RUBIN_GPC_UTLB_ID_LTP2;
        case NV_PFAULT_CLIENT_GPC_T1_3:
        case NV_PFAULT_CLIENT_GPC_PE_1:
        case NV_PFAULT_CLIENT_GPC_TPCCS_1:
            return UVM_RUBIN_GPC_UTLB_ID_LTP3;
        case NV_PFAULT_CLIENT_GPC_T1_4:
            return UVM_RUBIN_GPC_UTLB_ID_LTP4;
        case NV_PFAULT_CLIENT_GPC_T1_5:
        case NV_PFAULT_CLIENT_GPC_PE_2:
        case NV_PFAULT_CLIENT_GPC_TPCCS_2:
            return UVM_RUBIN_GPC_UTLB_ID_LTP5;
        case NV_PFAULT_CLIENT_GPC_T1_6:
            return UVM_RUBIN_GPC_UTLB_ID_LTP6;
        case NV_PFAULT_CLIENT_GPC_T1_7:
        case NV_PFAULT_CLIENT_GPC_PE_3:
        case NV_PFAULT_CLIENT_GPC_TPCCS_3:
            return UVM_RUBIN_GPC_UTLB_ID_LTP7;
        case NV_PFAULT_CLIENT_GPC_T1_8:
            return UVM_RUBIN_GPC_UTLB_ID_LTP8;
        case NV_PFAULT_CLIENT_GPC_T1_9:
        case NV_PFAULT_CLIENT_GPC_PE_4:
        case NV_PFAULT_CLIENT_GPC_TPCCS_4:
            return UVM_RUBIN_GPC_UTLB_ID_LTP9;
        case NV_PFAULT_CLIENT_GPC_T1_10:
            return UVM_RUBIN_GPC_UTLB_ID_LTP10;
        case NV_PFAULT_CLIENT_GPC_T1_11:
        case NV_PFAULT_CLIENT_GPC_PE_5:
        case NV_PFAULT_CLIENT_GPC_TPCCS_5:
            return UVM_RUBIN_GPC_UTLB_ID_LTP11;
        case NV_PFAULT_CLIENT_GPC_T1_12:
            return UVM_RUBIN_GPC_UTLB_ID_LTP12;
        case NV_PFAULT_CLIENT_GPC_T1_13:
        case NV_PFAULT_CLIENT_GPC_PE_6:
        case NV_PFAULT_CLIENT_GPC_TPCCS_6:
            return UVM_RUBIN_GPC_UTLB_ID_LTP13;
        case NV_PFAULT_CLIENT_GPC_T1_14:
            return UVM_RUBIN_GPC_UTLB_ID_LTP14;
        case NV_PFAULT_CLIENT_GPC_T1_15:
        case NV_PFAULT_CLIENT_GPC_PE_7:
        case NV_PFAULT_CLIENT_GPC_TPCCS_7:
            return UVM_RUBIN_GPC_UTLB_ID_LTP15;
        case NV_PFAULT_CLIENT_GPC_T1_16:
            return UVM_RUBIN_GPC_UTLB_ID_LTP16;
        case NV_PFAULT_CLIENT_GPC_T1_17:
        case NV_PFAULT_CLIENT_GPC_PE_8:
        case NV_PFAULT_CLIENT_GPC_TPCCS_8:
            return UVM_RUBIN_GPC_UTLB_ID_LTP17;
        case NV_PFAULT_CLIENT_GPC_T1_18:
            return UVM_RUBIN_GPC_UTLB_ID_LTP18;
        case NV_PFAULT_CLIENT_GPC_T1_19:
        case NV_PFAULT_CLIENT_GPC_PE_9:
        case NV_PFAULT_CLIENT_GPC_TPCCS_9:
            return UVM_RUBIN_GPC_UTLB_ID_LTP19;
        case NV_PFAULT_CLIENT_GPC_T1_20:
            return UVM_RUBIN_GPC_UTLB_ID_LTP20;
        case NV_PFAULT_CLIENT_GPC_T1_21:
        case NV_PFAULT_CLIENT_GPC_PE_10:
        case NV_PFAULT_CLIENT_GPC_TPCCS_10:
            return UVM_RUBIN_GPC_UTLB_ID_LTP21;
        case NV_PFAULT_CLIENT_GPC_T1_22:
            return UVM_RUBIN_GPC_UTLB_ID_LTP22;
        case NV_PFAULT_CLIENT_GPC_T1_23:
        case NV_PFAULT_CLIENT_GPC_PE_11:
        case NV_PFAULT_CLIENT_GPC_TPCCS_11:
            return UVM_RUBIN_GPC_UTLB_ID_LTP23;
        case NV_PFAULT_CLIENT_GPC_T1_24:
            return UVM_RUBIN_GPC_UTLB_ID_LTP24;
        case NV_PFAULT_CLIENT_GPC_T1_25:
        case NV_PFAULT_CLIENT_GPC_PE_12:
        case NV_PFAULT_CLIENT_GPC_TPCCS_12:
            return UVM_RUBIN_GPC_UTLB_ID_LTP25;
        case NV_PFAULT_CLIENT_GPC_T1_26:
            return UVM_RUBIN_GPC_UTLB_ID_LTP26;
        case NV_PFAULT_CLIENT_GPC_T1_27:
        case NV_PFAULT_CLIENT_GPC_PE_13:
        case NV_PFAULT_CLIENT_GPC_TPCCS_13:
            return UVM_RUBIN_GPC_UTLB_ID_LTP27;
        case NV_PFAULT_CLIENT_GPC_T1_28:
            return UVM_RUBIN_GPC_UTLB_ID_LTP28;
        case NV_PFAULT_CLIENT_GPC_T1_29:
        case NV_PFAULT_CLIENT_GPC_PE_14:
        case NV_PFAULT_CLIENT_GPC_TPCCS_14:
            return UVM_RUBIN_GPC_UTLB_ID_LTP29;
        default:
            UVM_ASSERT_MSG(false, "Invalid client value: 0x%x\n", client_id);
    }

    return 0;
}

// Copied from Hopper's MMU HAL, phys_addr is updated.
static NvU64 poisoned_pte_rubin(uvm_page_tree_t *tree)
{
    // An invalid PTE won't be fatal from faultable units like SM, which is the
    // most likely source of bad PTE accesses.

    // Engines with priv accesses won't fault on the priv PTE, so add a backup
    // mechanism using an impossible memory address. MMU will trigger an
    // interrupt when it detects a bad physical address, i.e., a physical
    // address > GPU memory size/capacity.
    //
    // This address has to fit within 40 bits (max vidmem address width) and be
    // aligned to page_size.
    NvU64 phys_addr = 0xffffbad000ULL;

    NvU64 pte_bits = tree->hal->make_pte(UVM_APERTURE_VID, phys_addr, UVM_PROT_READ_ONLY, UVM_MMU_PTE_FLAGS_NONE);
    return WRITE_HWCONST64(pte_bits, _MMU_VER3, PTE, PCF, PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACD);
}

uvm_mmu_mode_hal_t *uvm_hal_mmu_mode_rubin(void)
{
    static bool initialized = false;

    if (!initialized) {
        // GB100's Blackwell
        uvm_mmu_mode_hal_t *blackwell_mmu_mode_hal = uvm_hal_mmu_mode_blackwell();
        UVM_ASSERT(blackwell_mmu_mode_hal);

        // The assumption made is that arch_hal->mmu_mode_hal() will be called
        // under the global lock the first time, so check it here.
        uvm_assert_mutex_locked(&g_uvm_global.global_lock);

        rubin_mmu_mode_hal = *blackwell_mmu_mode_hal;
        rubin_mmu_mode_hal.poisoned_pte = poisoned_pte_rubin;

        initialized = true;
    }

    return &rubin_mmu_mode_hal;
}
