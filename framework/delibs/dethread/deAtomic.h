#ifndef _DEATOMIC_H
#define _DEATOMIC_H
/*-------------------------------------------------------------------------
 * drawElements Thread Library
 * ---------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Atomic operations.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#if (DE_COMPILER == DE_COMPILER_MSC)
#	include <intrin.h>
#endif

DE_BEGIN_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Atomic increment and fetch.
 * \param dstAddr	Destination address.
 * \return Incremented value.
 *//*--------------------------------------------------------------------*/
DE_INLINE deInt32 deAtomicIncrement32 (deInt32 volatile* dstAddr)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
	return _InterlockedIncrement((long volatile*)dstAddr);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
	return __sync_add_and_fetch(dstAddr, 1);
#else
#	error "Implement deAtomicIncrement32()"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic decrement and fetch.
 * \param dstAddr	Destination address.
 * \return Decremented value.
 *//*--------------------------------------------------------------------*/
DE_INLINE deInt32 deAtomicDecrement32 (deInt32 volatile* dstAddr)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
	return _InterlockedDecrement((long volatile*)dstAddr);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
	return __sync_sub_and_fetch(dstAddr, 1);
#else
#	error "Implement deAtomicDecrement32()"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic compare and exchange (CAS).
 * \param dstAddr	Destination address.
 * \param compare	Old value.
 * \param exchange	New value.
 * \return			compare value if CAS passes, *dstAddr value otherwise
 *
 * Performs standard Compare-And-Swap with 32b data. Dst value is compared
 * to compare value and if that comparison passes, value is replaced with
 * exchange value.
 *
 * If CAS succeeds, compare value is returned. Otherwise value stored in
 * dstAddr is returned.
 *//*--------------------------------------------------------------------*/
DE_INLINE deUint32 deAtomicCompareExchange32 (deUint32 volatile* dstAddr, deUint32 compare, deUint32 exchange)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
	return _InterlockedCompareExchange((long volatile*)dstAddr, exchange, compare);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
	return __sync_val_compare_and_swap(dstAddr, compare, exchange);
#else
#	error "Implement deAtomicCompareExchange32()"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Issue hardware memory read-write fence.
 *//*--------------------------------------------------------------------*/
#if (DE_COMPILER == DE_COMPILER_MSC)
void deMemoryReadWriteFence (void);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
DE_INLINE void deMemoryReadWriteFence (void)
{
	__sync_synchronize();
}
#else
#	error "Implement deMemoryReadWriteFence()"
#endif

DE_END_EXTERN_C

#endif /* _DEATOMIC_H */
