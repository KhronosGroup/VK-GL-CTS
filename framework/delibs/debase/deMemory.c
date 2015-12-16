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

#define DE_ALIGNED_MALLOC_POSIX		0
#define DE_ALIGNED_MALLOC_WIN32		1
#define DE_ALIGNED_MALLOC_GENERIC	2

#if (DE_OS == DE_OS_UNIX) || ((DE_OS == DE_OS_ANDROID) && (DE_ANDROID_API >= 21))
#	define DE_ALIGNED_MALLOC DE_ALIGNED_MALLOC_POSIX
#elif (DE_OS == DE_OS_WIN32)
#	define DE_ALIGNED_MALLOC DE_ALIGNED_MALLOC_WIN32
#	include <malloc.h>
#else
#	define DE_ALIGNED_MALLOC DE_ALIGNED_MALLOC_GENERIC
#endif

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
void* deMalloc (size_t numBytes)
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
void* deCalloc (size_t numBytes)
{
	void* ptr = deMalloc(numBytes);
	if (ptr)
		deMemset(ptr, 0, numBytes);
	return ptr;
}

void* deAlignedMalloc (size_t numBytes, size_t alignBytes)
{
#if (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_POSIX)
	void* ptr = DE_NULL;

	if (posix_memalign(&ptr, alignBytes, numBytes) == 0)
	{
		DE_ASSERT(ptr);
		return ptr;
	}
	else
	{
		DE_ASSERT(!ptr);
		return DE_NULL;
	}

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_WIN32)
	return _aligned_malloc(numBytes, alignBytes);

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_GENERIC)
	/* Generic implementation */
	size_t		ptrSize		= sizeof(void*);
	deUintptr	origPtr		= (deUintptr)deMalloc(numBytes + ptrSize + alignBytes);

	DE_ASSERT((size_t)(deUint32)alignBytes == alignBytes	&&
			  deInRange32((deUint32)alignBytes, 0, 256)		&&
			  deIsPowerOfTwo32((deUint32)alignBytes));

	if (origPtr)
	{
		deUintptr	alignedPtr	= (deUintptr)deAlignPtr((void*)(origPtr + ptrSize), (deUintptr)alignBytes);
		deUintptr	ptrPtr		= (alignedPtr - ptrSize);
		*(deUintptr*)ptrPtr = origPtr;
		return (void*)alignedPtr;
	}
	else
		return DE_NULL;
#else
#	error "Invalid DE_ALIGNED_MALLOC"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Reallocate a chunk of memory.
 * \param ptr		Pointer to previously allocated memory block
 * \param numBytes	New size in bytes
 * \return Pointer to the reallocated (and possibly moved) memory block
 *//*--------------------------------------------------------------------*/
void* deRealloc (void* ptr, size_t numBytes)
{
	return realloc(ptr, numBytes);
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
#if (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_POSIX)
	free(ptr);

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_WIN32)
	_aligned_free(ptr);

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_GENERIC)
	if (ptr)
	{
		size_t		ptrSize		= sizeof(void*);
		deUintptr	ptrPtr		= (deUintptr)ptr - ptrSize;
		deUintptr	origPtr		= *(deUintptr*)ptrPtr;
		DE_ASSERT(ptrPtr - origPtr < 256);
		deFree((void*)origPtr);
	}
#else
#	error "Invalid DE_ALIGNED_MALLOC"
#endif
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
