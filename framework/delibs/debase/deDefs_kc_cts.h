#ifndef _DEDEFS_KC_CTS_H
#define _DEDEFS_KC_CTS_H
/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
 *
 * Copyright 2024 The Android Open Source Project
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
 * \brief Deprecated definitions still used by kc-cts
 *//*--------------------------------------------------------------------*/

// The following definitions are deprecated.  DO NOT USE.
//
// This header is included by deDefs.h, and the definitions are guarded by the header guard used by
// gtfWrapper.h in KC-CTS.  This file can be removed once KC-CTS is moved away from these types.
#ifdef DEQP_GTF_AVAILABLE
typedef int8_t deInt8;
typedef uint8_t deUint8;
typedef int16_t deInt16;
typedef uint16_t deUint16;
typedef int32_t deInt32;
typedef uint32_t deUint32;
typedef int64_t deInt64;
typedef uint64_t deUint64;
typedef intptr_t deIntptr;
typedef uintptr_t deUintptr;

/** Boolean type. */
typedef int deBool;
#define DE_TRUE 1  /*!< True value for deBool. */
#define DE_FALSE 0 /*!< False value for deBool. */
#endif             /* DEQP_GTF_AVAILABLE */

#if !defined(__cplusplus)
#define DE_NULL ((void *)0)
#endif

#endif /* _DEDEFS_KC_CTS_H */
