/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 Google LLC.
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
 * \brief Tests multiple image copies without barriers
 *//*--------------------------------------------------------------------*/

#include "vktImageConcurrentCopyTests.hpp"

#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "deRandom.hpp"
#include "ycbcr/vktYCbCrUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "deThread.hpp"
#include "deSharedPtr.hpp"
#include "vkRef.hpp"

#include <set>
#include <algorithm>

namespace vkt
{
namespace image
{
namespace
{

struct TestParameters
{
    vk::VkFormat format;
    vk::VkImageTiling tiling;
    vk::VkImageType type;
    bool hostCopy;
    bool read;
    bool singleCommand;
    bool randomData;
};

class ConcurrentCopyTestInstance : public vkt::TestInstance
{
public:
    ConcurrentCopyTestInstance(vkt::Context &context, const TestParameters &parameters)
        : vkt::TestInstance(context)
        , m_parameters(parameters)
    {
    }

private:
    tcu::TestStatus iterate(void);

    const TestParameters m_parameters;
};

#ifndef CTS_USES_VULKANSC
class HostCopyThread : public de::Thread
{
public:
    HostCopyThread(const vk::DeviceInterface &vk, const vk::VkDevice device, const vk::VkImage image,
                   const vk::VkImageLayout imageLayout, const vk::VkMemoryToImageCopyEXT &region, const bool read,
                   const uint32_t pixelSize)
        : de::Thread()
        , m_vk(vk)
        , m_device(device)
        , m_image(image)
        , m_imageLayout(imageLayout)
        , m_region(region)
        , m_read(read)
        , m_pixelSize(pixelSize)
        , m_failed(false)
    {
    }
    virtual ~HostCopyThread(void)
    {
    }

    virtual void run()
    {
        vk::VkCopyMemoryToImageInfoEXT copyInfo = {
            vk::VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkHostImageCopyFlagsEXT flags;
            m_image,                                             // VkImage dstImage;
            m_imageLayout,                                       // VkImageLayout dstImageLayout;
            1u,                                                  // uint32_t regionCount;
            &m_region,                                           // const VkMemoryToImageCopyEXT* pRegions;
        };
        m_vk.copyMemoryToImage(m_device, &copyInfo);

        if (m_read)
        {
            std::vector<uint8_t> data(m_region.imageExtent.width * m_region.imageExtent.height *
                                      m_region.imageExtent.depth * m_pixelSize);

            vk::VkImageToMemoryCopyEXT readRegion = {
                vk::VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT, // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                data.data(),                                    // void* pHostPointer;
                0u,                                             // uint32_t memoryRowLength;
                0u,                                             // uint32_t memoryImageHeight;
                m_region.imageSubresource,                      // VkImageSubresourceLayers imageSubresource;
                m_region.imageOffset,                           // VkOffset3D imageOffset;
                m_region.imageExtent,                           // VkExtent3D imageExtent;
            };
            vk::VkCopyImageToMemoryInfoEXT readInfo = {
                vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT, // VkStructureType sType;
                nullptr,                                             // const void* pNext;
                0u,                                                  // VkHostImageCopyFlagsEXT flags;
                m_image,                                             // VkImage srcImage;
                m_imageLayout,                                       // VkImageLayout srcImageLayout;
                1u,                                                  // uint32_t regionCount;
                &readRegion,                                         // const VkImageToMemoryCopyEXT* pRegions;
            };

            m_vk.copyImageToMemory(m_device, &readInfo);

            const uint32_t rowLength = m_region.imageExtent.width * m_pixelSize;
            for (uint32_t k = 0; k < m_region.imageExtent.depth; ++k)
            {
                for (uint32_t j = 0; j < m_region.imageExtent.height; ++j)
                {
                    const uint32_t srcOffset =
                        (m_region.memoryRowLength * j + m_region.memoryRowLength * m_region.memoryImageHeight * k) *
                        m_pixelSize;
                    void *src                = &((uint8_t *)m_region.pHostPointer)[srcOffset];
                    const uint32_t dstOffset = (m_region.imageExtent.width * j +
                                                m_region.imageExtent.width * m_region.imageExtent.height * k) *
                                               m_pixelSize;
                    void *dst = &data[dstOffset];
                    if (memcmp(src, dst, rowLength) != 0)
                    {
                        m_failed = true;
                    }
                }
            }
        }
    }

    bool hasFailed() const
    {
        return m_failed;
    }

private:
    const vk::DeviceInterface &m_vk;
    const vk::VkDevice m_device;
    const vk::VkImage m_image;
    const vk::VkImageLayout m_imageLayout;
    const vk::VkMemoryToImageCopyEXT m_region;
    const bool m_read;
    const uint32_t m_pixelSize;
    bool m_failed;
};
#endif

void splitRegion(de::Random &randomGen, uint32_t size, std::vector<uint32_t> &output)
{
    uint32_t pos = 0u;
    while (pos < size)
    {
        uint32_t current = randomGen.getUint32() % 32u + 1u;
        if (current + pos > size)
        {
            current = size - pos;
        }
        output.push_back(current);
        pos += current;
    }
}

tcu::TestStatus ConcurrentCopyTestInstance::iterate(void)
{
    const vk::DeviceInterface &vk   = m_context.getDeviceInterface();
    const vk::VkDevice device       = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const vk::VkQueue queue         = m_context.getUniversalQueue();
    auto &alloc                     = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const vk::Move<vk::VkCommandPool> cmdPool(
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const vk::Move<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const uint32_t width  = 128u;
    const uint32_t height = 128u;
    const uint32_t depth  = m_parameters.type == vk::VK_IMAGE_TYPE_3D ? 32u : 1u;

    const vk::VkImageLayout imageLayout =
        m_parameters.read ? vk::VK_IMAGE_LAYOUT_GENERAL : vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    const vk::VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    const uint32_t pixelSize  = tcu::getPixelSize(vk::mapVkFormat(m_parameters.format));
    const uint32_t bufferSize = width * height * depth * pixelSize;
    std::vector<uint8_t> testData(bufferSize);
    de::Random randomGen(deInt32Hash((uint32_t)m_parameters.format) ^ deInt32Hash((uint32_t)bufferSize));
    if (m_parameters.randomData)
    {
        ycbcr::fillRandomNoNaN(&randomGen, testData.data(), bufferSize, m_parameters.format);
    }
    else
    {
        for (uint32_t i = 0; i < width; ++i)
        {
            for (uint32_t j = 0; j < height; ++j)
            {
                for (uint32_t k = 0; k < depth; ++k)
                {
                    uint32_t p = i + j * width + k * width * height;
                    uint32_t v = de::max(de::max(i, j), k);
                    if (pixelSize == 1)
                        testData[p] = uint8_t(v % 256);
                    else if (pixelSize == 2)
                        ((uint16_t *)testData.data())[p] = uint16_t(v);
                    else
                    {
                        for (uint32_t l = 0; l < pixelSize / 4; ++l)
                        {
                            ((uint32_t *)testData.data())[p * pixelSize / 4 + l] = v + l;
                        }
                    }
                }
            }
        }
    }

    de::MovePtr<vk::BufferWithMemory> srcBuffer = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        vk::MemoryRequirement::HostVisible));

    de::MovePtr<vk::BufferWithMemory> dstBuffer = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        vk::MemoryRequirement::HostVisible));

    auto &srcBufferAlloc = srcBuffer->getAllocation();
    memcpy(srcBufferAlloc.getHostPtr(), testData.data(), bufferSize);
    flushAlloc(vk, device, srcBufferAlloc);

    vk::VkImageUsageFlags usage = vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
#ifndef CTS_USES_VULKANSC
    if (m_parameters.hostCopy)
        usage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
#endif

    vk::VkImageCreateInfo imageCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType
        nullptr,                                 // const void*              pNext
        0u,                                      // VkImageCreateFlags       flags
        m_parameters.type,                       // VkImageType              imageType
        m_parameters.format,                     // VkFormat                 format
        {width, height, depth},                  // VkExtent3D               extent
        1u,                                      // uint32_t                 mipLevels
        1u,                                      // uint32_t                 arrayLayers
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
        m_parameters.tiling,                     // VkImageTiling            tiling
        usage,                                   // VkImageUsageFlags        usage
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
        0,                                       // uint32_t                 queueFamilyIndexCount
        nullptr,                                 // const uint32_t*          pQueueFamilyIndices
        vk::VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout            initialLayout
    };

    de::MovePtr<vk::ImageWithMemory> image = de::MovePtr<vk::ImageWithMemory>(
        new vk::ImageWithMemory(vk, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any));

    if (m_parameters.hostCopy)
    {
#ifndef CTS_USES_VULKANSC
        vk::VkHostImageLayoutTransitionInfoEXT transition = {
            vk::VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            **image,                                                     // VkImage image;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,                               // VkImageLayout oldLayout;
            imageLayout,                                                 // VkImageLayout newLayout;
            subresourceRange,                                            // VkImageSubresourceRange subresourceRange;
        };
        vk.transitionImageLayout(device, 1u, &transition);
#endif
    }
    else
    {
        m_context.resetCommandPoolForVKSC(device, *cmdPool);
        vk::beginCommandBuffer(vk, *cmdBuffer);
        auto preImageMemoryBarrier =
            makeImageMemoryBarrier(0u, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, imageLayout,
                                   **image, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_NONE_KHR, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1, &preImageMemoryBarrier);
        vk::endCommandBuffer(vk, *cmdBuffer);
        vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    std::vector<uint32_t> widths;
    splitRegion(randomGen, width, widths);
    std::vector<uint32_t> heights;
    splitRegion(randomGen, height, heights);
    std::vector<uint32_t> depths;
    if (m_parameters.type == vk::VK_IMAGE_TYPE_2D)
        depths.push_back(1u);
    else
        splitRegion(randomGen, depth, depths);

    std::vector<vk::VkBufferImageCopy> regions;
    int posWidth = 0u;
    for (const auto w : widths)
    {
        int posHeight = 0u;
        for (const auto h : heights)
        {
            int posDepth = 0u;
            for (const auto d : depths)
            {
                vk::VkBufferImageCopy region;
                region.bufferOffset       = (width * height * posDepth + width * posHeight + posWidth) * pixelSize;
                region.bufferRowLength    = width;
                region.bufferImageHeight  = height;
                region.imageSubresource   = {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
                region.imageOffset        = {posWidth, posHeight, posDepth};
                region.imageExtent.width  = w;
                region.imageExtent.height = h;
                region.imageExtent.depth  = d;
                regions.push_back(region);

                posDepth += d;
            }
            posHeight += h;
        }
        posWidth += w;
    }

    if (m_parameters.hostCopy)
    {
#ifndef CTS_USES_VULKANSC

        std::vector<vk::VkMemoryToImageCopyEXT> memoryToImageCopies;
        for (uint32_t i = 0; i < (uint32_t)regions.size(); ++i)
        {
            const auto &region = regions[i];
            void *hostPointer  = (uint8_t *)srcBufferAlloc.getHostPtr() + region.bufferOffset;

            vk::VkMemoryToImageCopyEXT regionInfo = {
                vk::VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT, // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                hostPointer,                                    // const void* pHostPointer;
                region.bufferRowLength,                         // uint32_t     memoryRowLength;
                region.bufferImageHeight,                       // uint32_t     memoryImageHeight;
                region.imageSubresource,                        // VkImageSubresourceLayers     imageSubresource;
                region.imageOffset,                             // VkOffset3D imageOffset;
                region.imageExtent,                             // VkExtent3D imageExtent;
            };

            memoryToImageCopies.push_back(regionInfo);
        }

        if (m_parameters.singleCommand)
        {
            vk::VkCopyMemoryToImageInfoEXT copyInfo = {
                vk::VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT, // VkStructureType sType;
                nullptr,                                             // const void* pNext;
                0u,                                                  // VkHostImageCopyFlagsEXT flags;
                **image,                                             // VkImage dstImage;
                imageLayout,                                         // VkImageLayout dstImageLayout;
                (uint32_t)memoryToImageCopies.size(),                // uint32_t regionCount;
                memoryToImageCopies.data(),                          // const VkMemoryToImageCopyEXT* pRegions;
            };
            vk.copyMemoryToImage(device, &copyInfo);
        }
        else
        {
            const uint32_t batch_size  = 256;
            const uint32_t num_batches = ((uint32_t)(memoryToImageCopies.size()) / batch_size) + 1;

            for (uint32_t batch = 0; batch < num_batches; ++batch)
            {
                std::vector<de::SharedPtr<HostCopyThread>> threads;
                const uint32_t from = batch * batch_size;
                const uint32_t to   = std::min((batch + 1) * batch_size, (uint32_t)memoryToImageCopies.size());

                for (uint32_t i = from; i < to; ++i)
                {
                    threads.push_back(de::SharedPtr<HostCopyThread>(new HostCopyThread(
                        vk, device, **image, imageLayout, memoryToImageCopies[i], m_parameters.read, pixelSize)));
                }

                for (auto &thread : threads)
                    thread->start();

                for (auto &thread : threads)
                    thread->join();

                for (const auto &thread : threads)
                {
                    if (thread->hasFailed())
                    {
                        return tcu::TestStatus::fail("Fail");
                    }
                }
            }
        }
#endif
    }
    else
    {
        m_context.resetCommandPoolForVKSC(device, *cmdPool);
        vk::beginCommandBuffer(vk, *cmdBuffer);
        if (m_parameters.singleCommand)
        {
            vk.cmdCopyBufferToImage(*cmdBuffer, **srcBuffer, **image, imageLayout, (uint32_t)regions.size(),
                                    regions.data());
        }
        else
        {
            for (const auto &region : regions)
            {
                vk.cmdCopyBufferToImage(*cmdBuffer, **srcBuffer, **image, imageLayout, 1u, &region);
            }
        }
        vk::endCommandBuffer(vk, *cmdBuffer);
        vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    m_context.resetCommandPoolForVKSC(device, *cmdPool);
    vk::beginCommandBuffer(vk, *cmdBuffer);
    auto postImageMemoryBarrier =
        makeImageMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, imageLayout,
                               vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **image, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1, &postImageMemoryBarrier);
    vk::VkBufferImageCopy region;
    region.bufferOffset      = 0u;
    region.bufferRowLength   = 0u;
    region.bufferImageHeight = 0u;
    region.imageSubresource  = {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    region.imageOffset       = {0, 0, 0};
    region.imageExtent       = {width, height, depth};
    vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **dstBuffer, 1u, &region);
    vk::endCommandBuffer(vk, *cmdBuffer);
    vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    auto &dstBufferAlloc = dstBuffer->getAllocation();
    invalidateAlloc(vk, device, dstBufferAlloc);
    if (memcmp(srcBufferAlloc.getHostPtr(), dstBufferAlloc.getHostPtr(), bufferSize) != 0)
    {
        uint8_t *srcPtr = (uint8_t *)srcBufferAlloc.getHostPtr();
        uint8_t *dstPtr = (uint8_t *)dstBufferAlloc.getHostPtr();
        for (uint32_t i = 0; i < bufferSize; ++i)
        {
            if (srcPtr[i] != dstPtr[i])
            {
                log << tcu::TestLog::Message << "Mismatch at byte " << i << ". Src value: " << srcPtr[i]
                    << ", dst value: " << dstPtr[i] << "." << tcu::TestLog::EndMessage;
            }
        }
        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class ConcurrentCopyTestCase : public vkt::TestCase
{
public:
    ConcurrentCopyTestCase(tcu::TestContext &context, const char *name, const TestParameters &parameters)
        : TestCase(context, name)
        , m_parameters(parameters)
    {
    }

private:
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new ConcurrentCopyTestInstance(context, m_parameters);
    }
    void checkSupport(vkt::Context &context) const;

    const TestParameters m_parameters;
};

void ConcurrentCopyTestCase::checkSupport(vkt::Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

#ifndef CTS_USES_VULKANSC
    if (m_parameters.hostCopy)
        context.requireDeviceFunctionality("VK_EXT_host_image_copy");
#endif

    vk::VkImageUsageFlags usage = vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
#ifndef CTS_USES_VULKANSC
    if (m_parameters.hostCopy)
        usage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
#endif

    vk::VkImageFormatProperties imageFormatProperties;
    const auto result = vki.getPhysicalDeviceImageFormatProperties(
        physicalDevice, m_parameters.format, m_parameters.type, m_parameters.tiling, usage, 0, &imageFormatProperties);

    if (result != vk::VK_SUCCESS)
    {
        TCU_THROW(NotSupportedError, "Format unsupported");
    }

#ifndef CTS_USES_VULKANSC
    if (m_parameters.hostCopy)
    {
        const vk::VkImageLayout requiredDstLayout =
            m_parameters.read ? vk::VK_IMAGE_LAYOUT_GENERAL : vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        vk::VkPhysicalDeviceHostImageCopyProperties hostImageCopyProperties = vk::initVulkanStructure();
        vk::VkPhysicalDeviceProperties2 properties2 = vk::initVulkanStructure(&hostImageCopyProperties);
        vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);
        std::vector<vk::VkImageLayout> srcLayouts(hostImageCopyProperties.copySrcLayoutCount);
        std::vector<vk::VkImageLayout> dstLayouts(hostImageCopyProperties.copyDstLayoutCount);
        hostImageCopyProperties.pCopySrcLayouts = srcLayouts.data();
        hostImageCopyProperties.pCopyDstLayouts = dstLayouts.data();
        vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);
        bool hasRequiredLayout = false;
        for (const auto &dstLayout : dstLayouts)
        {
            if (dstLayout == requiredDstLayout)
            {
                hasRequiredLayout = true;
                break;
            }
        }
        if (!hasRequiredLayout)
        {
            TCU_THROW(NotSupportedError, "Required layout not supported in "
                                         "VkPhysicalDeviceHostImageCopyPropertiesEXT::pCopyDstLayouts");
        }
    }
#endif
}

} // namespace

tcu::TestCaseGroup *createImageConcurrentCopyTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "concurrent_copy"));

    const std::set<vk::VkFormat> formats{
        vk::VK_FORMAT_R8G8B8A8_UNORM,
        vk::VK_FORMAT_R8_UNORM,
        vk::VK_FORMAT_R32G32_SFLOAT,
    };

    const std::set<vk::VkImageTiling> tilings{
        vk::VK_IMAGE_TILING_LINEAR,
        vk::VK_IMAGE_TILING_OPTIMAL,
    };

    const std::set<vk::VkImageType> types{
        vk::VK_IMAGE_TYPE_2D,
        vk::VK_IMAGE_TYPE_3D,
    };

    constexpr struct CopyType
    {
        bool hostCopy;
        const char *name;
    } copyTypes[] = {
        {false, "device"},
#ifndef CTS_USES_VULKANSC
        {true, "host"},
#endif
    };

    constexpr struct AccessType
    {
        bool read;
        const char *name;
    } accessTypes[] = {
        {false, "write"},
#ifndef CTS_USES_VULKANSC
        {true, "read_and_write"},
#endif
    };

    constexpr struct CommandType
    {
        bool singleCommand;
        const char *name;
    } commandTypes[] = {
        {true, "single"},
        {false, "multiple"},
    };

    constexpr struct DataType
    {
        bool random;
        const char *name;
    } dataTypes[] = {
        {true, "random"},
        {false, "gradient"},
    };

    for (const auto format : formats)
    {
        de::MovePtr<tcu::TestCaseGroup> formatGroup(
            new tcu::TestCaseGroup(testCtx, de::toLower(vk::getFormatName(format)).c_str()));
        for (const auto tiling : tilings)
        {
            de::MovePtr<tcu::TestCaseGroup> tilingGroup(
                new tcu::TestCaseGroup(testCtx, de::toLower(vk::getImageTilingName(tiling)).c_str()));
            for (const auto type : types)
            {
                de::MovePtr<tcu::TestCaseGroup> typeGroup(
                    new tcu::TestCaseGroup(testCtx, de::toLower(vk::getImageTypeName(type)).c_str()));
                for (const auto commandType : commandTypes)
                {
                    de::MovePtr<tcu::TestCaseGroup> commandTypeGroup(new tcu::TestCaseGroup(testCtx, commandType.name));
                    for (const auto dataType : dataTypes)
                    {
                        de::MovePtr<tcu::TestCaseGroup> dataTypeGroup(new tcu::TestCaseGroup(testCtx, dataType.name));
                        for (const auto copyType : copyTypes)
                        {
                            de::MovePtr<tcu::TestCaseGroup> copyTypeGroup(
                                new tcu::TestCaseGroup(testCtx, copyType.name));
                            for (const auto accessType : accessTypes)
                            {
                                if (accessType.read && !copyType.hostCopy)
                                    continue;

                                TestParameters parameters;
                                parameters.format        = format;
                                parameters.tiling        = tiling;
                                parameters.type          = type;
                                parameters.hostCopy      = copyType.hostCopy;
                                parameters.read          = accessType.read;
                                parameters.singleCommand = commandType.singleCommand;
                                parameters.randomData    = dataType.random;

                                copyTypeGroup->addChild(
                                    new ConcurrentCopyTestCase(testCtx, accessType.name, parameters));
                            }
                            dataTypeGroup->addChild(copyTypeGroup.release());
                        }
                        commandTypeGroup->addChild(dataTypeGroup.release());
                    }
                    typeGroup->addChild(commandTypeGroup.release());
                }
                tilingGroup->addChild(typeGroup.release());
            }
            formatGroup->addChild(tilingGroup.release());
        }
        testGroup->addChild(formatGroup.release());
    }

    return testGroup.release();
}

} // namespace image
} // namespace vkt
