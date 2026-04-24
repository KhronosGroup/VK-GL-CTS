/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google Inc.
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
 * \brief Tests for VK_EXT_external_memory_acquire_unmodified
 *
 * We expect the driver to implement VkExternalMemoryAcquireUnmodifiedEXT::acquireUnmodifiedMemory as a no-op when
 * acquiring ownership from VK_QUEUE_FAMILY_EXTERNAL because of the spec's requirements[1] on the queue.  Therefore, we
 * only test VkExternalMemoryHandleTypeFlagBits that support VK_QUEUE_FAMILY_FOREIGN_EXT, which has no restriction.
 *
 * [1]: The Vulkan 1.3.238 spec says:
 *        The special queue family index VK_QUEUE_FAMILY_EXTERNAL represents any queue external to the resource's current
 *        Vulkan instance, as long as the queue uses the same underlying device group or physical device, and the same
 *        driver version as the resource's VkDevice, as indicated by VkPhysicalDeviceIDProperties::deviceUUID and
 *        VkPhysicalDeviceIDProperties::driverUUID.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryExternalMemoryAcquireUnmodifiedTests.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestCase.hpp"
#include "tcuTextureUtil.hpp"

#include "vktExternalMemoryUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace memory
{
namespace
{

using namespace vk;

using de::MovePtr;
using tcu::PixelBufferAccess;
using tcu::Vec4;
using vk::Move;

struct TestParams;
class TestCase;
class TestInstance;
class ImageWithMemory;

const VkExtent3D imageExtent                        = {512, 512, 1};
const VkImageSubresourceRange imageSubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

struct TestParams
{
    VkFormat format;
    VkExternalMemoryHandleTypeFlagBits externalMemoryType;
};

class TestCase : public vkt::TestCase
{
public:
    TestCase(tcu::TestContext &context, const char *name, TestParams params)
        : vkt::TestCase(context, name)
        , m_params(params)
    {
    }

    void checkSupport(Context &context) const override;
    vkt::TestInstance *createInstance(Context &context) const override;

    const TestParams m_params;
};

class TestInstance : public vkt::TestInstance
{
public:
    TestInstance(Context &context, TestParams params)
        : vkt::TestInstance(context)
        , m_params(params)
        , m_textureFormat(mapVkFormat(params.format))
        , m_queue(m_context.getUniversalQueue())
        , m_queueFamilyIndex(m_context.getUniversalQueueFamilyIndex())
        , m_allocator(context.getDefaultAllocator())
    {
    }

    tcu::TestStatus iterate(void) override;
    bool testDmaBuf(void);
    bool testDmaBufWithDrmFormatModifer(uint64_t drmFormatModifier);
    bool testAndroidHardwareBuffer(void);
    bool testImage(const ImageWithMemory &image);

private:
    const TestParams m_params;
    const tcu::TextureFormat m_textureFormat;
    const VkQueue m_queue;
    const uint32_t m_queueFamilyIndex;

    Allocator &m_allocator;
    Move<VkCommandPool> m_cmdPool;

    size_t m_bufferSize;

    MovePtr<BufferWithMemory> m_src1Buffer;
    PixelBufferAccess m_src1Access;

    MovePtr<BufferWithMemory> m_src2Buffer;
    PixelBufferAccess m_src2TotalAccess;
    VkRect2D m_src2UpdateRect;
    PixelBufferAccess m_src2UpdateAccess;

    MovePtr<BufferWithMemory> m_resultBuffer;
    PixelBufferAccess m_resultAccess;
};

class ImageWithMemory
{
public:
    ImageWithMemory(Context &context, VkFormat format);
    virtual ~ImageWithMemory(void) = 0;

    const VkImage &get(void) const
    {
        return m_image.get();
    }
    const VkImage &operator*(void) const
    {
        return m_image.get();
    }

    VkFormat getFormat(void) const
    {
        return m_format;
    }

protected:
    static constexpr VkImageType m_imageType     = VK_IMAGE_TYPE_2D;
    static const uint32_t m_mipLevels            = 1;
    static const uint32_t m_arrayLayers          = 1;
    static const VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
    static const VkImageUsageFlags m_usage       = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    static const VkFormatFeatureFlags m_formatFeatures =
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

    Context &m_context;
    VkFormat m_format;
    Move<VkImage> m_image;
    Move<VkDeviceMemory> m_memory;

    // "deleted"
    ImageWithMemory(const ImageWithMemory &);
    ImageWithMemory &operator=(const ImageWithMemory &);
};

class DmaBufImageWithMemory : public ImageWithMemory
{
public:
    DmaBufImageWithMemory(Context &context, VkFormat format, uint64_t drmFormatModifier);
    static std::vector<uint64_t> getCompatibleDrmFormatModifiers(Context &context, VkFormat format);

private:
    static std::vector<uint64_t> getDrmFormatModifiersForFormat(Context &context, VkFormat format);
    static bool isDrmFormatModifierCompatible(Context &context, VkFormat format, uint64_t modifier);
};

class AhbBufImageWithMemory : public ImageWithMemory
{
public:
    AhbBufImageWithMemory(Context &context, VkFormat format);
};

ptrdiff_t ptrDiff(const void *x, const void *y)
{
    return static_cast<const char *>(x) - static_cast<const char *>(y);
}

void TestCase::checkSupport(Context &context) const
{
    // Do not explicitly require extensions that are transitively required.

    context.requireDeviceFunctionality("VK_EXT_external_memory_acquire_unmodified");

    switch (m_params.externalMemoryType)
    {
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
        context.requireDeviceFunctionality("VK_EXT_external_memory_dma_buf");
        context.requireDeviceFunctionality("VK_EXT_image_drm_format_modifier");
        break;
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID:
        context.requireDeviceFunctionality("VK_ANDROID_external_memory_android_hardware_buffer");
        break;
    default:
        DE_ASSERT(false);
        break;
    }
}

vkt::TestInstance *TestCase::createInstance(Context &context) const
{
    return new TestInstance(context, m_params);
}

struct MemoryTypeFilter
{
    uint32_t allowedIndexes;
    VkMemoryPropertyFlags requiredProps;
    VkMemoryPropertyFlags preferredProps;
};

// Return UINT32_MAX on failure.
uint32_t chooseMemoryType(Context &context, const MemoryTypeFilter &filter)
{
    const InstanceInterface &vki    = context.getInstanceInterface();
    VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    const VkPhysicalDeviceMemoryProperties memProps = vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);
    uint32_t score[VK_MAX_MEMORY_TYPES]             = {0};

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if (!(filter.allowedIndexes & (1 << i)))
            continue;

        VkMemoryPropertyFlags curProps = memProps.memoryTypes[i].propertyFlags;

        if (filter.requiredProps != (filter.requiredProps & curProps))
            continue;

        if (!filter.preferredProps)
        {
            // Choose the first match
            return i;
        }

        score[i] = 1 + dePop32(filter.preferredProps & curProps);
    }

    uint32_t bestIndex = UINT32_MAX;
    uint32_t bestScore = 0;

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if (score[i] > bestScore)
        {
            bestIndex = i;
            bestScore = score[i];
        }
    }

    return bestIndex;
}

tcu::TestStatus TestInstance::iterate(void)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();

    m_cmdPool = createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

    m_bufferSize = m_textureFormat.getPixelSize() * imageExtent.width * imageExtent.height;

    m_src1Buffer = MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, m_allocator, makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        MemoryRequirement::HostVisible));
    m_src1Access = PixelBufferAccess(m_textureFormat, imageExtent.width, imageExtent.height, 1,
                                     m_src1Buffer->getAllocation().getHostPtr());

    m_src2Buffer                   = MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, m_allocator, makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        MemoryRequirement::HostVisible));
    m_src2TotalAccess              = PixelBufferAccess(m_textureFormat, imageExtent.width, imageExtent.height, 1,
                                                       m_src2Buffer->getAllocation().getHostPtr());
    m_src2UpdateRect.offset.x      = imageExtent.width / 4;
    m_src2UpdateRect.offset.y      = imageExtent.height / 4;
    m_src2UpdateRect.extent.width  = imageExtent.width / 2;
    m_src2UpdateRect.extent.height = imageExtent.height / 2;
    m_src2UpdateAccess = tcu::getSubregion(m_src2TotalAccess, m_src2UpdateRect.offset.x, m_src2UpdateRect.offset.y,
                                           m_src2UpdateRect.extent.width, m_src2UpdateRect.extent.height);

    m_resultBuffer = MovePtr<BufferWithMemory>(new BufferWithMemory(
        vkd, device, m_allocator, makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));
    m_resultAccess = PixelBufferAccess(m_textureFormat, imageExtent.width, imageExtent.height, 1,
                                       m_resultBuffer->getAllocation().getHostPtr());

    // Fill the first source buffer with a gradient.
    {
        const Vec4 minColor(0.1f, 0.0f, 0.8f, 1.0f);
        const Vec4 maxColor(0.9f, 0.7f, 0.2f, 1.0f);
        tcu::fillWithComponentGradients2(m_src1Access, minColor, maxColor);
        flushAlloc(vkd, device, m_src1Buffer->getAllocation());
    }

    // Fill the second source buffer. Its content is a copy of the first source buffer, with a subrect filled with
    // a different gradient.
    {
        const Vec4 minColor(0.9f, 0.2f, 0.1f, 1.0f);
        const Vec4 maxColor(0.3f, 0.4f, 0.5f, 1.0f);

        deMemcpy(m_src2Buffer->getAllocation().getHostPtr(), m_src1Buffer->getAllocation().getHostPtr(), m_bufferSize);
        tcu::fillWithComponentGradients2(m_src2UpdateAccess, minColor, maxColor);
        flushAlloc(vkd, device, m_src2Buffer->getAllocation());
    }

    bool result = false;
    switch (m_params.externalMemoryType)
    {
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID:
        result = this->testAndroidHardwareBuffer();
        break;
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
        result = this->testDmaBuf();
        break;
    default:
        TCU_THROW(InternalError, "unexpected VkExternalMemoryHandleTypeFlagBits");
    }

    if (result)
    {
        return tcu::TestStatus::pass("Pass");
    }
    else
    {
        return tcu::TestStatus::fail("Fail");
    }
}

bool TestInstance::testAndroidHardwareBuffer(void)
{
    return this->testImage(AhbBufImageWithMemory(m_context, m_params.format));
}

bool TestInstance::testDmaBuf(void)
{
    bool result                = true;
    std::vector<uint64_t> mods = DmaBufImageWithMemory::getCompatibleDrmFormatModifiers(m_context, m_params.format);

    if (mods.empty())
        TCU_THROW(NotSupportedError, "failed to find compatible DRM format modifier");

    // Test each DRM format modifier. Continue if a modifier fails, for the benefit of logging.
    for (const uint64_t &mod : mods)
    {
        if (!this->testDmaBufWithDrmFormatModifer(mod))
        {
            result = false;
        }
    }

    return result;
}

bool TestInstance::testDmaBufWithDrmFormatModifer(uint64_t drmFormatModifier)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    std::ostringstream section;
    section << "Test DRM format modifier 0x" << std::hex << drmFormatModifier;
    log << tcu::TestLog::Section(section.str(), "");

    bool result           = this->testImage(DmaBufImageWithMemory(m_context, m_params.format, drmFormatModifier));
    const char *resultStr = result ? "passed" : "failed";

    log << tcu::TestLog::Message << "DRM format modifier 0x" << std::hex << drmFormatModifier << " " << resultStr
        << tcu::TestLog::EndMessage << tcu::TestLog::EndSection;

    return result;
}

bool TestInstance::testImage(const ImageWithMemory &image)
{
    tcu::TestLog &log          = m_context.getTestContext().getLog();
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();

    vkd.resetCommandPool(device, *m_cmdPool, 0);
    deMemset(m_resultBuffer->getAllocation().getHostPtr(), 0, m_bufferSize);
    invalidateAlloc(vkd, device, m_resultBuffer->getAllocation());

    // Copy the gradient to the image, filling the whole image. Then release ownership of image to foreign queue.
    {
        Move<VkCommandBuffer> cmdBuffer =
            vk::allocateCommandBuffer(vkd, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        vk::beginCommandBuffer(vkd, *cmdBuffer, 0);

        {
            // Prepare buffer as copy source.
            VkBufferMemoryBarrier bufferBarrier = initVulkanStructure();
            bufferBarrier.srcAccessMask         = VK_ACCESS_HOST_WRITE_BIT;
            bufferBarrier.dstAccessMask         = VK_ACCESS_TRANSFER_READ_BIT;
            bufferBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer                = **m_src1Buffer;
            bufferBarrier.offset                = 0;
            bufferBarrier.size                  = VK_WHOLE_SIZE;

            // Prepare image as copy dest.
            VkImageMemoryBarrier imageBarrier = initVulkanStructure();
            imageBarrier.srcAccessMask        = 0;
            imageBarrier.dstAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrier.newLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.image                = image.get();
            imageBarrier.subresourceRange     = imageSubresourceRange;

            vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   (VkDependencyFlags)0, 0, VK_NULL_HANDLE, 1, &bufferBarrier, 1, &imageBarrier);
        }

        {
            // Copy the gradient to the whole image.
            VkBufferImageCopy copy;
            copy.bufferOffset                    = 0;
            copy.bufferRowLength                 = m_src1Access.getWidth();
            copy.bufferImageHeight               = m_src1Access.getHeight();
            copy.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel       = 0;
            copy.imageSubresource.baseArrayLayer = 0;
            copy.imageSubresource.layerCount     = 1;
            copy.imageOffset.x                   = 0;
            copy.imageOffset.y                   = 0;
            copy.imageOffset.z                   = 0;
            copy.imageExtent                     = imageExtent;

            vkd.cmdCopyBufferToImage(*cmdBuffer, **m_src1Buffer, image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                     &copy);
        }

        {
            // Release ownership of image to foreign queue.
            VkImageMemoryBarrier imageBarrier = initVulkanStructure();
            imageBarrier.srcAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier.dstAccessMask        = VK_ACCESS_NONE;
            imageBarrier.oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.newLayout            = VK_IMAGE_LAYOUT_GENERAL;
            imageBarrier.srcQueueFamilyIndex  = m_queueFamilyIndex;
            imageBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_FOREIGN_EXT;
            imageBarrier.image                = image.get();
            imageBarrier.subresourceRange     = imageSubresourceRange;

            vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_NONE,
                                   (VkDependencyFlags)0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &imageBarrier);
        }

        vk::endCommandBuffer(vkd, *cmdBuffer);
        vk::submitCommandsAndWait(vkd, device, m_queue, *cmdBuffer);
    }

    // Acquire ownership of the image from the foreign queue. Then copy the new gradient in the updated region of the
    // buffer to the corresponding region of the image. We do not overwrite the full image because we wish to test the
    // interaction of partial updates with VK_EXT_external_memory_acquire_unmodified.
    {
        Move<VkCommandBuffer> cmdBuffer =
            vk::allocateCommandBuffer(vkd, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        vk::beginCommandBuffer(vkd, *cmdBuffer, 0);

        {
            // Prepare buffer as copy source.
            VkBufferMemoryBarrier bufferBarrier = initVulkanStructure();
            bufferBarrier.srcAccessMask         = VK_ACCESS_HOST_WRITE_BIT;
            bufferBarrier.dstAccessMask         = VK_ACCESS_TRANSFER_READ_BIT;
            bufferBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer                = **m_src2Buffer;
            bufferBarrier.offset                = 0;
            bufferBarrier.size                  = VK_WHOLE_SIZE;

            // VUID-vkCmdPipelineBarrier-srcStageMask-09633
            vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   (VkDependencyFlags)0, 0, VK_NULL_HANDLE, 1, &bufferBarrier, 0, VK_NULL_HANDLE);

            // Image is unmodified since the most recent release
            VkExternalMemoryAcquireUnmodifiedEXT acquireUnmodified = initVulkanStructure();
            acquireUnmodified.acquireUnmodifiedMemory              = VK_TRUE;

            // Acquire ownership of image and prepare as copy dest.
            VkImageMemoryBarrier imageBarrier = initVulkanStructure();
            imageBarrier.pNext                = &acquireUnmodified;
            imageBarrier.srcAccessMask        = VK_ACCESS_NONE;
            imageBarrier.dstAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier.oldLayout            = VK_IMAGE_LAYOUT_GENERAL;
            imageBarrier.newLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_FOREIGN_EXT;
            imageBarrier.dstQueueFamilyIndex  = m_queueFamilyIndex;
            imageBarrier.image                = image.get();
            imageBarrier.subresourceRange     = imageSubresourceRange;

            vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   (VkDependencyFlags)0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &imageBarrier);
        }

        {
            // Copy the updated region of the reference buffer to the image. This is a partial copy.
            VkBufferImageCopy copy;
            copy.bufferOffset    = ptrDiff(m_src2UpdateAccess.getDataPtr(), m_src2Buffer->getAllocation().getHostPtr());
            copy.bufferRowLength = m_src2TotalAccess.getWidth();
            copy.bufferImageHeight               = m_src2TotalAccess.getHeight();
            copy.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel       = 0;
            copy.imageSubresource.baseArrayLayer = 0;
            copy.imageSubresource.layerCount     = 1;
            copy.imageOffset.x                   = m_src2UpdateRect.offset.x;
            copy.imageOffset.y                   = m_src2UpdateRect.offset.y;
            copy.imageOffset.z                   = 0;
            copy.imageExtent.width               = m_src2UpdateRect.extent.width;
            copy.imageExtent.height              = m_src2UpdateRect.extent.height;
            copy.imageExtent.depth               = 1;

            vkd.cmdCopyBufferToImage(*cmdBuffer, **m_src2Buffer, image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                     &copy);
        }

        {
            // Prepare image as copy source.
            VkImageMemoryBarrier imageBarrier = initVulkanStructure();
            imageBarrier.srcAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier.dstAccessMask        = VK_ACCESS_TRANSFER_READ_BIT;
            imageBarrier.oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.newLayout            = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.image                = image.get();
            imageBarrier.subresourceRange     = imageSubresourceRange;

            vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   (VkDependencyFlags)0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &imageBarrier);
        }

        {
            // Copy image to results buffer.
            VkBufferImageCopy copy;
            copy.bufferOffset                    = 0;
            copy.bufferRowLength                 = 0;
            copy.bufferImageHeight               = 0;
            copy.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel       = 0;
            copy.imageSubresource.baseArrayLayer = 0;
            copy.imageSubresource.layerCount     = 1;
            copy.imageOffset.x                   = 0;
            copy.imageOffset.y                   = 0;
            copy.imageOffset.z                   = 0;
            copy.imageExtent                     = imageExtent;

            vkd.cmdCopyImageToBuffer(*cmdBuffer, image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_resultBuffer, 1,
                                     &copy);
        }

        {
            // Prepare results buffer for host read.
            VkBufferMemoryBarrier bufferBarrier = initVulkanStructure();
            bufferBarrier.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufferBarrier.dstAccessMask         = VK_ACCESS_HOST_READ_BIT;
            bufferBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer                = **m_resultBuffer;
            bufferBarrier.offset                = 0;
            bufferBarrier.size                  = VK_WHOLE_SIZE;

            vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                   (VkDependencyFlags)0, 0, VK_NULL_HANDLE, 1, &bufferBarrier, 0, VK_NULL_HANDLE);
        }

        vk::endCommandBuffer(vkd, *cmdBuffer);
        vk::submitCommandsAndWait(vkd, device, m_queue, *cmdBuffer);
    }

    // Compare reference buffer and results buffer.
    if (vk::isFloatFormat(image.getFormat()))
    {
        const Vec4 threshold(0.0f);
        if (!tcu::floatThresholdCompare(log, "Compare", "Result comparison", m_src2TotalAccess, m_resultAccess,
                                        threshold, tcu::COMPARE_LOG_ON_ERROR))
        {
            log << tcu::TestLog::Message << "Image comparison failed" << tcu::TestLog::EndMessage;
            return false;
        }
    }
    else if (vk::isUnormFormat(image.getFormat()))
    {
        const tcu::UVec4 threshold(0u);
        if (!tcu::intThresholdCompare(log, "Compare", "Result comparison", m_src2TotalAccess, m_resultAccess, threshold,
                                      tcu::COMPARE_LOG_ON_ERROR))
        {
            log << tcu::TestLog::Message << "Image comparison failed" << tcu::TestLog::EndMessage;
            return false;
        }
    }
    else
    {
        TCU_THROW(InternalError, "unexpected format datatype");
    }

    log << tcu::TestLog::Message << "Image comparison passed" << tcu::TestLog::EndMessage;
    return true;
}

ImageWithMemory::ImageWithMemory(Context &context, VkFormat format) : m_context(context), m_format(format)
{
}

ImageWithMemory::~ImageWithMemory(void)
{
}

std::vector<uint64_t> DmaBufImageWithMemory::getCompatibleDrmFormatModifiers(Context &context, VkFormat format)
{
    std::vector<uint64_t> mods;

    for (const uint64_t &mod : getDrmFormatModifiersForFormat(context, format))
    {
        if (isDrmFormatModifierCompatible(context, format, mod))
        {
            mods.push_back(mod);
        }
    }

    return mods;
}

std::vector<uint64_t> DmaBufImageWithMemory::getDrmFormatModifiersForFormat(Context &context, VkFormat format)
{
    const InstanceInterface &vki    = context.getInstanceInterface();
    VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    VkDrmFormatModifierPropertiesListEXT modifierList = initVulkanStructure();
    modifierList.drmFormatModifierCount               = 0;
    modifierList.pDrmFormatModifierProperties         = nullptr;

    VkFormatProperties2 formatProperties2 = initVulkanStructure();
    formatProperties2.pNext               = &modifierList;

    vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties2);

    std::vector<VkDrmFormatModifierPropertiesEXT> modifierProperties;
    modifierProperties.resize(modifierList.drmFormatModifierCount);
    modifierList.pDrmFormatModifierProperties = modifierProperties.data();

    vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties2);

    std::vector<uint64_t> modifiers;

    for (const auto &props : modifierProperties)
    {
        if (m_formatFeatures == (m_formatFeatures & props.drmFormatModifierTilingFeatures))
        {
            modifiers.push_back(props.drmFormatModifier);
        }
    }

    return modifiers;
}

bool DmaBufImageWithMemory::isDrmFormatModifierCompatible(Context &context, VkFormat format, uint64_t modifier)
{
    const InstanceInterface &vki    = context.getInstanceInterface();
    VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo = initVulkanStructure();
    modifierInfo.drmFormatModifier                             = modifier;
    modifierInfo.sharingMode                                   = VK_SHARING_MODE_EXCLUSIVE;
    modifierInfo.queueFamilyIndexCount                         = 0;
    modifierInfo.pQueueFamilyIndices                           = NULL;

    VkPhysicalDeviceExternalImageFormatInfo externalImageInfo = initVulkanStructure();
    externalImageInfo.pNext                                   = &modifierInfo;
    externalImageInfo.handleType                              = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkPhysicalDeviceImageFormatInfo2 imageInfo2 = initVulkanStructure();
    imageInfo2.pNext                            = &externalImageInfo;
    imageInfo2.format                           = format;
    imageInfo2.type                             = m_imageType;
    imageInfo2.tiling                           = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    imageInfo2.usage                            = m_usage;
    imageInfo2.flags                            = 0;

    VkExternalImageFormatProperties externalImageProperties = initVulkanStructure();

    VkImageFormatProperties2 imageProperties2 = initVulkanStructure();
    imageProperties2.pNext                    = &externalImageProperties;

    if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageInfo2, &imageProperties2) ==
        VK_ERROR_FORMAT_NOT_SUPPORTED)
        return false;

    // We check only that the image will support being bound to an imported dma_buf, as that's universally supported (as
    // of 2022-12-31) by all known drivers that support VK_EXT_external_memory_dma_buf and
    // VK_EXT_image_drm_format_modifier. Some drivers do not support exporting dma_buf.
    if (!(externalImageProperties.externalMemoryProperties.externalMemoryFeatures &
          VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
        return false;

    if (imageExtent.width > imageProperties2.imageFormatProperties.maxExtent.width ||
        imageExtent.height > imageProperties2.imageFormatProperties.maxExtent.height ||
        imageExtent.depth > imageProperties2.imageFormatProperties.maxExtent.depth)
        return false;

    return true;
}

DmaBufImageWithMemory::DmaBufImageWithMemory(Context &context, VkFormat format, uint64_t drmFormatModifier)
    : ImageWithMemory(context, format)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();

    // Create VkImage
    {
        VkImageDrmFormatModifierListCreateInfoEXT modifierInfo = initVulkanStructure();
        modifierInfo.drmFormatModifierCount                    = 1;
        modifierInfo.pDrmFormatModifiers                       = &drmFormatModifier;

        VkExternalMemoryImageCreateInfo externalInfo = initVulkanStructure();
        externalInfo.pNext                           = &modifierInfo;
        externalInfo.handleTypes                     = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

        VkImageCreateInfo imageInfo     = initVulkanStructure();
        imageInfo.pNext                 = &externalInfo;
        imageInfo.flags                 = 0;
        imageInfo.imageType             = m_imageType;
        imageInfo.format                = m_format;
        imageInfo.extent                = imageExtent;
        imageInfo.mipLevels             = m_mipLevels;
        imageInfo.arrayLayers           = m_arrayLayers;
        imageInfo.samples               = m_samples;
        imageInfo.tiling                = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
        imageInfo.usage                 = m_usage;
        imageInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.queueFamilyIndexCount = 0;
        imageInfo.pQueueFamilyIndices   = nullptr;
        imageInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

        m_image = vk::createImage(vkd, device, &imageInfo);
    }

    // Allocate VkDeviceMemory
    //
    // We do not import a dma_buf because dEQP has no utilities to create dma_bufs with a non-Vulkan allocator (as of
    // 2022-12-31). However, we do create the image with VkExternalMemoryImageCreateInfo::handleTypes = DMA_BUF, and
    // that should be sufficient for testing a well-written Vulkan driver in isolation. A well-written Vulkan driver, if
    // that bit is set, will produce the same behavior whether we use Vulkan as the memory allocator or use an external
    // dma_buf allocator, such as GBM. But this is insufficient for testing the full graphics stack. To test the full
    // stack, as it is commonly used in production, we must allocate the dma_buf with GBM.
    //
    // TODO: Test two memory allocation paths: (1) Vulkan as memory allocator and (2) GBM as dma_buf allocator.
    {
        VkImageMemoryRequirementsInfo2 memReqsInfo2 = initVulkanStructure();
        memReqsInfo2.image                          = *m_image;

        VkMemoryDedicatedRequirements dedicatedReqs = initVulkanStructure();

        VkMemoryRequirements2 memReqs2 = initVulkanStructure();
        memReqs2.pNext                 = &dedicatedReqs;

        vkd.getImageMemoryRequirements2(device, &memReqsInfo2, &memReqs2);

        MemoryTypeFilter filter;
        filter.allowedIndexes = memReqs2.memoryRequirements.memoryTypeBits;
        filter.requiredProps  = 0;
        filter.preferredProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        uint32_t memTypeIndex = chooseMemoryType(m_context, filter);
        DE_ASSERT(memTypeIndex != UINT32_MAX);

        VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = initVulkanStructure();
        dedicatedAllocInfo.image                         = *m_image;

        VkMemoryAllocateInfo allocInfo = initVulkanStructure();
        if (dedicatedReqs.requiresDedicatedAllocation)
            allocInfo.pNext = &dedicatedAllocInfo;
        allocInfo.allocationSize  = memReqs2.memoryRequirements.size;
        allocInfo.memoryTypeIndex = memTypeIndex;

        m_memory = vk::allocateMemory(vkd, device, &allocInfo);
    }

    VK_CHECK(vkd.bindImageMemory(device, *m_image, *m_memory, 0));
}

AhbBufImageWithMemory::AhbBufImageWithMemory(Context &context, VkFormat format) : ImageWithMemory(context, format)
{
    vkt::ExternalMemoryUtil::AndroidHardwareBufferExternalApi *ahbApi =
        vkt::ExternalMemoryUtil::AndroidHardwareBufferExternalApi::getInstance();
    if (!ahbApi)
        TCU_THROW(NotSupportedError, "Android Hardware Buffer not supported");

    const DeviceInterface &vk       = m_context.getDeviceInterface();
    VkDevice device                 = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    // While the texture is used as source and destination transfer only, there's no actual AHB equivalent and therefore usage will be 0
    // Vulkan forbids usage being 0 through VUID-vkGetAndroidHardwareBufferPropertiesANDROID-buffer-01884
    // Ideally at some point there may be an equivalent for source and destination transfer only for AHB
    uint64_t requiredAhbUsage        = ahbApi->vkUsageToAhbUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    pt::AndroidHardwareBufferPtr ahb = ahbApi->allocate(imageExtent.width, imageExtent.height, m_arrayLayers,
                                                        ahbApi->vkFormatToAhbFormat(m_format), requiredAhbUsage);

    if (ahb.internal == nullptr)
        TCU_THROW(NotSupportedError, "Required number of layers for Android Hardware Buffer not supported");

    vkt::ExternalMemoryUtil::NativeHandle nativeHandle(ahb);
    m_image = vkt::ExternalMemoryUtil::createExternalImage(
        vk, device, queueFamilyIndex, VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID, m_format,
        imageExtent.width, imageExtent.height, VK_IMAGE_TILING_OPTIMAL, 0u, m_usage, m_mipLevels, m_arrayLayers);

    VkAndroidHardwareBufferPropertiesANDROID ahbProperties = {
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, // VkStructureType    sType
        nullptr,                                                      // const void*        pNext
        0u,                                                           // VkDeviceSize        allocationSize
        0                                                             // uint32_t            memoryTypeBits
    };

    vk.getAndroidHardwareBufferPropertiesANDROID(device, nativeHandle.getAndroidHardwareBuffer(), &ahbProperties);

    const VkImportAndroidHardwareBufferInfoANDROID importInfo = {
        VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID, // VkStructureType            sType
        nullptr,                                                       // const void*                pNext
        nativeHandle.getAndroidHardwareBuffer()                        // struct AHardwareBuffer*    buffer
    };

    const VkMemoryDedicatedAllocateInfo dedicatedInfo = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR, // VkStructureType    sType
        &importInfo,                                          // const void*        pNext
        *m_image,                                             // VkImage            image
        VK_NULL_HANDLE                                        // VkBuffer            buffer
    };

    const VkMemoryAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,                                 // VkStructureType    sType
        (const void *)&dedicatedInfo,                                           // const void*        pNext
        ahbProperties.allocationSize,                                           // VkDeviceSize        allocationSize
        vkt::ExternalMemoryUtil::chooseMemoryType(ahbProperties.memoryTypeBits) // uint32_t            memoryTypeIndex
    };

    m_memory = allocateMemory(vk, device, &allocateInfo);
    VK_CHECK(vk.bindImageMemory(device, *m_image, *m_memory, 0u));
}

std::string formatToName(VkFormat format)
{
    const std::string formatStr = de::toString(format);
    const std::string prefix    = "VK_FORMAT_";

    DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

    return de::toLower(formatStr.substr(prefix.length()));
}

} // namespace

tcu::TestCaseGroup *createExternalMemoryAcquireUnmodifiedTests(tcu::TestContext &testCtx)
{
    typedef de::MovePtr<tcu::TestCaseGroup> Group;
    typedef de::MovePtr<tcu::TestCase> Case;

    const VkExternalMemoryHandleTypeFlagBits extMemTypes[] = {
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
    };

    const VkFormat formats[] = {
        VK_FORMAT_R8G8B8A8_UNORM,      VK_FORMAT_B8G8R8A8_UNORM,      VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
    };

    Group rootGroup(new tcu::TestCaseGroup(testCtx, "external_memory_acquire_unmodified",
                                           "Tests for VK_EXT_external_memory_acquire_unmodified"));

    for (auto extMemType : extMemTypes)
    {
        auto extMemName = vkt::ExternalMemoryUtil::externalMemoryTypeToName(extMemType);
        auto extMemStr  = de::toString(vk::getExternalMemoryHandleTypeFlagsStr(extMemType));
        Group extMemGroup(new tcu::TestCaseGroup(testCtx, extMemName, extMemStr.c_str()));

        for (auto format : formats)
        {
            TestParams params;
            params.format             = format;
            params.externalMemoryType = extMemType;

            auto formatName = formatToName(format);
            auto formatStr  = de::toString(vk::getFormatStr(format));
            Case formatCase(new TestCase(testCtx, formatName.c_str(), params));

            extMemGroup->addChild(formatCase.release());
        }

        rootGroup->addChild(extMemGroup.release());
    }

    return rootGroup.release();
}

} // namespace memory
} // namespace vkt
