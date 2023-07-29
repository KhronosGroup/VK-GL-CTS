#ifndef _VKTEXTERNALMEMORYUTIL_HPP
#define _VKTEXTERNALMEMORYUTIL_HPP
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

#include "tcuDefs.hpp"

#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"

#include "deMemory.h"
#include "deInt32.h"

namespace vkt
{
namespace ExternalMemoryUtil
{

class NativeHandle
{
public:
    enum Win32HandleType
    {
        WIN32HANDLETYPE_NT = 0,
        WIN32HANDLETYPE_KMT,

        WIN32HANDLETYPE_LAST
    };

    NativeHandle(void);
    NativeHandle(const NativeHandle &other);
    NativeHandle(int fd);
    NativeHandle(Win32HandleType type, vk::pt::Win32Handle handle);
    NativeHandle(vk::pt::AndroidHardwareBufferPtr buffer);
    ~NativeHandle(void);

    NativeHandle &operator=(int fd);
    NativeHandle &operator=(vk::pt::AndroidHardwareBufferPtr buffer);

    void setWin32Handle(Win32HandleType type, vk::pt::Win32Handle handle);
    vk::pt::Win32Handle getWin32Handle(void) const;
    void setHostPtr(void *hostPtr);
    void *getHostPtr(void) const;
    int getFd(void) const;
    vk::pt::AndroidHardwareBufferPtr getAndroidHardwareBuffer(void) const;
    void disown(void);
    void reset(void);

private:
    int m_fd;
    Win32HandleType m_win32HandleType;
    vk::pt::Win32Handle m_win32Handle;
    vk::pt::AndroidHardwareBufferPtr m_androidHardwareBuffer;
    void *m_hostPtr;

    // Disabled
    NativeHandle &operator=(const NativeHandle &);
};

class AndroidHardwareBufferExternalApi
{
public:
    /**
     * getInstance obtains the object, that provides an interface to AHB system APIs .
     * If the AHB system API is not supported or if it is not built as supported with the CTS,
     * then this function would return a null object.
     */
    static AndroidHardwareBufferExternalApi *getInstance();

    /* Is AndroidHardwareBuffer supported? */
    static bool supportsAhb();

    /* Are Cube maps supported on current api level? */
    static bool supportsCubeMap();

    /**
     * Allocates a buffer that backs an AHardwareBuffer using the passed parameter as follows:
     * width;      - width in pixels
     * height;     - height in pixels
     * layers;     - number of images
     * format;     - One of AHARDWAREBUFFER_FORMAT_*
     * usage;      - Combination of AHARDWAREBUFFER_USAGE_*
     *
     * Returns a valid AndroidHardwareBufferPtr object on success, or an null AndroidHardwareBufferPtr if
     * the allocation fails for any reason.
     */
    virtual vk::pt::AndroidHardwareBufferPtr allocate(uint32_t width, uint32_t height, uint32_t layers, uint32_t format,
                                                      uint64_t usage) = 0;

    /**
     * Acquire a reference on the given AHardwareBuffer object.  This prevents the
     * object from being deleted until the last reference is removed.
     */
    virtual void acquire(vk::pt::AndroidHardwareBufferPtr buffer) = 0;

    /**
     * Remove a reference that was previously acquired with
     * AHardwareBuffer_acquire().
     */
    virtual void release(vk::pt::AndroidHardwareBufferPtr buffer) = 0;

    /**
     * Return a description of the AHardwareBuffer in the passed in the following fields, if not NULL:
     * width;      - width in pixels
     * height;     - height in pixels
     * layers;     - number of images
     * format;     - One of AHARDWAREBUFFER_FORMAT_*
     * usage;      - Combination of AHARDWAREBUFFER_USAGE_*
     *
     */
    virtual void describe(const vk::pt::AndroidHardwareBufferPtr buffer, uint32_t *width, uint32_t *height,
                          uint32_t *layers, uint32_t *format, uint64_t *usage, uint32_t *stride) = 0;

    virtual uint64_t vkUsageToAhbUsage(vk::VkImageUsageFlagBits vkFlag)   = 0;
    virtual uint64_t vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlag) = 0;
    virtual uint32_t vkFormatToAhbFormat(vk::VkFormat vkFormat)           = 0;
    virtual uint64_t mustSupportAhbUsageFlags()                           = 0;
    virtual bool ahbFormatIsBlob(uint32_t format)                         = 0;

    virtual ~AndroidHardwareBufferExternalApi();

protected:
    // Protected Constructor
    AndroidHardwareBufferExternalApi();

private:
    // Stop the compiler generating methods of copy the object
    AndroidHardwareBufferExternalApi(AndroidHardwareBufferExternalApi const &copy);            // Not Implemented
    AndroidHardwareBufferExternalApi &operator=(AndroidHardwareBufferExternalApi const &copy); // Not Implemented

    static bool loadAhbDynamicApis(int32_t sdkVersion);
};

const char *externalSemaphoreTypeToName(vk::VkExternalSemaphoreHandleTypeFlagBits type);
const char *externalFenceTypeToName(vk::VkExternalFenceHandleTypeFlagBits type);
const char *externalMemoryTypeToName(vk::VkExternalMemoryHandleTypeFlagBits type);

enum Permanence
{
    PERMANENCE_PERMANENT = 0,
    PERMANENCE_TEMPORARY
};

enum Transference
{
    TRANSFERENCE_COPY = 0,
    TRANSFERENCE_REFERENCE
};

struct ExternalHostMemory
{
    ExternalHostMemory(vk::VkDeviceSize aSize, vk::VkDeviceSize aAlignment)
        : size(deAlignSize(static_cast<size_t>(aSize), static_cast<size_t>(aAlignment)))
    {
        data = deAlignedMalloc(this->size, static_cast<size_t>(aAlignment));
    }

    ~ExternalHostMemory()
    {
        if (data != DE_NULL)
        {
            deAlignedFree(data);
        }
    }

    size_t size;
    void *data;
};

bool isSupportedPermanence(vk::VkExternalSemaphoreHandleTypeFlagBits type, Permanence permanence);
Transference getHandelTypeTransferences(vk::VkExternalSemaphoreHandleTypeFlagBits type);

bool isSupportedPermanence(vk::VkExternalFenceHandleTypeFlagBits type, Permanence permanence);
Transference getHandelTypeTransferences(vk::VkExternalFenceHandleTypeFlagBits type);

int getMemoryFd(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceMemory memory,
                vk::VkExternalMemoryHandleTypeFlagBits externalType);

void getMemoryNative(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceMemory memory,
                     vk::VkExternalMemoryHandleTypeFlagBits externalType, NativeHandle &nativeHandle);

vk::Move<vk::VkSemaphore> createExportableSemaphore(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                    vk::VkExternalSemaphoreHandleTypeFlagBits externalType);

vk::Move<vk::VkSemaphore> createExportableSemaphoreType(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                        vk::VkSemaphoreType semaphoreType,
                                                        vk::VkExternalSemaphoreHandleTypeFlagBits externalType);

int getSemaphoreFd(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkSemaphore semaphore,
                   vk::VkExternalSemaphoreHandleTypeFlagBits externalType);

void getSemaphoreNative(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkSemaphore semaphore,
                        vk::VkExternalSemaphoreHandleTypeFlagBits externalType, NativeHandle &nativeHandle);

void importSemaphore(const vk::DeviceInterface &vkd, const vk::VkDevice device, const vk::VkSemaphore semaphore,
                     vk::VkExternalSemaphoreHandleTypeFlagBits externalType, NativeHandle &handle,
                     vk::VkSemaphoreImportFlags flags);

vk::Move<vk::VkSemaphore> createAndImportSemaphore(const vk::DeviceInterface &vkd, const vk::VkDevice device,
                                                   vk::VkExternalSemaphoreHandleTypeFlagBits externalType,
                                                   NativeHandle &handle, vk::VkSemaphoreImportFlags flags);

vk::Move<vk::VkFence> createExportableFence(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                            vk::VkExternalFenceHandleTypeFlagBits externalType);

int getFenceFd(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkFence fence,
               vk::VkExternalFenceHandleTypeFlagBits externalType);

void getFenceNative(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkFence fence,
                    vk::VkExternalFenceHandleTypeFlagBits externalType, NativeHandle &nativeHandle,
                    bool expectFenceUnsignaled = true);

void importFence(const vk::DeviceInterface &vkd, const vk::VkDevice device, const vk::VkFence fence,
                 vk::VkExternalFenceHandleTypeFlagBits externalType, NativeHandle &handle,
                 vk::VkFenceImportFlags flags);

vk::Move<vk::VkFence> createAndImportFence(const vk::DeviceInterface &vkd, const vk::VkDevice device,
                                           vk::VkExternalFenceHandleTypeFlagBits externalType, NativeHandle &handle,
                                           vk::VkFenceImportFlags flags);

uint32_t chooseMemoryType(uint32_t bits);

uint32_t chooseHostVisibleMemoryType(uint32_t bits, const vk::VkPhysicalDeviceMemoryProperties properties);

// If buffer is not null use dedicated allocation
vk::Move<vk::VkDeviceMemory> allocateExportableMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                      vk::VkDeviceSize allocationSize, uint32_t memoryTypeIndex,
                                                      vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                      vk::VkBuffer buffer);

// If image is not null use dedicated allocation
vk::Move<vk::VkDeviceMemory> allocateExportableMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                      vk::VkDeviceSize allocationSize, uint32_t memoryTypeIndex,
                                                      vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                      vk::VkImage image);

/*
// \note hostVisible argument is strict. Setting it to false will cause NotSupportedError to be thrown if non-host visible memory doesn't exist.
// If buffer is not null use dedicated allocation
vk::Move<vk::VkDeviceMemory>    allocateExportableMemory            (const vk::InstanceInterface&                vki,
                                                                     vk::VkPhysicalDevice                        physicalDevice,
                                                                     const vk::DeviceInterface&                    vkd,
                                                                     vk::VkDevice                                device,
                                                                     const vk::VkMemoryRequirements&            requirements,
                                                                     vk::VkExternalMemoryHandleTypeFlagBits        externalType,
                                                                     bool                                        hostVisible,
                                                                     vk::VkBuffer                                buffer,
                                                                     uint32_t&                                    exportedMemoryTypeIndex);
*/

vk::Move<vk::VkDeviceMemory> importMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                          const vk::VkMemoryRequirements &requirements,
                                          vk::VkExternalMemoryHandleTypeFlagBits externalType, uint32_t memoryTypeIndex,
                                          NativeHandle &handle);

vk::Move<vk::VkDeviceMemory> importDedicatedMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                   vk::VkBuffer buffer, const vk::VkMemoryRequirements &requirements,
                                                   vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                   uint32_t memoryTypeIndex, NativeHandle &handle);

vk::Move<vk::VkDeviceMemory> importDedicatedMemory(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                   vk::VkImage image, const vk::VkMemoryRequirements &requirements,
                                                   vk::VkExternalMemoryHandleTypeFlagBits externalType,
                                                   uint32_t memoryTypeIndex, NativeHandle &handle);

vk::Move<vk::VkBuffer> createExternalBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                            uint32_t queueFamilyIndex,
                                            vk::VkExternalMemoryHandleTypeFlagBits externalType, vk::VkDeviceSize size,
                                            vk::VkBufferCreateFlags createFlags, vk::VkBufferUsageFlags usageFlags);

vk::Move<vk::VkImage> createExternalImage(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                          uint32_t queueFamilyIndex,
                                          vk::VkExternalMemoryHandleTypeFlagBits externalType, vk::VkFormat format,
                                          uint32_t width, uint32_t height, vk::VkImageTiling tiling,
                                          vk::VkImageCreateFlags createFlags, vk::VkImageUsageFlags usageFlags,
                                          uint32_t mipLevels = 1u, uint32_t arrayLayers = 1u);

vk::VkPhysicalDeviceExternalMemoryHostPropertiesEXT getPhysicalDeviceExternalMemoryHostProperties(
    const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice);

} // namespace ExternalMemoryUtil
} // namespace vkt

#endif // _VKTEXTERNALMEMORYUTIL_HPP
