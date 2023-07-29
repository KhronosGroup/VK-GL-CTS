/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \file  vktGlobalPriorityQueueUtils.cpp
 * \brief Global Priority Queue Utils
 *//*--------------------------------------------------------------------*/

#include "vktGlobalPriorityQueueUtils.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "deStringUtil.hpp"
#include <vector>

using namespace vk;

namespace vkt
{
namespace synchronization
{

uint32_t findQueueFamilyIndex(const InstanceInterface &vki, VkPhysicalDevice dev, VkQueueFlags includeFlags,
                              VkQueueFlags excludeFlags, bool priorityQueryEnabled, QueueGlobalPriorities priorities,
                              const bool eitherAnyOrAll)
{
    uint32_t queueFamilyPropertyCount = 0;
    vki.getPhysicalDeviceQueueFamilyProperties2(dev, &queueFamilyPropertyCount, nullptr);

    std::vector<VkQueueFamilyGlobalPriorityPropertiesKHR> familyPriorityProperties(
        priorityQueryEnabled ? queueFamilyPropertyCount : 0);
    std::vector<VkQueueFamilyProperties2> familyProperties2(queueFamilyPropertyCount);

    for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i)
    {
        VkQueueFamilyGlobalPriorityPropertiesKHR *item = nullptr;
        if (priorityQueryEnabled)
        {
            item        = &familyPriorityProperties[i];
            *item       = {};
            item->sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_KHR;
        }

        VkQueueFamilyProperties2 &item2 = familyProperties2[i];
        item2                           = {};
        item2.sType                     = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        item2.pNext                     = item;
    }

    vki.getPhysicalDeviceQueueFamilyProperties2(dev, &queueFamilyPropertyCount, familyProperties2.data());

    for (uint32_t familyIndex = 0; familyIndex < queueFamilyPropertyCount; ++familyIndex)
    {
        const VkQueueFamilyProperties &props = familyProperties2[familyIndex].queueFamilyProperties;
        const VkQueueFlags queueFlags        = props.queueFlags;

        if (((queueFlags & excludeFlags) == 0) && ((queueFlags & includeFlags) == includeFlags))
        {
            bool matches = true;
            if (priorityQueryEnabled)
            {
                const QueueGlobalPriorities prios(familyPriorityProperties[familyIndex]);
                if (eitherAnyOrAll)
                    matches = priorities.any(prios);
                else
                    matches = priorities.all(prios);
            }
            if (matches)
            {
                return familyIndex;
            }
        }
    }

    return INVALID_UINT32;
}

QueueGlobalPriorities::QueueGlobalPriorities() : m_priorities{}
{
}
QueueGlobalPriorities::QueueGlobalPriorities(const QueueGlobalPriorities &other) : m_priorities(other.m_priorities)
{
}
QueueGlobalPriorities::QueueGlobalPriorities(const VkQueueFamilyGlobalPriorityPropertiesKHR &source) : m_priorities()
{
    for (uint32_t i = 0; i < source.priorityCount; ++i)
        m_priorities.insert(source.priorities[i]);
}
QueueGlobalPriorities::QueueGlobalPriorities(std::initializer_list<Priority> priorities)
{
    for (auto i = priorities.begin(); i != priorities.end(); ++i)
        m_priorities.insert(*i);
}
QueueGlobalPriorities QueueGlobalPriorities::full()
{
    return QueueGlobalPriorities({VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR, VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR,
                                  VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR, VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR});
}
bool QueueGlobalPriorities::insert(const Priority &prio)
{
    auto item = m_priorities.find(prio);
    if (m_priorities.end() == item)
    {
        m_priorities.insert(prio);
        return true;
    }
    return false;
}
bool QueueGlobalPriorities::remove(const Priority &prio)
{
    auto item = m_priorities.find(prio);
    if (m_priorities.end() != item)
    {
        m_priorities.erase(item);
        return true;
    }
    return false;
}
bool QueueGlobalPriorities::any(const Priority &prio) const
{
    return m_priorities.find(prio) != m_priorities.end();
}
auto QueueGlobalPriorities::make(void *pNext) const -> VkQueueFamilyGlobalPriorityPropertiesKHR
{
    auto start = m_priorities.begin();
    VkQueueFamilyGlobalPriorityPropertiesKHR res{};
    res.sType         = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_KHR;
    res.pNext         = pNext;
    res.priorityCount = static_cast<uint32_t>(m_priorities.size());
    for (auto i = start; i != m_priorities.end(); ++i)
        res.priorities[std::distance(start, i)] = *i;
    return res;
}
bool QueueGlobalPriorities::any(const QueueGlobalPriorities &other) const
{
    for (auto i = other.m_priorities.begin(); i != other.m_priorities.end(); ++i)
        if (any(*i))
            return true;
    return false;
}
bool QueueGlobalPriorities::all(const QueueGlobalPriorities &other) const
{
    return m_priorities == other.m_priorities;
}

SpecialDevice::SpecialDevice(Context &ctx, VkQueueFlagBits transitionFrom, VkQueueFlagBits transitionTo,
                             VkQueueGlobalPriorityKHR priorityFrom, VkQueueGlobalPriorityKHR priorityTo,
                             bool enableProtected, bool enableSparseBinding)
    : queueFamilyIndexFrom(m_queueFamilyIndexFrom)
    , queueFamilyIndexTo(m_queueFamilyIndexTo)
    , device(m_device)
    , queueFrom(m_queueFrom)
    , queueTo(m_queueTo)
    , m_vkd(ctx.getDeviceInterface())
    , m_transitionFrom(transitionFrom)
    , m_transitionTo(transitionTo)
    , m_queueFamilyIndexFrom(INVALID_UINT32)
    , m_queueFamilyIndexTo(INVALID_UINT32)
    , m_device(0)
    , m_queueFrom(0)
    , m_queueTo(0)
    , m_allocator()
    , m_creationResult(VK_RESULT_MAX_ENUM)
{
    const InstanceInterface &vki                            = ctx.getInstanceInterface();
    const DeviceInterface &vkd                              = ctx.getDeviceInterface();
    const VkPhysicalDevice dev                              = ctx.getPhysicalDevice();
    const VkPhysicalDeviceMemoryProperties memoryProperties = getPhysicalDeviceMemoryProperties(vki, dev);

    VkQueueFlags flagFrom = transitionFrom;
    VkQueueFlags flagTo   = transitionTo;
    if (enableProtected)
    {
        flagFrom |= VK_QUEUE_PROTECTED_BIT;
        flagTo |= VK_QUEUE_PROTECTED_BIT;
    }
    if (enableSparseBinding)
    {
        flagFrom |= VK_QUEUE_SPARSE_BINDING_BIT;
        flagTo |= VK_QUEUE_SPARSE_BINDING_BIT;
    }

    m_queueFamilyIndexFrom =
        findQueueFamilyIndex(vki, dev, flagFrom, getColissionFlags(transitionFrom), true, {priorityFrom});
    m_queueFamilyIndexTo = findQueueFamilyIndex(vki, dev, flagTo, getColissionFlags(transitionTo), true, {priorityTo});

    DE_ASSERT(m_queueFamilyIndexFrom != INVALID_UINT32);
    DE_ASSERT(m_queueFamilyIndexTo != INVALID_UINT32);

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[2];
    VkDeviceQueueGlobalPriorityCreateInfoKHR priorityCreateInfos[2];
    {
        priorityCreateInfos[0].sType = priorityCreateInfos[1].sType =
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR;
        priorityCreateInfos[0].pNext = priorityCreateInfos[1].pNext = nullptr;

        queueCreateInfos[0].sType = queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[0].flags                             = queueCreateInfos[1].flags =
            enableProtected ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;
        queueCreateInfos[0].queueCount = queueCreateInfos[1].queueCount = 1;

        priorityCreateInfos[0].globalPriority = priorityFrom;
        queueCreateInfos[0].pNext             = &priorityCreateInfos[0];
        queueCreateInfos[0].queueFamilyIndex  = m_queueFamilyIndexFrom;

        priorityCreateInfos[1].globalPriority = priorityTo;
        queueCreateInfos[1].pNext             = &priorityCreateInfos[1];
        queueCreateInfos[1].queueFamilyIndex  = m_queueFamilyIndexTo;

        queueCreateInfos[0].pQueuePriorities = &queuePriority;
        queueCreateInfos[1].pQueuePriorities = &queuePriority;
    }

    const VkPhysicalDeviceFeatures &deviceFeatures = ctx.getDeviceFeatures();

    std::vector<const char *> extensions;
    std::vector<std::string> filteredExtensions;
    {
        const uint32_t majorApi                                = VK_API_VERSION_MAJOR(ctx.getUsedApiVersion());
        const uint32_t minorApi                                = VK_API_VERSION_MINOR(ctx.getUsedApiVersion());
        std::vector<VkExtensionProperties> availableExtensions = enumerateDeviceExtensionProperties(vki, dev, nullptr);
        const bool khrBufferAddress =
            availableExtensions.end() !=
            std::find_if(availableExtensions.begin(), availableExtensions.end(),
                         [](const VkExtensionProperties &p)
                         { return deStringEqual(p.extensionName, "VK_KHR_buffer_device_address"); });
        for (const auto &ext : availableExtensions)
        {
            if (khrBufferAddress && deStringEqual(ext.extensionName, "VK_EXT_buffer_device_address"))
                continue;
            if (VK_API_VERSION_MAJOR(ext.specVersion) <= majorApi && VK_API_VERSION_MINOR(ext.specVersion) <= minorApi)
                filteredExtensions.emplace_back(ext.extensionName);
        }
        extensions.resize(filteredExtensions.size());
        std::transform(filteredExtensions.begin(), filteredExtensions.end(), extensions.begin(),
                       [](const std::string &s) { return s.c_str(); });
    }

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 2;
    deviceCreateInfo.pQueueCreateInfos    = queueCreateInfos;

    deviceCreateInfo.pEnabledFeatures        = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = de::dataOrNull(extensions);
    deviceCreateInfo.ppEnabledLayerNames     = nullptr;
    deviceCreateInfo.enabledLayerCount       = 0;

    m_creationResult = vki.createDevice(dev, &deviceCreateInfo, nullptr, &m_device);

    if (VK_SUCCESS == m_creationResult && VkDevice(0) != m_device)
    {
        m_allocator = de::MovePtr<vk::Allocator>(new SimpleAllocator(vkd, m_device, memoryProperties));

        if (enableProtected)
        {
            VkDeviceQueueInfo2 queueInfo{};
            queueInfo.sType      = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
            queueInfo.flags      = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
            queueInfo.queueIndex = 0;

            queueInfo.queueFamilyIndex = m_queueFamilyIndexFrom;
            m_vkd.getDeviceQueue2(m_device, &queueInfo, &m_queueFrom);

            queueInfo.queueFamilyIndex = m_queueFamilyIndexTo;
            m_vkd.getDeviceQueue2(m_device, &queueInfo, &m_queueTo);
        }
        else
        {
            m_vkd.getDeviceQueue(m_device, m_queueFamilyIndexFrom, 0, &m_queueFrom);
            m_vkd.getDeviceQueue(m_device, m_queueFamilyIndexTo, 0, &m_queueTo);
        }
    }
}
VkQueueFlags SpecialDevice::getColissionFlags(VkQueueFlagBits bits)
{
    switch (bits)
    {
    case VK_QUEUE_TRANSFER_BIT:
        return (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    case VK_QUEUE_COMPUTE_BIT:
        return VK_QUEUE_GRAPHICS_BIT;
    case VK_QUEUE_GRAPHICS_BIT:
        return 0;
    default:
        break;
    }
    DE_ASSERT(false);
    return 0;
}
Allocator &SpecialDevice::getAllocator() const
{
    return *m_allocator;
}
SpecialDevice::SpecialDevice(SpecialDevice &&src)
    : queueFamilyIndexFrom(m_queueFamilyIndexFrom)
    , queueFamilyIndexTo(m_queueFamilyIndexTo)
    , device(m_device)
    , queueFrom(m_queueFrom)
    , queueTo(m_queueTo)
    , m_vkd(src.m_vkd)
    , m_transitionFrom(src.m_transitionFrom)
    , m_transitionTo(src.m_transitionTo)
    , m_queueFamilyIndexFrom(src.m_queueFamilyIndexFrom)
    , m_queueFamilyIndexTo(src.m_queueFamilyIndexTo)
    , m_device(src.m_device)
    , m_queueFrom(src.m_queueFrom)
    , m_queueTo(src.m_queueTo)
    , m_allocator(src.m_allocator.release())
    , m_creationResult(src.m_creationResult)
{
    src.m_queueFamilyIndexFrom = INVALID_UINT32;
    src.m_queueFamilyIndexTo   = INVALID_UINT32;
    src.m_device               = VkDevice(0);
    src.m_queueFrom            = VkQueue(0);
    src.m_queueTo              = VkQueue(0);
}
SpecialDevice::~SpecialDevice()
{
    if (VkDevice(0) != m_device)
    {
        m_vkd.destroyDevice(m_device, nullptr);
        m_device = VkDevice(0);
    }
}
bool SpecialDevice::isValid(VkResult &creationResult) const
{
    creationResult = m_creationResult;
    return (VkDevice(0) != m_device);
}

BufferWithMemory::BufferWithMemory(const vk::InstanceInterface &vki, const DeviceInterface &vkd,
                                   const vk::VkPhysicalDevice phys, const VkDevice device, Allocator &allocator,
                                   const VkBufferCreateInfo &bufferCreateInfo,
                                   const MemoryRequirement memoryRequirement, const VkQueue sparseQueue)
    : m_amISparse((bufferCreateInfo.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0)
    , m_buffer(createBuffer(vkd, device, &bufferCreateInfo))
    , m_requirements(getBufferMemoryRequirements(vkd, device, *m_buffer))
{
    if (m_amISparse)
    {
        DE_ASSERT(sparseQueue != VkQueue(0));
        const VkPhysicalDeviceMemoryProperties memoryProperties = getPhysicalDeviceMemoryProperties(vki, phys);
        const uint32_t memoryTypeIndex =
            selectMatchingMemoryType(memoryProperties, m_requirements.memoryTypeBits, memoryRequirement);
        const VkDeviceSize alignment     = 0;
        const VkDeviceSize lastChunkSize = m_requirements.size % m_requirements.alignment;
        const uint32_t chunkCount =
            static_cast<uint32_t>(m_requirements.size / m_requirements.alignment + (lastChunkSize ? 1 : 0));
        Move<VkFence> fence = createFence(vkd, device);

        std::vector<VkSparseMemoryBind> bindings(chunkCount);

        for (uint32_t i = 0; i < chunkCount; ++i)
        {
            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.pNext           = nullptr;
            allocInfo.allocationSize  = m_requirements.alignment;
            allocInfo.memoryTypeIndex = memoryTypeIndex;

            de::MovePtr<Allocation> allocation = allocator.allocate(allocInfo, alignment /*unreferenced parameter*/);

            VkSparseMemoryBind &binding = bindings[i];
            binding.resourceOffset      = m_requirements.alignment * i;
            binding.size                = m_requirements.alignment;
            binding.memory              = allocation->getMemory();
            binding.memoryOffset        = allocation->getOffset();
            binding.flags               = 0;

            m_allocations.emplace_back(allocation.release());
        }

        VkSparseBufferMemoryBindInfo bindInfo{};
        bindInfo.buffer    = *m_buffer;
        bindInfo.bindCount = chunkCount;
        bindInfo.pBinds    = de::dataOrNull(bindings);

        VkBindSparseInfo sparseInfo{};
        sparseInfo.sType                = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        sparseInfo.pNext                = nullptr;
        sparseInfo.waitSemaphoreCount   = 0;
        sparseInfo.pWaitSemaphores      = nullptr;
        sparseInfo.bufferBindCount      = 1;
        sparseInfo.pBufferBinds         = &bindInfo;
        sparseInfo.imageOpaqueBindCount = 0;
        sparseInfo.pImageOpaqueBinds    = nullptr;
        sparseInfo.imageBindCount       = 0;
        sparseInfo.pImageBinds          = nullptr;
        sparseInfo.signalSemaphoreCount = 0;
        sparseInfo.pSignalSemaphores    = nullptr;

        VK_CHECK(vkd.queueBindSparse(sparseQueue, 1, &sparseInfo, *fence));
        VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), true, ~0ull));
    }
    else
    {
        de::MovePtr<Allocation> allocation = allocator.allocate(m_requirements, memoryRequirement);
        VK_CHECK(vkd.bindBufferMemory(device, *m_buffer, allocation->getMemory(), allocation->getOffset()));
        m_allocations.emplace_back(allocation.release());
    }
}

void BufferWithMemory::assertIAmSparse() const
{
    if (m_amISparse)
        TCU_THROW(NotSupportedError, "Host access pointer not implemented for sparse buffers");
}

void *BufferWithMemory::getHostPtr(void) const
{
    assertIAmSparse();
    return m_allocations[0]->getHostPtr();
}

void BufferWithMemory::invalidateAlloc(const DeviceInterface &vk, const VkDevice device) const
{
    assertIAmSparse();
    ::vk::invalidateAlloc(vk, device, *m_allocations[0]);
}

void BufferWithMemory::flushAlloc(const DeviceInterface &vk, const VkDevice device) const
{
    assertIAmSparse();
    ::vk::flushAlloc(vk, device, *m_allocations[0]);
}

ImageWithMemory::ImageWithMemory(const InstanceInterface &vki, const DeviceInterface &vkd, const VkPhysicalDevice phys,
                                 const VkDevice device, Allocator &allocator, const VkImageCreateInfo &imageCreateInfo,
                                 const VkQueue sparseQueue, const MemoryRequirement memoryRequirement)
    : m_image(((imageCreateInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) != 0) ?
                  (new image::SparseImage(vkd, device, phys, vki, imageCreateInfo, sparseQueue, allocator,
                                          mapVkFormat(imageCreateInfo.format))) :
                  (new image::Image(vkd, device, allocator, imageCreateInfo, memoryRequirement)))
{
}

} // namespace synchronization
} // namespace vkt
