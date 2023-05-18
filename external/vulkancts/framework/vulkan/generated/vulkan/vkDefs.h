#ifndef _VKDEFS_H
#define _VKDEFS_H
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 Intel Corporation
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
 * \brief Vulkan header for C files
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#if (DE_OS == DE_OS_ANDROID) && defined(__ARM_ARCH) && defined(__ARM_32BIT_STATE)
#	define VKAPI_ATTR __attribute__((pcs("aapcs-vfp")))
#else
#	define VKAPI_ATTR
#endif

#if (DE_OS == DE_OS_WIN32) && ((defined(_MSC_VER) && _MSC_VER >= 800) || defined(__MINGW32__) || defined(_STDCALL_SUPPORTED))
#	define VKAPI_CALL __stdcall
#   define VKAPI_PTR  VKAPI_CALL
#else
#	define VKAPI_CALL
#   define VKAPI_PTR  VKAPI_ATTR
#endif

#include "vkVulkan_c.inl"

#endif /* _VKDEFS_H */
