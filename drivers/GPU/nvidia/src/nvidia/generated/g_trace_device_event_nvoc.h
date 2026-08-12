
#ifndef _G_TRACE_DEVICE_EVENT_NVOC_H_
#define _G_TRACE_DEVICE_EVENT_NVOC_H_

// Version of generated metadata structures
#ifdef NVOC_METADATA_VERSION
#undef NVOC_METADATA_VERSION
#endif
#define NVOC_METADATA_VERSION 2

#include "nvoc/runtime.h"
#include "nvoc/rtti.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/******************************************************************************
 *
 *   Description:
 *      This file contains functions to grant trace device event capability
 *
 *   Key attributes of TraceDeviceEvent class:
 *   - hClient is parent of TraceDeviceEvent.
 *   - TraceDeviceEvent allocation requires a privileged client only if
 *     the platform doesn't implement this capability
 *   - RmApi lock must be held.
 *****************************************************************************/

#pragma once
#include "g_trace_device_event_nvoc.h"

#ifndef TRACE_CONTEXT_EVENT_H
#define TRACE_CONTEXT_EVENT_H


#include "rmapi/resource.h"


// ****************************************************************************
//                          Type Definitions
// ****************************************************************************


// Private field names are wrapped in PRIVATE_FIELD, which does nothing for
// the matching C source file, but causes diagnostics to be issued if another
// source file references the field.
#ifdef NVOC_TRACE_DEVICE_EVENT_H_PRIVATE_ACCESS_ALLOWED
#define PRIVATE_FIELD(x) x
#else
#define PRIVATE_FIELD(x) NVOC_PRIVATE_FIELD(x)
#endif


// Metadata with per-class RTTI and vtable with ancestor(s)
struct NVOC_METADATA__TraceDeviceEvent;
struct NVOC_METADATA__RmResource;
struct NVOC_VTABLE__TraceDeviceEvent;


struct TraceDeviceEvent {

    // Metadata starts with RTTI structure.
    union {
         const struct NVOC_METADATA__TraceDeviceEvent *__nvoc_metadata_ptr;
         const struct NVOC_RTTI *__nvoc_rtti;
    };

    // Parent (i.e. superclass or base class) objects
    struct RmResource __nvoc_base_RmResource;

    // Ancestor object pointers for `staticCast` feature
    struct Object *__nvoc_pbase_Object;    // obj super^3
    struct RsResource *__nvoc_pbase_RsResource;    // res super^2
    struct RmResourceCommon *__nvoc_pbase_RmResourceCommon;    // rmrescmn super^2
    struct RmResource *__nvoc_pbase_RmResource;    // rmres super
    struct TraceDeviceEvent *__nvoc_pbase_TraceDeviceEvent;    // traceDeviceEvent

    // Data members
    NvU64 PRIVATE_FIELD(dupedCapDescriptor);
};


// Vtable with 21 per-class function pointers
struct NVOC_VTABLE__TraceDeviceEvent {
    NvBool (*__traceDeviceEventCanCopy__)(struct TraceDeviceEvent * /*this*/);  // virtual override (res) base (rmres)
    NvBool (*__traceDeviceEventAccessCallback__)(struct TraceDeviceEvent * /*this*/, struct RsClient *, void *, RsAccessRight);  // virtual inherited (rmres) base (rmres)
    NvBool (*__traceDeviceEventShareCallback__)(struct TraceDeviceEvent * /*this*/, struct RsClient *, struct RsResourceRef *, RS_SHARE_POLICY *);  // virtual inherited (rmres) base (rmres)
    NV_STATUS (*__traceDeviceEventGetMemInterMapParams__)(struct TraceDeviceEvent * /*this*/, RMRES_MEM_INTER_MAP_PARAMS *);  // virtual inherited (rmres) base (rmres)
    NV_STATUS (*__traceDeviceEventCheckMemInterUnmap__)(struct TraceDeviceEvent * /*this*/, NvBool);  // virtual inherited (rmres) base (rmres)
    NV_STATUS (*__traceDeviceEventGetMemoryMappingDescriptor__)(struct TraceDeviceEvent * /*this*/, struct MEMORY_DESCRIPTOR **);  // virtual inherited (rmres) base (rmres)
    NV_STATUS (*__traceDeviceEventControlSerialization_Prologue__)(struct TraceDeviceEvent * /*this*/, struct CALL_CONTEXT *, struct RS_RES_CONTROL_PARAMS_INTERNAL *);  // virtual inherited (rmres) base (rmres)
    void (*__traceDeviceEventControlSerialization_Epilogue__)(struct TraceDeviceEvent * /*this*/, struct CALL_CONTEXT *, struct RS_RES_CONTROL_PARAMS_INTERNAL *);  // virtual inherited (rmres) base (rmres)
    NV_STATUS (*__traceDeviceEventControl_Prologue__)(struct TraceDeviceEvent * /*this*/, struct CALL_CONTEXT *, struct RS_RES_CONTROL_PARAMS_INTERNAL *);  // virtual inherited (rmres) base (rmres)
    void (*__traceDeviceEventControl_Epilogue__)(struct TraceDeviceEvent * /*this*/, struct CALL_CONTEXT *, struct RS_RES_CONTROL_PARAMS_INTERNAL *);  // virtual inherited (rmres) base (rmres)
    NV_STATUS (*__traceDeviceEventIsDuplicate__)(struct TraceDeviceEvent * /*this*/, NvHandle, NvBool *);  // virtual inherited (res) base (rmres)
    void (*__traceDeviceEventPreDestruct__)(struct TraceDeviceEvent * /*this*/);  // virtual inherited (res) base (rmres)
    NV_STATUS (*__traceDeviceEventControl__)(struct TraceDeviceEvent * /*this*/, struct CALL_CONTEXT *, struct RS_RES_CONTROL_PARAMS_INTERNAL *);  // virtual inherited (res) base (rmres)
    NV_STATUS (*__traceDeviceEventControlFilter__)(struct TraceDeviceEvent * /*this*/, struct CALL_CONTEXT *, struct RS_RES_CONTROL_PARAMS_INTERNAL *);  // virtual inherited (res) base (rmres)
    NV_STATUS (*__traceDeviceEventMap__)(struct TraceDeviceEvent * /*this*/, struct CALL_CONTEXT *, RS_CPU_MAP_PARAMS *, RsCpuMapping *);  // virtual inherited (res) base (rmres)
    NV_STATUS (*__traceDeviceEventUnmap__)(struct TraceDeviceEvent * /*this*/, struct CALL_CONTEXT *, RsCpuMapping *);  // virtual inherited (res) base (rmres)
    NvBool (*__traceDeviceEventIsPartialUnmapSupported__)(struct TraceDeviceEvent * /*this*/);  // inline virtual inherited (res) base (rmres) body
    NV_STATUS (*__traceDeviceEventMapTo__)(struct TraceDeviceEvent * /*this*/, RS_RES_MAP_TO_PARAMS *);  // virtual inherited (res) base (rmres)
    NV_STATUS (*__traceDeviceEventUnmapFrom__)(struct TraceDeviceEvent * /*this*/, RS_RES_UNMAP_FROM_PARAMS *);  // virtual inherited (res) base (rmres)
    NvU32 (*__traceDeviceEventGetRefCount__)(struct TraceDeviceEvent * /*this*/);  // virtual inherited (res) base (rmres)
    void (*__traceDeviceEventAddAdditionalDependants__)(struct RsClient *, struct TraceDeviceEvent * /*this*/, RsResourceRef *);  // virtual inherited (res) base (rmres)
};

// Metadata with per-class RTTI and vtable with ancestor(s)
struct NVOC_METADATA__TraceDeviceEvent {
    const struct NVOC_RTTI rtti;
    const struct NVOC_METADATA__RmResource metadata__RmResource;
    const struct NVOC_VTABLE__TraceDeviceEvent vtable;
};

#ifndef __nvoc_class_id_TraceDeviceEvent
#define __nvoc_class_id_TraceDeviceEvent 0x64ce63u
typedef struct TraceDeviceEvent TraceDeviceEvent;
#endif /* __nvoc_class_id_TraceDeviceEvent */

// Casting support
extern const struct NVOC_CLASS_DEF __nvoc_class_def_TraceDeviceEvent;

#define __staticCast_TraceDeviceEvent(pThis) \
    ((pThis)->__nvoc_pbase_TraceDeviceEvent)

#ifdef __nvoc_trace_device_event_h_disabled
#define __dynamicCast_TraceDeviceEvent(pThis) ((TraceDeviceEvent*) NULL)
#else //__nvoc_trace_device_event_h_disabled
#define __dynamicCast_TraceDeviceEvent(pThis) \
    ((TraceDeviceEvent*) __nvoc_dynamicCast(staticCast((pThis), Dynamic), classInfo(TraceDeviceEvent)))
#endif //__nvoc_trace_device_event_h_disabled

NV_STATUS __nvoc_objCreateDynamic_TraceDeviceEvent(Dynamic**, Dynamic*, NvU32, va_list);

NV_STATUS __nvoc_objCreate_TraceDeviceEvent(TraceDeviceEvent**, Dynamic*, NvU32, struct CALL_CONTEXT *pCallContext, struct RS_RES_ALLOC_PARAMS_INTERNAL *pParams);
#define __objCreate_TraceDeviceEvent(__nvoc_ppNewObj, __nvoc_pParent, __nvoc_createFlags, pCallContext, pParams) \
    __nvoc_objCreate_TraceDeviceEvent((__nvoc_ppNewObj), staticCast((__nvoc_pParent), Dynamic), (__nvoc_createFlags), pCallContext, pParams)


// Wrapper macros for implementation functions
NV_STATUS traceDeviceEventConstruct_IMPL(struct TraceDeviceEvent *traceDeviceEvent, struct CALL_CONTEXT *pCallContext, struct RS_RES_ALLOC_PARAMS_INTERNAL *pParams);
#define __nvoc_traceDeviceEventConstruct(traceDeviceEvent, pCallContext, pParams) traceDeviceEventConstruct_IMPL(traceDeviceEvent, pCallContext, pParams)

void traceDeviceEventDestruct_IMPL(struct TraceDeviceEvent *traceDeviceEvent);
#define __nvoc_traceDeviceEventDestruct(traceDeviceEvent) traceDeviceEventDestruct_IMPL(traceDeviceEvent)


// Wrapper macros for halified functions
#define traceDeviceEventCanCopy_FNPTR(traceDeviceEvent) traceDeviceEvent->__nvoc_metadata_ptr->vtable.__traceDeviceEventCanCopy__
#define traceDeviceEventCanCopy(traceDeviceEvent) traceDeviceEventCanCopy_DISPATCH(traceDeviceEvent)
#define traceDeviceEventAccessCallback_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresAccessCallback__
#define traceDeviceEventAccessCallback(pResource, pInvokingClient, pAllocParams, accessRight) traceDeviceEventAccessCallback_DISPATCH(pResource, pInvokingClient, pAllocParams, accessRight)
#define traceDeviceEventShareCallback_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresShareCallback__
#define traceDeviceEventShareCallback(pResource, pInvokingClient, pParentRef, pSharePolicy) traceDeviceEventShareCallback_DISPATCH(pResource, pInvokingClient, pParentRef, pSharePolicy)
#define traceDeviceEventGetMemInterMapParams_FNPTR(pRmResource) pRmResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresGetMemInterMapParams__
#define traceDeviceEventGetMemInterMapParams(pRmResource, pParams) traceDeviceEventGetMemInterMapParams_DISPATCH(pRmResource, pParams)
#define traceDeviceEventCheckMemInterUnmap_FNPTR(pRmResource) pRmResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresCheckMemInterUnmap__
#define traceDeviceEventCheckMemInterUnmap(pRmResource, bSubdeviceHandleProvided) traceDeviceEventCheckMemInterUnmap_DISPATCH(pRmResource, bSubdeviceHandleProvided)
#define traceDeviceEventGetMemoryMappingDescriptor_FNPTR(pRmResource) pRmResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresGetMemoryMappingDescriptor__
#define traceDeviceEventGetMemoryMappingDescriptor(pRmResource, ppMemDesc) traceDeviceEventGetMemoryMappingDescriptor_DISPATCH(pRmResource, ppMemDesc)
#define traceDeviceEventControlSerialization_Prologue_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresControlSerialization_Prologue__
#define traceDeviceEventControlSerialization_Prologue(pResource, pCallContext, pParams) traceDeviceEventControlSerialization_Prologue_DISPATCH(pResource, pCallContext, pParams)
#define traceDeviceEventControlSerialization_Epilogue_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresControlSerialization_Epilogue__
#define traceDeviceEventControlSerialization_Epilogue(pResource, pCallContext, pParams) traceDeviceEventControlSerialization_Epilogue_DISPATCH(pResource, pCallContext, pParams)
#define traceDeviceEventControl_Prologue_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresControl_Prologue__
#define traceDeviceEventControl_Prologue(pResource, pCallContext, pParams) traceDeviceEventControl_Prologue_DISPATCH(pResource, pCallContext, pParams)
#define traceDeviceEventControl_Epilogue_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_metadata_ptr->vtable.__rmresControl_Epilogue__
#define traceDeviceEventControl_Epilogue(pResource, pCallContext, pParams) traceDeviceEventControl_Epilogue_DISPATCH(pResource, pCallContext, pParams)
#define traceDeviceEventIsDuplicate_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resIsDuplicate__
#define traceDeviceEventIsDuplicate(pResource, hMemory, pDuplicate) traceDeviceEventIsDuplicate_DISPATCH(pResource, hMemory, pDuplicate)
#define traceDeviceEventPreDestruct_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resPreDestruct__
#define traceDeviceEventPreDestruct(pResource) traceDeviceEventPreDestruct_DISPATCH(pResource)
#define traceDeviceEventControl_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resControl__
#define traceDeviceEventControl(pResource, pCallContext, pParams) traceDeviceEventControl_DISPATCH(pResource, pCallContext, pParams)
#define traceDeviceEventControlFilter_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resControlFilter__
#define traceDeviceEventControlFilter(pResource, pCallContext, pParams) traceDeviceEventControlFilter_DISPATCH(pResource, pCallContext, pParams)
#define traceDeviceEventMap_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resMap__
#define traceDeviceEventMap(pResource, pCallContext, pParams, pCpuMapping) traceDeviceEventMap_DISPATCH(pResource, pCallContext, pParams, pCpuMapping)
#define traceDeviceEventUnmap_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resUnmap__
#define traceDeviceEventUnmap(pResource, pCallContext, pCpuMapping) traceDeviceEventUnmap_DISPATCH(pResource, pCallContext, pCpuMapping)
#define traceDeviceEventIsPartialUnmapSupported_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resIsPartialUnmapSupported__
#define traceDeviceEventIsPartialUnmapSupported(pResource) traceDeviceEventIsPartialUnmapSupported_DISPATCH(pResource)
#define traceDeviceEventMapTo_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resMapTo__
#define traceDeviceEventMapTo(pResource, pParams) traceDeviceEventMapTo_DISPATCH(pResource, pParams)
#define traceDeviceEventUnmapFrom_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resUnmapFrom__
#define traceDeviceEventUnmapFrom(pResource, pParams) traceDeviceEventUnmapFrom_DISPATCH(pResource, pParams)
#define traceDeviceEventGetRefCount_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resGetRefCount__
#define traceDeviceEventGetRefCount(pResource) traceDeviceEventGetRefCount_DISPATCH(pResource)
#define traceDeviceEventAddAdditionalDependants_FNPTR(pResource) pResource->__nvoc_base_RmResource.__nvoc_base_RsResource.__nvoc_metadata_ptr->vtable.__resAddAdditionalDependants__
#define traceDeviceEventAddAdditionalDependants(pClient, pResource, pReference) traceDeviceEventAddAdditionalDependants_DISPATCH(pClient, pResource, pReference)

// Dispatch functions
static inline NvBool traceDeviceEventCanCopy_DISPATCH(struct TraceDeviceEvent *traceDeviceEvent) {
    return traceDeviceEvent->__nvoc_metadata_ptr->vtable.__traceDeviceEventCanCopy__(traceDeviceEvent);
}

static inline NvBool traceDeviceEventAccessCallback_DISPATCH(struct TraceDeviceEvent *pResource, struct RsClient *pInvokingClient, void *pAllocParams, RsAccessRight accessRight) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventAccessCallback__(pResource, pInvokingClient, pAllocParams, accessRight);
}

static inline NvBool traceDeviceEventShareCallback_DISPATCH(struct TraceDeviceEvent *pResource, struct RsClient *pInvokingClient, struct RsResourceRef *pParentRef, RS_SHARE_POLICY *pSharePolicy) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventShareCallback__(pResource, pInvokingClient, pParentRef, pSharePolicy);
}

static inline NV_STATUS traceDeviceEventGetMemInterMapParams_DISPATCH(struct TraceDeviceEvent *pRmResource, RMRES_MEM_INTER_MAP_PARAMS *pParams) {
    return pRmResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventGetMemInterMapParams__(pRmResource, pParams);
}

static inline NV_STATUS traceDeviceEventCheckMemInterUnmap_DISPATCH(struct TraceDeviceEvent *pRmResource, NvBool bSubdeviceHandleProvided) {
    return pRmResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventCheckMemInterUnmap__(pRmResource, bSubdeviceHandleProvided);
}

static inline NV_STATUS traceDeviceEventGetMemoryMappingDescriptor_DISPATCH(struct TraceDeviceEvent *pRmResource, struct MEMORY_DESCRIPTOR **ppMemDesc) {
    return pRmResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventGetMemoryMappingDescriptor__(pRmResource, ppMemDesc);
}

static inline NV_STATUS traceDeviceEventControlSerialization_Prologue_DISPATCH(struct TraceDeviceEvent *pResource, struct CALL_CONTEXT *pCallContext, struct RS_RES_CONTROL_PARAMS_INTERNAL *pParams) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventControlSerialization_Prologue__(pResource, pCallContext, pParams);
}

static inline void traceDeviceEventControlSerialization_Epilogue_DISPATCH(struct TraceDeviceEvent *pResource, struct CALL_CONTEXT *pCallContext, struct RS_RES_CONTROL_PARAMS_INTERNAL *pParams) {
    pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventControlSerialization_Epilogue__(pResource, pCallContext, pParams);
}

static inline NV_STATUS traceDeviceEventControl_Prologue_DISPATCH(struct TraceDeviceEvent *pResource, struct CALL_CONTEXT *pCallContext, struct RS_RES_CONTROL_PARAMS_INTERNAL *pParams) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventControl_Prologue__(pResource, pCallContext, pParams);
}

static inline void traceDeviceEventControl_Epilogue_DISPATCH(struct TraceDeviceEvent *pResource, struct CALL_CONTEXT *pCallContext, struct RS_RES_CONTROL_PARAMS_INTERNAL *pParams) {
    pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventControl_Epilogue__(pResource, pCallContext, pParams);
}

static inline NV_STATUS traceDeviceEventIsDuplicate_DISPATCH(struct TraceDeviceEvent *pResource, NvHandle hMemory, NvBool *pDuplicate) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventIsDuplicate__(pResource, hMemory, pDuplicate);
}

static inline void traceDeviceEventPreDestruct_DISPATCH(struct TraceDeviceEvent *pResource) {
    pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventPreDestruct__(pResource);
}

static inline NV_STATUS traceDeviceEventControl_DISPATCH(struct TraceDeviceEvent *pResource, struct CALL_CONTEXT *pCallContext, struct RS_RES_CONTROL_PARAMS_INTERNAL *pParams) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventControl__(pResource, pCallContext, pParams);
}

static inline NV_STATUS traceDeviceEventControlFilter_DISPATCH(struct TraceDeviceEvent *pResource, struct CALL_CONTEXT *pCallContext, struct RS_RES_CONTROL_PARAMS_INTERNAL *pParams) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventControlFilter__(pResource, pCallContext, pParams);
}

static inline NV_STATUS traceDeviceEventMap_DISPATCH(struct TraceDeviceEvent *pResource, struct CALL_CONTEXT *pCallContext, RS_CPU_MAP_PARAMS *pParams, RsCpuMapping *pCpuMapping) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventMap__(pResource, pCallContext, pParams, pCpuMapping);
}

static inline NV_STATUS traceDeviceEventUnmap_DISPATCH(struct TraceDeviceEvent *pResource, struct CALL_CONTEXT *pCallContext, RsCpuMapping *pCpuMapping) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventUnmap__(pResource, pCallContext, pCpuMapping);
}

static inline NvBool traceDeviceEventIsPartialUnmapSupported_DISPATCH(struct TraceDeviceEvent *pResource) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventIsPartialUnmapSupported__(pResource);
}

static inline NV_STATUS traceDeviceEventMapTo_DISPATCH(struct TraceDeviceEvent *pResource, RS_RES_MAP_TO_PARAMS *pParams) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventMapTo__(pResource, pParams);
}

static inline NV_STATUS traceDeviceEventUnmapFrom_DISPATCH(struct TraceDeviceEvent *pResource, RS_RES_UNMAP_FROM_PARAMS *pParams) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventUnmapFrom__(pResource, pParams);
}

static inline NvU32 traceDeviceEventGetRefCount_DISPATCH(struct TraceDeviceEvent *pResource) {
    return pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventGetRefCount__(pResource);
}

static inline void traceDeviceEventAddAdditionalDependants_DISPATCH(struct RsClient *pClient, struct TraceDeviceEvent *pResource, RsResourceRef *pReference) {
    pResource->__nvoc_metadata_ptr->vtable.__traceDeviceEventAddAdditionalDependants__(pClient, pResource, pReference);
}

// Virtual method declarations and/or inline definitions
NvBool traceDeviceEventCanCopy_IMPL(struct TraceDeviceEvent *traceDeviceEvent);

// Exported method declarations and/or inline definitions
// HAL method declarations without bodies
// Inline HAL method definitions
// Static dispatch method declarations
// Static inline method definitions
#undef PRIVATE_FIELD



#endif // TRACE_DEVICE_EVENT

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _G_TRACE_DEVICE_EVENT_NVOC_H_
