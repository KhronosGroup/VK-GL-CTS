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
 * \brief Win32 implementation of thread management.
 *//*--------------------------------------------------------------------*/

#include "deThread.h"

#if (DE_OS == DE_OS_WIN32 || DE_OS == DE_OS_WINCE)

#include "deMemory.h"

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Thread handle equals deThread in this implementation. */
DE_STATIC_ASSERT(sizeof(deThread) >= sizeof(HANDLE));

typedef struct ThreadEntry_s
{
	deThreadFunc	func;
	void*			arg;
} ThreadEntry;

static int mapPriority (deThreadPriority priority)
{
	switch (priority)
	{
		case DE_THREADPRIORITY_LOWEST:	return THREAD_PRIORITY_IDLE;
		case DE_THREADPRIORITY_LOW:		return THREAD_PRIORITY_LOWEST;
		case DE_THREADPRIORITY_NORMAL:	return THREAD_PRIORITY_NORMAL;
		case DE_THREADPRIORITY_HIGH:	return THREAD_PRIORITY_ABOVE_NORMAL;
		case DE_THREADPRIORITY_HIGHEST:	return THREAD_PRIORITY_HIGHEST;
		default:	DE_ASSERT(DE_FALSE);
	}
	return 0;
}

static DWORD __stdcall startThread (LPVOID entryPtr)
{
	ThreadEntry*	entry	= (ThreadEntry*)entryPtr;
	deThreadFunc	func	= entry->func;
	void*			arg		= entry->arg;
	
	deFree(entry);
	
	func(arg);
	
	return 0;
}

deThread deThread_create (deThreadFunc func, void* arg, const deThreadAttributes* attributes)
{
	ThreadEntry*	entry	= (ThreadEntry*)deMalloc(sizeof(ThreadEntry));
	HANDLE			thread	= 0;
	
	if (!entry)
		return 0;
	
	entry->func	= func;
	entry->arg	= arg;
	
	thread = CreateThread(DE_NULL, 0, startThread, entry, 0, DE_NULL);
	if (!thread)
	{
		deFree(entry);
		return 0;
	}
	
	if (attributes)
		SetThreadPriority(thread, mapPriority(attributes->priority));

	return (deThread)thread;
}

deBool deThread_join (deThread thread)
{
	HANDLE	handle		= (HANDLE)thread;
	WaitForSingleObject(handle, INFINITE);

	return DE_TRUE;
}

void deThread_destroy (deThread thread)
{
	HANDLE	handle		= (HANDLE)thread;
	CloseHandle(handle);
}

void deSleep (deUint32 milliseconds)
{
	Sleep((DWORD)milliseconds);
}

void deYield (void)
{
	SwitchToThread();	
}

#endif /* DE_OS */
