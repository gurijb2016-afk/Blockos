/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#ifndef _NV_CONTAINERS_HASHMAP_H_
#define _NV_CONTAINERS_HASHMAP_H_

// Contains mix of C/C++ declarations.
#include "containers/type_safety.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "nvtypes.h"
#include "nvmisc.h"
#include "nvport/nvport.h"
#include "utils/nvassert.h"

/**
 * @defgroup NV_CONTAINERS_HASHMAP HashMap
 *
 * @brief Unordered map from 64-bit integer keys to user-defined values.
 *
 * @details Open-addressing hash map with linear probing. Only a non-intrusive
 * variant is provided, since open addressing requires values to reside in
 * contiguous slot storage managed by the container.
 *
 * @note Value pointers returned by insert/find are invalidated by any
 * subsequent insert or remove operation that triggers an internal resize.
 *
 * - Time Complexity:
 *  * Insert, Find, Remove are \b O(1) amortized.
 *  * Iteration is O(capacity).
 *
 * - Memory Usage:
 *  * \b O(N) memory is required for N values.
 *  * Non-intrusive only. See @ref mem-ownership for further details.
 *
 * - Synchronization:
 *  * \b None. The container is not thread-safe.
 *  * Locking must be handled by the user if required.
 */

#define MAKE_HASHMAP(hmTypeName, dataType)                                    \
    typedef union hmTypeName##Iter                                            \
    {                                                                         \
        dataType *pValue;                                                     \
        HashMapIterBase iter;                                                 \
    } hmTypeName##Iter;                                                       \
    typedef union hmTypeName                                                  \
    {                                                                         \
        NonIntrusiveHashMap real;                                             \
        CONT_TAG_TYPE(HashMapBase, dataType, hmTypeName##Iter);              \
        CONT_TAG_NON_INTRUSIVE(dataType);                                    \
    } hmTypeName

#define DECLARE_HASHMAP(hmTypeName)                                          \
    typedef union hmTypeName##Iter hmTypeName##Iter;                         \
    typedef union hmTypeName hmTypeName

/**
 * @brief Slot states for open addressing.
 */
#define HASHMAP_SLOT_EMPTY    0
#define HASHMAP_SLOT_OCCUPIED 1
#define HASHMAP_SLOT_DELETED  2

/**
 * @brief Internal slot header stored before each value in the table.
 */
typedef struct HashMapSlot HashMapSlot;

/**
 * @brief Base type for the hash map.
 */
typedef struct HashMapBase HashMapBase;

/**
 * @brief Non-intrusive hash map (container-managed memory).
 */
typedef struct NonIntrusiveHashMap NonIntrusiveHashMap;

/**
 * @brief Iterator over hash map entries.
 */
typedef struct HashMapIterBase HashMapIterBase;

struct HashMapSlot
{
    /// @privatesection
    NvU64   key;
    NvU32   state;
    NvU32   _pad;
};

struct HashMapIterBase
{
    void           *pValue;
    HashMapBase    *pMap;
    NvU32           slotIndex;
    NvU32           slotEnd;
#if PORT_IS_CHECKED_BUILD
    NvU32           versionNumber;
    NvBool          bValid;
#endif
};

HashMapIterBase hashMapIterRange_IMPL(HashMapBase *pMap, void *pFirst,
                                      void *pLast);
CONT_VTABLE_DECL(HashMapBase, HashMapIterBase);

struct HashMapBase
{
    CONT_VTABLE_FIELD(HashMapBase);
    NvU8       *pSlots;
    NvU32       capacity;
    NvU32       count;
    NvU32       countDead;
    NvU32       slotSize;
#if PORT_IS_CHECKED_BUILD
    NvU32       versionNumber;
#endif
};

struct NonIntrusiveHashMap
{
    HashMapBase         base;
    PORT_MEM_ALLOCATOR *pAllocator;
    NvU32               valueSize;
};

#define hashMapInit(pMap, pAllocator)                                         \
    hashMapInit_IMPL(&((pMap)->real), pAllocator, sizeof(*(pMap)->valueSize))

#define hashMapDestroy(pMap)                                                  \
    hashMapDestroy_IMPL(&((pMap)->real))

#define hashMapCount(pMap)                                                    \
    hashMapCount_IMPL(&((pMap)->real).base)

#define hashMapKey(pMap, pValue)                                              \
    hashMapKey_IMPL(&((pMap)->real).base, pValue)

#define hashMapInsertNew(pMap, key)                                           \
    CONT_CAST_ELEM(pMap, hashMapInsertNew_IMPL(&(pMap)->real, key),           \
                   hashMapIsValid_IMPL)

#define hashMapInsertValue(pMap, key, pValue)                                 \
    CONT_CAST_ELEM(pMap,                                                      \
        hashMapInsertValue_IMPL(&(pMap)->real, key,                           \
            CONT_CHECK_ARG(pMap, pValue)), hashMapIsValid_IMPL)

#define hashMapRemove(pMap, pValue)                                           \
    hashMapRemove_IMPL(&((pMap)->real),                                       \
        CONT_CHECK_ARG(pMap, pValue))

#define hashMapRemoveByKey(pMap, key)                                         \
    hashMapRemoveByKey_IMPL(&((pMap)->real), key)

#define hashMapClear(pMap)                                                    \
    hashMapDestroy(pMap)

#define hashMapFind(pMap, key)                                                \
    CONT_CAST_ELEM(pMap,                                                      \
        hashMapFind_IMPL(&((pMap)->real).base, key), hashMapIsValid_IMPL)

#define hashMapIterAll(pMap)                                                  \
    CONT_ITER_RANGE(pMap, &hashMapIterRange_IMPL, NULL, NULL,                 \
                    hashMapIsValid_IMPL)

#define hashMapIterNext(pIt)                                                  \
    hashMapIterNext_IMPL(&((pIt)->iter))

void  hashMapInit_IMPL(NonIntrusiveHashMap *pMap,
                        PORT_MEM_ALLOCATOR *pAllocator, NvU32 valueSize);
void  hashMapDestroy_IMPL(NonIntrusiveHashMap *pMap);

NvU32 hashMapCount_IMPL(HashMapBase *pMap);
NvU64 hashMapKey_IMPL(HashMapBase *pMap, void *pValue);

void *hashMapInsertNew_IMPL(NonIntrusiveHashMap *pMap, NvU64 key);
void *hashMapInsertValue_IMPL(NonIntrusiveHashMap *pMap, NvU64 key,
                               const void *pValue);
void  hashMapRemove_IMPL(NonIntrusiveHashMap *pMap, void *pValue);
void  hashMapRemoveByKey_IMPL(NonIntrusiveHashMap *pMap, NvU64 key);

void *hashMapFind_IMPL(HashMapBase *pMap, NvU64 key);

HashMapIterBase hashMapIterRange_IMPL(HashMapBase *pMap, void *pFirst,
                                      void *pLast);
NvBool hashMapIterNext_IMPL(HashMapIterBase *pIt);

static NV_FORCEINLINE HashMapSlot *
hashMapSlotAt(HashMapBase *pMap, NvU32 index)
{
    if (NULL == pMap || NULL == pMap->pSlots) return NULL;
    return (HashMapSlot *)(pMap->pSlots + (NvU64)index * pMap->slotSize);
}

static NV_FORCEINLINE void *
hashMapSlotToValue(HashMapSlot *pSlot)
{
    if (NULL == pSlot) return NULL;
    return (NvU8 *)pSlot + sizeof(HashMapSlot);
}

static NV_FORCEINLINE HashMapSlot *
hashMapValueToSlot(void *pValue)
{
    if (NULL == pValue) return NULL;
    return (HashMapSlot *)((NvU8 *)pValue - sizeof(HashMapSlot));
}

NvBool hashMapIsValid_IMPL(void *pMap);

#ifdef __cplusplus
}
#endif

#endif // _NV_CONTAINERS_HASHMAP_H_
