/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
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
 * \brief Memory management.
 *//*--------------------------------------------------------------------*/

#include "deMemory.h"
#include "deInt32.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if defined(DE_VALGRIND_BUILD)
#	include <valgrind/valgrind.h>
#	if defined(HAVE_VALGRIND_MEMCHECK_H)
#		include <valgrind/memcheck.h>
#	endif
#endif

DE_BEGIN_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Allocate a chunk of memory.
 * \param numBytes	Number of bytes to allocate.
 * \return Pointer to the allocated memory (or null on failure).
 *//*--------------------------------------------------------------------*/
void* deMalloc (int numBytes)
{
	void* ptr;

	DE_ASSERT(numBytes > 0);

	ptr = malloc((size_t)numBytes);

#if defined(DE_DEBUG)
	/* Trash memory in debug builds (if under Valgrind, don't make it think we're initializing data here). */

	if (ptr)
		memset(ptr, 0xcd, numBytes);

#if defined(DE_VALGRIND_BUILD) && defined(HAVE_VALGRIND_MEMCHECK_H)
	if (ptr && RUNNING_ON_VALGRIND)
	{
		VALGRIND_MAKE_MEM_UNDEFINED(ptr, numBytes);
	}
#endif
#endif

	return ptr;
}

/*--------------------------------------------------------------------*//*!
 * \brief Allocate a chunk of memory and initialize it to zero.
 * \param numBytes	Number of bytes to allocate.
 * \return Pointer to the allocated memory (or null on failure).
 *//*--------------------------------------------------------------------*/
void* deCalloc (int numBytes)
{
	void* ptr = deMalloc(numBytes);
	if (ptr)
		deMemset(ptr, 0, numBytes);
	return ptr;
}

void* deAlignedMalloc (int numBytes, int alignBytes)
{
	int			ptrSize		= sizeof(void*);
	deUintptr	origPtr		= (deUintptr)deMalloc(numBytes + ptrSize + alignBytes);
	if (origPtr)
	{
		deUintptr	alignedPtr	= (deUintptr)deAlignPtr((void*)(origPtr + ptrSize), alignBytes);
		deUintptr	ptrPtr		= (alignedPtr - ptrSize);
		DE_ASSERT(deInRange32(alignBytes, 0, 256) && deIsPowerOfTwo32(alignBytes));
		*(deUintptr*)ptrPtr = origPtr;
		return (void*)alignedPtr;
	}
	else
		return DE_NULL;
}

/*--------------------------------------------------------------------*//*!
 * \brief Reallocate a chunk of memory.
 * \param ptr		Pointer to previously allocated memory block
 * \param numBytes	New size in bytes
 * \return Pointer to the reallocated (and possibly moved) memory block
 *//*--------------------------------------------------------------------*/
void* deRealloc (void* ptr, int numBytes)
{
	return realloc(ptr, (size_t)numBytes);
}

/*--------------------------------------------------------------------*//*!
 * \brief Free a chunk of memory.
 * \param ptr	Pointer to memory to free.
 *//*--------------------------------------------------------------------*/
void deFree (void* ptr)
{
	free(ptr);
}

void deAlignedFree (void* ptr)
{
	if (ptr)
	{
		int			ptrSize		= sizeof(void*);
		deUintptr	ptrPtr		= (deUintptr)ptr - ptrSize;
		deUintptr	origPtr		= *(deUintptr*)ptrPtr;
		DE_ASSERT(ptrPtr - origPtr < 256);
		deFree((void*)origPtr);
	}
}

char* deStrdup (const char* str)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
	return _strdup(str);
#elif (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS)
	/* For some reason Steve doesn't like stdrup(). */
	size_t	len		= strlen(str);
	char*	copy	= malloc(len+1);
	memcpy(copy, str, len);
	copy[len] = 0;
	return copy;
#else
	return strdup(str);
#endif
}

DE_END_EXTERN_C
