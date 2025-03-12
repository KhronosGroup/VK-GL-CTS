#ifndef _VKDEFS_HPP
#define _VKDEFS_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Vulkan utilites.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include <cstddef>
#include <type_traits>

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

// A helper to make sure VK_NULL_HANDLE is not accidentally used where it doesn't make sense.  When
// assigned to a handle defined by VK_DEFINE_HANDLE below (a pointer), it is automatically cast to
// nullptr.  When assigned to a Handle object, a special constructor that accepts this type sets it
// internal value to 0.
struct VkNullHandleType
{
    template <typename T>
    operator T *()
    {
        return nullptr;
    }
};

#define VK_NULL_HANDLE \
    VkNullHandleType   \
    {                  \
    }
#define VK_DEFINE_HANDLE(NAME, TYPE) typedef struct NAME##_s *NAME
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(NAME, TYPE) typedef Handle<TYPE> NAME

#define VK_DEFINE_PLATFORM_TYPE(NAME, COMPATIBLE)                 \
    namespace pt                                                  \
    {                                                             \
    struct NAME                                                   \
    {                                                             \
        COMPATIBLE internal;                                      \
        explicit NAME(COMPATIBLE internal_) : internal(internal_) \
        {                                                         \
        }                                                         \
        NAME(std::nullptr_t) : internal()                         \
        {                                                         \
        }                                                         \
    };                                                            \
    } // pt

#define VK_MAKE_API_VERSION(VARIANT, MAJOR, MINOR, PATCH) \
    ((((uint32_t)(VARIANT)) << 29) | (((uint32_t)(MAJOR)) << 22) | (((uint32_t)(MINOR)) << 12) | ((uint32_t)(PATCH)))
#define VKSC_API_VARIANT 1
#define VK_MAKE_VERSION(MAJOR, MINOR, PATCH) VK_MAKE_API_VERSION(0, MAJOR, MINOR, PATCH)
#define VK_BIT(NUM) (1u << (uint32_t)(NUM))

#define VK_API_VERSION_VARIANT(version) ((uint32_t)(version) >> 29)
#define VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22) & 0x7FU)
#define VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3FFU)
#define VK_API_VERSION_PATCH(version) ((uint32_t)(version)&0xFFFU)

#define VK_CHECK(EXPR) vk::checkResult((EXPR), #EXPR, __FILE__, __LINE__)
#define VK_CHECK_SUPPORTED(EXPR) vk::checkResultSupported((EXPR), #EXPR, __FILE__, __LINE__)
#define VK_CHECK_MSG(EXPR, MSG) vk::checkResult((EXPR), MSG, __FILE__, __LINE__)
#define VK_CHECK_WSI(EXPR) vk::checkWsiResult((EXPR), #EXPR, __FILE__, __LINE__)

#define VK_MAKE_VIDEO_STD_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

/*--------------------------------------------------------------------*//*!
 * \brief Vulkan utilities
 *//*--------------------------------------------------------------------*/
namespace vk
{

typedef uint64_t VkDeviceSize;
typedef uint32_t VkSampleMask;
typedef uint32_t VkBool32;
typedef uint32_t VkFlags;
typedef uint64_t VkFlags64;
typedef uint64_t VkDeviceAddress;

// enum HandleType { HANDLE_TYPE_INSTANCE, ... };
#include "vkHandleType.inl"

template <HandleType Type>
class Handle
{
public:
    Handle(void)
    {
    } // \note Left uninitialized on purpose
    Handle(VkNullHandleType) : m_internal(0)
    {
    }

    // Helpers for vkNullDriver (an internal implementation of a Vulkan driver).
    template <typename T>
    Handle(const T *obj) : m_internal(reinterpret_cast<uintptr_t>(obj))
    {
    }
    template <typename T>
    T *as() const
    {
        return reinterpret_cast<T *>(m_internal);
    }

    Handle &operator=(VkNullHandleType)
    {
        m_internal = 0;
        return *this;
    }

    bool operator==(const Handle<Type> &other) const
    {
        return this->m_internal == other.m_internal;
    }
    bool operator!=(const Handle<Type> &other) const
    {
        return this->m_internal != other.m_internal;
    }

    bool operator!(void) const
    {
        return !m_internal;
    }

    uint64_t getInternal(void) const
    {
        return m_internal;
    }

    enum
    {
        HANDLE_TYPE = Type
    };

private:
    uint64_t m_internal;
};

template <HandleType Type>
bool operator<(const Handle<Type> &lhs, const Handle<Type> &rhs)
{
    return lhs.getInternal() < rhs.getInternal();
}

#include "vkBasicTypes.inl"

#define VK_CORE_FORMAT_LAST ((vk::VkFormat)(vk::VK_FORMAT_ASTC_12x12_SRGB_BLOCK + 1))
#define VK_CORE_IMAGE_TILING_LAST ((vk::VkImageTiling)(vk::VK_IMAGE_TILING_LINEAR + 1))
#define VK_CORE_IMAGE_TYPE_LAST ((vk::VkImageType)(vk::VK_IMAGE_TYPE_3D + 1))

enum SpirvVersion
{
    SPIRV_VERSION_1_0 = 0, //!< SPIR-V 1.0
    SPIRV_VERSION_1_1 = 1, //!< SPIR-V 1.1
    SPIRV_VERSION_1_2 = 2, //!< SPIR-V 1.2
    SPIRV_VERSION_1_3 = 3, //!< SPIR-V 1.3
    SPIRV_VERSION_1_4 = 4, //!< SPIR-V 1.4
    SPIRV_VERSION_1_5 = 5, //!< SPIR-V 1.5
    SPIRV_VERSION_1_6 = 6, //!< SPIR-V 1.6

    SPIRV_VERSION_LAST
};

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t generator;
    uint32_t bound;
} SpirvBinaryHeader;

namespace wsi
{

enum Type
{
    TYPE_XLIB = 0,
    TYPE_XCB,
    TYPE_WAYLAND,
    TYPE_ANDROID,
    TYPE_WIN32,
    TYPE_METAL,
    TYPE_HEADLESS,
    TYPE_DIRECT_DRM,

    TYPE_LAST
};

} // namespace wsi

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

typedef void *VkRemoteAddressNV;

extern "C"
{
#include "vkFunctionPointerTypes.inl"
}

class PlatformInterface
{
public:
#include "vkVirtualPlatformInterface.inl"

    virtual GetInstanceProcAddrFunc getGetInstanceProcAddr() const = 0;

    virtual ~PlatformInterface()
    {
    }
    PlatformInterface(const PlatformInterface &)            = delete;
    PlatformInterface &operator=(const PlatformInterface &) = delete;

protected:
    PlatformInterface(void)
    {
    }
};

class InstanceInterface
{
public:
#include "vkVirtualInstanceInterface.inl"

    virtual ~InstanceInterface()
    {
    }
    InstanceInterface(const InstanceInterface &)            = delete;
    InstanceInterface &operator=(const InstanceInterface &) = delete;

protected:
    InstanceInterface(void)
    {
    }
};

class DeviceInterface
{
public:
#include "vkVirtualDeviceInterface.inl"

#ifdef CTS_USES_VULKANSC
    virtual VkResult createShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
                                        const VkAllocationCallbacks *pAllocator,
                                        VkShaderModule *pShaderModule) const = 0;
#endif // CTS_USES_VULKANSC

    virtual ~DeviceInterface()
    {
    }
    DeviceInterface(const DeviceInterface &)            = delete;
    DeviceInterface &operator=(const DeviceInterface &) = delete;

protected:
    DeviceInterface(void)
    {
    }
};

class Error : public tcu::TestError
{
public:
    Error(VkResult error, const char *message, const char *expr, const char *file, int line, qpTestResult result);
    Error(VkResult error, const char *message, const char *expr, const char *file, int line);
    Error(VkResult error, const std::string &message);
    virtual ~Error(void) throw();

    VkResult getError(void) const
    {
        return m_error;
    }

private:
    const VkResult m_error;
};

class NotSupportedError : public tcu::NotSupportedError
{
public:
    NotSupportedError(VkResult error, const char *message, const char *expr, const char *file, int line);
    NotSupportedError(VkResult error, const std::string &message);
    virtual ~NotSupportedError(void) throw();

    VkResult getError(void) const
    {
        return m_error;
    }

private:
    const VkResult m_error;
};

class OutOfMemoryError : public tcu::ResourceError
{
public:
    OutOfMemoryError(VkResult error, const char *message, const char *expr, const char *file, int line);
    OutOfMemoryError(VkResult error, const std::string &message);
    virtual ~OutOfMemoryError(void) throw();

    VkResult getError(void) const
    {
        return m_error;
    }

private:
    const VkResult m_error;
};

void checkResult(VkResult result, const char *message, const char *file, int line);
void checkResultSupported(VkResult result, const char *message, const char *file, int line);
void checkWsiResult(VkResult result, const char *message, const char *file, int line);

} // namespace vk

#endif // _VKDEFS_HPP
