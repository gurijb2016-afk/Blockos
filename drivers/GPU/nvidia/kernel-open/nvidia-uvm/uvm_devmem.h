/*******************************************************************************
    Copyright (c) 2015-2026 NVIDIA Corporation

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

#ifndef __UVM_DEVMEM_H__
#define __UVM_DEVMEM_H__

#include "uvm_forward_decl.h"
#include "uvm_linux.h"
#include "uvm_types.h"
#include "uvm_va_space.h"

#include <linux/memremap.h>

#if defined(CONFIG_PCI_P2PDMA)
typedef struct
{
    // For g_uvm_global.pci_p2pdma_devices
    struct list_head list_node;

    // A reference to the pci device associated with parent gpu
    struct pci_dev *pdev;

    // PCI attributes to lookup pci_dev during exit. On cleanup, this will be used to find/get a
    // reference on pci_dev
    unsigned int domain;
    unsigned int bus;
    unsigned int func;

    // Starting pfn for the device, used to detect if a pgmap is already present
    unsigned long dev_start_pfn;
} uvm_pmm_gpu_pci_dev_list_t;

#endif
void uvm_devmem_pci_p2pdma_cache_exit(void);

#if UVM_IS_CONFIG_HMM() || defined(NV_MEMORY_DEVICE_COHERENT_PRESENT)
struct uvm_pmm_gpu_devmem_struct
{
    // For g_uvm_global.devmem_ranges
    struct list_head list_node;

    // Size that was requested when created this region. This may be less than
    // the size actually allocated by the kernel due to alignment contraints.
    // Figuring out the required alignment at compile time is difficult due to
    // unexported macros, so just use the requested size as the search key.
    unsigned long size;

    struct dev_pagemap pagemap;
};

// Return the GPU chunk for a given device private struct page.
uvm_gpu_chunk_t *uvm_devmem_page_to_chunk(struct page *page);

#endif

#if UVM_IS_CONFIG_HMM()
typedef struct uvm_pmm_gpu_struct uvm_pmm_gpu_t;

// Return the va_space for a given device private struct page.
uvm_va_space_t *uvm_devmem_page_to_va_space(struct page *page);

// Return the GPU id for a given device private struct page.
uvm_gpu_id_t uvm_devmem_page_to_gpu_id(struct page *page);

// Return the PFN of the device private struct page for the given GPU chunk.
unsigned long uvm_devmem_get_gpu_pfn(uvm_pmm_gpu_t *pmm, uvm_gpu_chunk_t *chunk);
#endif

static inline NV_STATUS uvm_devmem_get_page_ref_if_device_coherent(struct page *page)
{
#if UVM_CDMM_PAGES_SUPPORTED()
    // RM doesn't use DEVICE_COHERENT pages and therefore won't already hold
    // a reference to them, so take one now if using DEVICE_COHERENT pages.
    if (is_device_coherent_page(page)) {
        if (!page_ref_count(page)) {
            set_page_count(page, 1);
            NV_GET_DEV_PAGEMAP(page_to_pfn(page));
        } else {
            UVM_ASSERT(0);
            // Shared mappings are not supported with managed memory
            return NV_ERR_NOT_SUPPORTED;
        }
    }
#endif
    return NV_OK;
}

static inline void uvm_devmem_put_page_ref_if_device_coherent(struct page *page)
{
#if UVM_CDMM_PAGES_SUPPORTED()
    // Drop the reference taken before vm_insert_page()
    if (is_device_coherent_page(page))
        put_page(page);
#endif
}

static inline bool uvm_is_device_page(struct page *page)
{
#if UVM_CDMM_PAGES_SUPPORTED()
    return is_device_private_page(page) || is_device_coherent_page(page);
#else
    return is_device_private_page(page);
#endif
}

static inline bool uvm_devmem_cdmm_present(uvm_parent_gpu_t *parent_gpu)
{
    if (!parent_gpu->cdmm_enabled)
        return false;

#if UVM_CDMM_PAGES_SUPPORTED()
    return parent_gpu->devmem != NULL;
#else
    return false;
#endif
}

// Allocate and initialise struct page data in the kernel to support HMM.
NV_STATUS uvm_devmem_init(uvm_parent_gpu_t *gpu);
void uvm_devmem_deinit(uvm_parent_gpu_t *parent_gpu);

void uvm_devmem_device_p2p_init(uvm_parent_gpu_t *gpu);
void uvm_devmem_device_p2p_deinit(uvm_parent_gpu_t *gpu);

// Free unused ZONE_DEVICE pages.
void uvm_devmem_exit(void);

bool uvm_devmem_check_orphan_pages(uvm_pmm_gpu_t *pmm);

// This represents a physical RM allocation used for device peer-to-peer access.
// This is distinct from the va_range type because the physical allocations can be
// shared by multiple va_ranges.
typedef struct
{
    // The physical GPU memory backing device P2P ranges can be referenced by
    // two entities - kernel device drivers and the va_range(s) that UVM uses
    // to create CPU mappings. The physical memory can not be freed until after
    // both entities have finished accessing it. This refcount tracks total
    // kernel users of the memory.
    nv_kref_t refcount;

    // The number of va ranges referencing this physical range.
    nv_kref_t va_range_count;

    uvm_gpu_t *gpu;
    NvHandle rm_memory_handle;
    NvU64 *pfns;
    NvU64 pfn_count;
    NvLength page_size;
    uvm_deferred_free_object_t deferred_free;
    struct list_head *deferred_free_list;
    wait_queue_head_t waitq;
} uvm_device_p2p_mem_t;

// Assigned to page->zone_device_data for DEVICE_COHERENT pages where these
// could be used to track a device p2p allocation made with cudaMalloc() or
// system allocated memory in a va_block.
typedef struct {
    uvm_gpu_chunk_t *gpu_chunk;
    uvm_device_p2p_mem_t *p2p_mem;
} uvm_coherent_devmem_page_t;

NV_STATUS uvm_devmem_global_init(void);
void uvm_devmem_global_deinit(void);

#if (defined(CONFIG_PCI_P2PDMA) || UVM_CDMM_PAGES_SUPPORTED()) && defined(NV_STRUCT_PAGE_HAS_ZONE_DEVICE_DATA)
extern struct kmem_cache *g_uvm_coherent_devmem_page_cache;

// page->zone_device_data does not exist in kernels versions older than v5.3
// which don't support CONFIG_PCI_P2PDMA. Therefore we need these accessor
// functions to ensure compilation succeeeds on older kernels.
static void page_set_zone_device_p2p_data(struct page *page, uvm_device_p2p_mem_t *p2p_mem)
{
#if UVM_CDMM_PAGES_SUPPORTED()
    if (is_device_coherent_page(page)) {
        uvm_coherent_devmem_page_t *coherent_devmem_page;

        if (!page->zone_device_data) {
            coherent_devmem_page = kmem_cache_zalloc(g_uvm_coherent_devmem_page_cache, NV_UVM_GFP_FLAGS);
            page->zone_device_data = coherent_devmem_page;
            WARN_ON(!coherent_devmem_page);
        }

        coherent_devmem_page->p2p_mem = p2p_mem;
    }
    else {
        page->zone_device_data = p2p_mem;
    }
#else
    page->zone_device_data = p2p_mem;
#endif
}

static uvm_device_p2p_mem_t *page_get_zone_device_p2p_data(struct page *page)
{
    if (!page->zone_device_data)
        return NULL;
#if UVM_CDMM_PAGES_SUPPORTED()
    else if (is_device_coherent_page(page))
        return ((uvm_coherent_devmem_page_t *) page->zone_device_data)->p2p_mem;
#endif
    else
        return page->zone_device_data;
}

static void page_set_zone_device_chunk_data(struct page *page, uvm_gpu_chunk_t *gpu_chunk)
{
#if UVM_CDMM_PAGES_SUPPORTED()
    if (is_device_coherent_page(page)) {
        uvm_coherent_devmem_page_t *coherent_devmem_page;

        if (!page->zone_device_data) {
            coherent_devmem_page = kmem_cache_zalloc(g_uvm_coherent_devmem_page_cache, NV_UVM_GFP_FLAGS);
            page->zone_device_data = coherent_devmem_page;
            WARN_ON(!coherent_devmem_page);
        }

        coherent_devmem_page->gpu_chunk = gpu_chunk;
    }
    else {
        page->zone_device_data = gpu_chunk;
    }
#else
    page->zone_device_data = gpu_chunk;
#endif
}

static uvm_gpu_chunk_t *page_get_zone_device_chunk_data(struct page *page)
{
    if (!page->zone_device_data)
        return NULL;
#if UVM_CDMM_PAGES_SUPPORTED()
    else if (is_device_coherent_page(page))
        return ((uvm_coherent_devmem_page_t *) page->zone_device_data)->gpu_chunk;
#endif
    else
        return page->zone_device_data;
}
#else
static void page_set_zone_device_p2p_data(struct page *page, uvm_device_p2p_mem_t *zone_device_data)
{
    UVM_ASSERT(0);
}

static uvm_device_p2p_mem_t *page_get_zone_device_p2p_data(struct page *page)
{
    UVM_ASSERT(0);
    return NULL;
}
#endif
#endif
