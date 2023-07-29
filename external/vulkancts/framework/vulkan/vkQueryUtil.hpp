#ifndef _VKQUERYUTIL_HPP
#define _VKQUERYUTIL_HPP
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
 * \brief Vulkan query utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuMaybe.hpp"
#include "deMemory.h"

#include <vector>
#include <string>

namespace vk
{

// API version introspection

void getCoreInstanceExtensions(uint32_t apiVersion, std::vector<const char *> &dst);
void getCoreDeviceExtensions(uint32_t apiVersion, std::vector<const char *> &dst);
bool isCoreInstanceExtension(const uint32_t apiVersion, const std::string &extension);
bool isCoreDeviceExtension(const uint32_t apiVersion, const std::string &extension);

// API queries

std::vector<VkPhysicalDevice> enumeratePhysicalDevices(const InstanceInterface &vk, VkInstance instance);
std::vector<VkPhysicalDeviceGroupProperties> enumeratePhysicalDeviceGroups(const InstanceInterface &vk,
                                                                           VkInstance instance);
std::vector<VkQueueFamilyProperties> getPhysicalDeviceQueueFamilyProperties(const InstanceInterface &vk,
                                                                            VkPhysicalDevice physicalDevice);
VkPhysicalDeviceFeatures getPhysicalDeviceFeatures(const InstanceInterface &vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceFeatures2 getPhysicalDeviceFeatures2(const InstanceInterface &vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkan11Features getPhysicalDeviceVulkan11Features(const InstanceInterface &vk,
                                                                   VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkan12Features getPhysicalDeviceVulkan12Features(const InstanceInterface &vk,
                                                                   VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkan11Properties getPhysicalDeviceVulkan11Properties(const InstanceInterface &vk,
                                                                       VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkan12Properties getPhysicalDeviceVulkan12Properties(const InstanceInterface &vk,
                                                                       VkPhysicalDevice physicalDevice);
VkPhysicalDeviceProperties getPhysicalDeviceProperties(const InstanceInterface &vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties(const InstanceInterface &vk,
                                                                   VkPhysicalDevice physicalDevice);
VkFormatProperties getPhysicalDeviceFormatProperties(const InstanceInterface &vk, VkPhysicalDevice physicalDevice,
                                                     VkFormat format);
VkImageFormatProperties getPhysicalDeviceImageFormatProperties(const InstanceInterface &vk,
                                                               VkPhysicalDevice physicalDevice, VkFormat format,
                                                               VkImageType type, VkImageTiling tiling,
                                                               VkImageUsageFlags usage, VkImageCreateFlags flags);

#ifndef CTS_USES_VULKANSC
std::vector<VkSparseImageFormatProperties> getPhysicalDeviceSparseImageFormatProperties(
    const InstanceInterface &vk, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type,
    VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling);
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
VkPhysicalDeviceVulkanSC10Features getPhysicalDeviceVulkanSC10Features(const InstanceInterface &vk,
                                                                       VkPhysicalDevice physicalDevice);
VkPhysicalDeviceVulkanSC10Properties getPhysicalDeviceVulkanSC10Properties(const InstanceInterface &vk,
                                                                           VkPhysicalDevice physicalDevice);
#endif // CTS_USES_VULKANSC
VkMemoryRequirements getBufferMemoryRequirements(const DeviceInterface &vk, VkDevice device, VkBuffer buffer);
VkMemoryRequirements getImageMemoryRequirements(const DeviceInterface &vk, VkDevice device, VkImage image);
VkMemoryRequirements getImagePlaneMemoryRequirements(const DeviceInterface &vk, VkDevice device, VkImage image,
                                                     VkImageAspectFlagBits planeAspect);
#ifndef CTS_USES_VULKANSC
std::vector<VkSparseImageMemoryRequirements> getImageSparseMemoryRequirements(const DeviceInterface &vk,
                                                                              VkDevice device, VkImage image);
#endif // CTS_USES_VULKANSC

std::vector<VkLayerProperties> enumerateInstanceLayerProperties(const PlatformInterface &vkp);
std::vector<VkExtensionProperties> enumerateInstanceExtensionProperties(const PlatformInterface &vkp,
                                                                        const char *layerName);
std::vector<VkLayerProperties> enumerateDeviceLayerProperties(const InstanceInterface &vki,
                                                              VkPhysicalDevice physicalDevice);
std::vector<VkExtensionProperties> enumerateDeviceExtensionProperties(const InstanceInterface &vki,
                                                                      VkPhysicalDevice physicalDevice,
                                                                      const char *layerName);

VkQueue getDeviceQueue(const DeviceInterface &vkd, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex);
VkQueue getDeviceQueue2(const DeviceInterface &vkd, VkDevice device, const VkDeviceQueueInfo2 *queueInfo);

// Feature / extension support

bool isShaderStageSupported(const VkPhysicalDeviceFeatures &deviceFeatures, VkShaderStageFlagBits stage);

struct RequiredExtension
{
    std::string name;
    tcu::Maybe<uint32_t> minVersion;
    tcu::Maybe<uint32_t> maxVersion;

    explicit RequiredExtension(const std::string &name_, tcu::Maybe<uint32_t> minVersion_ = tcu::Nothing,
                               tcu::Maybe<uint32_t> maxVersion_ = tcu::Nothing)
        : name(name_)
        , minVersion(minVersion_)
        , maxVersion(maxVersion_)
    {
    }
};

struct RequiredLayer
{
    std::string name;
    tcu::Maybe<uint32_t> minSpecVersion;
    tcu::Maybe<uint32_t> maxSpecVersion;
    tcu::Maybe<uint32_t> minImplVersion;
    tcu::Maybe<uint32_t> maxImplVersion;

    explicit RequiredLayer(const std::string &name_, tcu::Maybe<uint32_t> minSpecVersion_ = tcu::Nothing,
                           tcu::Maybe<uint32_t> maxSpecVersion_ = tcu::Nothing,
                           tcu::Maybe<uint32_t> minImplVersion_ = tcu::Nothing,
                           tcu::Maybe<uint32_t> maxImplVersion_ = tcu::Nothing)
        : name(name_)
        , minSpecVersion(minSpecVersion_)
        , maxSpecVersion(maxSpecVersion_)
        , minImplVersion(minImplVersion_)
        , maxImplVersion(maxImplVersion_)
    {
    }
};

bool isCompatible(const VkExtensionProperties &extensionProperties, const RequiredExtension &required);
bool isCompatible(const VkLayerProperties &layerProperties, const RequiredLayer &required);

template <typename ExtensionIterator>
bool isExtensionSupported(ExtensionIterator begin, ExtensionIterator end, const RequiredExtension &required);
bool isExtensionSupported(const std::vector<VkExtensionProperties> &extensions, const RequiredExtension &required);

bool isInstanceExtensionSupported(const uint32_t instanceVersion, const std::vector<std::string> &extensions,
                                  const std::string &required);

template <typename LayerIterator>
bool isLayerSupported(LayerIterator begin, LayerIterator end, const RequiredLayer &required);
bool isLayerSupported(const std::vector<VkLayerProperties> &layers, const RequiredLayer &required);

const void *findStructureInChain(const void *first, VkStructureType type);
void *findStructureInChain(void *first, VkStructureType type);

template <typename StructType>
VkStructureType getStructureType(void);

template <typename StructType>
const StructType *findStructure(const void *first)
{
    return reinterpret_cast<const StructType *>(findStructureInChain(first, getStructureType<StructType>()));
}

template <typename StructType>
StructType *findStructure(void *first)
{
    return reinterpret_cast<StructType *>(findStructureInChain(first, getStructureType<StructType>()));
}

struct initVulkanStructure
{
    initVulkanStructure(void *pNext = DE_NULL) : m_next(pNext)
    {
    }

    template <class StructType>
    operator StructType()
    {
        StructType result;

        deMemset(&result, 0x00, sizeof(StructType));

        result.sType = getStructureType<StructType>();
        result.pNext = m_next;

        return result;
    }

private:
    void *m_next;
};

template <class StructType>
void addToChainVulkanStructure(void ***chainPNextPtr, StructType &structType)
{
    DE_ASSERT(chainPNextPtr != DE_NULL);

    (**chainPNextPtr) = &structType;

    (*chainPNextPtr) = &structType.pNext;
}

struct initVulkanStructureConst
{
    initVulkanStructureConst(const void *pNext = DE_NULL) : m_next(pNext)
    {
    }

    template <class StructType>
    operator const StructType()
    {
        StructType result;

        deMemset(&result, 0x00, sizeof(StructType));

        result.sType = getStructureType<StructType>();
        result.pNext = const_cast<void *>(m_next);

        return result;
    }

private:
    const void *m_next;
};

struct getPhysicalDeviceExtensionProperties
{
    getPhysicalDeviceExtensionProperties(const InstanceInterface &vki, VkPhysicalDevice physicalDevice)
        : m_vki(vki)
        , m_physicalDevice(physicalDevice)
    {
    }

    template <class ExtensionProperties>
    operator ExtensionProperties()
    {
        VkPhysicalDeviceProperties2 properties2;
        ExtensionProperties extensionProperties;

        deMemset(&extensionProperties, 0x00, sizeof(ExtensionProperties));
        extensionProperties.sType = getStructureType<ExtensionProperties>();

        deMemset(&properties2, 0x00, sizeof(properties2));
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &extensionProperties;

        m_vki.getPhysicalDeviceProperties2(m_physicalDevice, &properties2);

        return extensionProperties;
    }

    operator VkPhysicalDeviceProperties2()
    {
        VkPhysicalDeviceProperties2 properties2;

        deMemset(&properties2, 0x00, sizeof(properties2));
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        m_vki.getPhysicalDeviceProperties2(m_physicalDevice, &properties2);

        return properties2;
    }

private:
    const InstanceInterface &m_vki;
    const VkPhysicalDevice m_physicalDevice;
};

// Walks through chain to find empty pNext and assigns what to found pNext
void appendStructurePtrToVulkanChain(const void **chainHead, const void *structurePtr);

namespace ValidateQueryBits
{

typedef struct
{
    size_t offset;
    size_t size;
} QueryMemberTableEntry;

template <typename Context, typename Interface, typename Type>
//!< Return variable initialization validation
bool validateInitComplete(Context context, void (Interface::*Function)(Context, Type *) const,
                          const Interface &interface, const QueryMemberTableEntry *queryMemberTableEntry)
{
    const QueryMemberTableEntry *iterator;
    Type vec[2];
    deMemset(&vec[0], 0x00, sizeof(Type));
    deMemset(&vec[1], 0xFF, sizeof(Type));

    (interface.*Function)(context, &vec[0]);
    (interface.*Function)(context, &vec[1]);

    for (iterator = queryMemberTableEntry; iterator->size != 0; iterator++)
    {
        if (deMemCmp(((uint8_t *)(&vec[0])) + iterator->offset, ((uint8_t *)(&vec[1])) + iterator->offset,
                     iterator->size) != 0)
            return false;
    }

    return true;
}

template <typename Type>
//!< Return variable initialization validation
bool validateStructsWithGuard(const QueryMemberTableEntry *queryMemberTableEntry, Type *vec[2],
                              const uint8_t guardValue, const uint32_t guardSize)
{
    const QueryMemberTableEntry *iterator;

    for (iterator = queryMemberTableEntry; iterator->size != 0; iterator++)
    {
        if (deMemCmp(((uint8_t *)(vec[0])) + iterator->offset, ((uint8_t *)(vec[1])) + iterator->offset,
                     iterator->size) != 0)
            return false;
    }

    for (uint32_t vecNdx = 0; vecNdx < 2; ++vecNdx)
    {
        for (uint32_t ndx = 0; ndx < guardSize; ndx++)
        {
            if (((uint8_t *)(vec[vecNdx]))[ndx + sizeof(Type)] != guardValue)
                return false;
        }
    }

    return true;
}

template <typename IterT>
//! Overwrite a range of objects with an 8-bit pattern.
inline void fillBits(IterT beg, const IterT end, const uint8_t pattern = 0xdeu)
{
    for (; beg < end; ++beg)
        deMemset(&(*beg), static_cast<int>(pattern), sizeof(*beg));
}

template <typename IterT>
//! Verify that each byte of a range of objects is equal to an 8-bit pattern.
bool checkBits(IterT beg, const IterT end, const uint8_t pattern = 0xdeu)
{
    for (; beg < end; ++beg)
    {
        const uint8_t *elementBytes = reinterpret_cast<const uint8_t *>(&(*beg));
        for (std::size_t i = 0u; i < sizeof(*beg); ++i)
        {
            if (elementBytes[i] != pattern)
                return false;
        }
    }
    return true;
}

} // namespace ValidateQueryBits

// Template implementations

template <typename ExtensionIterator>
bool isExtensionSupported(ExtensionIterator begin, ExtensionIterator end, const RequiredExtension &required)
{
    for (ExtensionIterator cur = begin; cur != end; ++cur)
    {
        if (isCompatible(*cur, required))
            return true;
    }
    return false;
}

template <typename LayerIterator>
bool isLayerSupported(LayerIterator begin, LayerIterator end, const RequiredLayer &required)
{
    for (LayerIterator cur = begin; cur != end; ++cur)
    {
        if (isCompatible(*cur, required))
            return true;
    }
    return false;
}

} // namespace vk

#endif // _VKQUERYUTIL_HPP
