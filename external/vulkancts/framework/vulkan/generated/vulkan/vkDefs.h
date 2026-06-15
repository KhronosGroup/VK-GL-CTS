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
 * \brief Vulkan header for externl dependencies.
 * Note this is legacy solution. External dependencies should switch to
 * using vkDefs.hpp.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#if (DE_OS == DE_OS_ANDROID) && defined(__ARM_ARCH) && defined(__ARM_32BIT_STATE)
#define VKAPI_ATTR __attribute__((pcs("aapcs-vfp")))
#else
#define VKAPI_ATTR
#endif

#if (DE_OS == DE_OS_WIN32) && \
    ((defined(_MSC_VER) && _MSC_VER >= 800) || defined(__MINGW32__) || defined(_STDCALL_SUPPORTED))
#define VKAPI_CALL __stdcall
#else
#define VKAPI_CALL
#endif

#ifndef VK_USE_64_BIT_PTR_DEFINES
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__) ||                   \
    (defined(__riscv) && __riscv_xlen == 64)
#define VK_USE_64_BIT_PTR_DEFINES 1
#else
#define VK_USE_64_BIT_PTR_DEFINES 0
#endif
#endif

#ifndef VK_DEFINE_NON_DISPATCHABLE_HANDLE
#if (VK_USE_64_BIT_PTR_DEFINES == 1)
#if (defined(__cplusplus) && (__cplusplus >= 201103L)) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 201103L))
#define VK_NULL_HANDLE nullptr
#else
#define VK_NULL_HANDLE ((void *)0)
#endif
#else
#define VK_NULL_HANDLE 0ULL
#endif
#endif
#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE 0
#endif

// note 'type' parameter is  not used it is needed for consistancy with defines in vkDefs.hpp
#define VK_DEFINE_HANDLE(object, type) typedef struct object##_T *object;

#ifndef VK_DEFINE_NON_DISPATCHABLE_HANDLE
#if (VK_USE_64_BIT_PTR_DEFINES == 1)
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object, type) typedef struct object##_T *object;
#else
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object, type) typedef uint64_t object;
#endif
#endif

#define VK_MAKE_API_VERSION(VARIANT, MAJOR, MINOR, PATCH) \
    ((((uint32_t)(VARIANT)) << 29) | (((uint32_t)(MAJOR)) << 22) | (((uint32_t)(MINOR)) << 12) | ((uint32_t)(PATCH)))

#define VK_MAKE_VIDEO_STD_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

#define VK_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22U) | (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))

#define VK_DEFINE_PLATFORM_TYPE(NAME, COMPATIBLE)                 \
    namespace pt                                                  \
    {                                                             \
    struct NAME                                                   \
    {                                                             \
        COMPATIBLE internal;                                      \
        explicit NAME(COMPATIBLE internal_) : internal(internal_) \
        {                                                         \
        }                                                         \
        NAME() : internal()                                       \
        {                                                         \
        }                                                         \
    };                                                            \
    } // pt

typedef uint64_t VkDeviceSize;
typedef uint32_t VkSampleMask;
typedef uint32_t VkBool32;
typedef uint32_t VkFlags;
typedef uint64_t VkFlags64;
typedef uint64_t VkDeviceAddress;

#include "vkBasicTypes.inl"

typedef VKAPI_ATTR void(VKAPI_CALL *PFN_vkVoidFunction)(void);

typedef VKAPI_ATTR void *(VKAPI_CALL *PFN_vkAllocationFunction)(void *pUserData, size_t size, size_t alignment,
                                                                VkSystemAllocationScope allocationScope);
typedef VKAPI_ATTR void *(VKAPI_CALL *PFN_vkReallocationFunction)(void *pUserData, void *pOriginal, size_t size,
                                                                  size_t alignment,
                                                                  VkSystemAllocationScope allocationScope);
typedef VKAPI_ATTR void(VKAPI_CALL *PFN_vkFreeFunction)(void *pUserData, void *pMem);
typedef VKAPI_ATTR void(VKAPI_CALL *PFN_vkInternalAllocationNotification)(void *pUserData, size_t size,
                                                                          VkInternalAllocationType allocationType,
                                                                          VkSystemAllocationScope allocationScope);
typedef VKAPI_ATTR void(VKAPI_CALL *PFN_vkInternalFreeNotification)(void *pUserData, size_t size,
                                                                    VkInternalAllocationType allocationType,
                                                                    VkSystemAllocationScope allocationScope);

#ifndef CTS_USES_VULKANSC

typedef VKAPI_ATTR VkBool32(VKAPI_CALL *PFN_vkDebugReportCallbackEXT)(VkDebugReportFlagsEXT flags,
                                                                      VkDebugReportObjectTypeEXT objectType,
                                                                      uint64_t object, size_t location,
                                                                      int32_t messageCode, const char *pLayerPrefix,
                                                                      const char *pMessage, void *pUserData);

typedef VKAPI_ATTR PFN_vkVoidFunction(VKAPI_CALL *PFN_vkGetInstanceProcAddrLUNARG)(VkInstance instance,
                                                                                   const char pName);

#endif // CTS_USES_VULKANSC

typedef VKAPI_ATTR VkBool32(VKAPI_CALL *PFN_vkDebugUtilsMessengerCallbackEXT)(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const struct VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

typedef VKAPI_ATTR void(VKAPI_CALL *PFN_vkDeviceMemoryReportCallbackEXT)(
    const struct VkDeviceMemoryReportCallbackDataEXT *pCallbackData, void *pUserData);

#ifdef CTS_USES_VULKANSC
struct VkFaultData;
typedef VKAPI_ATTR void(VKAPI_CALL *PFN_vkFaultCallbackFunction)(VkBool32 incompleteFaultData, uint32_t faultCount,
                                                                 const VkFaultData *pFaultData);
#endif // CTS_USES_VULKANSC

#include "vkStructTypes.inl"
#include "vkPfnTypes.inl"

#endif /* _VKDEFS_H */
