/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Tests for render pass multisample resolve
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassMultisampleResolveTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkBarrierUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuFloat.hpp"
#include "tcuImageCompare.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include <numeric>

using namespace vk;

using tcu::BVec4;
using tcu::IVec2;
using tcu::IVec4;
using tcu::UVec2;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;
using tcu::TestLog;

using std::vector;

typedef de::SharedPtr<Allocation> AllocationSp;
typedef de::SharedPtr<vk::Unique<VkImage>> VkImageSp;
typedef de::SharedPtr<vk::Unique<VkImageView>> VkImageViewSp;
typedef de::SharedPtr<vk::Unique<VkBuffer>> VkBufferSp;
typedef de::SharedPtr<vk::Unique<VkSampler>> VkSamplerSp;
typedef de::SharedPtr<vk::Unique<VkPipeline>> VkPipelineSp;
typedef de::SharedPtr<vk::Unique<VkDescriptorSetLayout>> VkDescriptorSetLayoutSp;
typedef de::SharedPtr<vk::Unique<VkDescriptorPool>> VkDescriptorPoolSp;
typedef de::SharedPtr<vk::Unique<VkDescriptorSet>> VkDescriptorSetSp;

namespace vkt
{
namespace
{

using namespace renderpass;

template <typename T>
de::SharedPtr<T> safeSharedPtr(T *ptr)
{
    try
    {
        return de::SharedPtr<T>(ptr);
    }
    catch (...)
    {
        delete ptr;
        throw;
    }
}

VkImageLayout chooseInputImageLayout(const SharedGroupParams groupParams)
{
#ifndef CTS_USES_VULKANSC
    if (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        // use general layout for local reads for some tests
        if (groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            return VK_IMAGE_LAYOUT_GENERAL;
        return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
    }
#else
    DE_UNREF(groupParams);
#endif
    return VK_IMAGE_LAYOUT_GENERAL;
}

#ifndef CTS_USES_VULKANSC
void beginSecondaryCmdBuffer(const DeviceInterface &vk, VkCommandBuffer secCmdBuffer, uint32_t colorAttachmentsCount,
                             VkSampleCountFlagBits rasterizationSamples)
{
    VkCommandBufferUsageFlags usageFlags(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    const std::vector<VkFormat> colorAttachmentFormats(colorAttachmentsCount, VK_FORMAT_R8G8B8A8_UNORM);

    const VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        nullptr,                                                         // const void* pNext;
        0u,                                                              // VkRenderingFlagsKHR flags;
        0u,                                                              // uint32_t viewMask;
        colorAttachmentsCount,                                           // uint32_t colorAttachmentCount;
        colorAttachmentFormats.data(),                                   // const VkFormat* pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        rasterizationSamples                                             // VkSampleCountFlagBits rasterizationSamples;
    };
    const VkCommandBufferInheritanceInfo bufferInheritanceInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
        &inheritanceRenderingInfo,                         // const void* pNext;
        VK_NULL_HANDLE,                                    // VkRenderPass renderPass;
        0u,                                                // uint32_t subpass;
        VK_NULL_HANDLE,                                    // VkFramebuffer framebuffer;
        VK_FALSE,                                          // VkBool32 occlusionQueryEnable;
        (VkQueryControlFlags)0u,                           // VkQueryControlFlags queryFlags;
        (VkQueryPipelineStatisticFlags)0u                  // VkQueryPipelineStatisticFlags pipelineStatistics;
    };
    const VkCommandBufferBeginInfo commandBufBeginParams{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo                       // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };
    VK_CHECK(vk.beginCommandBuffer(secCmdBuffer, &commandBufBeginParams));
}
#endif

enum TestType
{
    RESOLVE = 0,
    MAX_ATTACHMENTS,
    COMPATIBILITY
};

struct TestConfig
{
    TestType testType;
    VkFormat format;
    uint32_t sampleCount;
    uint32_t layerCount;
    uint32_t baseLayer;
    uint32_t attachmentCount;
    uint32_t width;
    uint32_t height;
    const SharedGroupParams groupParams;
};

struct TestConfig2 : TestConfig
{
    TestConfig2(const TestConfig &src, uint32_t level) : TestConfig(src), resolveLevel(level)
    {
    }
    uint32_t resolveLevel;
};

// Render pass traits that groups render pass related types together and by that help
// to reduce number of template parrameters passed to number of functions in those tests
struct RenderPass1Trait
{
    typedef AttachmentDescription1 AttDesc;
    typedef AttachmentReference1 AttRef;
    typedef SubpassDescription1 SubpassDesc;
    typedef SubpassDependency1 SubpassDep;
    typedef RenderPassCreateInfo1 RenderPassCreateInfo;
};
struct RenderPass2Trait
{
    typedef AttachmentDescription2 AttDesc;
    typedef AttachmentReference2 AttRef;
    typedef SubpassDescription2 SubpassDesc;
    typedef SubpassDependency2 SubpassDep;
    typedef RenderPassCreateInfo2 RenderPassCreateInfo;
};

class MultisampleRenderPassTestBase : public TestInstance
{
public:
    MultisampleRenderPassTestBase(Context &context, TestConfig config);
    ~MultisampleRenderPassTestBase(void) = default;

protected:
    Move<VkImage> createImage(VkSampleCountFlagBits sampleCountBit, VkImageUsageFlags usage) const;
    Move<VkImage> createImage(VkSampleCountFlagBits sampleCountBit, VkImageUsageFlags usage, uint32_t width,
                              uint32_t height, uint32_t mipLevels) const;
    vector<VkImageSp> createImages(VkSampleCountFlagBits sampleCountBit, VkImageUsageFlags usage) const;
    vector<VkImageSp> createImages(VkSampleCountFlagBits sampleCountBit, VkImageUsageFlags usage, uint32_t width,
                                   uint32_t height, uint32_t mipLevels) const;
    vector<AllocationSp> createImageMemory(const vector<VkImageSp> &images) const;
    vector<VkImageViewSp> createImageViews(const vector<VkImageSp> &images, uint32_t mipLevel = 0,
                                           uint32_t baseLayers = 0) const;

    vector<VkBufferSp> createBuffers() const;
    vector<VkBufferSp> createBuffers(uint32_t width, uint32_t height, uint32_t mipLevels) const;
    vector<AllocationSp> createBufferMemory(const vector<VkBufferSp> &buffers) const;

    Move<VkFramebuffer> createFramebuffer(const std::vector<VkImageViewSp> multisampleImageViews,
                                          const std::vector<VkImageViewSp> singlesampleImageViews,
                                          VkRenderPass renderPass) const;

    VkClearValue getClearValue() const;
    void clearAttachments(VkCommandBuffer commandBuffer) const;
    VkDeviceSize getPixelSize() const;
    tcu::Vec4 getFormatThreshold() const;
    VkSampleCountFlagBits sampleCountBitFromSampleCount(uint32_t count) const;
    void logImage(const std::string &name, const tcu::ConstPixelBufferAccess &image) const;
    uint32_t totalLayers() const;

protected:
    const bool m_testCompatibility;
    const SharedGroupParams m_groupParams;

    const VkFormat m_format;
    const VkSampleCountFlagBits m_sampleCount;
    const VkImageLayout m_inputImageReadLayout;
    const uint32_t m_layerCount;
    const uint32_t m_baseLayer;
    const uint32_t m_attachmentsCount;
    const uint32_t m_width;
    const uint32_t m_height;
};

MultisampleRenderPassTestBase::MultisampleRenderPassTestBase(Context &context, TestConfig config)
    : TestInstance(context)
    , m_testCompatibility(config.testType == COMPATIBILITY)
    , m_groupParams(config.groupParams)
    , m_format(config.format)
    , m_sampleCount(sampleCountBitFromSampleCount(config.sampleCount))
    , m_inputImageReadLayout(chooseInputImageLayout(m_groupParams))
    , m_layerCount(config.layerCount)
    , m_baseLayer(config.baseLayer)
    , m_attachmentsCount(config.attachmentCount)
    , m_width(config.width)
    , m_height(config.height)
{
}

Move<VkImage> MultisampleRenderPassTestBase::createImage(VkSampleCountFlagBits sampleCountBit,
                                                         VkImageUsageFlags usage) const
{
    return createImage(sampleCountBit, usage, m_width, m_height, 1u);
}

Move<VkImage> MultisampleRenderPassTestBase::createImage(VkSampleCountFlagBits sampleCountBit, VkImageUsageFlags usage,
                                                         uint32_t width, uint32_t height, uint32_t mipLevels) const
{
    const InstanceInterface &vki    = m_context.getInstanceInterface();
    const DeviceInterface &vkd      = m_context.getDeviceInterface();
    VkDevice device                 = m_context.getDevice();
    VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const tcu::TextureFormat format(mapVkFormat(m_format));
    const VkImageType imageType(VK_IMAGE_TYPE_2D);
    const VkImageTiling imageTiling(VK_IMAGE_TILING_OPTIMAL);
    const VkFormatProperties formatProperties(getPhysicalDeviceFormatProperties(vki, physicalDevice, m_format));
    const VkExtent3D imageExtent = {width, height, 1u};

    try
    {
        const VkImageFormatProperties imageFormatProperties(
            getPhysicalDeviceImageFormatProperties(vki, physicalDevice, m_format, imageType, imageTiling, usage, 0u));
        const auto isDSFormat = (tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order));

        if (isDSFormat &&
            (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
            TCU_THROW(NotSupportedError, "Format can't be used as depth stencil attachment");

        if (!isDSFormat && (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
            TCU_THROW(NotSupportedError, "Format can't be used as color attachment");

        if (imageFormatProperties.maxExtent.width < imageExtent.width ||
            imageFormatProperties.maxExtent.height < imageExtent.height ||
            ((imageFormatProperties.sampleCounts & m_sampleCount) == 0) ||
            imageFormatProperties.maxArrayLayers < m_layerCount)
        {
            TCU_THROW(NotSupportedError, "Image type not supported");
        }

        const VkImageCreateInfo pCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                               nullptr,
                                               0u,
                                               imageType,
                                               m_format,
                                               imageExtent,
                                               mipLevels,
                                               totalLayers(),
                                               sampleCountBit,
                                               imageTiling,
                                               usage,
                                               VK_SHARING_MODE_EXCLUSIVE,
                                               0u,
                                               nullptr,
                                               VK_IMAGE_LAYOUT_UNDEFINED};

        return ::createImage(vkd, device, &pCreateInfo);
    }
    catch (const vk::Error &error)
    {
        if (error.getError() == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Image format not supported");

        throw;
    }
}

vector<VkImageSp> MultisampleRenderPassTestBase::createImages(VkSampleCountFlagBits sampleCountBit,
                                                              VkImageUsageFlags usage) const
{
    std::vector<VkImageSp> images(m_attachmentsCount);
    for (size_t imageNdx = 0; imageNdx < m_attachmentsCount; imageNdx++)
        images[imageNdx] = safeSharedPtr(new Unique<VkImage>(createImage(sampleCountBit, usage)));
    return images;
}

vector<VkImageSp> MultisampleRenderPassTestBase::createImages(VkSampleCountFlagBits sampleCountBit,
                                                              VkImageUsageFlags usage, uint32_t width, uint32_t height,
                                                              uint32_t mipLevels) const
{
    std::vector<VkImageSp> images(m_attachmentsCount);
    for (size_t imageNdx = 0; imageNdx < m_attachmentsCount; imageNdx++)
        images[imageNdx] =
            safeSharedPtr(new Unique<VkImage>(createImage(sampleCountBit, usage, width, height, mipLevels)));
    return images;
}

vector<AllocationSp> MultisampleRenderPassTestBase::createImageMemory(const vector<VkImageSp> &images) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();
    Allocator &allocator       = m_context.getDefaultAllocator();
    std::vector<AllocationSp> memory(images.size());

    for (size_t memoryNdx = 0; memoryNdx < memory.size(); memoryNdx++)
    {
        VkImage image                     = **images[memoryNdx];
        VkMemoryRequirements requirements = getImageMemoryRequirements(vkd, device, image);

        de::MovePtr<Allocation> allocation(allocator.allocate(requirements, MemoryRequirement::Any));
        VK_CHECK(vkd.bindImageMemory(device, image, allocation->getMemory(), allocation->getOffset()));
        memory[memoryNdx] = safeSharedPtr(allocation.release());
    }
    return memory;
}

vector<VkImageViewSp> MultisampleRenderPassTestBase::createImageViews(const vector<VkImageSp> &images,
                                                                      uint32_t mipLevel, uint32_t baseLayer) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();
    std::vector<VkImageViewSp> views(images.size());
    const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1u, baseLayer, m_layerCount};

    for (size_t imageNdx = 0; imageNdx < images.size(); imageNdx++)
    {
        const VkImageViewCreateInfo pCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0u,
            **images[imageNdx],
            VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            m_format,
            makeComponentMappingRGBA(),
            range,
        };
        views[imageNdx] = safeSharedPtr(new Unique<VkImageView>(createImageView(vkd, device, &pCreateInfo)));
    }

    return views;
}

vector<VkBufferSp> MultisampleRenderPassTestBase::createBuffers() const
{
    return createBuffers(m_width, m_height, 1u);
}

vector<VkBufferSp> MultisampleRenderPassTestBase::createBuffers(uint32_t width, uint32_t height,
                                                                uint32_t mipLevels) const
{
    DE_ASSERT(mipLevels);

    VkDeviceSize size = 0;
    for (uint32_t level = 0; level < mipLevels; ++level)
    {
        DE_ASSERT(width && height);

        size += (width * height);
        height /= 2;
        width /= 2;
    }

    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();
    std::vector<VkBufferSp> buffers(m_attachmentsCount);
    const VkDeviceSize pixelSize(getPixelSize());
    const VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                           nullptr,
                                           0u,

                                           size * totalLayers() * pixelSize,
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,

                                           VK_SHARING_MODE_EXCLUSIVE,
                                           0u,
                                           nullptr};

    for (size_t bufferNdx = 0; bufferNdx < buffers.size(); bufferNdx++)
        buffers[bufferNdx] = safeSharedPtr(new Unique<VkBuffer>(createBuffer(vkd, device, &createInfo)));

    return buffers;
}

vector<AllocationSp> MultisampleRenderPassTestBase::createBufferMemory(const vector<VkBufferSp> &buffers) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();
    Allocator &allocator       = m_context.getDefaultAllocator();
    std::vector<de::SharedPtr<Allocation>> memory(buffers.size());

    for (size_t memoryNdx = 0; memoryNdx < memory.size(); memoryNdx++)
    {
        VkBuffer buffer                   = **buffers[memoryNdx];
        VkMemoryRequirements requirements = getBufferMemoryRequirements(vkd, device, buffer);
        de::MovePtr<Allocation> allocation(allocator.allocate(requirements, MemoryRequirement::HostVisible));

        VK_CHECK(vkd.bindBufferMemory(device, buffer, allocation->getMemory(), allocation->getOffset()));
        memory[memoryNdx] = safeSharedPtr(allocation.release());
    }
    return memory;
}

Move<VkFramebuffer> MultisampleRenderPassTestBase::createFramebuffer(
    const std::vector<VkImageViewSp> multisampleImageViews, const std::vector<VkImageViewSp> singlesampleImageViews,
    VkRenderPass renderPass) const
{
    // when RenderPass was not created then we are testing dynamic rendering
    // and we can't create framebuffer without valid RenderPass object
    if (!renderPass)
        return Move<VkFramebuffer>();

    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();

    std::vector<VkImageView> attachments;
    attachments.reserve(multisampleImageViews.size() + singlesampleImageViews.size());

    DE_ASSERT(multisampleImageViews.size() == singlesampleImageViews.size());

    for (size_t ndx = 0; ndx < multisampleImageViews.size(); ndx++)
    {
        attachments.push_back(**multisampleImageViews[ndx]);
        attachments.push_back(**singlesampleImageViews[ndx]);
    }

    const VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                nullptr,
                                                0u,

                                                renderPass,
                                                (uint32_t)attachments.size(),
                                                &attachments[0],

                                                m_width,
                                                m_height,
                                                m_layerCount};

    return ::createFramebuffer(vkd, device, &createInfo);
}

VkClearValue MultisampleRenderPassTestBase::getClearValue() const
{
    const tcu::TextureFormat format(mapVkFormat(m_format));
    const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(format.type));

    switch (channelClass)
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        return makeClearValueColorF32(-1.0f, -1.0f, -1.0f, -1.0f);

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return makeClearValueColorF32(0.0f, 0.0f, 0.0f, 0.0f);

    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        return makeClearValueColorF32(-1.0f, -1.0f, -1.0f, -1.0f);

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return makeClearValueColorI32(-128, -128, -128, -128);

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return makeClearValueColorU32(0u, 0u, 0u, 0u);

    default:
        DE_FATAL("Unknown channel class");
    }

    return makeClearValueColorU32(0u, 0u, 0u, 0u);
}

void MultisampleRenderPassTestBase::clearAttachments(VkCommandBuffer commandBuffer) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkClearValue value         = getClearValue();

    std::vector<VkClearAttachment> colors(m_attachmentsCount);
    for (uint32_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
    {
        colors[attachmentNdx].aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        colors[attachmentNdx].colorAttachment = attachmentNdx;
        colors[attachmentNdx].clearValue      = value;
    }
    const VkClearRect rect = {
        {{0u, 0u}, {m_width, m_height}},
        0u,
        m_layerCount,
    };
    vkd.cmdClearAttachments(commandBuffer, uint32_t(colors.size()), &colors[0], 1u, &rect);
}

VkDeviceSize MultisampleRenderPassTestBase::getPixelSize() const
{
    const tcu::TextureFormat format(mapVkFormat(m_format));
    return format.getPixelSize();
}

tcu::Vec4 MultisampleRenderPassTestBase::getFormatThreshold() const
{
    const tcu::TextureFormat tcuFormat(mapVkFormat(m_format));
    const bool isAlphaOnly = isAlphaOnlyFormat(m_format);
    const uint32_t componentCount(isAlphaOnly ? 4u : tcu::getNumUsedChannels(tcuFormat.order));

    if (isSnormFormat(m_format))
    {
        return Vec4((componentCount >= 1) ? 1.5f * getRepresentableDiffSnorm(m_format, 0) : 0.0f,
                    (componentCount >= 2) ? 1.5f * getRepresentableDiffSnorm(m_format, 1) : 0.0f,
                    (componentCount >= 3) ? 1.5f * getRepresentableDiffSnorm(m_format, 2) : 0.0f,
                    (componentCount == 4) ? 1.5f * getRepresentableDiffSnorm(m_format, 3) : 0.0f);
    }
    else if (isUnormFormat(m_format))
    {
        return Vec4((componentCount >= 1 && !isAlphaOnly) ? 1.5f * getRepresentableDiffUnorm(m_format, 0) : 0.0f,
                    (componentCount >= 2 && !isAlphaOnly) ? 1.5f * getRepresentableDiffUnorm(m_format, 1) : 0.0f,
                    (componentCount >= 3 && !isAlphaOnly) ? 1.5f * getRepresentableDiffUnorm(m_format, 2) : 0.0f,
                    (componentCount == 4) ? 1.5f * getRepresentableDiffUnorm(m_format, 3) : 0.0f);
    }
    else if (isFloatFormat(m_format))
    {
        return (tcuFormat.type == tcu::TextureFormat::HALF_FLOAT) ? tcu::Vec4(0.005f) : Vec4(0.00001f);
    }
    else
        return Vec4(0.001f);
}

VkSampleCountFlagBits MultisampleRenderPassTestBase::sampleCountBitFromSampleCount(uint32_t count) const
{
    switch (count)
    {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    case 32:
        return VK_SAMPLE_COUNT_32_BIT;
    case 64:
        return VK_SAMPLE_COUNT_64_BIT;

    default:
        DE_FATAL("Invalid sample count");
        return (VkSampleCountFlagBits)0x0;
    }
}

void MultisampleRenderPassTestBase::logImage(const std::string &name, const tcu::ConstPixelBufferAccess &image) const
{
    m_context.getTestContext().getLog() << tcu::LogImage(name.c_str(), name.c_str(), image);

    const auto totalLayerCount = totalLayers();
    for (uint32_t layerNdx = m_baseLayer; layerNdx < totalLayerCount; ++layerNdx)
    {
        const std::string layerName(name + " Layer:" + de::toString(layerNdx));
        tcu::ConstPixelBufferAccess layerImage(image.getFormat(), m_width, m_height, 1,
                                               image.getPixelPtr(0, 0, layerNdx));

        m_context.getTestContext().getLog() << tcu::LogImage(layerName.c_str(), layerName.c_str(), layerImage);
    }
}

uint32_t MultisampleRenderPassTestBase::totalLayers() const
{
    return (m_layerCount + m_baseLayer);
}

class MultisampleRenderPassTestInstance : public MultisampleRenderPassTestBase
{
public:
    MultisampleRenderPassTestInstance(Context &context, TestConfig config);
    ~MultisampleRenderPassTestInstance(void) = default;

    tcu::TestStatus iterate(void);

private:
    void drawCommands(VkCommandBuffer cmdBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout) const;

    template <typename RenderpassSubpass>
    void submit(void);
    void submitDynamicRendering(void);
    void submitSwitch(const SharedGroupParams groupParams);
    void verify(void);

    template <typename RenderPassTrait>
    Move<VkRenderPass> createRenderPass(bool usedResolveAttachment);
    Move<VkRenderPass> createRenderPassSwitch(bool usedResolveAttachment);
    Move<VkRenderPass> createRenderPassCompatible(void);
    Move<VkPipelineLayout> createRenderPipelineLayout(void);
    Move<VkPipeline> createRenderPipeline(void);

#ifndef CTS_USES_VULKANSC
    void beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer) const;
#endif // CTS_USES_VULKANSC

private:
    const std::vector<VkImageSp> m_multisampleImages;
    const std::vector<AllocationSp> m_multisampleImageMemory;
    const std::vector<VkImageViewSp> m_multisampleImageViews;

    const std::vector<VkImageSp> m_singlesampleImages;
    const std::vector<AllocationSp> m_singlesampleImageMemory;
    const std::vector<VkImageViewSp> m_singlesampleImageViews;

    const Unique<VkRenderPass> m_renderPass;
    const Unique<VkRenderPass> m_renderPassCompatible;
    const Unique<VkFramebuffer> m_framebuffer;

    const Unique<VkPipelineLayout> m_renderPipelineLayout;
    const Unique<VkPipeline> m_renderPipeline;

    const std::vector<VkBufferSp> m_buffers;
    const std::vector<AllocationSp> m_bufferMemory;

    const Unique<VkCommandPool> m_commandPool;
    tcu::TextureLevel m_sum;
    tcu::TextureLevel m_sumSrgb;
    uint32_t m_sampleMask;
    tcu::ResultCollector m_resultCollector;

protected:
    MultisampleRenderPassTestInstance(Context &context, TestConfig config, uint32_t renderLevel);

    const uint32_t m_renderLevel;
};

MultisampleRenderPassTestInstance::MultisampleRenderPassTestInstance(Context &context, TestConfig config)
    : MultisampleRenderPassTestInstance(context, config, /*defaulf render level*/ 0u)
{
}

MultisampleRenderPassTestInstance::MultisampleRenderPassTestInstance(Context &context, TestConfig config,
                                                                     uint32_t renderLevel)
    : MultisampleRenderPassTestBase(context, config)

    , m_multisampleImages(createImages(m_sampleCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
    , m_multisampleImageMemory(createImageMemory(m_multisampleImages))
    , m_multisampleImageViews(createImageViews(m_multisampleImages))

    , m_singlesampleImages(createImages(VK_SAMPLE_COUNT_1_BIT,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                        (1u << renderLevel) * m_width, (1u << renderLevel) * m_height, renderLevel + 1))
    , m_singlesampleImageMemory(createImageMemory(m_singlesampleImages))
    , m_singlesampleImageViews(createImageViews(m_singlesampleImages, renderLevel, m_baseLayer))

    // The "normal" render pass has an unused resolve attachment when testing compatibility.
    , m_renderPass(createRenderPassSwitch(!m_testCompatibility))
    , m_renderPassCompatible(createRenderPassCompatible())
    , m_framebuffer(createFramebuffer(m_multisampleImageViews, m_singlesampleImageViews, *m_renderPass))

    , m_renderPipelineLayout(createRenderPipelineLayout())
    , m_renderPipeline(createRenderPipeline())

    , m_buffers(createBuffers((1u << renderLevel) * m_width, (1u << renderLevel) * m_height, renderLevel + 1))
    , m_bufferMemory(createBufferMemory(m_buffers))

    , m_commandPool(createCommandPool(context.getDeviceInterface(), context.getDevice(),
                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
    , m_sum(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), m_width, m_height, totalLayers())
    , m_sumSrgb(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT), m_width, m_height,
                totalLayers())
    , m_sampleMask(0x0u)

    , m_renderLevel(renderLevel)
{
    tcu::clear(m_sum.getAccess(), Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    tcu::clear(m_sumSrgb.getAccess(), Vec4(0.0f, 0.0f, 0.0f, 0.0f));
}

void MultisampleRenderPassTestInstance::drawCommands(VkCommandBuffer cmdBuffer, VkPipeline pipeline,
                                                     VkPipelineLayout pipelineLayout) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    // Clear everything to black
    clearAttachments(cmdBuffer);

    // Render black samples
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkd.cmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(m_sampleMask),
                         &m_sampleMask);
    vkd.cmdDraw(cmdBuffer, 6u, 1u, 0u, 0u);
}

template <typename RenderpassSubpass>
void MultisampleRenderPassTestInstance::submit(void)
{
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> commandBuffer(
        allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vkd, *commandBuffer);

    // Memory barriers between previous copies and rendering
    {
        std::vector<VkImageMemoryBarrier> barriers;

        for (size_t dstNdx = 0; dstNdx < m_singlesampleImages.size(); dstNdx++)
        {
            const VkImageMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,

                VK_ACCESS_TRANSFER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,

                **m_singlesampleImages[dstNdx],
                {VK_IMAGE_ASPECT_COLOR_BIT, m_renderLevel, 1u, m_baseLayer, m_layerCount}};

            barriers.push_back(barrier);
        }

        vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, nullptr, 0u, nullptr,
                               (uint32_t)barriers.size(), &barriers[0]);
    }

    VkRect2D renderArea = makeRect2D(m_width, m_height);
    const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(nullptr, VK_SUBPASS_CONTENTS_INLINE);
    const VkRenderPassBeginInfo beginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                             nullptr,

                                             m_testCompatibility ? *m_renderPassCompatible : *m_renderPass,
                                             *m_framebuffer,
                                             renderArea,

                                             0u,
                                             nullptr};
    RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);

    drawCommands(*commandBuffer, *m_renderPipeline, *m_renderPipelineLayout);

    const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(nullptr);
    RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

    for (size_t dstNdx = 0; dstNdx < m_singlesampleImages.size(); dstNdx++)
    {
        // assume that buffer(s) have enough memory to store desired amount of mipmaps
        copyImageToBuffer(vkd, *commandBuffer, **m_singlesampleImages[dstNdx], **m_buffers[dstNdx], m_format,
                          tcu::IVec2((1u << m_renderLevel) * m_width, (1u << m_renderLevel) * m_height), m_renderLevel,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, totalLayers());
    }

    endCommandBuffer(vkd, *commandBuffer);

    submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *commandBuffer);

    for (size_t memoryBufferNdx = 0; memoryBufferNdx < m_bufferMemory.size(); memoryBufferNdx++)
        invalidateMappedMemoryRange(vkd, device, m_bufferMemory[memoryBufferNdx]->getMemory(), 0u, VK_WHOLE_SIZE);
}

void MultisampleRenderPassTestInstance::submitDynamicRendering(void)
{
#ifndef CTS_USES_VULKANSC

    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    Move<VkCommandBuffer> secCmdBuffer;

    // Memory barriers between previous copies and rendering
    std::vector<VkImageMemoryBarrier> singlesampleImageBarriers(
        m_singlesampleImages.size(), {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      nullptr,

                                      VK_ACCESS_TRANSFER_READ_BIT,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

                                      VK_QUEUE_FAMILY_IGNORED,
                                      VK_QUEUE_FAMILY_IGNORED,

                                      VK_NULL_HANDLE,
                                      {VK_IMAGE_ASPECT_COLOR_BIT, m_renderLevel, 1u, 0u, totalLayers()}});
    for (size_t dstNdx = 0; dstNdx < m_singlesampleImages.size(); dstNdx++)
        singlesampleImageBarriers[dstNdx].image = **m_singlesampleImages[dstNdx];

    // Memory barriers to set multisample image layout to COLOR_ATTACHMENT_OPTIMAL
    std::vector<VkImageMemoryBarrier> multisampleImageBarriers(m_multisampleImages.size(),
                                                               {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                                nullptr,

                                                                0,
                                                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

                                                                VK_IMAGE_LAYOUT_UNDEFINED,
                                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

                                                                VK_QUEUE_FAMILY_IGNORED,
                                                                VK_QUEUE_FAMILY_IGNORED,

                                                                VK_NULL_HANDLE,
                                                                {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_layerCount}});
    for (size_t dstNdx = 0; dstNdx < m_multisampleImages.size(); dstNdx++)
        multisampleImageBarriers[dstNdx].image = **m_multisampleImages[dstNdx];

    VkRect2D renderArea           = makeRect2D(m_width, m_height);
    const VkClearValue clearValue = makeClearValueColor({0.0f, 0.0f, 0.0f, 1.0f});
    std::vector<vk::VkRenderingAttachmentInfoKHR> colorAttachments(
        m_attachmentsCount,
        {
            vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            VK_NULL_HANDLE,                                      // VkImageView imageView;
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout imageLayout;
            vk::VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
            VK_NULL_HANDLE,                                      // VkImageView resolveImageView;
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout resolveImageLayout;
            vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,                 // VkAttachmentLoadOp loadOp;
            vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                // VkAttachmentStoreOp storeOp;
            clearValue                                           // VkClearValue clearValue;
        });

    for (uint32_t i = 0; i < m_attachmentsCount; ++i)
    {
        colorAttachments[i].imageView        = **m_multisampleImageViews[i];
        colorAttachments[i].resolveImageView = **m_singlesampleImageViews[i];
        if (isIntFormat(m_format) || isUintFormat(m_format))
            colorAttachments[i].resolveMode = vk::VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
        else
            colorAttachments[i].resolveMode = vk::VK_RESOLVE_MODE_AVERAGE_BIT;
    }

    vk::VkRenderingInfoKHR renderingInfo{
        vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        nullptr,
        0,                       // VkRenderingFlagsKHR flags;
        renderArea,              // VkRect2D renderArea;
        m_layerCount,            // uint32_t layerCount;
        0u,                      // uint32_t viewMask;
        m_attachmentsCount,      // uint32_t colorAttachmentCount;
        colorAttachments.data(), // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                 // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                 // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
    {
        secCmdBuffer = allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

        // record secondary command buffer
        beginSecondaryCmdBuffer(*secCmdBuffer);
        vkd.cmdBeginRendering(*secCmdBuffer, &renderingInfo);
        drawCommands(*secCmdBuffer, *m_renderPipeline, *m_renderPipelineLayout);
        vkd.cmdEndRendering(*secCmdBuffer);

        endCommandBuffer(vkd, *secCmdBuffer);

        // record primary command buffer
        beginCommandBuffer(vkd, *cmdBuffer);

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, nullptr, 0u, nullptr,
                               (uint32_t)singlesampleImageBarriers.size(), &singlesampleImageBarriers[0]);
        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, nullptr, 0u, nullptr,
                               (uint32_t)multisampleImageBarriers.size(), &multisampleImageBarriers[0]);

        vkd.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);
    }
    else
    {
        beginCommandBuffer(vkd, *cmdBuffer);

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, nullptr, 0u, nullptr,
                               (uint32_t)singlesampleImageBarriers.size(), &singlesampleImageBarriers[0]);
        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, nullptr, 0u, nullptr,
                               (uint32_t)multisampleImageBarriers.size(), &multisampleImageBarriers[0]);

        vkd.cmdBeginRendering(*cmdBuffer, &renderingInfo);
        drawCommands(*cmdBuffer, *m_renderPipeline, *m_renderPipelineLayout);
        vkd.cmdEndRendering(*cmdBuffer);
    }

    // Memory barriers to set single-sample image layout to TRANSFER_SRC_OPTIMAL
    {
        std::vector<VkImageMemoryBarrier> barriers;

        for (size_t dstNdx = 0; dstNdx < m_singlesampleImages.size(); dstNdx++)
        {
            const VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                  nullptr,

                                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                  VK_ACCESS_TRANSFER_READ_BIT,

                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  VK_QUEUE_FAMILY_IGNORED,

                                                  **m_singlesampleImages[dstNdx],
                                                  {VK_IMAGE_ASPECT_COLOR_BIT, m_renderLevel, 1u, 0u, totalLayers()}};

            barriers.push_back(barrier);
        }

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, (uint32_t)barriers.size(),
                               &barriers[0]);
    }

    for (size_t dstNdx = 0; dstNdx < m_singlesampleImages.size(); dstNdx++)
    {
        // assume that buffer(s) have enough memory to store desired amount of mipmaps
        copyImageToBuffer(vkd, *cmdBuffer, **m_singlesampleImages[dstNdx], **m_buffers[dstNdx], m_format,
                          tcu::IVec2((1u << m_renderLevel) * m_width, (1u << m_renderLevel) * m_height), m_renderLevel,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, totalLayers());
    }

    endCommandBuffer(vkd, *cmdBuffer);

    submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *cmdBuffer);

    for (size_t memoryBufferNdx = 0; memoryBufferNdx < m_bufferMemory.size(); memoryBufferNdx++)
        invalidateMappedMemoryRange(vkd, device, m_bufferMemory[memoryBufferNdx]->getMemory(), 0u, VK_WHOLE_SIZE);

#endif // CTS_USES_VULKANSC
}

void MultisampleRenderPassTestInstance::submitSwitch(const SharedGroupParams groupParams)
{
    switch (groupParams->renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        submit<RenderpassSubpass1>();
        break;
    case RENDERING_TYPE_RENDERPASS2:
        submit<RenderpassSubpass2>();
        break;
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        submitDynamicRendering();
        break;
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

void MultisampleRenderPassTestInstance::verify(void)
{
    const Vec4 errorColor(1.0f, 0.0f, 0.0f, 1.0f);
    const Vec4 okColor(0.0f, 1.0f, 0.0f, 1.0f);
    const tcu::TextureFormat format(mapVkFormat(m_format));
    const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(format.type));

    uint32_t offset(0u);
    uint32_t width((1u << m_renderLevel) * m_width);
    uint32_t height((1u << m_renderLevel) * m_height);
    uint32_t pixelSize(static_cast<uint32_t>(getPixelSize()));
    for (uint32_t level = 0; level < m_renderLevel; ++level)
    {
        offset += (width * height * pixelSize);
        height /= 2;
        width /= 2;
    }

    std::vector<tcu::ConstPixelBufferAccess> accesses;
    for (uint32_t attachmentIdx = 0; attachmentIdx < m_attachmentsCount; ++attachmentIdx)
    {
        void *const ptr = static_cast<uint8_t *>(m_bufferMemory[attachmentIdx]->getHostPtr()) + offset;
        accesses.push_back(tcu::ConstPixelBufferAccess(format, m_width, m_height, totalLayers(), ptr));
    }

    tcu::TextureLevel errorMask(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), m_width,
                                m_height, totalLayers());
    tcu::TestLog &log(m_context.getTestContext().getLog());

    switch (channelClass)
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    {
        const bool isAlphaOnly = isAlphaOnlyFormat(m_format);
        const int componentCount(isAlphaOnly ? 4 : tcu::getNumUsedChannels(format.order));
        bool isOk = true;
        float clearValue;
        float renderValue;

        switch (channelClass)
        {
        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            clearValue  = -1.0f;
            renderValue = 1.0f;
            break;

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            clearValue  = 0.0f;
            renderValue = 1.0f;
            break;

        default:
            clearValue  = 0.0f;
            renderValue = 0.0f;
            DE_FATAL("Unknown channel class");
        }

        for (uint32_t z = m_baseLayer; z < totalLayers(); z++)
            for (uint32_t y = 0; y < m_height; y++)
                for (uint32_t x = 0; x < m_width; x++)
                {
                    // Color has to be black if no samples were covered, white if all samples were covered or same in every attachment
                    const Vec4 firstColor(accesses[0].getPixel(x, y, z));
                    const Vec4 refColor(m_sampleMask == 0x0u ?
                                            Vec4((isAlphaOnly ? 0.0f : clearValue),
                                                 componentCount > 1 && !isAlphaOnly ? clearValue : 0.0f,
                                                 componentCount > 2 && !isAlphaOnly ? clearValue : 0.0f,
                                                 componentCount > 3 ? clearValue : 1.0f) :
                                        m_sampleMask == ((0x1u << m_sampleCount) - 1u) ?
                                            Vec4((isAlphaOnly ? 0.0f : renderValue),
                                                 componentCount > 1 && !isAlphaOnly ? renderValue : 0.0f,
                                                 componentCount > 2 && !isAlphaOnly ? renderValue : 0.0f,
                                                 componentCount > 3 ? renderValue : 1.0f) :
                                            firstColor);

                    errorMask.getAccess().setPixel(okColor, x, y, z);

                    for (size_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
                    {
                        const Vec4 color(accesses[attachmentNdx].getPixel(x, y, z));

                        if (refColor != color)
                        {
                            isOk = false;
                            errorMask.getAccess().setPixel(errorColor, x, y, z);
                            break;
                        }
                    }

                    {
                        const Vec4 old = m_sum.getAccess().getPixel(x, y, z);
                        m_sum.getAccess().setPixel(
                            old + (tcu::isSRGB(format) ? tcu::sRGBToLinear(firstColor) : firstColor), x, y, z);

                        const Vec4 oldSrgb = m_sumSrgb.getAccess().getPixel(x, y, z);
                        m_sumSrgb.getAccess().setPixel(oldSrgb + firstColor, x, y, z);
                    }
                }

        if (!isOk)
        {
            const std::string sectionName("ResolveVerifyWithMask" + de::toString(m_sampleMask));
            const tcu::ScopedLogSection section(log, sectionName, sectionName);

            for (size_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
                logImage(std::string("Attachment") + de::toString(attachmentNdx), accesses[attachmentNdx]);

            logImage("ErrorMask", errorMask.getAccess());

            if (m_sampleMask == 0x0u)
            {
                m_context.getTestContext().getLog() << tcu::TestLog::Message << "Empty sample mask didn't produce all "
                                                    << clearValue << " pixels" << tcu::TestLog::EndMessage;
                m_resultCollector.fail("Empty sample mask didn't produce correct pixel values");
            }
            else if (m_sampleMask == ((0x1u << m_sampleCount) - 1u))
            {
                m_context.getTestContext().getLog() << tcu::TestLog::Message << "Full sample mask didn't produce all "
                                                    << renderValue << " pixels" << tcu::TestLog::EndMessage;
                m_resultCollector.fail("Full sample mask didn't produce correct pixel values");
            }
            else
            {
                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "Resolve is inconsistent between attachments"
                    << tcu::TestLog::EndMessage;
                m_resultCollector.fail("Resolve is inconsistent between attachments");
            }
        }
        break;
    }

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
    {
        const int componentCount(tcu::getNumUsedChannels(format.order));
        const UVec4 bitDepth(tcu::getTextureFormatBitDepth(format).cast<uint32_t>());
        const UVec4 renderValue(tcu::select((UVec4(1u) << tcu::min(UVec4(8u), bitDepth)) - UVec4(1u),
                                            UVec4(0u, 0u, 0u, 1u),
                                            tcu::lessThan(IVec4(0, 1, 2, 3), IVec4(componentCount))));
        const UVec4 clearValue(
            tcu::select(UVec4(0u), UVec4(0u, 0u, 0u, 1u), tcu::lessThan(IVec4(0, 1, 2, 3), IVec4(componentCount))));
        bool unexpectedValues        = false;
        bool inconsistentComponents  = false;
        bool inconsistentAttachments = false;

        for (uint32_t z = m_baseLayer; z < totalLayers(); z++)
            for (uint32_t y = 0; y < m_height; y++)
                for (uint32_t x = 0; x < m_width; x++)
                {
                    // Color has to be all zeros if no samples were covered, all 255 if all samples were covered or consistent across all attachments
                    const UVec4 refColor(m_sampleMask == 0x0u ? clearValue :
                                         m_sampleMask == ((0x1u << m_sampleCount) - 1u) ?
                                                                renderValue :
                                                                accesses[0].getPixelUint(x, y, z));
                    bool isOk = true;

                    // If reference value was taken from first attachment, check that it is valid value i.e. clear or render value
                    if (m_sampleMask != 0x0u && m_sampleMask != ((0x1u << m_sampleCount) - 1u))
                    {
                        // Each component must be resolved same way
                        const BVec4 isRenderValue(refColor == renderValue);
                        const BVec4 isClearValue(refColor == clearValue);
                        const bool unexpectedValue(
                            tcu::anyNotEqual(tcu::logicalOr(isRenderValue, isClearValue), BVec4(true)));
                        const bool inconsistentComponent(
                            !(tcu::allEqual(isRenderValue, BVec4(true)) || tcu::allEqual(isClearValue, BVec4(true))));

                        unexpectedValues |= unexpectedValue;
                        inconsistentComponents |= inconsistentComponent;

                        if (unexpectedValue || inconsistentComponent)
                            isOk = false;
                    }

                    for (size_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
                    {
                        const UVec4 color(accesses[attachmentNdx].getPixelUint(x, y, z));

                        if (refColor != color)
                        {
                            isOk                    = false;
                            inconsistentAttachments = true;
                            break;
                        }
                    }

                    errorMask.getAccess().setPixel((isOk ? okColor : errorColor), x, y, z);
                }

        if (unexpectedValues || inconsistentComponents || inconsistentAttachments)
        {
            const std::string sectionName("ResolveVerifyWithMask" + de::toString(m_sampleMask));
            const tcu::ScopedLogSection section(log, sectionName, sectionName);

            for (size_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
                logImage(std::string("Attachment") + de::toString(attachmentNdx), accesses[attachmentNdx]);

            logImage("ErrorMask", errorMask.getAccess());

            if (m_sampleMask == 0x0u)
            {
                m_context.getTestContext().getLog() << tcu::TestLog::Message << "Empty sample mask didn't produce all "
                                                    << clearValue << " pixels" << tcu::TestLog::EndMessage;
                m_resultCollector.fail("Empty sample mask didn't produce correct pixels");
            }
            else if (m_sampleMask == ((0x1u << m_sampleCount) - 1u))
            {
                m_context.getTestContext().getLog() << tcu::TestLog::Message << "Full sample mask didn't produce all "
                                                    << renderValue << " pixels" << tcu::TestLog::EndMessage;
                m_resultCollector.fail("Full sample mask didn't produce correct pixels");
            }
            else
            {
                if (unexpectedValues)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Resolve produced unexpected values i.e. not " << clearValue
                        << " or " << renderValue << tcu::TestLog::EndMessage;
                    m_resultCollector.fail("Resolve produced unexpected values");
                }

                if (inconsistentComponents)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message
                        << "Different components of attachment were resolved to different values."
                        << tcu::TestLog::EndMessage;
                    m_resultCollector.fail("Different components of attachment were resolved to different values.");
                }

                if (inconsistentAttachments)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Different attachments were resolved to different values."
                        << tcu::TestLog::EndMessage;
                    m_resultCollector.fail("Different attachments were resolved to different values.");
                }
            }
        }
        break;
    }

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
    {
        const int componentCount(tcu::getNumUsedChannels(format.order));
        const IVec4 bitDepth(tcu::getTextureFormatBitDepth(format));
        const IVec4 renderValue(tcu::select((IVec4(1) << (tcu::min(IVec4(8), bitDepth) - IVec4(1))) - IVec4(1),
                                            IVec4(0, 0, 0, 1),
                                            tcu::lessThan(IVec4(0, 1, 2, 3), IVec4(componentCount))));
        const IVec4 clearValue(tcu::select(-(IVec4(1) << (tcu::min(IVec4(8), bitDepth) - IVec4(1))), IVec4(0, 0, 0, 1),
                                           tcu::lessThan(IVec4(0, 1, 2, 3), IVec4(componentCount))));
        bool unexpectedValues        = false;
        bool inconsistentComponents  = false;
        bool inconsistentAttachments = false;

        for (uint32_t z = m_baseLayer; z < totalLayers(); z++)
            for (uint32_t y = 0; y < m_height; y++)
                for (uint32_t x = 0; x < m_width; x++)
                {
                    // Color has to be all zeros if no samples were covered, all 255 if all samples were covered or consistent across all attachments
                    const IVec4 refColor(m_sampleMask == 0x0u ? clearValue :
                                         m_sampleMask == ((0x1u << m_sampleCount) - 1u) ?
                                                                renderValue :
                                                                accesses[0].getPixelInt(x, y, z));
                    bool isOk = true;

                    // If reference value was taken from first attachment, check that it is valid value i.e. clear or render value
                    if (m_sampleMask != 0x0u && m_sampleMask != ((0x1u << m_sampleCount) - 1u))
                    {
                        // Each component must be resolved same way
                        const BVec4 isRenderValue(refColor == renderValue);
                        const BVec4 isClearValue(refColor == clearValue);
                        const bool unexpectedValue(
                            tcu::anyNotEqual(tcu::logicalOr(isRenderValue, isClearValue), BVec4(true)));
                        const bool inconsistentComponent(
                            !(tcu::allEqual(isRenderValue, BVec4(true)) || tcu::allEqual(isClearValue, BVec4(true))));

                        unexpectedValues |= unexpectedValue;
                        inconsistentComponents |= inconsistentComponent;

                        if (unexpectedValue || inconsistentComponent)
                            isOk = false;
                    }

                    for (size_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
                    {
                        const IVec4 color(accesses[attachmentNdx].getPixelInt(x, y, z));

                        if (refColor != color)
                        {
                            isOk                    = false;
                            inconsistentAttachments = true;
                            break;
                        }
                    }

                    errorMask.getAccess().setPixel((isOk ? okColor : errorColor), x, y, z);
                }

        if (unexpectedValues || inconsistentComponents || inconsistentAttachments)
        {
            const std::string sectionName("ResolveVerifyWithMask" + de::toString(m_sampleMask));
            const tcu::ScopedLogSection section(log, sectionName, sectionName);

            for (size_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
                logImage(std::string("Attachment") + de::toString(attachmentNdx), accesses[attachmentNdx]);

            logImage("ErrorMask", errorMask.getAccess());

            if (m_sampleMask == 0x0u)
            {
                m_context.getTestContext().getLog() << tcu::TestLog::Message << "Empty sample mask didn't produce all "
                                                    << clearValue << " pixels" << tcu::TestLog::EndMessage;
                m_resultCollector.fail("Empty sample mask didn't produce correct pixels");
            }
            else if (m_sampleMask == ((0x1u << m_sampleCount) - 1u))
            {
                m_context.getTestContext().getLog() << tcu::TestLog::Message << "Full sample mask didn't produce all "
                                                    << renderValue << " pixels" << tcu::TestLog::EndMessage;
                m_resultCollector.fail("Full sample mask didn't produce correct pixels");
            }
            else
            {
                if (unexpectedValues)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Resolve produced unexpected values i.e. not " << clearValue
                        << " or " << renderValue << tcu::TestLog::EndMessage;
                    m_resultCollector.fail("Resolve produced unexpected values");
                }

                if (inconsistentComponents)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message
                        << "Different components of attachment were resolved to different values."
                        << tcu::TestLog::EndMessage;
                    m_resultCollector.fail("Different components of attachment were resolved to different values.");
                }

                if (inconsistentAttachments)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Different attachments were resolved to different values."
                        << tcu::TestLog::EndMessage;
                    m_resultCollector.fail("Different attachments were resolved to different values.");
                }
            }
        }
        break;
    }

    default:
        DE_FATAL("Unknown channel class");
    }
}

tcu::TestStatus MultisampleRenderPassTestInstance::iterate(void)
{
    if (m_sampleMask == 0u)
    {
        const tcu::TextureFormat format(mapVkFormat(m_format));
        const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(format.type));
        tcu::TestLog &log(m_context.getTestContext().getLog());

        switch (channelClass)
        {
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            log << TestLog::Message
                << "Clearing target to zero and rendering 255 pixels with every possible sample mask"
                << TestLog::EndMessage;
            break;

        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            log << TestLog::Message
                << "Clearing target to -128 and rendering 127 pixels with every possible sample mask"
                << TestLog::EndMessage;
            break;

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            log << TestLog::Message
                << "Clearing target to black and rendering white pixels with every possible sample mask"
                << TestLog::EndMessage;
            break;

        default:
            DE_FATAL("Unknown channel class");
        }
    }

    submitSwitch(m_groupParams);
    verify();

    if (m_sampleMask == ((0x1u << m_sampleCount) - 1u))
    {
        const tcu::TextureFormat format(mapVkFormat(m_format));
        const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(format.type));
        const Vec4 threshold(getFormatThreshold());
        tcu::TestLog &log(m_context.getTestContext().getLog());

        if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
            channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT ||
            channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
        {
            const bool isAlphaOnly = isAlphaOnlyFormat(m_format);
            const int componentCount(isAlphaOnly ? 4 : tcu::getNumUsedChannels(format.order));
            const Vec4 errorColor(1.0f, 0.0f, 0.0f, 1.0f);
            const Vec4 okColor(0.0f, 1.0f, 0.0f, 1.0f);
            tcu::TextureLevel errorMask(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8),
                                        m_width, m_height, totalLayers());
            bool isOk = true;
            Vec4 maxDiff(0.0f);
            Vec4 expectedAverage;

            switch (channelClass)
            {
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            {
                expectedAverage =
                    Vec4((isAlphaOnly ? 0.0f : 0.5f), componentCount > 1 && !isAlphaOnly ? 0.5f : 0.0f,
                         componentCount > 2 && !isAlphaOnly ? 0.5f : 0.0f, componentCount > 3 ? 0.5f : 1.0f);
                break;
            }

            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            {
                expectedAverage = Vec4(0.0f, 0.0f, 0.0f, componentCount > 3 ? 0.0f : 1.0f);
                break;
            }

            default:
                DE_FATAL("Unknown channel class");
            }

            for (uint32_t z = m_baseLayer; z < totalLayers(); z++)
                for (uint32_t y = 0; y < m_height; y++)
                    for (uint32_t x = 0; x < m_width; x++)
                    {
                        const Vec4 sum(m_sum.getAccess().getPixel(x, y, z));
                        const Vec4 average(sum / Vec4((float)(0x1u << m_sampleCount)));
                        const Vec4 diff(tcu::abs(average - expectedAverage));

                        m_sum.getAccess().setPixel(average, x, y, z);
                        errorMask.getAccess().setPixel(okColor, x, y, z);

                        bool failThreshold;

                        if (!tcu::isSRGB(format))
                        {
                            failThreshold = (diff[0] > threshold.x() || diff[1] > threshold.y() ||
                                             diff[2] > threshold.z() || diff[3] > threshold.w());
                        }
                        else
                        {
                            const Vec4 sumSrgb(m_sumSrgb.getAccess().getPixel(x, y, z));
                            const Vec4 averageSrgb(sumSrgb / Vec4((float)(0x1u << m_sampleCount)));
                            const Vec4 diffSrgb(tcu::abs(averageSrgb - expectedAverage));

                            m_sumSrgb.getAccess().setPixel(averageSrgb, x, y, z);

                            // Spec doesn't restrict implementation to downsample in linear color space. So, comparing both non linear and
                            // linear diff's in case of srgb formats.
                            failThreshold = ((diff[0] > threshold.x() || diff[1] > threshold.y() ||
                                              diff[2] > threshold.z() || diff[3] > threshold.w()) &&
                                             (diffSrgb[0] > threshold.x() || diffSrgb[1] > threshold.y() ||
                                              diffSrgb[2] > threshold.z() || diffSrgb[3] > threshold.w()));
                        }

                        if (failThreshold)
                        {
                            isOk    = false;
                            maxDiff = tcu::max(maxDiff, diff);
                            errorMask.getAccess().setPixel(errorColor, x, y, z);
                        }
                    }

            log << TestLog::Image("Average resolved values in attachment 0", "Average resolved values in attachment 0",
                                  m_sum);

            if (!isOk)
            {
                std::stringstream message;

                m_context.getTestContext().getLog() << tcu::LogImage("ErrorMask", "ErrorMask", errorMask.getAccess());

                message << "Average resolved values differ from expected average values by more than ";

                switch (componentCount)
                {
                case 1:
                    message << threshold.x();
                    break;
                case 2:
                    message << "vec2" << Vec2(threshold.x(), threshold.y());
                    break;
                case 3:
                    message << "vec3" << Vec3(threshold.x(), threshold.y(), threshold.z());
                    break;
                default:
                    message << "vec4" << threshold;
                }

                message << ". Max diff " << maxDiff;
                log << TestLog::Message << message.str() << TestLog::EndMessage;

                m_resultCollector.fail("Average resolved values differ from expected average values");
            }
        }

        return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
    }
    else
    {
        m_sampleMask++;
        return tcu::TestStatus::incomplete();
    }
}

template <typename RenderPassTrait>
Move<VkRenderPass> MultisampleRenderPassTestInstance::createRenderPass(bool usedResolveAttachment)
{
    // make name for RenderPass1Trait or RenderPass2Trait shorter
    typedef RenderPassTrait RPT;
    typedef typename RPT::AttDesc AttDesc;
    typedef typename RPT::AttRef AttRef;
    typedef typename RPT::SubpassDesc SubpassDesc;
    typedef typename RPT::RenderPassCreateInfo RenderPassCreateInfo;

    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();
    std::vector<AttDesc> attachments;
    std::vector<AttRef> colorAttachmentRefs;
    std::vector<AttRef> resolveAttachmentRefs;

    for (size_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
    {
        {
            const AttDesc multisampleAttachment(
                // sType
                nullptr,                                 // pNext
                0u,                                      // flags
                m_format,                                // format
                m_sampleCount,                           // samples
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // loadOp
                VK_ATTACHMENT_STORE_OP_DONT_CARE,        // storeOp
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // stencilLoadOp
                VK_ATTACHMENT_STORE_OP_DONT_CARE,        // stencilStoreOp
                VK_IMAGE_LAYOUT_UNDEFINED,               // initialLayout
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // finalLayout
            );
            const AttRef attachmentRef(
                // sType
                nullptr,                                  // pNext
                (uint32_t)attachments.size(),             // attachment
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // layout
                0u                                        // aspectMask
            );
            colorAttachmentRefs.push_back(attachmentRef);
            attachments.push_back(multisampleAttachment);
        }
        {
            const AttDesc singlesampleAttachment(
                // sType
                nullptr,                             // pNext
                0u,                                  // flags
                m_format,                            // format
                VK_SAMPLE_COUNT_1_BIT,               // samples
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,     // loadOp
                VK_ATTACHMENT_STORE_OP_STORE,        // storeOp
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,     // stencilLoadOp
                VK_ATTACHMENT_STORE_OP_DONT_CARE,    // stencilStoreOp
                VK_IMAGE_LAYOUT_UNDEFINED,           // initialLayout
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL // finalLayout
            );
            const auto attachmentId =
                (usedResolveAttachment ? static_cast<uint32_t>(attachments.size()) : VK_ATTACHMENT_UNUSED);
            const AttRef attachmentRef(
                // sType
                nullptr,                                  // pNext
                attachmentId,                             // attachment
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // layout
                0u                                        // aspectMask
            );
            resolveAttachmentRefs.push_back(attachmentRef);
            attachments.push_back(singlesampleAttachment);
        }
    }

    DE_ASSERT(colorAttachmentRefs.size() == resolveAttachmentRefs.size());
    DE_ASSERT(attachments.size() == colorAttachmentRefs.size() + resolveAttachmentRefs.size());

    const SubpassDesc subpass(
        // sType
        nullptr,                              // pNext
        (VkSubpassDescriptionFlags)0,         // flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,      // pipelineBindPoint
        0u,                                   // viewMask
        0u,                                   // inputAttachmentCount
        nullptr,                              // pInputAttachments
        (uint32_t)colorAttachmentRefs.size(), // colorAttachmentCount
        &colorAttachmentRefs[0],              // pColorAttachments
        &resolveAttachmentRefs[0],            // pResolveAttachments
        nullptr,                              // pDepthStencilAttachment
        0u,                                   // preserveAttachmentCount
        nullptr                               // pPreserveAttachments
    );
    const RenderPassCreateInfo renderPassCreator(
        // sType
        nullptr,                      // pNext
        (VkRenderPassCreateFlags)0u,  // flags
        (uint32_t)attachments.size(), // attachmentCount
        &attachments[0],              // pAttachments
        1u,                           // subpassCount
        &subpass,                     // pSubpasses
        0u,                           // dependencyCount
        nullptr,                      // pDependencies
        0u,                           // correlatedViewMaskCount
        nullptr                       // pCorrelatedViewMasks
    );

    return renderPassCreator.createRenderPass(vkd, device);
}

Move<VkRenderPass> MultisampleRenderPassTestInstance::createRenderPassSwitch(bool usedResolveAttachment)
{
    switch (m_groupParams->renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        return createRenderPass<RenderPass1Trait>(usedResolveAttachment);
    case RENDERING_TYPE_RENDERPASS2:
        return createRenderPass<RenderPass2Trait>(usedResolveAttachment);
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        return Move<VkRenderPass>();
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

Move<VkRenderPass> MultisampleRenderPassTestInstance::createRenderPassCompatible(void)
{
    if (m_testCompatibility)
    {
        // The compatible render pass is always created with a used resolve attachment.
        return createRenderPassSwitch(true);
    }
    else
    {
        return {};
    }
}

Move<VkPipelineLayout> MultisampleRenderPassTestInstance::createRenderPipelineLayout(void)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();

    const VkPushConstantRange pushConstant      = {VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 4u};
    const VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                   nullptr,
                                                   (vk::VkPipelineLayoutCreateFlags)0,

                                                   0u,
                                                   nullptr,

                                                   1u,
                                                   &pushConstant};

    return createPipelineLayout(vkd, device, &createInfo);
}

Move<VkPipeline> MultisampleRenderPassTestInstance::createRenderPipeline(void)
{
    const DeviceInterface &vkd                   = m_context.getDeviceInterface();
    VkDevice device                              = m_context.getDevice();
    const vk::BinaryCollection &binaryCollection = m_context.getBinaryCollection();
    const Unique<VkShaderModule> vertexShaderModule(
        createShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u));
    const Unique<VkShaderModule> fragmentShaderModule(
        createShaderModule(vkd, device, binaryCollection.get("quad-frag"), 0u));
    const Move<VkShaderModule> geometryShaderModule(
        m_layerCount == 1 ? Move<VkShaderModule>() : createShaderModule(vkd, device, binaryCollection.get("geom"), 0u));
    // Disable blending
    const VkPipelineColorBlendAttachmentState attachmentBlendState{
        VK_FALSE,
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    std::vector<VkPipelineColorBlendAttachmentState> attachmentBlendStates(m_attachmentsCount, attachmentBlendState);
    const VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineVertexInputStateCreateFlags)0u,

        0u,
        nullptr,

        0u,
        nullptr};
    const tcu::UVec2 renderArea(m_width, m_height);
    const std::vector<VkViewport> viewports(1, makeViewport(renderArea));
    const std::vector<VkRect2D> scissors(1, makeRect2D(renderArea));

    const VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineMultisampleStateCreateFlags)0u,

        sampleCountBitFromSampleCount(m_sampleCount),
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineDepthStencilStateCreateFlags)0u,

        VK_FALSE,
        VK_TRUE,
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        VK_TRUE,
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, ~0u, ~0u,
         0xFFu / (m_sampleCount + 1)},
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, ~0u, ~0u,
         0xFFu / (m_sampleCount + 1)},

        0.0f,
        1.0f};
    const VkPipelineColorBlendStateCreateInfo blendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                            nullptr,
                                                            (VkPipelineColorBlendStateCreateFlags)0u,

                                                            VK_FALSE,
                                                            VK_LOGIC_OP_COPY,
                                                            uint32_t(attachmentBlendStates.size()),
                                                            &attachmentBlendStates[0],
                                                            {0.0f, 0.0f, 0.0f, 0.0f}};

    void *pNext = nullptr;

#ifndef CTS_USES_VULKANSC
    std::vector<VkFormat> attachmentFormats(m_attachmentsCount, m_format);
    VkPipelineRenderingCreateInfoKHR renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                         nullptr,
                                                         0u,
                                                         m_attachmentsCount,
                                                         attachmentFormats.data(),
                                                         VK_FORMAT_UNDEFINED,
                                                         VK_FORMAT_UNDEFINED};
    if (m_groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

    return makeGraphicsPipeline(
        vkd,                     // const DeviceInterface&                        vk
        device,                  // const VkDevice                                device
        *m_renderPipelineLayout, // const VkPipelineLayout                        pipelineLayout
        *vertexShaderModule,     // const VkShaderModule                          vertexShaderModule
        VK_NULL_HANDLE,          // const VkShaderModule                          tessellationControlShaderModule
        VK_NULL_HANDLE,          // const VkShaderModule                          tessellationEvalShaderModule
        m_layerCount != 1 ? *geometryShaderModule :
                            VK_NULL_HANDLE,  // const VkShaderModule                          geometryShaderModule
        *fragmentShaderModule,               // const VkShaderModule                          fragmentShaderModule
        *m_renderPass,                       // const VkRenderPass                            renderPass
        viewports,                           // const std::vector<VkViewport>&                viewports
        scissors,                            // const std::vector<VkRect2D>&                  scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
        0u,                                  // const uint32_t                                subpass
        0u,                                  // const uint32_t                                patchControlPoints
        &vertexInputState,                   // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
        nullptr,            // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
        &multisampleState,  // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
        &depthStencilState, // const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
        &blendState,        // const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
        nullptr,            // const VkPipelineDynamicStateCreateInfo*       dynamicStateCreateInfo
        pNext);             // const void*                                   pNext
}

#ifndef CTS_USES_VULKANSC
void MultisampleRenderPassTestInstance::beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    std::vector<VkFormat> formats(m_attachmentsCount, m_format);

    VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        nullptr,                                                         // const void* pNext;
        0u,                                                              // VkRenderingFlagsKHR flags;
        0u,                                                              // uint32_t viewMask;
        m_attachmentsCount,                                              // uint32_t colorAttachmentCount;
        formats.data(),                                                  // const VkFormat* pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        m_sampleCount,                                                   // VkSampleCountFlagBits rasterizationSamples;
    };

    const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);
    VkCommandBufferUsageFlags usageFlags                       = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    const VkCommandBufferBeginInfo commandBufBeginParams{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo};

    VK_CHECK(vkd.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

class MaxAttachmenstsRenderPassTestInstance : public MultisampleRenderPassTestBase
{
public:
    MaxAttachmenstsRenderPassTestInstance(Context &context, TestConfig config);
    ~MaxAttachmenstsRenderPassTestInstance(void) = default;

    tcu::TestStatus iterate(void);

private:
    template <typename RenderpassSubpass>
    void submit(void);
    void submitDynamicRendering(void);

#ifndef CTS_USES_VULKANSC
    void preRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void inbetweenRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
#endif // CTS_USES_VULKANSC
    void drawFirstSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void drawSecondSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void postRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);

    void submitSwitch(RenderingType renderingType);
    void verify(void);

    Move<VkDescriptorSetLayout> createDescriptorSetLayout(void);
    Move<VkDescriptorPool> createDescriptorPool(void);
    Move<VkDescriptorSet> createDescriptorSet(void);

    template <typename RenderPassTrait>
    Move<VkRenderPass> createRenderPass(void);
    Move<VkRenderPass> createRenderPassSwitch(const RenderingType renderingType);
    void createRenderPipeline(GraphicsPipelineWrapper &graphicsPipeline, bool secondSubpass);

private:
    const std::vector<VkImageSp> m_multisampleImages;
    const std::vector<AllocationSp> m_multisampleImageMemory;
    const std::vector<VkImageViewSp> m_multisampleImageViews;

    const std::vector<VkImageSp> m_singlesampleImages;
    const std::vector<AllocationSp> m_singlesampleImageMemory;
    const std::vector<VkImageViewSp> m_singlesampleImageViews;

    const Unique<VkDescriptorSetLayout> m_descriptorSetLayout;
    const Unique<VkDescriptorPool> m_descriptorPool;
    const Unique<VkDescriptorSet> m_descriptorSet;

    const Unique<VkRenderPass> m_renderPass;
    const Unique<VkFramebuffer> m_framebuffer;

    PipelineLayoutWrapper m_pipelineLayoutPass0;
    GraphicsPipelineWrapper m_pipelinePass0;
    PipelineLayoutWrapper m_pipelineLayoutPass1;
    GraphicsPipelineWrapper m_pipelinePass1;

    const std::vector<VkBufferSp> m_buffers;
    const std::vector<AllocationSp> m_bufferMemory;

    const Unique<VkCommandPool> m_commandPool;
    tcu::ResultCollector m_resultCollector;
};

MaxAttachmenstsRenderPassTestInstance::MaxAttachmenstsRenderPassTestInstance(Context &context, TestConfig config)
    : MultisampleRenderPassTestBase(context, config)

    , m_multisampleImages(createImages(m_sampleCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
    , m_multisampleImageMemory(createImageMemory(m_multisampleImages))
    , m_multisampleImageViews(createImageViews(m_multisampleImages))

    , m_singlesampleImages(createImages(VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    , m_singlesampleImageMemory(createImageMemory(m_singlesampleImages))
    , m_singlesampleImageViews(createImageViews(m_singlesampleImages, m_baseLayer))

    , m_descriptorSetLayout(createDescriptorSetLayout())
    , m_descriptorPool(createDescriptorPool())
    , m_descriptorSet(createDescriptorSet())

    , m_renderPass(createRenderPassSwitch(config.groupParams->renderingType))
    , m_framebuffer(createFramebuffer(m_multisampleImageViews, m_singlesampleImageViews, *m_renderPass))

    , m_pipelineLayoutPass0(config.groupParams->pipelineConstructionType, context.getDeviceInterface(),
                            context.getDevice())
    , m_pipelinePass0(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                      context.getDevice(), context.getDeviceExtensions(), m_groupParams->pipelineConstructionType)
    , m_pipelineLayoutPass1(config.groupParams->pipelineConstructionType, context.getDeviceInterface(),
                            context.getDevice(), *m_descriptorSetLayout)
    , m_pipelinePass1(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                      context.getDevice(), context.getDeviceExtensions(), m_groupParams->pipelineConstructionType)

    , m_buffers(createBuffers())
    , m_bufferMemory(createBufferMemory(m_buffers))

    , m_commandPool(createCommandPool(context.getDeviceInterface(), context.getDevice(),
                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
    createRenderPipeline(m_pipelinePass0, 0);
    createRenderPipeline(m_pipelinePass1, 1);
}

template <typename RenderpassSubpass>
void MaxAttachmenstsRenderPassTestInstance::submit(void)
{
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> commandBuffer(
        allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(nullptr, VK_SUBPASS_CONTENTS_INLINE);
    const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(nullptr);

    beginCommandBuffer(vkd, *commandBuffer);

    // Memory barriers between previous copies and rendering
    {
        std::vector<VkImageMemoryBarrier> barriers;

        for (size_t dstNdx = 0; dstNdx < m_singlesampleImages.size(); dstNdx++)
        {
            const VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                  nullptr,

                                                  VK_ACCESS_TRANSFER_READ_BIT,
                                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

                                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  VK_QUEUE_FAMILY_IGNORED,

                                                  **m_singlesampleImages[dstNdx],
                                                  {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, m_baseLayer, m_layerCount}};

            barriers.push_back(barrier);
        }

        vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, nullptr, 0u, nullptr,
                               (uint32_t)barriers.size(), &barriers[0]);
    }

    {
        const VkRenderPassBeginInfo beginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                 nullptr,

                                                 *m_renderPass,
                                                 *m_framebuffer,

                                                 {{0u, 0u}, {m_width, m_height}},

                                                 0u,
                                                 nullptr};
        RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
    }

    // Clear everything to black
    clearAttachments(*commandBuffer);

    // First subpass - render black samples
    drawFirstSubpass(vkd, *commandBuffer);

    // Second subpasss - merge attachments
    RenderpassSubpass::cmdNextSubpass(vkd, *commandBuffer, &subpassBeginInfo, &subpassEndInfo);
    drawSecondSubpass(vkd, *commandBuffer);

    RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

    postRenderCommands(vkd, *commandBuffer);

    endCommandBuffer(vkd, *commandBuffer);

    submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *commandBuffer);

    for (size_t memoryBufferNdx = 0; memoryBufferNdx < m_bufferMemory.size(); memoryBufferNdx++)
        invalidateMappedMemoryRange(vkd, device, m_bufferMemory[memoryBufferNdx]->getMemory(), 0u, VK_WHOLE_SIZE);
}

void MaxAttachmenstsRenderPassTestInstance::submitDynamicRendering()
{
#ifndef CTS_USES_VULKANSC
    const DeviceInterface &vk(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    Move<VkCommandBuffer> secCmdBuffers[2];

    const tcu::TextureFormat format(mapVkFormat(m_format));
    const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(format.type);
    const bool isIntClass                       = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER) ||
                            (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER);
    VkResolveModeFlagBits resolveMode = isIntClass ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT : VK_RESOLVE_MODE_AVERAGE_BIT;

    std::vector<VkRenderingAttachmentInfo> firstColorAttachments(
        m_multisampleImages.size(),
        {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            VK_NULL_HANDLE,                              // VkImageView imageView;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout imageLayout;
            resolveMode,                                 // VkResolveModeFlagBits resolveMode;
            VK_NULL_HANDLE,                              // VkImageView resolveImageView;
            m_inputImageReadLayout,                      // VkImageLayout resolveImageLayout;
            VK_ATTACHMENT_LOAD_OP_CLEAR,                 // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                // VkAttachmentStoreOp storeOp;
            getClearValue()                              // VkClearValue clearValue;
        });
    std::vector<VkRenderingAttachmentInfo> secondColorAttachments(m_multisampleImages.size(), firstColorAttachments[0]);
    for (size_t i = 0; i < m_multisampleImages.size(); ++i)
    {
        firstColorAttachments[i].imageView        = **m_multisampleImageViews[i];
        firstColorAttachments[i].resolveImageView = **m_singlesampleImageViews[i];

        secondColorAttachments[i].imageView   = **m_singlesampleImageViews[i];
        secondColorAttachments[i].imageLayout = m_inputImageReadLayout;
        secondColorAttachments[i].resolveMode = VK_RESOLVE_MODE_NONE;
        secondColorAttachments[i].loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    VkRenderingInfo firstRenderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        0,                                      // VkRenderingFlagsKHR flags;
        makeRect2D(m_width, m_height),          // VkRect2D renderArea;
        1u,                                     // uint32_t layerCount;
        0u,                                     // uint32_t viewMask;
        (uint32_t)firstColorAttachments.size(), // uint32_t colorAttachmentCount;
        firstColorAttachments.data(),           // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                                // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                                // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };
    VkRenderingInfo secondRenderingInfo   = firstRenderingInfo;
    secondRenderingInfo.pColorAttachments = secondColorAttachments.data();

    std::vector<uint32_t> colorAttachmentLocationsAndInputs(firstColorAttachments.size());
    std::iota(colorAttachmentLocationsAndInputs.begin(), colorAttachmentLocationsAndInputs.end(), 0);

    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo = initVulkanStructure();
    renderingAttachmentLocationInfo.colorAttachmentCount      = (uint32_t)colorAttachmentLocationsAndInputs.size();
    renderingAttachmentLocationInfo.pColorAttachmentLocations = colorAttachmentLocationsAndInputs.data();

    VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo = initVulkanStructure();
    renderingInputAttachmentIndexInfo.colorAttachmentCount         = (uint32_t)colorAttachmentLocationsAndInputs.size();
    renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices = colorAttachmentLocationsAndInputs.data();

    if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
    {
        secCmdBuffers[0] = allocateCommandBuffer(vk, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        secCmdBuffers[1] = allocateCommandBuffer(vk, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

        // record secondary command buffer for first subpass
        beginSecondaryCmdBuffer(vk, *secCmdBuffers[0], (uint32_t)m_multisampleImages.size(), m_sampleCount);
        vk.cmdBeginRendering(*secCmdBuffers[0], &firstRenderingInfo);
        drawFirstSubpass(vk, *secCmdBuffers[0]);
        vk.cmdEndRendering(*secCmdBuffers[0]);
        endCommandBuffer(vk, *secCmdBuffers[0]);

        // record secondary command buffer for second subpass
        beginSecondaryCmdBuffer(vk, *secCmdBuffers[1], (uint32_t)m_multisampleImages.size(), VK_SAMPLE_COUNT_1_BIT);
        vk.cmdBeginRendering(*secCmdBuffers[1], &secondRenderingInfo);
        vk.cmdSetRenderingAttachmentLocationsKHR(*secCmdBuffers[1], &renderingAttachmentLocationInfo);
        vk.cmdSetRenderingInputAttachmentIndicesKHR(*secCmdBuffers[1], &renderingInputAttachmentIndexInfo);
        drawSecondSubpass(vk, *secCmdBuffers[1]);
        vk.cmdEndRendering(*secCmdBuffers[1]);
        endCommandBuffer(vk, *secCmdBuffers[1]);

        renderingInputAttachmentIndexInfo.pNext = nullptr;

        // record primary command buffer
        beginCommandBuffer(vk, *cmdBuffer);
        preRenderCommands(vk, *cmdBuffer);
        vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffers[0]);
        inbetweenRenderCommands(vk, *cmdBuffer);
        vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffers[1]);
        postRenderCommands(vk, *cmdBuffer);
        endCommandBuffer(vk, *cmdBuffer);
    }
    else
    {
        beginCommandBuffer(vk, *cmdBuffer);

        preRenderCommands(vk, *cmdBuffer);

        // First dynamic render pass - render black samples
        vk.cmdBeginRendering(*cmdBuffer, &firstRenderingInfo);
        drawFirstSubpass(vk, *cmdBuffer);
        vk.cmdEndRendering(*cmdBuffer);

        inbetweenRenderCommands(vk, *cmdBuffer);

        // Second dynamic render pass - merge resolved attachments
        vk.cmdBeginRendering(*cmdBuffer, &secondRenderingInfo);
        vk.cmdSetRenderingAttachmentLocationsKHR(*cmdBuffer, &renderingAttachmentLocationInfo);
        vk.cmdSetRenderingInputAttachmentIndicesKHR(*cmdBuffer, &renderingInputAttachmentIndexInfo);
        drawSecondSubpass(vk, *cmdBuffer);
        vk.cmdEndRendering(*cmdBuffer);

        postRenderCommands(vk, *cmdBuffer);
        endCommandBuffer(vk, *cmdBuffer);
    }

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    for (size_t memoryBufferNdx = 0; memoryBufferNdx < m_bufferMemory.size(); memoryBufferNdx++)
        invalidateMappedMemoryRange(vk, device, m_bufferMemory[memoryBufferNdx]->getMemory(), 0u, VK_WHOLE_SIZE);
#endif
}

#ifndef CTS_USES_VULKANSC
void MaxAttachmenstsRenderPassTestInstance::preRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkImageSubresourceRange subresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_layerCount};
    std::vector<VkImageMemoryBarrier> barriers(
        m_multisampleImages.size() + m_singlesampleImages.size(),
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_NULL_HANDLE,
                               subresourceRange));

    for (size_t i = 0; i < m_multisampleImages.size(); ++i)
        barriers[i].image = **m_multisampleImages[i];
    for (size_t i = m_multisampleImages.size(); i < barriers.size(); ++i)
    {
        barriers[i].image     = **m_singlesampleImages[i - m_multisampleImages.size()];
        barriers[i].newLayout = m_inputImageReadLayout;
    }

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
                          0u, nullptr, 0u, nullptr, (uint32_t)barriers.size(), barriers.data());
}

void MaxAttachmenstsRenderPassTestInstance::inbetweenRenderCommands(const DeviceInterface &vk,
                                                                    VkCommandBuffer cmdBuffer)
{
    VkMemoryBarrier memoryBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1u, &memoryBarrier, 0u,
                          nullptr, 0, nullptr);
}
#endif // CTS_USES_VULKANSC

void MaxAttachmenstsRenderPassTestInstance::drawFirstSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelinePass0.getPipeline());
    vk.cmdDraw(cmdBuffer, 6u, 1u, 0u, 0u);
}

void MaxAttachmenstsRenderPassTestInstance::drawSecondSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelinePass1.getPipeline());
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutPass1, 0, 1u,
                             &*m_descriptorSet, 0, NULL);
    vk.cmdDraw(cmdBuffer, 6u, 1u, 0u, 0u);
}

void MaxAttachmenstsRenderPassTestInstance::postRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkImageSubresourceRange subresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_layerCount};

    // Memory barriers between rendering and copies
    std::vector<VkImageMemoryBarrier> imageBarriers(
        m_singlesampleImages.size(),
        makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                               m_inputImageReadLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_NULL_HANDLE,
                               subresourceRange));

    for (size_t dstNdx = 0; dstNdx < m_singlesampleImages.size(); dstNdx++)
        imageBarriers[dstNdx].image = **m_singlesampleImages[dstNdx];

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0u, nullptr, 0u, nullptr, (uint32_t)imageBarriers.size(), imageBarriers.data());

    // Copy image memory to buffers
    const VkBufferImageCopy region =
        makeBufferImageCopy({m_width, m_height, 1u}, {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, m_layerCount});
    for (size_t dstNdx = 0; dstNdx < m_singlesampleImages.size(); dstNdx++)
        vk.cmdCopyImageToBuffer(cmdBuffer, **m_singlesampleImages[dstNdx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                **m_buffers[dstNdx], 1u, &region);

    // Memory barriers between copies and host access
    std::vector<VkBufferMemoryBarrier> bufferBarriers(
        m_buffers.size(), makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_NULL_HANDLE,
                                                  0u, VK_WHOLE_SIZE));
    for (size_t i = 0u; i < bufferBarriers.size(); ++i)
        bufferBarriers[i].buffer = **m_buffers[i];

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr,
                          (uint32_t)bufferBarriers.size(), bufferBarriers.data(), 0u, nullptr);
}

void MaxAttachmenstsRenderPassTestInstance::submitSwitch(RenderingType renderingType)
{
    switch (renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        submit<RenderpassSubpass1>();
        break;
    case RENDERING_TYPE_RENDERPASS2:
        submit<RenderpassSubpass2>();
        break;
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        submitDynamicRendering();
        break;
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

template <typename VecType>
bool isValueAboveThreshold1(const VecType &vale, const VecType &threshold)
{
    return (vale[0] > threshold[0]);
}

template <typename VecType>
bool isValueAboveThreshold2(const VecType &vale, const VecType &threshold)
{
    return (vale[0] > threshold[0]) || (vale[1] > threshold[1]);
}

template <typename VecType>
bool isValueAboveThreshold3(const VecType &vale, const VecType &threshold)
{
    return (vale[0] > threshold[0]) || (vale[1] > threshold[1]) || (vale[2] > threshold[2]);
}

template <typename VecType>
bool isValueAboveThreshold4(const VecType &vale, const VecType &threshold)
{
    return (vale[0] > threshold[0]) || (vale[1] > threshold[1]) || (vale[2] > threshold[2]) || (vale[3] > threshold[3]);
}

void MaxAttachmenstsRenderPassTestInstance::verify(void)
{
    const Vec4 errorColor(1.0f, 0.0f, 0.0f, 1.0f);
    const Vec4 okColor(0.0f, 1.0f, 0.0f, 1.0f);
    const tcu::TextureFormat format(mapVkFormat(m_format));
    const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(format.type));
    const int componentCount(tcu::getNumUsedChannels(format.order));
    const int outputsCount = m_attachmentsCount / 2;

    DE_ASSERT((componentCount >= 0) && (componentCount < 5));

    std::vector<tcu::ConstPixelBufferAccess> accesses;
    for (int outputNdx = 0; outputNdx < outputsCount; ++outputNdx)
    {
        void *const ptr = m_bufferMemory[outputNdx]->getHostPtr();
        accesses.push_back(tcu::ConstPixelBufferAccess(format, m_width, m_height, 1, ptr));
    }

    tcu::TextureLevel errorMask(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), m_width,
                                m_height, outputsCount);
    tcu::TestLog &log(m_context.getTestContext().getLog());
    bool isOk = true;

    switch (channelClass)
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    {
        const Vec4 refColor(0.0f, 0.3f, 0.6f, 0.75f);
        const Vec4 threshold(getFormatThreshold());

        typedef bool (*ValueAboveThresholdFn)(const Vec4 &, const Vec4 &);
        ValueAboveThresholdFn componentToFnMap[4]   = {isValueAboveThreshold1<Vec4>, isValueAboveThreshold2<Vec4>,
                                                       isValueAboveThreshold3<Vec4>, isValueAboveThreshold4<Vec4>};
        ValueAboveThresholdFn isValueAboveThreshold = componentToFnMap[componentCount - 1];
        bool isSRGBFormat                           = tcu::isSRGB(format);

        for (int outputNdx = 0; outputNdx < outputsCount; outputNdx++)
            for (int y = 0; y < (int)m_height; y++)
                for (int x = 0; x < (int)m_width; x++)
                {
                    Vec4 color = accesses[outputNdx].getPixel(x, y);
                    if (isSRGBFormat)
                        color = tcu::sRGBToLinear(color);

                    const Vec4 diff(tcu::abs(color - refColor));

                    if (isValueAboveThreshold(diff, threshold))
                    {
                        isOk = false;
                        errorMask.getAccess().setPixel(errorColor, x, y, outputNdx);
                        break;
                    }
                    else
                        errorMask.getAccess().setPixel(okColor, x, y, outputNdx);
                }
        break;
    }

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
    {
        const UVec4 refColor(0, 48, 144, 189);
        UVec4 threshold(1, 1, 1, 1);

        if (m_format == VK_FORMAT_A2B10G10R10_UINT_PACK32)
            threshold[3] = 200;

        typedef bool (*ValueAboveThresholdFn)(const UVec4 &, const UVec4 &);
        ValueAboveThresholdFn componentToFnMap[4]   = {isValueAboveThreshold1<UVec4>, isValueAboveThreshold2<UVec4>,
                                                       isValueAboveThreshold3<UVec4>, isValueAboveThreshold4<UVec4>};
        ValueAboveThresholdFn isValueAboveThreshold = componentToFnMap[componentCount - 1];

        for (int outputNdx = 0; outputNdx < outputsCount; outputNdx++)
            for (int y = 0; y < (int)m_height; y++)
                for (int x = 0; x < (int)m_width; x++)
                {
                    const UVec4 color(accesses[outputNdx].getPixelUint(x, y));
                    const UVec4 diff(
                        std::abs(int(color.x()) - int(refColor.x())), std::abs(int(color.y()) - int(refColor.y())),
                        std::abs(int(color.z()) - int(refColor.z())), std::abs(int(color.w()) - int(refColor.w())));

                    if (isValueAboveThreshold(diff, threshold))
                    {
                        isOk = false;
                        errorMask.getAccess().setPixel(errorColor, x, y, outputNdx);
                        break;
                    }
                    else
                        errorMask.getAccess().setPixel(okColor, x, y, outputNdx);
                }
        break;
    }

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
    {
        const IVec4 refColor(0, 24, 75, 93);
        const IVec4 threshold(1, 1, 1, 1);

        typedef bool (*ValueAboveThresholdFn)(const IVec4 &, const IVec4 &);
        ValueAboveThresholdFn componentToFnMap[4]   = {isValueAboveThreshold1<IVec4>, isValueAboveThreshold2<IVec4>,
                                                       isValueAboveThreshold3<IVec4>, isValueAboveThreshold4<IVec4>};
        ValueAboveThresholdFn isValueAboveThreshold = componentToFnMap[componentCount - 1];

        for (int outputNdx = 0; outputNdx < outputsCount; outputNdx++)
            for (int y = 0; y < (int)m_height; y++)
                for (int x = 0; x < (int)m_width; x++)
                {
                    const IVec4 color(accesses[outputNdx].getPixelInt(x, y));
                    const IVec4 diff(std::abs(color.x() - refColor.x()), std::abs(color.y() - refColor.y()),
                                     std::abs(color.z() - refColor.z()), std::abs(color.w() - refColor.w()));

                    if (isValueAboveThreshold(diff, threshold))
                    {
                        isOk = false;
                        errorMask.getAccess().setPixel(errorColor, x, y, outputNdx);
                        break;
                    }
                    else
                        errorMask.getAccess().setPixel(okColor, x, y, outputNdx);
                }
        break;
    }

    default:
        DE_FATAL("Unknown channel class");
    }

    if (!isOk)
    {
        const std::string sectionName("MaxAttachmentsVerify");
        const tcu::ScopedLogSection section(log, sectionName, sectionName);

        logImage("ErrorMask", errorMask.getAccess());
        m_resultCollector.fail("Fail");
    }
}

tcu::TestStatus MaxAttachmenstsRenderPassTestInstance::iterate(void)
{
    submitSwitch(m_groupParams->renderingType);
    verify();

    return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

Move<VkDescriptorSetLayout> MaxAttachmenstsRenderPassTestInstance::createDescriptorSetLayout()
{
    const VkDescriptorSetLayoutBinding bindingTemplate = {
        0,                                   // binding
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // descriptorType
        1u,                                  // descriptorCount
        VK_SHADER_STAGE_FRAGMENT_BIT,        // stageFlags
        nullptr                              // pImmutableSamplers
    };

    std::vector<VkDescriptorSetLayoutBinding> bindings(m_attachmentsCount, bindingTemplate);
    for (uint32_t idx = 0; idx < m_attachmentsCount; ++idx)
        bindings[idx].binding = idx;

    const VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
        nullptr,                                             // pNext
        0u,                                                  // flags
        m_attachmentsCount,                                  // bindingCount
        &bindings[0]                                         // pBindings
    };

    return ::createDescriptorSetLayout(m_context.getDeviceInterface(), m_context.getDevice(), &createInfo);
}

Move<VkDescriptorPool> MaxAttachmenstsRenderPassTestInstance::createDescriptorPool()
{
    const VkDescriptorPoolSize size = {
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // type
        m_attachmentsCount                   // descriptorCount
    };

    const VkDescriptorPoolCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // sType
        nullptr,                                           // pNext
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // flags
        1u,                                                // maxSets
        1u,                                                // poolSizeCount
        &size                                              // pPoolSizes
    };

    return ::createDescriptorPool(m_context.getDeviceInterface(), m_context.getDevice(), &createInfo);
}

Move<VkDescriptorSet> MaxAttachmenstsRenderPassTestInstance::createDescriptorSet()
{
    const VkDescriptorSetAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // sType
        nullptr,                                        // pNext
        *m_descriptorPool,                              // descriptorPool
        1u,                                             // descriptorSetCount
        &*m_descriptorSetLayout                         // pSetLayouts
    };

    const vk::DeviceInterface &vkd      = m_context.getDeviceInterface();
    vk::VkDevice device                 = m_context.getDevice();
    Move<VkDescriptorSet> descriptorSet = allocateDescriptorSet(vkd, device, &allocateInfo);
    vector<VkDescriptorImageInfo> descriptorImageInfo(m_attachmentsCount);
    vector<VkWriteDescriptorSet> descriptorWrites(m_attachmentsCount);

    for (uint32_t idx = 0; idx < m_attachmentsCount; ++idx)
    {
        descriptorImageInfo[idx] = {
            VK_NULL_HANDLE,                  // VkSampler        sampler
            **m_singlesampleImageViews[idx], // VkImageView        imageView
            m_inputImageReadLayout           // VkImageLayout    imageLayout
        };

        descriptorWrites[idx] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                    sType
            nullptr,                                // const void*                        pNext
            *descriptorSet,                         // VkDescriptorSet                    dstSet
            (uint32_t)idx,                          // uint32_t                            dstBinding
            0u,                                     // uint32_t                            dstArrayElement
            1u,                                     // uint32_t                            descriptorCount
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,    // VkDescriptorType                    descriptorType
            &descriptorImageInfo[idx],              // const VkDescriptorImageInfo*        pImageInfo
            nullptr,                                // const VkDescriptorBufferInfo*    pBufferInfo
            nullptr                                 // const VkBufferView*                pTexelBufferView
        };
    }

    vkd.updateDescriptorSets(device, (uint32_t)descriptorWrites.size(), &descriptorWrites[0], 0u, nullptr);
    return descriptorSet;
}

template <typename RenderPassTrait>
Move<VkRenderPass> MaxAttachmenstsRenderPassTestInstance::createRenderPass(void)
{
    // make name for RenderPass1Trait or RenderPass2Trait shorter
    typedef RenderPassTrait RPT;

    typedef typename RPT::AttDesc AttDesc;
    typedef typename RPT::AttRef AttRef;
    typedef typename RPT::SubpassDep SubpassDep;
    typedef typename RPT::SubpassDesc SubpassDesc;
    typedef typename RPT::RenderPassCreateInfo RenderPassCreateInfo;

    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkDevice device            = m_context.getDevice();
    std::vector<AttDesc> attachments;
    std::vector<AttRef> sp0colorAttachmentRefs;
    std::vector<AttRef> sp0resolveAttachmentRefs;
    std::vector<AttRef> sp1inAttachmentRefs;
    std::vector<AttRef> sp1colorAttachmentRefs;

    for (size_t attachmentNdx = 0; attachmentNdx < m_attachmentsCount; attachmentNdx++)
    {
        // define first subpass outputs
        {
            const AttDesc multisampleAttachment(nullptr,                                 // pNext
                                                0u,                                      // flags
                                                m_format,                                // format
                                                m_sampleCount,                           // samples
                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // loadOp
                                                VK_ATTACHMENT_STORE_OP_STORE,            // storeOp
                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // stencilLoadOp
                                                VK_ATTACHMENT_STORE_OP_DONT_CARE,        // stencilStoreOp
                                                VK_IMAGE_LAYOUT_UNDEFINED,               // initialLayout
                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // finalLayout
            );
            const AttRef attachmentRef(nullptr,
                                       (uint32_t)attachments.size(),             // attachment
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // layout
                                       0u                                        // aspectMask
            );
            sp0colorAttachmentRefs.push_back(attachmentRef);
            attachments.push_back(multisampleAttachment);
        }
        // define first subpass resolve attachments
        {
            const AttDesc singlesampleAttachment(nullptr,                          // pNext
                                                 0u,                               // flags
                                                 m_format,                         // format
                                                 VK_SAMPLE_COUNT_1_BIT,            // samples
                                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // loadOp
                                                 VK_ATTACHMENT_STORE_OP_STORE,     // storeOp
                                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // stencilLoadOp
                                                 VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencilStoreOp
                                                 VK_IMAGE_LAYOUT_UNDEFINED,        // initialLayout
                                                 VK_IMAGE_LAYOUT_GENERAL           // finalLayout
            );
            const AttRef attachmentRef(nullptr,                      // pNext
                                       (uint32_t)attachments.size(), // attachment
                                       VK_IMAGE_LAYOUT_GENERAL,      // layout
                                       0u                            // aspectMask
            );
            sp0resolveAttachmentRefs.push_back(attachmentRef);
            attachments.push_back(singlesampleAttachment);
        }
        // define second subpass inputs
        {
            const AttRef attachmentRef(nullptr,                          // pNext
                                       (uint32_t)attachments.size() - 1, // attachment
                                       VK_IMAGE_LAYOUT_GENERAL,          // layout
                                       VK_IMAGE_ASPECT_COLOR_BIT         // aspectMask
            );
            sp1inAttachmentRefs.push_back(attachmentRef);
        }
        // define second subpass outputs - it merges pairs of
        // results that were produced by the first subpass
        if (attachmentNdx < (m_attachmentsCount / 2))
        {
            const AttRef colorAttachmentRef(nullptr,                          // pNext
                                            (uint32_t)attachments.size() - 1, // attachment
                                            VK_IMAGE_LAYOUT_GENERAL,          // layout
                                            0u                                // aspectMask
            );
            sp1colorAttachmentRefs.push_back(colorAttachmentRef);
        }
    }

    DE_ASSERT(sp0colorAttachmentRefs.size() == sp0resolveAttachmentRefs.size());
    DE_ASSERT(attachments.size() == sp0colorAttachmentRefs.size() + sp0resolveAttachmentRefs.size());

    {
        const SubpassDesc subpass0(
            // sType
            nullptr,                                 // pNext
            (VkSubpassDescriptionFlags)0,            // flags
            VK_PIPELINE_BIND_POINT_GRAPHICS,         // pipelineBindPoint
            0u,                                      // viewMask
            0u,                                      // inputAttachmentCount
            nullptr,                                 // pInputAttachments
            (uint32_t)sp0colorAttachmentRefs.size(), // colorAttachmentCount
            &sp0colorAttachmentRefs[0],              // pColorAttachments
            &sp0resolveAttachmentRefs[0],            // pResolveAttachments
            nullptr,                                 // pDepthStencilAttachment
            0u,                                      // preserveAttachmentCount
            nullptr                                  // pPreserveAttachments
        );
        const SubpassDesc subpass1(
            // sType
            nullptr,                                 // pNext
            (VkSubpassDescriptionFlags)0,            // flags
            VK_PIPELINE_BIND_POINT_GRAPHICS,         // pipelineBindPoint
            0u,                                      // viewMask
            (uint32_t)sp1inAttachmentRefs.size(),    // inputAttachmentCount
            &sp1inAttachmentRefs[0],                 // pInputAttachments
            (uint32_t)sp1colorAttachmentRefs.size(), // colorAttachmentCount
            &sp1colorAttachmentRefs[0],              // pColorAttachments
            nullptr,                                 // pResolveAttachments
            nullptr,                                 // pDepthStencilAttachment
            0u,                                      // preserveAttachmentCount
            nullptr                                  // pPreserveAttachments
        );
        SubpassDesc subpasses[] = {subpass0, subpass1};
        const SubpassDep subpassDependency(nullptr,                                       // pNext
                                           0u,                                            // srcSubpass
                                           1u,                                            // dstSubpass
                                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
                                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // dstStageMask
                                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccessMask
                                           VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,           // dstAccessMask
                                           0u,                                            // dependencyFlags
                                           0u                                             // viewOffset
        );
        const RenderPassCreateInfo renderPassCreator(
            // sType
            nullptr,                      // pNext
            (VkRenderPassCreateFlags)0u,  // flags
            (uint32_t)attachments.size(), // attachmentCount
            &attachments[0],              // pAttachments
            2u,                           // subpassCount
            subpasses,                    // pSubpasses
            1u,                           // dependencyCount
            &subpassDependency,           // pDependencies
            0u,                           // correlatedViewMaskCount
            nullptr                       // pCorrelatedViewMasks
        );

        return renderPassCreator.createRenderPass(vkd, device);
    }
}

Move<VkRenderPass> MaxAttachmenstsRenderPassTestInstance::createRenderPassSwitch(const RenderingType renderingType)
{
    switch (renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        return createRenderPass<RenderPass1Trait>();
    case RENDERING_TYPE_RENDERPASS2:
        return createRenderPass<RenderPass2Trait>();
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        return Move<VkRenderPass>();
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

void MaxAttachmenstsRenderPassTestInstance::createRenderPipeline(GraphicsPipelineWrapper &graphicsPipeline,
                                                                 bool secondSubpass)
{
    const DeviceInterface &vkd                   = m_context.getDeviceInterface();
    VkDevice device                              = m_context.getDevice();
    const vk::BinaryCollection &binaryCollection = m_context.getBinaryCollection();
    VkSampleCountFlagBits sampleCount            = sampleCountBitFromSampleCount(m_sampleCount);
    uint32_t blendStatesCount                    = m_attachmentsCount;
    std::string fragShaderNameBase               = "quad-frag-sp0-";

    if (secondSubpass)
    {
        sampleCount = VK_SAMPLE_COUNT_1_BIT;
        blendStatesCount /= 2;
        fragShaderNameBase = "quad-frag-sp1-";
    }

    if (*m_renderPass == VK_NULL_HANDLE)
        blendStatesCount = m_attachmentsCount;

    std::string fragShaderName = fragShaderNameBase + de::toString(m_attachmentsCount);
    ShaderWrapper vertexShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u);
    ShaderWrapper fragmentShaderModule(vkd, device, binaryCollection.get(fragShaderName), 0u);
    ShaderWrapper geometryShaderModule;

    if (m_layerCount != 1)
        geometryShaderModule = ShaderWrapper(vkd, device, binaryCollection.get("geom"), 0u);

    // Disable blending
    const VkPipelineColorBlendAttachmentState attachmentBlendState{
        VK_FALSE,
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    std::vector<VkPipelineColorBlendAttachmentState> attachmentBlendStates(blendStatesCount, attachmentBlendState);
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
    RenderingAttachmentLocationInfoWrapper renderingAttachmentLocationInfoWrapper;
    RenderingInputAttachmentIndexInfoWrapper renderingInputAttachmentIndexInfoWrapper;
    const tcu::UVec2 renderArea(m_width, m_height);
    const std::vector<VkViewport> viewports{makeViewport(renderArea)};
    const std::vector<VkRect2D> scissors{makeRect2D(renderArea)};

    const VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineMultisampleStateCreateFlags)0u,

        sampleCount,
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineDepthStencilStateCreateFlags)0u,

        VK_FALSE,
        VK_TRUE,
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        VK_TRUE,
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, ~0u, ~0u,
         0xFFu / (m_sampleCount + 1)},
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, ~0u, ~0u,
         0xFFu / (m_sampleCount + 1)},

        0.0f,
        1.0f};
    const VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                         nullptr,
                                                         (VkPipelineColorBlendStateCreateFlags)0u,

                                                         VK_FALSE,
                                                         VK_LOGIC_OP_COPY,
                                                         uint32_t(attachmentBlendStates.size()),
                                                         attachmentBlendStates.data(),
                                                         {0.0f, 0.0f, 0.0f, 0.0f}};

#ifndef CTS_USES_VULKANSC
    const std::vector<VkFormat> colorAttachmentFormats(m_attachmentsCount, m_format);
    VkPipelineRenderingCreateInfo renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                      nullptr,
                                                      0u,
                                                      (uint32_t)colorAttachmentFormats.size(),
                                                      colorAttachmentFormats.data(),
                                                      VK_FORMAT_UNDEFINED,
                                                      VK_FORMAT_UNDEFINED};

    std::vector<uint32_t> colorAttachmentLocationsAndInputs(colorAttachmentFormats.size());
    std::iota(colorAttachmentLocationsAndInputs.begin(), colorAttachmentLocationsAndInputs.end(), 0);

    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo = initVulkanStructure();
    renderingAttachmentLocationInfo.colorAttachmentCount                 = (uint32_t)colorAttachmentFormats.size();
    renderingAttachmentLocationInfo.pColorAttachmentLocations            = colorAttachmentLocationsAndInputs.data();

    VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo =
        initVulkanStructure(&renderingAttachmentLocationInfo);
    renderingInputAttachmentIndexInfo.colorAttachmentCount         = (uint32_t)colorAttachmentLocationsAndInputs.size();
    renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices = colorAttachmentLocationsAndInputs.data();

    if (*m_renderPass == VK_NULL_HANDLE)
    {
        renderingCreateInfoWrapper.ptr             = &renderingCreateInfo;
        renderingAttachmentLocationInfoWrapper.ptr = &renderingAttachmentLocationInfo;
        if (secondSubpass)
            renderingInputAttachmentIndexInfoWrapper.ptr = &renderingInputAttachmentIndexInfo;
    }
#endif // CTS_USES_VULKANSC

    PipelineLayoutWrapper &pipelineLayout(secondSubpass ? m_pipelineLayoutPass1 : m_pipelineLayoutPass0);
    graphicsPipeline.setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *m_renderPass, secondSubpass,
                                          vertexShaderModule, 0u, ShaderWrapper(), ShaderWrapper(),
                                          geometryShaderModule, nullptr, nullptr, renderingCreateInfoWrapper)
        .setupFragmentShaderState(pipelineLayout, *m_renderPass, secondSubpass, fragmentShaderModule,
                                  &depthStencilState, &multisampleState, 0, VK_NULL_HANDLE, {},
                                  renderingInputAttachmentIndexInfoWrapper)
        .setupFragmentOutputState(*m_renderPass, secondSubpass, &blendState, &multisampleState, VK_NULL_HANDLE, {},
                                  renderingAttachmentLocationInfoWrapper)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();
}

class MultisampleRenderPassResolveLevelTestInstance : public MultisampleRenderPassTestInstance
{
public:
    MultisampleRenderPassResolveLevelTestInstance(Context &context, TestConfig2 config);
    ~MultisampleRenderPassResolveLevelTestInstance(void) = default;
};

MultisampleRenderPassResolveLevelTestInstance::MultisampleRenderPassResolveLevelTestInstance(Context &context,
                                                                                             TestConfig2 config)
    : MultisampleRenderPassTestInstance(context, config, config.resolveLevel)
{
}

struct Programs
{
    void init(vk::SourceCollections &dst, TestConfig config) const
    {
        const tcu::TextureFormat format(mapVkFormat(config.format));
        const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(format.type));

        dst.glslSources.add("quad-vert") << glu::VertexSource(
            "#version 450\n"
            "out gl_PerVertex {\n"
            "\tvec4 gl_Position;\n"
            "};\n"
            "highp float;\n"
            "void main (void) {\n"
            "\tgl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
            "\t                   ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
            "}\n");

        if (config.layerCount > 1)
        {
            std::ostringstream src;

            src << "#version 450\n"
                << "highp float;\n"
                << "\n"
                << "layout(triangles) in;\n"
                << "layout(triangle_strip, max_vertices = " << 3 * 2 * config.layerCount << ") out;\n"
                << "\n"
                << "in gl_PerVertex {\n"
                << "    vec4 gl_Position;\n"
                << "} gl_in[];\n"
                << "\n"
                << "out gl_PerVertex {\n"
                << "    vec4 gl_Position;\n"
                << "};\n"
                << "\n"
                << "void main (void) {\n"
                << "    for (int layerNdx = 0; layerNdx < " << config.layerCount << "; ++layerNdx) {\n"
                << "        for(int vertexNdx = 0; vertexNdx < gl_in.length(); vertexNdx++) {\n"
                << "            gl_Position = gl_in[vertexNdx].gl_Position;\n"
                << "            gl_Layer    = layerNdx;\n"
                << "            EmitVertex();\n"
                << "        };\n"
                << "        EndPrimitive();\n"
                << "    };\n"
                << "}\n";

            dst.glslSources.add("geom") << glu::GeometrySource(src.str());
        }

        const tcu::StringTemplate genericLayoutTemplate(
            "layout(location = ${INDEX}) out ${TYPE_PREFIX}vec4 o_color${INDEX};\n");
        const tcu::StringTemplate genericBodyTemplate("\to_color${INDEX} = ${TYPE_PREFIX}vec4(${COLOR_VAL});\n");

        if (config.testType == RESOLVE || config.testType == COMPATIBILITY)
        {
            const tcu::StringTemplate fragTemplate("#version 450\n"
                                                   "layout(push_constant) uniform PushConstant {\n"
                                                   "\thighp uint sampleMask;\n"
                                                   "} pushConstants;\n"
                                                   "${LAYOUT}"
                                                   "void main (void)\n"
                                                   "{\n"
                                                   "${BODY}"
                                                   "}\n");

            std::map<std::string, std::string> parameters;
            switch (channelClass)
            {
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
                parameters["TYPE_PREFIX"] = "u";
                parameters["COLOR_VAL"]   = "255";
                break;

            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
                parameters["TYPE_PREFIX"] = "i";
                parameters["COLOR_VAL"]   = "127";
                break;

            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
                parameters["TYPE_PREFIX"] = "";
                parameters["COLOR_VAL"]   = "1.0";
                break;

            default:
                DE_FATAL("Unknown channel class");
            }

            std::string layoutDefinitions = "";
            std::string shaderBody        = "\tgl_SampleMask[0] = int(pushConstants.sampleMask);\n";

            for (uint32_t attIdx = 0; attIdx < config.attachmentCount; ++attIdx)
            {
                parameters["INDEX"] = de::toString(attIdx);
                layoutDefinitions += genericLayoutTemplate.specialize(parameters);
                shaderBody += genericBodyTemplate.specialize(parameters);
            }

            parameters["LAYOUT"] = layoutDefinitions;
            parameters["BODY"]   = shaderBody;
            dst.glslSources.add("quad-frag") << glu::FragmentSource(fragTemplate.specialize(parameters));
        }
        else // MAX_ATTACMENTS
        {
            const tcu::StringTemplate fragTemplate("#version 450\n"
                                                   "${LAYOUT}"
                                                   "void main (void)\n"
                                                   "{\n"
                                                   "${BODY}"
                                                   "}\n");

            std::map<std::string, std::string> parameters;
            switch (channelClass)
            {
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
                parameters["TYPE_PREFIX"] = "u";
                parameters["COLOR_VAL"]   = "0, 64, 192, 252";
                break;

            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
                parameters["TYPE_PREFIX"] = "i";
                parameters["COLOR_VAL"]   = "0, 32, 100, 124";
                break;

            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
                parameters["TYPE_PREFIX"] = "";
                parameters["COLOR_VAL"]   = "0.0, 0.4, 0.8, 1.0";
                break;

            default:
                DE_FATAL("Unknown channel class");
            }

            // parts of fragment shader for second subpass - Vulkan introduced a new uniform type and syntax to glsl for input attachments
            const tcu::StringTemplate subpassLayoutTemplate(
                "layout (input_attachment_index = ${INDEX}, set = 0, binding = ${INDEX}) uniform "
                "${TYPE_PREFIX}subpassInput i_color${INDEX};\n");
            const tcu::StringTemplate subpassFBodyTemplate(
                "\to_color${INDEX} = subpassLoad(i_color${INDEX})*0.5 + subpassLoad(i_color${MIX_INDEX})*0.25;\n");
            const tcu::StringTemplate subpassIBodyTemplate(
                "\to_color${INDEX} = subpassLoad(i_color${INDEX}) / 2 + subpassLoad(i_color${MIX_INDEX}) / 4;\n");

            bool selectIBody                               = isIntFormat(config.format) || isUintFormat(config.format);
            const tcu::StringTemplate &subpassBodyTemplate = selectIBody ? subpassIBodyTemplate : subpassFBodyTemplate;

            std::string sp0layoutDefinitions    = "";
            std::string sp0shaderBody           = "";
            std::string sp1inLayoutDefinitions  = "";
            std::string sp1outLayoutDefinitions = "";
            std::string sp1shaderBody           = "";

            uint32_t halfAttachments = config.attachmentCount / 2;
            for (uint32_t attIdx = 0; attIdx < config.attachmentCount; ++attIdx)
            {
                parameters["INDEX"] = de::toString(attIdx);

                sp0layoutDefinitions += genericLayoutTemplate.specialize(parameters);
                sp0shaderBody += genericBodyTemplate.specialize(parameters);

                sp1inLayoutDefinitions += subpassLayoutTemplate.specialize(parameters);
                if (attIdx < halfAttachments)
                {
                    // we are combining pairs of input attachments to produce half the number of outputs
                    parameters["MIX_INDEX"] = de::toString(halfAttachments + attIdx);
                    sp1outLayoutDefinitions += genericLayoutTemplate.specialize(parameters);
                    sp1shaderBody += subpassBodyTemplate.specialize(parameters);
                }
            }

            // construct fragment shaders for subpass1 and subpass2; note that there
            // is different shader definition depending on number of attachments
            std::string nameBase    = "quad-frag-sp";
            std::string namePostfix = de::toString(config.attachmentCount);
            parameters["LAYOUT"]    = sp0layoutDefinitions;
            parameters["BODY"]      = sp0shaderBody;
            dst.glslSources.add(nameBase + "0-" + namePostfix)
                << glu::FragmentSource(fragTemplate.specialize(parameters));
            parameters["LAYOUT"] = sp1inLayoutDefinitions + sp1outLayoutDefinitions;
            parameters["BODY"]   = sp1shaderBody;
            dst.glslSources.add(nameBase + "1-" + namePostfix)
                << glu::FragmentSource(fragTemplate.specialize(parameters));
        }
    }
};

template <class TestConfigType>
void checkSupport(Context &context, TestConfigType config)
{
#ifndef CTS_USES_VULKANSC
    if (config.format == VK_FORMAT_A8_UNORM_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

    if (config.layerCount > 1)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (config.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

    const InstanceInterface &vki                    = context.getInstanceInterface();
    vk::VkPhysicalDevice physicalDevice             = context.getPhysicalDevice();
    const vk::VkPhysicalDeviceProperties properties = vk::getPhysicalDeviceProperties(vki, physicalDevice);

    checkPipelineConstructionRequirements(vki, physicalDevice, config.groupParams->pipelineConstructionType);
    if (config.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
        if (config.testType == MAX_ATTACHMENTS)
        {
            context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");
            if (config.attachmentCount > properties.limits.maxColorAttachments)
                TCU_THROW(NotSupportedError, "Required number of color attachments not supported.");
        }
    }

#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().multisampleArrayImage &&
        (config.sampleCount != VK_SAMPLE_COUNT_1_BIT) && (config.layerCount != 1))
    {
        TCU_THROW(
            NotSupportedError,
            "VK_KHR_portability_subset: Implementation does not support image array with multiple samples per texel");
    }
#endif // CTS_USES_VULKANSC

    if (config.attachmentCount > properties.limits.maxColorAttachments)
        TCU_THROW(NotSupportedError, "Required number of color attachments not supported.");

    if (config.testType == MAX_ATTACHMENTS &&
        config.attachmentCount > properties.limits.maxPerStageDescriptorInputAttachments)
        TCU_THROW(NotSupportedError, "Required number of per stage descriptor input attachments not supported.");
}

std::string formatToName(VkFormat format)
{
    const std::string formatStr = de::toString(format);
    const std::string prefix    = "VK_FORMAT_";

    DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

    return de::toLower(formatStr.substr(prefix.length()));
}

void initTests(tcu::TestCaseGroup *group, const SharedGroupParams groupParams)
{
    static const VkFormat formats[] = {
        VK_FORMAT_R5G6B5_UNORM_PACK16,
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
#ifndef CTS_USES_VULKANSC
        VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_SINT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        VK_FORMAT_A8B8G8R8_SINT_PACK32,
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2B10G10R10_UINT_PACK32,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_SNORM,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_SINT,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
    };
    const uint32_t sampleCounts[]  = {2u, 4u, 8u};
    const uint32_t layerCounts[]   = {1u, 3u, 6u};
    const uint32_t resolveLevels[] = {2u, 3u, 4u};
    tcu::TestContext &testCtx(group->getTestContext());

    for (size_t layerCountNdx = 0; layerCountNdx < DE_LENGTH_OF_ARRAY(layerCounts); layerCountNdx++)
    {
        const uint32_t layerCount(layerCounts[layerCountNdx]);
        const std::string layerGroupName("layers_" + de::toString(layerCount));
        de::MovePtr<tcu::TestCaseGroup> layerGroup(new tcu::TestCaseGroup(testCtx, layerGroupName.c_str()));

        for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
        {
            const VkFormat format(formats[formatNdx]);
            const std::string formatName(formatToName(format));
            de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, formatName.c_str()));

            for (size_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
            {
                const uint32_t sampleCount(sampleCounts[sampleCountNdx]);

                // Skip this test as it is rather slow
                if (layerCount == 6 && sampleCount == 8)
                    continue;

                // Reduce number of tests for dynamic rendering cases where secondary command buffer is used
                if (groupParams->useSecondaryCmdBuffer && ((sampleCount > 2u) || (layerCount > 3u)))
                    continue;

                std::string testName("samples_" + de::toString(sampleCount));
                const TestConfig testConfig{RESOLVE, format, sampleCount, layerCount, 0, 4u, 32u, 32u, groupParams};

                // repeat only dynamic_rendering_local_read tests for GPL
                if (groupParams->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
                {
                    formatGroup->addChild(new InstanceFactory1WithSupport<MultisampleRenderPassTestInstance, TestConfig,
                                                                          FunctionSupport1<TestConfig>, Programs>(
                        testCtx, testName.c_str(), testConfig,
                        typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfig)));

                    const TestConfig testConfigBaseLayer{RESOLVE, format, sampleCount, layerCount, 1,
                                                         4u,      32u,    32u,         groupParams};
                    std::string testNameBaseLayer("samples_" + de::toString(sampleCount) + "_baseLayer1");

                    formatGroup->addChild(new InstanceFactory1WithSupport<MultisampleRenderPassTestInstance, TestConfig,
                                                                          FunctionSupport1<TestConfig>, Programs>(
                        testCtx, testNameBaseLayer.c_str(), testConfigBaseLayer,
                        typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfigBaseLayer)));

                    for (uint32_t resolveLevel : resolveLevels)
                    {
                        const TestConfig2 testConfig2(testConfig, resolveLevel);
                        std::string resolveLevelTestNameStr(testName + "_resolve_level_" + de::toString(resolveLevel));
                        const char *resolveLevelTestName = resolveLevelTestNameStr.c_str();

                        formatGroup->addChild(
                            new InstanceFactory1WithSupport<MultisampleRenderPassResolveLevelTestInstance, TestConfig2,
                                                            FunctionSupport1<TestConfig2>, Programs>(
                                testCtx, resolveLevelTestName, testConfig2,
                                typename FunctionSupport1<TestConfig2>::Args(checkSupport, testConfig2)));

                        // Reduce number of tests for dynamic rendering cases where secondary command buffer is used
                        if (groupParams->useSecondaryCmdBuffer)
                            break;
                    }
                }

                // MaxAttachmenstsRenderPassTest is ment to test extreme cases where applications might consume all available on-chip
                // memory. This is achieved by using maxColorAttachments attachments and two subpasses, but during test creation we
                // dont know what is the maximal number of attachments (spirv tools are not available on all platforms) so we cant
                // construct shaders during test execution. To be able to test this we need to execute tests for all available
                // numbers of attachments despite the fact that we are only interested in the maximal number; test construction code
                // assumes that the number of attachments is power of two
                if ((layerCount == 1) && (groupParams->useSecondaryCmdBuffer ==
                                          groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass))
                {
                    for (uint32_t power = 2; power < 5; ++power)
                    {
                        uint32_t attachmentCount = 1 << power;
                        std::string maxAttName   = "max_attachments_" + de::toString(attachmentCount) + "_" + testName;

                        TestConfig maxAttachmentsTestConfig      = testConfig;
                        maxAttachmentsTestConfig.testType        = MAX_ATTACHMENTS;
                        maxAttachmentsTestConfig.attachmentCount = attachmentCount;

                        formatGroup->addChild(
                            new InstanceFactory1WithSupport<MaxAttachmenstsRenderPassTestInstance, TestConfig,
                                                            FunctionSupport1<TestConfig>, Programs>(
                                testCtx, maxAttName.c_str(), maxAttachmentsTestConfig,
                                typename FunctionSupport1<TestConfig>::Args(checkSupport, maxAttachmentsTestConfig)));
                    }

                    if (groupParams->renderingType != RENDERING_TYPE_DYNAMIC_RENDERING)
                    {
                        std::string compatibilityTestName = "compatibility_" + testName;

                        TestConfig compatibilityTestConfig      = testConfig;
                        compatibilityTestConfig.testType        = COMPATIBILITY;
                        compatibilityTestConfig.attachmentCount = 1;

                        formatGroup->addChild(
                            new InstanceFactory1WithSupport<MultisampleRenderPassTestInstance, TestConfig,
                                                            FunctionSupport1<TestConfig>, Programs>(
                                testCtx, compatibilityTestName.c_str(), compatibilityTestConfig,
                                typename FunctionSupport1<TestConfig>::Args(checkSupport, compatibilityTestConfig)));
                    }
                }
            }

            if (layerCount == 1)
                group->addChild(formatGroup.release());
            else
                layerGroup->addChild(formatGroup.release());
        }

        if (layerCount != 1)
            group->addChild(layerGroup.release());
    }
}

} // namespace

tcu::TestCaseGroup *createRenderPassMultisampleResolveTests(tcu::TestContext &testCtx,
                                                            const renderpass::SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "multisample_resolve", initTests, groupParams);
}

} // namespace vkt
