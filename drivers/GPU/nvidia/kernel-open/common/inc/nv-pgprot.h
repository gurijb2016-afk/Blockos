/*
 * SPDX-FileCopyrightText: Copyright (c) 2015-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef __NV_PGPROT_H__

#define __NV_PGPROT_H__

#include "cpuopsys.h"

#include <linux/mm.h>

#if !defined(NV_VMWARE)
#if defined(NVCPU_X86_64)
static inline pgprot_t pgprot_modify_writecombine(pgprot_t old_prot)
{
    return __pgprot((pgprot_val(old_prot) & ~_PAGE_CACHE_MASK) |
                    cachemode2protval(_PAGE_CACHE_MODE_WC));
}
#endif /* defined(NVCPU_X86_64) */
#endif /* !defined(NV_VMWARE) */

#if defined(NVCPU_AARCH64)
extern NvBool nvos_is_chipset_io_coherent(void);
/*
 * Don't rely on the kernel's definition of pgprot_noncached(), as on 64-bit
 * ARM that's not for system memory, but device memory instead.
 */
#define NV_PGPROT_UNCACHED(old_prot)   \
     __pgprot_modify((old_prot), PTE_ATTRINDX_MASK, PTE_ATTRINDX(MT_NORMAL_NC))
#else
/*
 * Note: the kernel's implementation of pgprot_noncached() on x86-64 evaluates to
 *       UC- (noncached weak ordering) instead of strict UC.
 */
#define NV_PGPROT_UNCACHED(old_prot)          pgprot_noncached(old_prot)
#endif

#define NV_PGPROT_UNCACHED_DEVICE(old_prot)     pgprot_noncached(old_prot)
#if defined(NVCPU_AARCH64)
#define NV_PGPROT_WRITE_COMBINED(old_prot)      NV_PGPROT_UNCACHED(old_prot)
#define NV_PGPROT_READ_ONLY(old_prot)                                         \
            __pgprot_modify(old_prot, 0, PTE_RDONLY)
#elif defined(NVCPU_X86_64)
#define NV_PGPROT_UNCACHED_WEAK(old_prot)       pgprot_noncached(old_prot)
#define NV_PGPROT_WRITE_COMBINED(old_prot)                                    \
    pgprot_modify_writecombine(old_prot)
#define NV_PGPROT_READ_ONLY(old_prot)                                         \
    __pgprot(pgprot_val((old_prot)) & ~_PAGE_RW)
#elif defined(NVCPU_RISCV64)
#define NV_PGPROT_WRITE_COMBINED(old_prot)                                    \
    pgprot_writecombine(old_prot)
#define NV_PGPROT_READ_ONLY(old_prot)                                         \
            __pgprot(pgprot_val((old_prot)) & ~_PAGE_WRITE)
#else
/* Writecombine is not supported */
#undef NV_PGPROT_WRITE_COMBINED(old_prot)
#define NV_PGPROT_READ_ONLY(old_prot)
#endif

#endif /* __NV_PGPROT_H__ */
