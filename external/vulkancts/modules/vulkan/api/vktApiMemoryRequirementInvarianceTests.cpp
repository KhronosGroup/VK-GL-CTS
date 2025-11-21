/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2018 The Khronos Group Inc.
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
 *//*--------------------------------------------------------------------*/

#include "vktApiMemoryRequirementInvarianceTests.hpp"
#include "vktApiBufferAndImageAllocationUtil.hpp"
#include "deRandom.h"
#include "tcuTestLog.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkFormatLists.hpp"

namespace vkt
{
namespace api
{

using namespace vk;

// Number of items to allocate
#ifndef CTS_USES_VULKANSC
const unsigned int testCycles = 1000u;
#else
const unsigned int testCycles = 100u;
#endif // CTS_USES_VULKANSC

// All legal memory combinations (spec chapter 10.2: Device Memory)
const unsigned int legalMemoryTypeCount                        = 11u;
const MemoryRequirement legalMemoryTypes[legalMemoryTypeCount] = {
    MemoryRequirement::Any,
    MemoryRequirement::HostVisible | MemoryRequirement::Coherent,
    MemoryRequirement::HostVisible | MemoryRequirement::Cached,
    MemoryRequirement::HostVisible | MemoryRequirement::Cached | MemoryRequirement::Coherent,
    MemoryRequirement::Local,
    MemoryRequirement::Local | MemoryRequirement::HostVisible | MemoryRequirement::Coherent,
    MemoryRequirement::Local | MemoryRequirement::HostVisible | MemoryRequirement::Cached,
    MemoryRequirement::Local | MemoryRequirement::HostVisible | MemoryRequirement::Cached | MemoryRequirement::Coherent,
    MemoryRequirement::Local | MemoryRequirement::LazilyAllocated,
    MemoryRequirement::Protected,
    MemoryRequirement::Protected | MemoryRequirement::Local};

class IObjectAllocator
{
public:
    IObjectAllocator()
    {
    }
    virtual ~IObjectAllocator()
    {
    }
    virtual void allocate(Context &context)   = 0;
    virtual void deallocate(Context &context) = 0;
    virtual size_t getSize(Context &context)  = 0;
};

class BufferAllocator : public IObjectAllocator
{
public:
    BufferAllocator(deRandom &random, bool dedicated, std::vector<int> &memoryTypes);
    virtual ~BufferAllocator();
    virtual void allocate(Context &context);
    virtual void deallocate(Context &context);
    virtual size_t getSize(Context &context);

private:
    bool m_dedicated;
    Move<VkBuffer> m_buffer;
    VkDeviceSize m_size;
    VkBufferUsageFlags m_usage;
    int m_memoryType;
    de::MovePtr<Allocation> m_bufferAlloc;
};

BufferAllocator::BufferAllocator(deRandom &random, bool dedicated, std::vector<int> &memoryTypes)
{
    // If dedicated allocation is supported, randomly pick it
    m_dedicated = dedicated && deRandom_getBool(&random);
    // Random buffer sizes to find potential issues caused by strange alignment
    m_size = (deRandom_getUint32(&random) % 1024) + 7;
    // Pick a random usage from the 9 VkBufferUsageFlags.
    m_usage = 1 << (deRandom_getUint32(&random) % 9);
    // Pick random memory type from the supported ones
    m_memoryType = memoryTypes[deRandom_getUint32(&random) % memoryTypes.size()];
}

BufferAllocator::~BufferAllocator()
{
}

void BufferAllocator::allocate(Context &context)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    VkDevice vkDevice         = context.getDevice();
    Allocator &memAlloc       = context.getDefaultAllocator();
    de::MovePtr<IBufferAllocator> allocator;
    MemoryRequirement requirement = legalMemoryTypes[m_memoryType];

    if (m_dedicated)
        allocator = de::MovePtr<IBufferAllocator>(new BufferDedicatedAllocation);
    else
        allocator = de::MovePtr<IBufferAllocator>(new BufferSuballocation);

    allocator->createTestBuffer(vk, vkDevice, m_size, m_usage, context, memAlloc, m_buffer, requirement, m_bufferAlloc);
}

void BufferAllocator::deallocate(Context &context)
{
    const DeviceInterface &vk  = context.getDeviceInterface();
    const vk::VkDevice &device = context.getDevice();

    vk.destroyBuffer(device, m_buffer.disown(), nullptr);
    m_bufferAlloc.clear();
}

size_t BufferAllocator::getSize(Context &context)
{
    const DeviceInterface &vk  = context.getDeviceInterface();
    const vk::VkDevice &device = context.getDevice();
    VkMemoryRequirements memReq;

    vk.getBufferMemoryRequirements(device, *m_buffer, &memReq);

    return (size_t)memReq.size;
}

class ImageAllocator : public IObjectAllocator
{
public:
    ImageAllocator(deRandom &random, bool dedicated, std::vector<int> &linearformats, std::vector<int> &optimalformats,
                   std::vector<int> &memoryTypes);
    virtual ~ImageAllocator();
    virtual void allocate(Context &context);
    virtual void deallocate(Context &context);
    virtual size_t getSize(Context &context);

private:
    bool m_dedicated;
    bool m_linear;
    Move<vk::VkImage> m_image;
    tcu::IVec2 m_size;
    vk::VkFormat m_colorFormat;
    de::MovePtr<Allocation> m_imageAlloc;
    int m_memoryType;
};

ImageAllocator::ImageAllocator(deRandom &random, bool dedicated, std::vector<int> &linearformats,
                               std::vector<int> &optimalformats, std::vector<int> &memoryTypes)
{
    // If dedicated allocation is supported, pick it randomly
    m_dedicated = dedicated && deRandom_getBool(&random);
    // If linear formats are supported, pick it randomly
    m_linear = (linearformats.size() > 0) && deRandom_getBool(&random);

    if (m_linear)
        m_colorFormat = (VkFormat)linearformats[deRandom_getUint32(&random) % linearformats.size()];
    else
        m_colorFormat = (VkFormat)optimalformats[deRandom_getUint32(&random) % optimalformats.size()];

    int widthAlignment  = (isYCbCr420Format(m_colorFormat) || isYCbCr422Format(m_colorFormat)) ? 2 : 1;
    int heightAlignment = isYCbCr420Format(m_colorFormat) ? 2 : 1;

    // Random small size for causing potential alignment issues
    m_size = tcu::IVec2((deRandom_getUint32(&random) % 16 + 3) & ~(widthAlignment - 1),
                        (deRandom_getUint32(&random) % 16 + 3) & ~(heightAlignment - 1));
    // Pick random memory type from the supported set
    m_memoryType = memoryTypes[deRandom_getUint32(&random) % memoryTypes.size()];
}

ImageAllocator::~ImageAllocator()
{
}

void ImageAllocator::allocate(Context &context)
{
    Allocator &memAlloc = context.getDefaultAllocator();
    de::MovePtr<IImageAllocator> allocator;
    MemoryRequirement requirement = legalMemoryTypes[m_memoryType];

    if (m_dedicated)
        allocator = de::MovePtr<IImageAllocator>(new ImageDedicatedAllocation);
    else
        allocator = de::MovePtr<IImageAllocator>(new ImageSuballocation);

    allocator->createTestImage(m_size, m_colorFormat, context, memAlloc, m_image, requirement, m_imageAlloc,
                               m_linear ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL);
}

void ImageAllocator::deallocate(Context &context)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice &device    = context.getDevice();

    vk.destroyImage(device, m_image.disown(), nullptr);
    m_imageAlloc.clear();
}

size_t ImageAllocator::getSize(Context &context)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice &device    = context.getDevice();
    VkMemoryRequirements memReq;

    vk.getImageMemoryRequirements(device, *m_image, &memReq);

    return (size_t)memReq.size;
}

class InvarianceInstance : public vkt::TestInstance
{
public:
    InvarianceInstance(Context &context, const uint32_t seed);
    virtual ~InvarianceInstance(void);
    virtual tcu::TestStatus iterate(void);

private:
    deRandom m_random;
};

InvarianceInstance::InvarianceInstance(Context &context, const uint32_t seed) : vkt::TestInstance(context)
{
    deRandom_init(&m_random, seed);
}

InvarianceInstance::~InvarianceInstance(void)
{
}

tcu::TestStatus InvarianceInstance::iterate(void)
{
    de::MovePtr<IObjectAllocator> objs[testCycles];
    size_t refSizes[testCycles];
    unsigned int order[testCycles];
    bool supported[testCycles];
    bool allUnsupported                       = true;
    bool success                              = true;
    const bool isDedicatedAllocationSupported = m_context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation");
    const bool isYcbcrSupported          = m_context.isDeviceFunctionalitySupported("VK_KHR_sampler_ycbcr_conversion");
    const bool isYcbcrExtensionSupported = m_context.isDeviceFunctionalitySupported("VK_EXT_ycbcr_2plane_444_formats");
    const bool isPvrtcSupported          = m_context.isDeviceFunctionalitySupported("VK_IMG_format_pvrtc");
#ifndef CTS_USES_VULKANSC
    const bool isMaintenance5Supported = m_context.isDeviceFunctionalitySupported("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC
    std::vector<int> optimalFormats;
    std::vector<int> linearFormats;
    std::vector<int> memoryTypes;
    vk::VkPhysicalDeviceMemoryProperties memProperties;

    // Find supported image formats
    for (auto format : formats::allFormats)
    {
        if (isYCbCrFormat((VkFormat)format) && !isYcbcrSupported)
            continue;

        if (isYCbCrExtensionFormat((VkFormat)format) && !isYcbcrExtensionSupported)
            continue;

        if (isPvrtcFormat((VkFormat)format) && !isPvrtcSupported)
            continue;

#ifndef CTS_USES_VULKANSC
        if (!isMaintenance5Supported)
        {
            if (format == VK_FORMAT_A8_UNORM_KHR || format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
                continue;
        }
#endif // CTS_USES_VULKANSC

        vk::VkImageFormatProperties imageformatprops;

        // Check for support in linear tiling mode
        if (m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                m_context.getPhysicalDevice(), format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &imageformatprops) == VK_SUCCESS)
            linearFormats.push_back(format);

        // Check for support in optimal tiling mode
        if (m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                m_context.getPhysicalDevice(), format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                &imageformatprops) == VK_SUCCESS)
            optimalFormats.push_back(format);
    }

    // Check for supported heap types
    m_context.getInstanceInterface().getPhysicalDeviceMemoryProperties(m_context.getPhysicalDevice(), &memProperties);

    for (unsigned int j = 0; j < legalMemoryTypeCount; j++)
    {
        bool found = false;
        for (unsigned int i = 0; !found && i < memProperties.memoryTypeCount; i++)
        {
            if (legalMemoryTypes[j].matchesHeap(memProperties.memoryTypes[i].propertyFlags))
            {
                memoryTypes.push_back(j);
                found = true;
            }
        }
    }

    // Log the used image types and heap types
    tcu::TestLog &log = m_context.getTestContext().getLog();

    {
        std::ostringstream values;
        for (unsigned int i = 0; i < linearFormats.size(); i++)
            values << " " << linearFormats[i];
        log << tcu::TestLog::Message << "Using linear formats:" << values.str() << tcu::TestLog::EndMessage;
    }

    {
        std::ostringstream values;
        for (unsigned int i = 0; i < optimalFormats.size(); i++)
            values << " " << optimalFormats[i];
        log << tcu::TestLog::Message << "Using optimal formats:" << values.str() << tcu::TestLog::EndMessage;
    }

    {
        std::ostringstream values;
        for (unsigned int i = 0; i < memoryTypes.size(); i++)
            values << " " << memoryTypes[i];
        log << tcu::TestLog::Message << "Using memory types:" << values.str() << tcu::TestLog::EndMessage;
    }

    for (unsigned int i = 0; i < testCycles; i++)
    {
        if (deRandom_getBool(&m_random))
            objs[i] = de::MovePtr<IObjectAllocator>(
                new BufferAllocator(m_random, isDedicatedAllocationSupported, memoryTypes));
        else
            objs[i] = de::MovePtr<IObjectAllocator>(new ImageAllocator(m_random, isDedicatedAllocationSupported,
                                                                       linearFormats, optimalFormats, memoryTypes));
        order[i] = i;
    }

    // First get reference values for the object sizes
    for (unsigned int i = 0; i < testCycles; i++)
    {
        try
        {
            objs[i]->allocate(m_context);
            refSizes[i] = objs[i]->getSize(m_context);
            objs[i]->deallocate(m_context);
            supported[i]   = true;
            allUnsupported = false;
        }
        catch (const tcu::NotSupportedError &)
        {
            supported[i] = false;
        }
    }

    if (allUnsupported)
        TCU_THROW(NotSupportedError, "All allocations unsupported");

    // Shuffle order by swapping random pairs
    for (unsigned int i = 0; i < testCycles; i++)
    {
        int a = deRandom_getUint32(&m_random) % testCycles;
        int b = deRandom_getUint32(&m_random) % testCycles;
        std::swap(order[a], order[b]);
    }

    // Allocate objects in shuffled order
    for (unsigned int i = 0; i < testCycles; i++)
    {
        if (supported[order[i]])
            objs[order[i]]->allocate(m_context);
    }

    // Check for size mismatches
    for (unsigned int i = 0; i < testCycles; i++)
    {
        if (!supported[order[i]])
            continue;

        size_t val = objs[order[i]]->getSize(m_context);

        if (val != refSizes[order[i]])
        {
            success = false;
            log << tcu::TestLog::Message << "Object " << order[i] << " size mismatch (" << val
                << " != " << refSizes[order[i]] << ")" << tcu::TestLog::EndMessage;
        }
    }

    // Clean up
    for (unsigned int i = 0; i < testCycles; i++)
    {
        if (supported[order[i]])
            objs[order[i]]->deallocate(m_context);
    }

    if (success)
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus::fail("One or more allocation is not invariant");
}

class AlignmentMatchingInstance : public vkt::TestInstance
{
public:
    AlignmentMatchingInstance(Context &context);
    virtual ~AlignmentMatchingInstance(void) = default;
    virtual tcu::TestStatus iterate(void);
};

AlignmentMatchingInstance::AlignmentMatchingInstance(Context &context) : vkt::TestInstance(context)
{
}

tcu::TestStatus AlignmentMatchingInstance::iterate(void)
{
    const VkDevice device       = m_context.getDevice();
    const DeviceInterface &vk   = m_context.getDeviceInterface();
    const uint32_t objectsCount = 5;
    tcu::TestLog &log           = m_context.getTestContext().getLog();
    bool success                = true;
    VkExtent3D baseExtent       = {32, 31, 1};
    VkDeviceSize baseSize       = 1023;

    VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
        baseExtent,                          // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arraySize;
        VK_SAMPLE_COUNT_1_BIT,               // uint32_t samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT,     // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    VkBufferCreateInfo bufferCreateInfo{
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType        sType
        nullptr,                              // const void*            pNext
        0u,                                   // VkBufferCreateFlags    flags
        baseSize,                             // VkDeviceSize            size
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags    usage
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode
        0u,                                   // uint32_t                queueFamilyIndexCount
        nullptr                               // const uint32_t*        pQueueFamilyIndices
    };

    Move<VkImage> baseImage   = createImage(vk, device, &imageCreateInfo);
    Move<VkBuffer> baseBuffer = createBuffer(vk, device, &bufferCreateInfo);

    VkMemoryRequirements baseImageRequirements  = getImageMemoryRequirements(vk, device, *baseImage);
    VkMemoryRequirements baseBufferRequirements = getBufferMemoryRequirements(vk, device, *baseBuffer);

    // Create a bunch of VkBuffer and VkImage objects with the same
    // create infos and make sure their alignments all match.
    {
        std::vector<Move<VkImage>> images(objectsCount);
        std::vector<Move<VkBuffer>> buffers(objectsCount);

        for (uint32_t idx = 0; idx < objectsCount; ++idx)
        {
            images[idx]  = createImage(vk, device, &imageCreateInfo);
            buffers[idx] = createBuffer(vk, device, &bufferCreateInfo);

            VkMemoryRequirements imageRequirements   = getImageMemoryRequirements(vk, device, *images[idx]);
            VkMemoryRequirements buffersRequirements = getBufferMemoryRequirements(vk, device, *buffers[idx]);

            if (baseImageRequirements.alignment != imageRequirements.alignment)
            {
                success = false;
                log << tcu::TestLog::Message
                    << "Alignments for all VkImage objects created with the same create infos should match\n"
                    << tcu::TestLog::EndMessage;
            }
            if (baseBufferRequirements.alignment != buffersRequirements.alignment)
            {
                success = false;
                log << tcu::TestLog::Message
                    << "Alignments for all VkBuffer objects created with the same create infos should match\n"
                    << tcu::TestLog::EndMessage;
            }
        }
    }

    if (m_context.isDeviceFunctionalitySupported("VK_KHR_get_memory_requirements2"))
    {
#ifndef CTS_USES_VULKANSC
        VkBufferMemoryRequirementsInfo2 bufferMemoryRequirementsInfo{
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, // VkStructureType    sType
            nullptr,                                             // const void*        pNext
            *baseBuffer                                          // VkBuffer            buffer
        };
        VkImageMemoryRequirementsInfo2 imageMemoryRequirementsInfo{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, // VkStructureType    sType
            nullptr,                                            // const void*        pNext
            *baseImage                                          // VkImage            image
        };
        std::vector<VkMemoryRequirements2> requirements2(
            4,
            {
                VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, // VkStructureType        sType
                nullptr,                                 // void*                pNext
                {0, 0, 0}                                // VkMemoryRequirements    memoryRequirements
            });

        auto areRequirementsTheSame = [](VkMemoryRequirements2 &a, VkMemoryRequirements2 &b)
        {
            return ((a.memoryRequirements.size == b.memoryRequirements.size) &&
                    (a.memoryRequirements.alignment == b.memoryRequirements.alignment) &&
                    (a.memoryRequirements.memoryTypeBits == b.memoryRequirements.memoryTypeBits));
        };

        // The memory requirements returned by vkGetBufferCreateInfoMemoryRequirementsKHR are identical to those that
        // would be returned by vkGetBufferMemoryRequirements2 if it were called with a VkBuffer created with the same
        // VkBufferCreateInfo values.
        vk.getBufferMemoryRequirements2(device, &bufferMemoryRequirementsInfo, &requirements2[0]);
        const VkDeviceBufferMemoryRequirementsKHR bufferMemInfo = {
            VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS_KHR, nullptr, &bufferCreateInfo};
        vk.getDeviceBufferMemoryRequirements(device, &bufferMemInfo, &requirements2[1]);

        if (!areRequirementsTheSame(requirements2[0], requirements2[1]))
        {
            success = false;
            log << tcu::TestLog::Message
                << "vkGetDeviceBufferMemoryRequirements and vkGetBufferMemoryRequirements2\n"
                   "report diferent memory requirements\n"
                << tcu::TestLog::EndMessage;
        }

        VkMemoryDedicatedRequirements dedicatedRequirements1 = initVulkanStructure();
        requirements2[2].pNext                               = &dedicatedRequirements1;
        dedicatedRequirements1.prefersDedicatedAllocation    = 2;
        dedicatedRequirements1.requiresDedicatedAllocation   = 2;
        vk.getBufferMemoryRequirements2(device, &bufferMemoryRequirementsInfo, &requirements2[2]);

        if (!areRequirementsTheSame(requirements2[0], requirements2[2]))
        {
            success = false;
            log << tcu::TestLog::Message
                << "vkGetBufferMemoryRequirements2 and vkGetBufferMemoryRequirements2 with\n"
                   "VkMemoryDedicatedRequirements report diferent memory requirements\n"
                << tcu::TestLog::EndMessage;
        }

        VkMemoryDedicatedRequirements dedicatedRequirements2 = initVulkanStructure();
        requirements2[3].pNext                               = &dedicatedRequirements2;
        dedicatedRequirements2.prefersDedicatedAllocation    = 3;
        dedicatedRequirements2.requiresDedicatedAllocation   = 3;
        vk.getDeviceBufferMemoryRequirements(device, &bufferMemInfo, &requirements2[3]);

        if (!areRequirementsTheSame(requirements2[0], requirements2[3]))
        {
            success = false;
            log << tcu::TestLog::Message
                << "vkGetBufferMemoryRequirements2 with VkMemoryDedicatedRequirements\n"
                   "and vkGetDeviceBufferMemoryRequirements with VkMemoryDedicatedRequirements\n"
                   "report diferent memory requirements\n"
                << tcu::TestLog::EndMessage;
        }
        if (dedicatedRequirements1.prefersDedicatedAllocation != dedicatedRequirements2.prefersDedicatedAllocation ||
            dedicatedRequirements1.requiresDedicatedAllocation != dedicatedRequirements2.requiresDedicatedAllocation)
        {
            success = false;
            log << tcu::TestLog::Message
                << "VkMemoryDedicatedRequirements with vkGetBufferMemoryRequirements2\n"
                   " doesn't match VkMemoryDedicatedRequirements with vkGetDeviceBufferMemoryRequirements\n"
                << tcu::TestLog::EndMessage;
        }

        // Similarly, vkGetImageCreateInfoMemoryRequirementsKHR will report the same memory requirements as
        // vkGetImageMemoryRequirements2 would if called with a VkImage created with the supplied VkImageCreateInfo
        vk.getImageMemoryRequirements2(device, &imageMemoryRequirementsInfo, &requirements2[0]);
        const VkDeviceImageMemoryRequirementsKHR imageMemInfo = {VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS_KHR,
                                                                 nullptr, &imageCreateInfo,
                                                                 vk::VkImageAspectFlagBits(0)};
        vk.getDeviceImageMemoryRequirements(device, &imageMemInfo, &requirements2[1]);

        if (!areRequirementsTheSame(requirements2[0], requirements2[1]))
        {
            success = false;
            log << tcu::TestLog::Message
                << "vkGetDeviceImageMemoryRequirements and vkGetImageMemoryRequirements2\n"
                   "report diferent memory requirements\n"
                << tcu::TestLog::EndMessage;
        }

        dedicatedRequirements1.prefersDedicatedAllocation  = 2;
        dedicatedRequirements1.requiresDedicatedAllocation = 2;
        vk.getImageMemoryRequirements2(device, &imageMemoryRequirementsInfo, &requirements2[2]);

        if (!areRequirementsTheSame(requirements2[0], requirements2[2]))
        {
            success = false;
            log << tcu::TestLog::Message
                << "vkGetImageMemoryRequirements2 and vkGetImageMemoryRequirements2 with\n"
                   "VkMemoryDedicatedRequirements report diferent memory requirements\n"
                << tcu::TestLog::EndMessage;
        }

        dedicatedRequirements2.prefersDedicatedAllocation  = 3;
        dedicatedRequirements2.requiresDedicatedAllocation = 3;
        vk.getDeviceImageMemoryRequirements(device, &imageMemInfo, &requirements2[3]);

        if (!areRequirementsTheSame(requirements2[0], requirements2[3]))
        {
            success = false;
            log << tcu::TestLog::Message
                << "vkGetImageMemoryRequirements2 with VkMemoryDedicatedRequirements\n"
                   "and vkGetDeviceImageMemoryRequirements with VkMemoryDedicatedRequirements\n"
                   "report diferent memory requirements\n"
                << tcu::TestLog::EndMessage;
        }
        if (dedicatedRequirements1.prefersDedicatedAllocation != dedicatedRequirements2.prefersDedicatedAllocation ||
            dedicatedRequirements1.requiresDedicatedAllocation != dedicatedRequirements2.requiresDedicatedAllocation)
        {
            success = false;
            log << tcu::TestLog::Message
                << "VkMemoryDedicatedRequirements with vkGetImageMemoryRequirements2\n"
                   " doesn't match VkMemoryDedicatedRequirements with vkGetDeviceImageMemoryRequirements\n"
                << tcu::TestLog::EndMessage;
        }

#endif // CTS_USES_VULKANSC
    }

    // For a VkImage, the size memory requirement is never greater than that of another VkImage created with
    // a greater or equal extent dimension specified in VkImageCreateInfo, all other creation parameters being identical.
    // For a VkBuffer, the size memory requirement is never greater than that of another VkBuffer created with
    // a greater or equal size specified in VkBufferCreateInfo, all other creation parameters being identical.
    {
        std::vector<Move<VkImage>> images(objectsCount);
        std::vector<Move<VkBuffer>> buffers(objectsCount);

        for (uint32_t idx = 0; idx < objectsCount; ++idx)
        {
            imageCreateInfo.extent = {baseExtent.width + (idx % 2) * idx, baseExtent.height + idx, 1u};
            bufferCreateInfo.size  = baseSize + idx;

            images[idx]  = createImage(vk, device, &imageCreateInfo);
            buffers[idx] = createBuffer(vk, device, &bufferCreateInfo);

            VkMemoryRequirements imageRequirements   = getImageMemoryRequirements(vk, device, *images[idx]);
            VkMemoryRequirements buffersRequirements = getBufferMemoryRequirements(vk, device, *buffers[idx]);

            if (baseImageRequirements.size > imageRequirements.size)
            {
                success = false;
                log << tcu::TestLog::Message
                    << "Size memory requiremen for VkImage should never be greater than that of another VkImage\n"
                       "created with a greater or equal extent dimension specified in VkImageCreateInfo when all\n"
                       "other creation parameters are identical\n"
                    << tcu::TestLog::EndMessage;
            }
            if (baseBufferRequirements.size > buffersRequirements.size)
            {
                success = false;
                log << tcu::TestLog::Message
                    << "Size memory requiremen for VkBuffer should never be greater than that of another VkBuffer\n"
                       "created with a greater or size specified in VkImageCreateInfo when all\n"
                       "other creation parameters are identical\n"
                    << tcu::TestLog::EndMessage;
            }
        }
    }

    if (success)
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus::fail("Fail");
}

enum TestType
{
    TT_BASIC_INVARIANCE = 0,
    TT_REQUIREMENTS_MATCHING
};

class InvarianceCase : public vkt::TestCase
{
public:
    InvarianceCase(tcu::TestContext &testCtx, const std::string &name, TestType testType);
    virtual ~InvarianceCase(void) = default;

    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    TestType m_testType;
};

InvarianceCase::InvarianceCase(tcu::TestContext &testCtx, const std::string &name, TestType testType)
    : vkt::TestCase(testCtx, name)
    , m_testType(testType)
{
}

TestInstance *InvarianceCase::createInstance(Context &context) const
{
    if (TT_REQUIREMENTS_MATCHING == m_testType)
        return new AlignmentMatchingInstance(context);

    return new InvarianceInstance(context, 0x600613);
}

void InvarianceCase::checkSupport(Context &context) const
{
    if (TT_REQUIREMENTS_MATCHING == m_testType)
        context.requireDeviceFunctionality("VK_KHR_maintenance4");
}

tcu::TestCaseGroup *createMemoryRequirementInvarianceTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> invarianceTests(new tcu::TestCaseGroup(testCtx, "invariance"));

    invarianceTests->addChild(new InvarianceCase(testCtx, "random", TT_BASIC_INVARIANCE));
    invarianceTests->addChild(new InvarianceCase(testCtx, "memory_requirements_matching", TT_REQUIREMENTS_MATCHING));

    return invarianceTests.release();
}

} // namespace api
} // namespace vkt
