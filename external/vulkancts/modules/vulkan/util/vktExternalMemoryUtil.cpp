/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief Vulkan external memory utilities
 *//*--------------------------------------------------------------------*/

#include "vktExternalMemoryUtil.hpp"

#include "vkQueryUtil.hpp"

#ifndef CTS_USES_VULKANSC

#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX)
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if (DE_OS == DE_OS_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#if (DE_OS == DE_OS_FUCHSIA)
#include <zircon/syscalls.h>
#include <zircon/types.h>
#endif

#include <limits>

namespace vkt
{
namespace ExternalMemoryUtil
{
namespace
{

constexpr int kInvalidFd = std::numeric_limits<int>::min();

} // namespace

NativeHandle::NativeHandle(void)
    : m_fd(kInvalidFd)
    , m_zirconHandle(0u)
    , m_win32HandleType(WIN32HANDLETYPE_LAST)
    , m_win32Handle(nullptr)
    , m_androidHardwareBuffer(nullptr)
    , m_hostPtr(nullptr)
{
}

NativeHandle::NativeHandle(const NativeHandle &other)
    : m_fd(kInvalidFd)
    , m_zirconHandle(0u)
    , m_win32HandleType(WIN32HANDLETYPE_LAST)
    , m_win32Handle(nullptr)
    , m_androidHardwareBuffer(nullptr)
    , m_hostPtr(nullptr)
{
    if (other.m_fd >= 0)
    {
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX)
        DE_ASSERT(!other.m_win32Handle.internal);
        DE_ASSERT(!other.m_androidHardwareBuffer.internal);
        m_fd = dup(other.m_fd);
        TCU_CHECK(m_fd >= 0);
#else
        DE_FATAL("Platform doesn't support file descriptors");
#endif
    }
    else if (other.m_zirconHandle.internal)
    {
#if (DE_OS == DE_OS_FUCHSIA)
        DE_ASSERT(!other.m_win32Handle.internal);
        zx_handle_duplicate(other.m_zirconHandle.internal, ZX_RIGHT_SAME_RIGHTS, &m_zirconHandle.internal);
#else
        DE_FATAL("Platform doesn't support zircon handles");
#endif
    }
    else if (other.m_win32Handle.internal)
    {
#if (DE_OS == DE_OS_WIN32)
        m_win32HandleType = other.m_win32HandleType;

        switch (other.m_win32HandleType)
        {
        case WIN32HANDLETYPE_NT:
        {
            DE_ASSERT(other.m_fd == kInvalidFd);
            DE_ASSERT(!other.m_androidHardwareBuffer.internal);

            const HANDLE process = ::GetCurrentProcess();
            ::DuplicateHandle(process, other.m_win32Handle.internal, process, &m_win32Handle.internal, 0, TRUE,
                              DUPLICATE_SAME_ACCESS);

            break;
        }

        case WIN32HANDLETYPE_KMT:
        {
            m_win32Handle = other.m_win32Handle;
            break;
        }

        default:
            DE_FATAL("Unknown win32 handle type");
        }
#else
        DE_FATAL("Platform doesn't support win32 handles");
#endif
    }
    else if (other.m_androidHardwareBuffer.internal)
    {
        DE_ASSERT(other.m_fd == kInvalidFd);
        DE_ASSERT(!other.m_win32Handle.internal);
        m_androidHardwareBuffer = other.m_androidHardwareBuffer;

        if (AndroidHardwareBufferExternalApi *ahbApi = AndroidHardwareBufferExternalApi::getInstance())
            ahbApi->acquire(m_androidHardwareBuffer);
        else
            DE_FATAL("Platform doesn't support Android Hardware Buffer handles");
    }
    else if (other.m_metalHandle)
    {
#if (DE_OS == DE_OS_OSX)
        m_metalHandle = other.m_metalHandle;
#else
        DE_FATAL("Platform doesn't support Metal resources");
#endif
    }
    else
        DE_FATAL("Native handle can't be duplicated");
}

NativeHandle::NativeHandle(int fd)
    : m_fd(fd)
    , m_zirconHandle(0u)
    , m_win32HandleType(WIN32HANDLETYPE_LAST)
    , m_win32Handle(nullptr)
    , m_androidHardwareBuffer(nullptr)
    , m_hostPtr(nullptr)
{
}

NativeHandle::NativeHandle(Win32HandleType handleType, vk::pt::Win32Handle handle)
    : m_fd(kInvalidFd)
    , m_zirconHandle(0u)
    , m_win32HandleType(handleType)
    , m_win32Handle(handle)
    , m_androidHardwareBuffer(nullptr)
    , m_hostPtr(nullptr)
{
}

NativeHandle::NativeHandle(vk::pt::AndroidHardwareBufferPtr buffer)
    : m_fd(kInvalidFd)
    , m_zirconHandle(0u)
    , m_win32HandleType(WIN32HANDLETYPE_LAST)
    , m_win32Handle(nullptr)
    , m_androidHardwareBuffer(buffer)
    , m_hostPtr(nullptr)
{
}

NativeHandle::NativeHandle(void *handle)
    : m_fd(kInvalidFd)
    , m_zirconHandle(0u)
    , m_win32HandleType(WIN32HANDLETYPE_LAST)
    , m_win32Handle(nullptr)
    , m_androidHardwareBuffer(nullptr)
    , m_hostPtr(nullptr)
    , m_metalHandle(handle)
{
}

NativeHandle::~NativeHandle(void)
{
    reset();
}

void NativeHandle::reset(void)
{
    if (m_fd >= 0)
    {
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX)
        DE_ASSERT(!m_win32Handle.internal);
        DE_ASSERT(!m_androidHardwareBuffer.internal);
        ::close(m_fd);
#else
        DE_FATAL("Platform doesn't support file descriptors");
#endif
    }

    if (m_zirconHandle.internal != 0u)
    {
#if (DE_OS == DE_OS_FUCHSIA)
        zx_handle_close(m_zirconHandle.internal);
#else
        DE_FATAL("Platform doesn't support fuchsia handles");
#endif
    }

    if (m_win32Handle.internal)
    {
#if (DE_OS == DE_OS_WIN32)
        switch (m_win32HandleType)
        {
        case WIN32HANDLETYPE_NT:
            DE_ASSERT(m_fd == kInvalidFd);
            DE_ASSERT(!m_androidHardwareBuffer.internal);
            ::CloseHandle((HANDLE)m_win32Handle.internal);
            break;

        case WIN32HANDLETYPE_KMT:
            break;

        default:
            DE_FATAL("Unknown win32 handle type");
        }
#else
        DE_FATAL("Platform doesn't support win32 handles");
#endif
    }
    if (m_androidHardwareBuffer.internal)
    {
        DE_ASSERT(m_fd == kInvalidFd);
        DE_ASSERT(!m_win32Handle.internal);

        if (AndroidHardwareBufferExternalApi *ahbApi = AndroidHardwareBufferExternalApi::getInstance())
            ahbApi->release(m_androidHardwareBuffer);
        else
            DE_FATAL("Platform doesn't support Android Hardware Buffer handles");
    }
    if (m_metalHandle)
    {
#if (DE_OS == DE_OS_OSX)
        // TODO Aitor: Release resource
#else
        DE_FATAL("Platform doesn't support Metal resources");
#endif
    }

    m_fd                    = kInvalidFd;
    m_zirconHandle          = vk::pt::zx_handle_t(0u);
    m_win32Handle           = vk::pt::Win32Handle(nullptr);
    m_win32HandleType       = WIN32HANDLETYPE_LAST;
    m_androidHardwareBuffer = vk::pt::AndroidHardwareBufferPtr(nullptr);
    m_hostPtr               = nullptr;
    m_metalHandle           = nullptr;
}

NativeHandle &NativeHandle::operator=(int fd)
{
    reset();

    m_fd = fd;

    return *this;
}

NativeHandle &NativeHandle::operator=(vk::pt::AndroidHardwareBufferPtr buffer)
{
    reset();

    m_androidHardwareBuffer = buffer;

    return *this;
}

void NativeHandle::setWin32Handle(Win32HandleType type, vk::pt::Win32Handle handle)
{
    reset();

    m_win32HandleType = type;
    m_win32Handle     = handle;
}

void NativeHandle::setZirconHandle(vk::pt::zx_handle_t zirconHandle)
{
    reset();

    m_zirconHandle = zirconHandle;
}

void NativeHandle::setMetalHandle(void *metalHandle)
{
    reset();

    m_metalHandle = metalHandle;
}

void NativeHandle::setHostPtr(void *hostPtr)
{
    reset();

    m_hostPtr = hostPtr;
}

void NativeHandle::disown(void)
{
    m_fd                    = kInvalidFd;
    m_zirconHandle          = vk::pt::zx_handle_t(0u);
    m_win32Handle           = vk::pt::Win32Handle(nullptr);
    m_androidHardwareBuffer = vk::pt::AndroidHardwareBufferPtr(nullptr);
    m_hostPtr               = nullptr;
    m_metalHandle           = nullptr;
}

vk::pt::Win32Handle NativeHandle::getWin32Handle(void) const
{
    DE_ASSERT(m_fd == kInvalidFd);
    DE_ASSERT(!m_androidHardwareBuffer.internal);
    DE_ASSERT(m_hostPtr == nullptr);

    return m_win32Handle;
}

bool NativeHandle::hasValidFd(void) const
{
    return (m_fd != kInvalidFd);
}

int NativeHandle::getFd(void) const
{
    DE_ASSERT(!m_win32Handle.internal);
    DE_ASSERT(!m_androidHardwareBuffer.internal);
    DE_ASSERT(m_hostPtr == nullptr);
    DE_ASSERT(!m_metalHandle);
    return m_fd;
}

vk::pt::zx_handle_t NativeHandle::getZirconHandle(void) const
{
    DE_ASSERT(!m_win32Handle.internal);
    DE_ASSERT(!m_androidHardwareBuffer.internal);
    DE_ASSERT(!m_metalHandle);

    return m_zirconHandle;
}

vk::pt::AndroidHardwareBufferPtr NativeHandle::getAndroidHardwareBuffer(void) const
{
    DE_ASSERT(m_fd == kInvalidFd);
    DE_ASSERT(!m_win32Handle.internal);
    DE_ASSERT(m_hostPtr == nullptr);
    DE_ASSERT(!m_metalHandle);
    return m_androidHardwareBuffer;
}

void *NativeHandle::getHostPtr(void) const
{
    DE_ASSERT(m_fd == kInvalidFd);
    DE_ASSERT(!m_win32Handle.internal);
    DE_ASSERT(!m_metalHandle);
    return m_hostPtr;
}

void *NativeHandle::getMetalHandle(void) const
{
    DE_ASSERT(m_fd == kInvalidFd);
    DE_ASSERT(!m_win32Handle.internal);
    DE_ASSERT(m_hostPtr == nullptr);
    return m_metalHandle;
}

const char *externalSemaphoreTypeToName(vk::VkExternalSemaphoreHandleTypeFlagBits type)
{
    switch (type)
    {
    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
        return "opaque_fd";

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
        return "opaque_win32";

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
        return "opaque_win32_kmt";

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT:
        return "d3d12_fenc";

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
        return "sync_fd";

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA:
        return "zircon_event";

    default:
        DE_FATAL("Unknown external semaphore type");
        return nullptr;
    }
}

const char *externalFenceTypeToName(vk::VkExternalFenceHandleTypeFlagBits type)
{
    switch (type)
    {
    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT:
        return "opaque_fd";

    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
        return "opaque_win32";

    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
        return "opaque_win32_kmt";

    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT:
        return "sync_fd";

    default:
        DE_FATAL("Unknown external fence type");
        return nullptr;
    }
}

const char *externalMemoryTypeToName(vk::VkExternalMemoryHandleTypeFlagBits type)
{
    switch (type)
    {
    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT:
        return "opaque_fd";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
        return "opaque_win32";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
        return "opaque_win32_kmt";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT:
        return "d3d11_texture";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT:
        return "d3d11_texture_kmt";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT:
        return "d3d12_heap";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT:
        return "d3d12_resource";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID:
        return "android_hardware_buffer";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
        return "dma_buf";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT:
        return "host_allocation";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA:
        return "zircon_vmo";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_MTLBUFFER_BIT_EXT:
        return "mtlbuffer";

    case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_MTLTEXTURE_BIT_EXT:
        return "mtltexture";

    default:
        DE_FATAL("Unknown external memory type");
        return nullptr;
    }
}

bool isSupportedPermanence(vk::VkExternalSemaphoreHandleTypeFlagBits type, Permanence permanence)
{
    switch (type)
    {
    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
        return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
        return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
        return permanence == PERMANENCE_TEMPORARY;

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA:
        return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

    default:
        DE_FATAL("Unknown external semaphore type");
        return false;
    }
}

Transference getHandelTypeTransferences(vk::VkExternalSemaphoreHandleTypeFlagBits type)
{
    switch (type)
    {
    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
        return TRANSFERENCE_REFERENCE;

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
        return TRANSFERENCE_REFERENCE;

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
        return TRANSFERENCE_COPY;

    case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA:
        return TRANSFERENCE_REFERENCE;

    default:
        DE_FATAL("Unknown external semaphore type");
        return TRANSFERENCE_REFERENCE;
    }
}

bool isSupportedPermanence(vk::VkExternalFenceHandleTypeFlagBits type, Permanence permanence)
{
    switch (type)
    {
    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
        return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT:
        return permanence == PERMANENCE_PERMANENT || permanence == PERMANENCE_TEMPORARY;

    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT:
        return permanence == PERMANENCE_TEMPORARY;

    default:
        DE_FATAL("Unknown external fence type");
        return false;
    }
}

Transference getHandelTypeTransferences(vk::VkExternalFenceHandleTypeFlagBits type)
{
    switch (type)
    {
    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
        return TRANSFERENCE_REFERENCE;

    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT:
        return TRANSFERENCE_REFERENCE;

    case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT:
        return TRANSFERENCE_COPY;

    default:
        DE_FATAL("Unknown external fence type");
        return TRANSFERENCE_REFERENCE;
    }
}

int getMemoryFd(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceMemory memory,
                vk::VkExternalMemoryHandleTypeFlagBits externalType)
{
    const vk::VkMemoryGetFdInfoKHR info = {vk::VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, nullptr,

                                           memory, externalType};
    int fd                              = kInvalidFd;

    VK_CHECK(vkd.getMemoryFdKHR(device, &info, &fd));
    TCU_CHECK(fd >= 0);

    return fd;
}

void getMemoryNative(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceMemory memory,
                     vk::VkExternalMemoryHandleTypeFlagBits externalType, NativeHandle &nativeHandle)
{
    if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT ||
        externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
    {
        const vk::VkMemoryGetFdInfoKHR info = {vk::VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, nullptr,

                                               memory, externalType};
        int fd                              = kInvalidFd;

        VK_CHECK(vkd.getMemoryFdKHR(device, &info, &fd));
        TCU_CHECK(fd >= 0);
        nativeHandle = fd;
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA)
    {
        const vk::VkMemoryGetZirconHandleInfoFUCHSIA info = {
            vk::VK_STRUCTURE_TYPE_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA, nullptr,

            memory, externalType};
        vk::pt::zx_handle_t handle(0u);

        VK_CHECK(vkd.getMemoryZirconHandleFUCHSIA(device, &info, &handle));
        nativeHandle.setZirconHandle(handle);
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT ||
             externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
    {
        const vk::VkMemoryGetWin32HandleInfoKHR info = {vk::VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR, nullptr,

                                                        memory, externalType};
        vk::pt::Win32Handle handle(nullptr);

        VK_CHECK(vkd.getMemoryWin32HandleKHR(device, &info, &handle));

        switch (externalType)
        {
        case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
            nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_NT, handle);
            break;

        case vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
            nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_KMT, handle);
            break;

        default:
            DE_FATAL("Unknown external memory handle type");
        }
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
    {
        if (!AndroidHardwareBufferExternalApi::getInstance())
        {
            TCU_THROW(NotSupportedError, "Platform doesn't support Android Hardware Buffer handles");
        }
        const vk::VkMemoryGetAndroidHardwareBufferInfoANDROID info = {
            vk::VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
            nullptr,

            memory,
        };
        vk::pt::AndroidHardwareBufferPtr ahb(nullptr);

        VK_CHECK(vkd.getMemoryAndroidHardwareBufferANDROID(device, &info, &ahb));
        TCU_CHECK(ahb.internal);
        nativeHandle = ahb;
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_MTLBUFFER_BIT_EXT ||
             externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_MTLTEXTURE_BIT_EXT)
    {
        const vk::VkMemoryGetMetalHandleInfoEXT info = {vk::VK_STRUCTURE_TYPE_MEMORY_GET_METAL_HANDLE_INFO_EXT, nullptr,
                                                        memory, externalType};

        void *handle(nullptr);
        VK_CHECK(vkd.getMemoryMetalHandleEXT(device, &info, &handle));
        TCU_CHECK(handle);
        nativeHandle.setMetalHandle(handle);
    }
    else
        DE_FATAL("Unknown external memory handle type");
}

vk::Move<vk::VkFence> createExportableFence(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                            vk::VkExternalFenceHandleTypeFlagBits externalType)
{
    const vk::VkExportFenceCreateInfo exportCreateInfo = {vk::VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO, nullptr,
                                                          (vk::VkExternalFenceHandleTypeFlags)externalType};
    const vk::VkFenceCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, &exportCreateInfo, 0u};

    return vk::createFence(vkd, device, &createInfo);
}

int getFenceFd(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkFence fence,
               vk::VkExternalFenceHandleTypeFlagBits externalType)
{
    const vk::VkFenceGetFdInfoKHR info = {vk::VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR, nullptr,

                                          fence, externalType};
    int fd                             = kInvalidFd;

    VK_CHECK(vkd.getFenceFdKHR(device, &info, &fd));
    TCU_CHECK(fd >= 0);

    return fd;
}

void getFenceNative(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkFence fence,
                    vk::VkExternalFenceHandleTypeFlagBits externalType, NativeHandle &nativeHandle,
                    bool expectFenceUnsignaled)
{
    if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT ||
        externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT)
    {
        const vk::VkFenceGetFdInfoKHR info = {vk::VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR, nullptr,

                                              fence, externalType};
        int fd                             = kInvalidFd;

        VK_CHECK(vkd.getFenceFdKHR(device, &info, &fd));

        if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT)
        {
            TCU_CHECK(!expectFenceUnsignaled || (fd >= 0) || (fd == -1));
        }
        else
        {
            TCU_CHECK(fd >= 0);
        }

        nativeHandle = fd;
    }
    else if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT ||
             externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
    {
        const vk::VkFenceGetWin32HandleInfoKHR info = {vk::VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR, nullptr,

                                                       fence, externalType};
        vk::pt::Win32Handle handle(nullptr);

        VK_CHECK(vkd.getFenceWin32HandleKHR(device, &info, &handle));

        switch (externalType)
        {
        case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
            nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_NT, handle);
            break;

        case vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
            nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_KMT, handle);
            break;

        default:
            DE_FATAL("Unknow external memory handle type");
        }
    }
    else
        DE_FATAL("Unknow external fence handle type");
}

void importFence(const vk::DeviceInterface &vkd, const vk::VkDevice device, const vk::VkFence fence,
                 vk::VkExternalFenceHandleTypeFlagBits externalType, NativeHandle &handle, vk::VkFenceImportFlags flags)
{
    if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT ||
        externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT)
    {
        const vk::VkImportFenceFdInfoKHR importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR, nullptr, fence, flags, externalType, handle.getFd()};

        VK_CHECK(vkd.importFenceFdKHR(device, &importInfo));
        handle.disown();
    }
    else if (externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT ||
             externalType == vk::VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
    {
        const vk::VkImportFenceWin32HandleInfoKHR importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR,
            nullptr,
            fence,
            flags,
            externalType,
            handle.getWin32Handle(),
            (vk::pt::Win32LPCWSTR) nullptr};

        VK_CHECK(vkd.importFenceWin32HandleKHR(device, &importInfo));
        // \note Importing a fence payload from Windows handles does not transfer ownership of the handle to the Vulkan implementation,
        //   so we do not disown the handle until after all use has complete.
    }
    else
        DE_FATAL("Unknown fence external handle type");
}

vk::Move<vk::VkFence> createAndImportFence(const vk::DeviceInterface &vkd, const vk::VkDevice device,
                                           vk::VkExternalFenceHandleTypeFlagBits externalType, NativeHandle &handle,
                                           vk::VkFenceImportFlags flags)
{
    vk::Move<vk::VkFence> fence(createFence(vkd, device));

    importFence(vkd, device, *fence, externalType, handle, flags);

    return fence;
}

vk::Move<vk::VkSemaphore> createExportableSemaphore(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                    vk::VkExternalSemaphoreHandleTypeFlagBits externalType)
{
    const vk::VkExportSemaphoreCreateInfo exportCreateInfo = {vk::VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
                                                              nullptr,
                                                              (vk::VkExternalSemaphoreHandleTypeFlags)externalType};
    const vk::VkSemaphoreCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &exportCreateInfo, 0u};

    return vk::createSemaphore(vkd, device, &createInfo);
}

vk::Move<vk::VkSemaphore> createExportableSemaphoreType(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                        vk::VkSemaphoreType semaphoreType,
                                                        vk::VkExternalSemaphoreHandleTypeFlagBits externalType)
{
    const vk::VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {
        vk::VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        nullptr,
        semaphoreType,
        0,
    };
    const vk::VkExportSemaphoreCreateInfo exportCreateInfo = {vk::VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
                                                              &semaphoreTypeCreateInfo,
                                                              (vk::VkExternalSemaphoreHandleTypeFlags)externalType};
    const vk::VkSemaphoreCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &exportCreateInfo, 0u};

    return vk::createSemaphore(vkd, device, &createInfo);
}

int getSemaphoreFd(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkSemaphore semaphore,
                   vk::VkExternalSemaphoreHandleTypeFlagBits externalType)
{
    const vk::VkSemaphoreGetFdInfoKHR info = {vk::VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR, nullptr,

                                              semaphore, externalType};
    int fd                                 = kInvalidFd;

    VK_CHECK(vkd.getSemaphoreFdKHR(device, &info, &fd));
    TCU_CHECK(fd >= 0);

    return fd;
}

void getSemaphoreNative(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkSemaphore semaphore,
                        vk::VkExternalSemaphoreHandleTypeFlagBits externalType, NativeHandle &nativeHandle)
{
    if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT ||
        externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
    {
        const vk::VkSemaphoreGetFdInfoKHR info = {vk::VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR, nullptr,

                                                  semaphore, externalType};
        int fd                                 = kInvalidFd;

        VK_CHECK(vkd.getSemaphoreFdKHR(device, &info, &fd));

        if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT)
            TCU_CHECK(fd >= -1);
        else
            TCU_CHECK(fd >= 0);

        nativeHandle = fd;
    }
    else if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA)
    {
        const vk::VkSemaphoreGetZirconHandleInfoFUCHSIA info = {
            vk::VK_STRUCTURE_TYPE_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA, nullptr,

            semaphore, externalType};
        vk::pt::zx_handle_t zirconHandle(0u);

        VK_CHECK(vkd.getSemaphoreZirconHandleFUCHSIA(device, &info, &zirconHandle));
        nativeHandle.setZirconHandle(zirconHandle);
    }
    else if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT ||
             externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
    {
        const vk::VkSemaphoreGetWin32HandleInfoKHR info = {vk::VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
                                                           nullptr,

                                                           semaphore, externalType};
        vk::pt::Win32Handle handle(nullptr);

        VK_CHECK(vkd.getSemaphoreWin32HandleKHR(device, &info, &handle));

        switch (externalType)
        {
        case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
            nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_NT, handle);
            break;

        case vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
            nativeHandle.setWin32Handle(NativeHandle::WIN32HANDLETYPE_KMT, handle);
            break;

        default:
            DE_FATAL("Unknow external memory handle type");
        }
    }
    else
        DE_FATAL("Unknow external semaphore handle type");
}

void importSemaphore(const vk::DeviceInterface &vkd, const vk::VkDevice device, const vk::VkSemaphore semaphore,
                     vk::VkExternalSemaphoreHandleTypeFlagBits externalType, NativeHandle &handle,
                     vk::VkSemaphoreImportFlags flags)
{
    if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT ||
        externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
    {
        const vk::VkImportSemaphoreFdInfoKHR importInfo = {vk::VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
                                                           nullptr,
                                                           semaphore,
                                                           flags,
                                                           externalType,
                                                           handle.getFd()};

        VK_CHECK(vkd.importSemaphoreFdKHR(device, &importInfo));
        handle.disown();
    }
    else if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA)
    {
        const vk::VkImportSemaphoreZirconHandleInfoFUCHSIA importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA,
            nullptr,
            semaphore,
            flags,
            externalType,
            handle.getZirconHandle()};

        VK_CHECK(vkd.importSemaphoreZirconHandleFUCHSIA(device, &importInfo));
        handle.disown();
    }
    else if (externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT ||
             externalType == vk::VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
    {
        const vk::VkImportSemaphoreWin32HandleInfoKHR importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
            nullptr,
            semaphore,
            flags,
            externalType,
            handle.getWin32Handle(),
            (vk::pt::Win32LPCWSTR) nullptr};

        VK_CHECK(vkd.importSemaphoreWin32HandleKHR(device, &importInfo));
        // \note Importing a semaphore payload from Windows handles does not transfer ownership of the handle to the Vulkan implementation,
        //   so we do not disown the handle until after all use has complete.
    }
    else
        DE_FATAL("Unknown semaphore external handle type");
}

vk::Move<vk::VkSemaphore> createAndImportSemaphore(const vk::DeviceInterface &vkd, const vk::VkDevice device,
                                                   vk::VkExternalSemaphoreHandleTypeFlagBits externalType,
                                                   NativeHandle &handle, vk::VkSemaphoreImportFlags flags)
{
    vk::Move<vk::VkSemaphore> semaphore(createSemaphore(vkd, device));

    importSemaphore(vkd, device, *semaphore, externalType, handle, flags);

    return semaphore;
}

uint32_t chooseMemoryType(uint32_t bits)
{
    if (bits == 0)
        return 0;

    for (uint32_t memoryTypeIndex = 0; (1u << memoryTypeIndex) <= bits; memoryTypeIndex++)
    {
        if ((bits & (1u << memoryTypeIndex)) != 0)
            return memoryTypeIndex;
    }

    DE_FATAL("No supported memory types");
    return -1;
}

uint32_t chooseHostVisibleMemoryType(uint32_t bits, const vk::VkPhysicalDeviceMemoryProperties properties)
{
    DE_ASSERT(bits != 0);

    for (uint32_t memoryTypeIndex = 0; (1u << memoryTypeIndex) <= bits; memoryTypeIndex++)
    {
        if (((bits & (1u << memoryTypeIndex)) != 0) &&
            ((properties.memoryTypes[memoryTypeIndex].propertyFlags & vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0))
            return memoryTypeIndex;
    }

    TCU_THROW(NotSupportedError, "No supported memory type found");
    return -1;
}

vk::VkMemoryRequirements getImageMemoryRequirements(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                    vk::VkImage image,
                                                    vk::VkExternalMemoryHandleTypeFlagBits externalType)
{
    if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
    {
        return {0u, 0u, 0u};
    }
    else
    {
        return vk::getImageMemoryRequirements(vkd, device, image);
    }
}

vk::Move<vk::VkDeviceMemory> allocateExportableMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                      vk::VkDeviceSize allocationSize, uint32_t memoryTypeIndex,
                                                      vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                      vk::VkBuffer buffer)
{
    const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                                                             nullptr,

                                                             VK_NULL_HANDLE, buffer};
    const vk::VkExportMemoryAllocateInfo exportInfo       = {vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
                                                       !!buffer ? &dedicatedInfo : nullptr,
                                                             (vk::VkExternalMemoryHandleTypeFlags)externalType};
    const vk::VkMemoryAllocateInfo info = {vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &exportInfo, allocationSize,
                                           memoryTypeIndex};
    return vk::allocateMemory(vkd, device, &info);
}

vk::Move<vk::VkDeviceMemory> allocateExportableMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                      vk::VkDeviceSize allocationSize, uint32_t memoryTypeIndex,
                                                      vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                      vk::VkImage image)
{
    const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                                                             nullptr,

                                                             image, VK_NULL_HANDLE};
    const vk::VkExportMemoryAllocateInfo exportInfo       = {vk::VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
                                                       !!image ? &dedicatedInfo : nullptr,
                                                             (vk::VkExternalMemoryHandleTypeFlags)externalType};
    const vk::VkMemoryAllocateInfo info = {vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &exportInfo, allocationSize,
                                           memoryTypeIndex};
    return vk::allocateMemory(vkd, device, &info);
}

static vk::Move<vk::VkDeviceMemory> importMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                 vk::VkBuffer buffer, vk::VkImage image,
                                                 const vk::VkMemoryRequirements &requirements,
                                                 vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                 uint32_t memoryTypeIndex, NativeHandle &handle)
{
    const bool isDedicated = !!buffer || !!image;

    DE_ASSERT(!buffer || !image);

    if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT ||
        externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
    {
        const vk::VkImportMemoryFdInfoKHR importInfo = {vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr,
                                                        externalType, handle.getFd()};
        const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {
            vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            &importInfo,
            image,
            buffer,
        };
        const vk::VkMemoryAllocateInfo info = {
            vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            (isDedicated ? (const void *)&dedicatedInfo : (const void *)&importInfo), requirements.size,
            (memoryTypeIndex == ~0U) ? chooseMemoryType(requirements.memoryTypeBits) : memoryTypeIndex};
        vk::Move<vk::VkDeviceMemory> memory(vk::allocateMemory(vkd, device, &info));

        handle.disown();

        return memory;
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA)
    {
        const vk::VkImportMemoryZirconHandleInfoFUCHSIA importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_ZIRCON_HANDLE_INFO_FUCHSIA, nullptr, externalType,
            handle.getZirconHandle()};
        const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {
            vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            &importInfo,
            image,
            buffer,
        };
        const vk::VkMemoryAllocateInfo info = {
            vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            (isDedicated ? (const void *)&dedicatedInfo : (const void *)&importInfo), requirements.size,
            (memoryTypeIndex == ~0U) ? chooseMemoryType(requirements.memoryTypeBits) : memoryTypeIndex};
        vk::Move<vk::VkDeviceMemory> memory(vk::allocateMemory(vkd, device, &info));

        handle.disown();

        return memory;
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT ||
             externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
    {
        const vk::VkImportMemoryWin32HandleInfoKHR importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR, nullptr, externalType, handle.getWin32Handle(),
            (vk::pt::Win32LPCWSTR) nullptr};
        const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {
            vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            &importInfo,
            image,
            buffer,
        };
        const vk::VkMemoryAllocateInfo info = {
            vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            (isDedicated ? (const void *)&dedicatedInfo : (const void *)&importInfo), requirements.size,
            (memoryTypeIndex == ~0U) ? chooseMemoryType(requirements.memoryTypeBits) : memoryTypeIndex};
        vk::Move<vk::VkDeviceMemory> memory(vk::allocateMemory(vkd, device, &info));

        // The handle's owned reference must also be released. Do not discard the handle below.
        if (externalType != vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT)
            handle.disown();

        return memory;
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID)
    {
        AndroidHardwareBufferExternalApi *ahbApi = AndroidHardwareBufferExternalApi::getInstance();
        if (!ahbApi)
        {
            TCU_THROW(NotSupportedError, "Platform doesn't support Android Hardware Buffer handles");
        }

        uint32_t ahbFormat = 0;
        ahbApi->describe(handle.getAndroidHardwareBuffer(), nullptr, nullptr, nullptr, &ahbFormat, nullptr, nullptr);
        DE_ASSERT(ahbApi->ahbFormatIsBlob(ahbFormat) || image != VK_NULL_HANDLE);

        vk::VkAndroidHardwareBufferPropertiesANDROID ahbProperties = {
            vk::VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, nullptr, 0u, 0u};

        VK_CHECK(
            vkd.getAndroidHardwareBufferPropertiesANDROID(device, handle.getAndroidHardwareBuffer(), &ahbProperties));

        vk::VkImportAndroidHardwareBufferInfoANDROID importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID, nullptr,
            handle.getAndroidHardwareBuffer()};
        const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {
            vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
            &importInfo,
            image,
            buffer,
        };
        const vk::VkMemoryAllocateInfo info = {
            vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            (isDedicated ? (const void *)&dedicatedInfo : (const void *)&importInfo), ahbProperties.allocationSize,
            (memoryTypeIndex == ~0U) ? chooseMemoryType(ahbProperties.memoryTypeBits) : memoryTypeIndex};
        vk::Move<vk::VkDeviceMemory> memory(vk::allocateMemory(vkd, device, &info));

        return memory;
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT)
    {
        DE_ASSERT(memoryTypeIndex != ~0U);

        const vk::VkImportMemoryHostPointerInfoEXT importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT, nullptr, externalType, handle.getHostPtr()};
        const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {
            vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            &importInfo,
            image,
            buffer,
        };
        const vk::VkMemoryAllocateInfo info = {vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                               (isDedicated ? (const void *)&dedicatedInfo : (const void *)&importInfo),
                                               requirements.size, memoryTypeIndex};
        vk::Move<vk::VkDeviceMemory> memory(vk::allocateMemory(vkd, device, &info));

        return memory;
    }
    else if (externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_MTLBUFFER_BIT_EXT ||
             externalType == vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_MTLTEXTURE_BIT_EXT)
    {
        const vk::VkImportMemoryMetalHandleInfoEXT importInfo = {
            vk::VK_STRUCTURE_TYPE_IMPORT_MEMORY_METAL_HANDLE_INFO_EXT, nullptr, externalType, handle.getMetalHandle()};
        const vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {
            vk::VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            &importInfo,
            image,
            buffer,
        };
        const vk::VkMemoryAllocateInfo info = {
            vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            (isDedicated ? (const void *)&dedicatedInfo : (const void *)&importInfo), requirements.size,
            (memoryTypeIndex == ~0U) ? chooseMemoryType(requirements.memoryTypeBits) : memoryTypeIndex};
        vk::Move<vk::VkDeviceMemory> memory(vk::allocateMemory(vkd, device, &info));

        return memory;
    }
    else
    {
        DE_FATAL("Unknown external memory type");
        return vk::Move<vk::VkDeviceMemory>();
    }
}

vk::Move<vk::VkDeviceMemory> importMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                          const vk::VkMemoryRequirements &requirements,
                                          vk::VkExternalMemoryHandleTypeFlagBits externalType, uint32_t memoryTypeIndex,
                                          NativeHandle &handle)
{
    return importMemory(vkd, device, VK_NULL_HANDLE, VK_NULL_HANDLE, requirements, externalType, memoryTypeIndex,
                        handle);
}

vk::Move<vk::VkDeviceMemory> importDedicatedMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                   vk::VkBuffer buffer, const vk::VkMemoryRequirements &requirements,
                                                   vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                   uint32_t memoryTypeIndex, NativeHandle &handle)
{
    return importMemory(vkd, device, buffer, VK_NULL_HANDLE, requirements, externalType, memoryTypeIndex, handle);
}

vk::Move<vk::VkDeviceMemory> importDedicatedMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                   vk::VkImage image, const vk::VkMemoryRequirements &requirements,
                                                   vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                   uint32_t memoryTypeIndex, NativeHandle &handle)
{
    return importMemory(vkd, device, VK_NULL_HANDLE, image, requirements, externalType, memoryTypeIndex, handle);
}

vk::Move<vk::VkBuffer> createExternalBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                            uint32_t queueFamilyIndex,
                                            vk::VkExternalMemoryHandleTypeFlagBits externalType, vk::VkDeviceSize size,
                                            vk::VkBufferCreateFlags createFlags, vk::VkBufferUsageFlags usageFlags)
{
    const vk::VkExternalMemoryBufferCreateInfo externalCreateInfo = {
        vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
        (vk::VkExternalMemoryHandleTypeFlags)externalType};
    const vk::VkBufferCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                               &externalCreateInfo,
                                               createFlags,
                                               size,
                                               usageFlags,
                                               vk::VK_SHARING_MODE_EXCLUSIVE,
                                               1u,
                                               &queueFamilyIndex};

    return vk::createBuffer(vkd, device, &createInfo);
}

vk::Move<vk::VkImage> createExternalImage(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                          uint32_t queueFamilyIndex,
                                          vk::VkExternalMemoryHandleTypeFlagBits externalType, vk::VkFormat format,
                                          uint32_t width, uint32_t height, vk::VkImageTiling tiling,
                                          vk::VkImageCreateFlags createFlags, vk::VkImageUsageFlags usageFlags,
                                          uint32_t mipLevels, uint32_t arrayLayers)
{
    if (createFlags & vk::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT && arrayLayers < 6u)
        arrayLayers = 6u;

    const vk::VkExternalMemoryImageCreateInfo externalCreateInfo = {
        vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, nullptr,
        (vk::VkExternalMemoryHandleTypeFlags)externalType};
    const vk::VkImageCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                              &externalCreateInfo,
                                              createFlags,
                                              vk::VK_IMAGE_TYPE_2D,
                                              format,
                                              {
                                                  width,
                                                  height,
                                                  1u,
                                              },
                                              mipLevels,
                                              arrayLayers,
                                              vk::VK_SAMPLE_COUNT_1_BIT,
                                              tiling,
                                              usageFlags,
                                              vk::VK_SHARING_MODE_EXCLUSIVE,
                                              1,
                                              &queueFamilyIndex,
                                              vk::VK_IMAGE_LAYOUT_UNDEFINED};

    return vk::createImage(vkd, device, &createInfo);
}

vk::VkPhysicalDeviceExternalMemoryHostPropertiesEXT getPhysicalDeviceExternalMemoryHostProperties(
    const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice)
{
    vk::VkPhysicalDeviceExternalMemoryHostPropertiesEXT externalProps = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
        nullptr,
        0u,
    };

    vk::VkPhysicalDeviceProperties2 props2;
    deMemset(&props2, 0, sizeof(props2));
    props2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &externalProps;

    vki.getPhysicalDeviceProperties2(physicalDevice, &props2);

    return externalProps;
}

} // namespace ExternalMemoryUtil
} // namespace vkt

#endif // CTS_USES_VULKANSC
