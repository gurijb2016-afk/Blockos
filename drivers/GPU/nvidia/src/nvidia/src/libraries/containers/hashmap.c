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
#include "containers/hashmap.h"
#include "utils/nvassert.h"

CONT_VTABLE_DEFN(HashMapBase, hashMapIterRange_IMPL, NULL);

#define HASHMAP_INITIAL_CAPACITY    16
#define HASHMAP_LOAD_NUM            7
#define HASHMAP_LOAD_DEN            10

/**
 * @brief Computes a hash bucket index for a 64-bit key.
 *
 * Uses Fibonacci / multiplicative hashing. The capacity must be a power of two.
 *
 * @param[in] key      The 64-bit key to hash.
 * @param[in] capacity The number of buckets (must be a power of two).
 *
 * @return Bucket index in the range [0, capacity).
 */
static NV_FORCEINLINE NvU32
_hashMapHash(NvU64 key, NvU32 capacity)
{
    /* 
     * Fibonacci / multiplicative hash for 64-bit keys.
     * capacity must be a power of two.
     */
    key *= 0x9E3779B97F4A7C15ULL;
    return (NvU32)(key >> 32) & (capacity - 1);
}

/**
 * @brief Checks whether the hash map has exceeded its load factor threshold.
 *
 * The load factor is defined as (count + countDead) / capacity >= 7/10.
 *
 * @param[in] pBase Pointer to the hash map base structure.
 *
 * @return NV_TRUE if the map should grow, NV_FALSE otherwise.
 */
static NV_FORCEINLINE NvBool
_hashMapShouldGrow(HashMapBase *pBase)
{
    return ((pBase->count + pBase->countDead) * HASHMAP_LOAD_DEN) >=
            (pBase->capacity * HASHMAP_LOAD_NUM);
}

static NvBool _hashMapResize(NonIntrusiveHashMap *pMap, NvU32 newCapacity);


/**
 * @brief Initializes a non-intrusive hash map.
 *
 * Allocates the initial slot table with @ref HASHMAP_INITIAL_CAPACITY entries
 * and zeroes all slots. If the allocation fails, the map is left with zero
 * capacity.
 *
 * @param[in,out] pMap       Pointer to the hash map to initialize.
 * @param[in]     pAllocator Memory allocator to use for internal storage.
 * @param[in]     valueSize  Size in bytes of each value stored in the map.
 */
void hashMapInit_IMPL
(
    NonIntrusiveHashMap *pMap,
    PORT_MEM_ALLOCATOR  *pAllocator,
    NvU32                valueSize
)
{
    NV_ASSERT_OR_RETURN_VOID(pMap != NULL);
    NV_ASSERT_OR_RETURN_VOID(pAllocator != NULL);

    portMemSet(&pMap->base, 0, sizeof(pMap->base));
    CONT_VTABLE_INIT(HashMapBase, &pMap->base);
    pMap->pAllocator = pAllocator;
    pMap->valueSize  = valueSize;

    // Slot size: header + value, aligned to 8 bytes for NvU64 alignment.
    pMap->base.slotSize = (NvU32)NV_ALIGN_UP(sizeof(HashMapSlot) + valueSize, sizeof(NvU64));

    // Allocate initial table.
    pMap->base.capacity = HASHMAP_INITIAL_CAPACITY;
    pMap->base.pSlots = (NvU8 *)PORT_ALLOC(pAllocator,
        (NvU64)pMap->base.slotSize * HASHMAP_INITIAL_CAPACITY);

    if (pMap->base.pSlots != NULL)
    {
        portMemSet(pMap->base.pSlots, 0,
                   (NvU64)pMap->base.slotSize * HASHMAP_INITIAL_CAPACITY);
    }
    else
    {
        pMap->base.capacity = 0;
    }
}

/**
 * @brief Destroys a non-intrusive hash map and frees its internal storage.
 *
 * After this call the map is empty and its slot table is deallocated. The map
 * structure itself is not freed (it may be stack- or struct-embedded).
 *
 * @param[in,out] pMap Pointer to the hash map to destroy.
 */
void hashMapDestroy_IMPL(NonIntrusiveHashMap *pMap)
{
    NV_ASSERT_OR_RETURN_VOID(pMap != NULL);

    if (pMap->base.pSlots != NULL)
    {
        PORT_FREE(pMap->pAllocator, pMap->base.pSlots);
        pMap->base.pSlots = NULL;
    }

    pMap->base.count     = 0;
    pMap->base.countDead = 0;
    pMap->base.capacity  = 0;
    NV_CHECKED_ONLY(pMap->base.versionNumber++);
}

/**
 * @brief Returns the number of live (occupied) entries in the hash map.
 *
 * @param[in] pMap Pointer to the hash map base structure.
 *
 * @return The number of entries, or 0 if @p pMap is NULL.
 */
NvU32 hashMapCount_IMPL(HashMapBase *pMap)
{
    NV_ASSERT_OR_RETURN(pMap != NULL, 0);
    return pMap->count;
}

/**
 * @brief Retrieves the key associated with a value pointer in the hash map.
 *
 * The value pointer must have been obtained from a prior insert or find
 * operation and must still be occupied.
 *
 * @param[in] pMap   Pointer to the hash map base structure.
 * @param[in] pValue Pointer to a value within the map.
 *
 * @return The 64-bit key for the entry, or 0 on error.
 */
NvU64 hashMapKey_IMPL(HashMapBase *pMap, void *pValue)
{
    HashMapSlot *pSlot;
    NV_ASSERT_OR_RETURN(pMap != NULL, 0);
    NV_ASSERT_OR_RETURN(pValue != NULL, 0);
    pSlot = hashMapValueToSlot(pValue);
    NV_ASSERT_OR_RETURN(pSlot->state == HASHMAP_SLOT_OCCUPIED, 0);
    return pSlot->key;
}

/**
 * @brief Resizes the hash map to a new capacity and rehashes all entries.
 *
 * Allocates a new slot table of @p newCapacity entries, rehashes every
 * occupied slot from the old table into the new one, and frees the old table.
 * Tombstones (deleted slots) are discarded during rehash.
 *
 * @param[in,out] pMap        Pointer to the hash map to resize.
 * @param[in]     newCapacity New number of slots (must be a power of two and > 0).
 *
 * @return NV_TRUE on success, NV_FALSE if allocation failed or
 *         @p newCapacity is invalid.
 */
static NvBool _hashMapResize(NonIntrusiveHashMap *pMap, NvU32 newCapacity)
{
    NvU8  *pOldSlots    = pMap->base.pSlots;
    NvU32  oldCapacity  = pMap->base.capacity;
    NvU32  slotSize     = pMap->base.slotSize;
    NvU8  *pNewSlots;
    NvU32  i;

    NV_ASSERT_OR_RETURN(newCapacity > 0, NV_FALSE);
    NV_ASSERT_OR_RETURN((newCapacity & (newCapacity - 1)) == 0, NV_FALSE);

    pNewSlots = (NvU8 *)PORT_ALLOC(pMap->pAllocator,
                                    (NvU64)(slotSize * newCapacity));
    if (pNewSlots == NULL)
    {
        return NV_FALSE;
    }

    portMemSet(pNewSlots, 0, (NvU64)(slotSize * newCapacity));

    // Rehash all occupied slots into the new table.
    for (i = 0; i < oldCapacity; i++)
    {
        HashMapSlot *pOld = (HashMapSlot*)(pOldSlots + (NvU64)i * slotSize);
        if (pOld->state == HASHMAP_SLOT_OCCUPIED)
        {
            NvU32 idx = _hashMapHash(pOld->key, newCapacity);
            for (;;)
            {
                HashMapSlot *pNew = (HashMapSlot*)(pNewSlots +
                                                    (NvU64)idx * slotSize);
                if (pNew->state == HASHMAP_SLOT_EMPTY)
                {
                    pNew->key   = pOld->key;
                    pNew->state = HASHMAP_SLOT_OCCUPIED;
                    portMemCopy(hashMapSlotToValue(pNew), pMap->valueSize,
                                hashMapSlotToValue(pOld), pMap->valueSize);
                    break;
                }
                idx = (idx + 1) & (newCapacity - 1);
            }
        }
    }

    PORT_FREE(pMap->pAllocator, pOldSlots);

    pMap->base.pSlots    = pNewSlots;
    pMap->base.capacity  = newCapacity;
    pMap->base.countDead = 0;
    NV_CHECKED_ONLY(pMap->base.versionNumber++);

    return NV_TRUE;
}

/**
 * @brief Inserts a new entry with the given key and returns a pointer to its
 *        zero-initialized value.
 *
 * If the key already exists, no insertion is performed and a pointer to the
 * existing value is returned.
 * The map is grown automatically when the load factor threshold is exceeded.
 * Tombstone slots are reused when encountered during linear probing.
 *
 * @param[in,out] pMap Pointer to the hash map.
 * @param[in]     key  The 64-bit key for the new entry.
 *
 * @return Pointer to the inserted or existing value, or NULL if allocation
 *         failed.
 */
void *hashMapInsertNew_IMPL(NonIntrusiveHashMap *pMap, NvU64 key)
{
    HashMapSlot *pSlot;
    HashMapSlot *pTombstone = NULL;
    NvU32 idx;

    NV_ASSERT_OR_RETURN(pMap != NULL, NULL);

    // Re-allocate table if it was destroyed (e.g. via hashMapClear).
    if (pMap->base.pSlots == NULL)
    {
        pMap->base.capacity = HASHMAP_INITIAL_CAPACITY;
        pMap->base.pSlots = (NvU8 *)PORT_ALLOC(pMap->pAllocator,
            (NvU64)pMap->base.slotSize * HASHMAP_INITIAL_CAPACITY);
        NV_ASSERT_OR_RETURN(pMap->base.pSlots != NULL, NULL);
        portMemSet(pMap->base.pSlots, 0,
                   (NvU64)pMap->base.slotSize * HASHMAP_INITIAL_CAPACITY);
    }

    // Grow if load factor exceeded.
    if (_hashMapShouldGrow(&pMap->base))
    {
        if (!_hashMapResize(pMap, pMap->base.capacity * 2))
        {
            return NULL;
        }
    }

    idx = _hashMapHash(key, pMap->base.capacity);

    for (;;)
    {
        pSlot = hashMapSlotAt(&pMap->base, idx);

        if (pSlot->state == HASHMAP_SLOT_EMPTY)
        {
            // Use earlier tombstone if we found one during probing.
            if (pTombstone != NULL)
            {
                pSlot = pTombstone;
                pMap->base.countDead--;
            }

            pSlot->key   = key;
            pSlot->state = HASHMAP_SLOT_OCCUPIED;
            portMemSet(hashMapSlotToValue(pSlot), 0, pMap->valueSize);
            pMap->base.count++;
            NV_CHECKED_ONLY(pMap->base.versionNumber++);
            return hashMapSlotToValue(pSlot);
        }

        if (pSlot->state == HASHMAP_SLOT_DELETED)
        {
            // Remember first tombstone for reuse.
            if (pTombstone == NULL)
            {
                pTombstone = pSlot;
            }
        }
        else if (pSlot->key == key)
        {
            // Duplicate key — return existing value instead of NULL.
            return hashMapSlotToValue(pSlot);
        }

        idx = (idx + 1) & (pMap->base.capacity - 1);
    }
}

/**
 * @brief Inserts a new entry and copies a value into it.
 *
 * Equivalent to @ref hashMapInsertNew_IMPL followed by a memcpy of @p pValue
 * into the new slot.
 *
 * @param[in,out] pMap   Pointer to the hash map.
 * @param[in]     key    The 64-bit key for the new entry.
 * @param[in]     pValue Pointer to the value data to copy (must not be NULL).
 *
 * @return Pointer to the copied value in the map, or NULL on failure.
 */
void *hashMapInsertValue_IMPL
(
    NonIntrusiveHashMap *pMap,
    NvU64                key,
    const void          *pValue
)
{
    void *pCurrent;

    NV_ASSERT_OR_RETURN(pValue != NULL, NULL);

    // If the key already exists, return the existing value without overwriting.
    pCurrent = hashMapFind_IMPL(&pMap->base, key);
    if (pCurrent != NULL)
    {
        return pCurrent;
    }

    pCurrent = hashMapInsertNew_IMPL(pMap, key);
    if (pCurrent == NULL)
    {
        return NULL;
    }

    return portMemCopy(pCurrent, pMap->valueSize, pValue, pMap->valueSize);
}

/**
 * @brief Removes an entry from the hash map by value pointer.
 *
 * Marks the slot as deleted (tombstone) so that linear-probe chains remain
 * intact. Does nothing if @p pValue is NULL.
 *
 * @param[in,out] pMap   Pointer to the hash map.
 * @param[in]     pValue Pointer to the value to remove (obtained from insert
 *                       or find). May be NULL.
 */
void hashMapRemove_IMPL(NonIntrusiveHashMap *pMap, void *pValue)
{
    HashMapSlot *pSlot;

    if (pValue == NULL)
    {
        return;
    }

    NV_ASSERT_OR_RETURN_VOID(pMap != NULL);

    pSlot = hashMapValueToSlot(pValue);
    NV_ASSERT_OR_RETURN_VOID(pSlot->state == HASHMAP_SLOT_OCCUPIED);

    pSlot->state = HASHMAP_SLOT_DELETED;
    pMap->base.count--;
    pMap->base.countDead++;
    NV_CHECKED_ONLY(pMap->base.versionNumber++);
}

/**
 * @brief Removes an entry from the hash map by key.
 *
 * Looks up the key and, if found, removes the corresponding entry. If the key
 * is not present, this is a no-op.
 *
 * @param[in,out] pMap Pointer to the hash map.
 * @param[in]     key  The 64-bit key of the entry to remove.
 */
void hashMapRemoveByKey_IMPL(NonIntrusiveHashMap *pMap, NvU64 key)
{
    hashMapRemove_IMPL(pMap, hashMapFind_IMPL(&pMap->base, key));
}

/**
 * @brief Finds an entry in the hash map by key.
 *
 * Uses linear probing to locate the slot. Deleted (tombstone) slots are
 * skipped during the probe.
 *
 * @param[in] pMap Pointer to the hash map base structure.
 * @param[in] key  The 64-bit key to search for.
 *
 * @return Pointer to the value if found, or NULL if the key is not present.
 */
void *hashMapFind_IMPL(HashMapBase *pMap, NvU64 key)
{
    HashMapSlot *pSlot;
    NvU32 idx;

    NV_ASSERT_OR_RETURN(pMap != NULL, NULL);

    if ((pMap->pSlots == NULL) || (pMap->capacity == 0))
    {
        return NULL;
    }

    idx = _hashMapHash(key, pMap->capacity);

    for (;;)
    {
        pSlot = hashMapSlotAt(pMap, idx);

        if (pSlot->state == HASHMAP_SLOT_EMPTY)
        {
            return NULL;
        }

        if ((pSlot->state == HASHMAP_SLOT_OCCUPIED) && (pSlot->key == key))
        {
            return hashMapSlotToValue(pSlot);
        }

        idx = (idx + 1) & (pMap->capacity - 1);
    }
}

/**
 * @brief Creates an iterator over all entries in the hash map.
 *
 * The @p pFirst and @p pLast parameters are unused; the iterator always covers
 * the entire map. Call @ref hashMapIterNext_IMPL to advance.
 *
 * @param[in] pMap   Pointer to the hash map base structure.
 * @param[in] pFirst Unused (kept for vtable signature compatibility).
 * @param[in] pLast  Unused (kept for vtable signature compatibility).
 *
 * @return An initialized iterator positioned before the first entry.
 */
HashMapIterBase hashMapIterRange_IMPL
(
    HashMapBase *pMap,
    void        *pFirst,
    void        *pLast
)
{
    HashMapIterBase it;

    (void)pFirst;
    (void)pLast;

    NV_ASSERT(NULL != pMap);

    portMemSet(&it, 0, sizeof(it));
    it.pMap     = pMap;
    it.slotEnd  = pMap->capacity;
    NV_CHECKED_ONLY(it.versionNumber = pMap->versionNumber);
    NV_CHECKED_ONLY(it.bValid = NV_TRUE);

    return it;
}

/**
 * @brief Advances the hash map iterator to the next occupied entry.
 *
 * On success, @c pIt->pValue points to the next value. When no more entries
 * remain, @c pIt->pValue is set to NULL and NV_FALSE is returned. In checked
 * builds, asserts if the map has been mutated since the iterator was created.
 *
 * @param[in,out] pIt Pointer to the iterator to advance.
 *
 * @return NV_TRUE if a next entry was found, NV_FALSE if iteration is complete.
 */
NvBool hashMapIterNext_IMPL(HashMapIterBase *pIt)
{
    NV_ASSERT_OR_RETURN(pIt != NULL, NV_FALSE);

#if PORT_IS_CHECKED_BUILD
    if (pIt->bValid && !CONT_ITER_IS_VALID(pIt->pMap, pIt))
    {
        NV_ASSERT(CONT_ITER_IS_VALID(pIt->pMap, pIt));
        PORT_DUMP_STACK();
        pIt->bValid = NV_FALSE;
    }
#endif

    while (pIt->slotIndex < pIt->slotEnd)
    {
        HashMapSlot *pSlot = hashMapSlotAt(pIt->pMap, pIt->slotIndex);
        pIt->slotIndex++;

        if (pSlot->state == HASHMAP_SLOT_OCCUPIED)
        {
            pIt->pValue = hashMapSlotToValue(pSlot);
            return NV_TRUE;
        }
    }

    pIt->pValue = NULL;
    return NV_FALSE;
}

/**
 * @brief Validates the hash map's vtable.
 *
 * On builds with @c NV_TYPEOF_SUPPORTED this is a no-op that returns NV_TRUE.
 * Otherwise, checks that the vtable pointer is valid and, if not, reinitializes
 * it (with an assertion failure).
 *
 * @param[in] pMap Pointer to the hash map (cast to void*).
 *
 * @return NV_TRUE if the vtable is valid, NV_FALSE if it had to be repaired.
 */
NvBool hashMapIsValid_IMPL(void *pMap)
{
#if NV_TYPEOF_SUPPORTED
    return NV_TRUE;
#else
    if (CONT_VTABLE_VALID((HashMapBase*)pMap))
    {
        return NV_TRUE;
    }

    NV_ASSERT_FAILED("vtable not valid!");
    CONT_VTABLE_INIT(HashMapBase, (HashMapBase*)pMap);
    return NV_FALSE;
#endif
}
