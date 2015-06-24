#ifndef _OSINCLUDE_H
#define _OSINCLUDE_H
/*-------------------------------------------------------------------------
 * dEQP glslang integration
 * ------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief glslang OS interface.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deThreadLocal.h"
#include "deThread.h"

namespace glslang
{

// Thread-local

typedef deThreadLocal OS_TLSIndex;

#define OS_INVALID_TLS_INDEX	DE_NULL

OS_TLSIndex	OS_AllocTLSIndex		(void);
bool		OS_SetTLSValue			(OS_TLSIndex nIndex, void* lpvValue);
bool		OS_FreeTLSIndex			(OS_TLSIndex nIndex);

void*		OS_GetTLSValue			(OS_TLSIndex nIndex);

// Global lock?

void		InitGlobalLock			(void);
void		GetGlobalLock			(void);
void		ReleaseGlobalLock		(void);

// Threading

typedef deThreadFunc TThreadEntrypoint;

void*		OS_CreateThread			(TThreadEntrypoint);
void		OS_WaitForAllThreads	(void* threads, int numThreads);

void		OS_Sleep				(int milliseconds);

void		OS_DumpMemoryCounters	(void);

} // glslang

#endif /* _OSINCLUDE_H */
