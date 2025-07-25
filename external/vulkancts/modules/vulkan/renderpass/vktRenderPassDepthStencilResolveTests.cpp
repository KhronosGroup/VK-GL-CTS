/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 *//*!
 * \file
 * \brief VK_KHR_depth_stencil_resolve tests.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassDepthStencilResolveTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
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
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deMath.h"

#include <limits>
#include <map>

using namespace vk;

using tcu::TestLog;
using tcu::Vec4;

typedef de::SharedPtr<vk::Unique<VkImage>> VkImageSp;
typedef de::SharedPtr<vk::Unique<VkImageView>> VkImageViewSp;
typedef de::SharedPtr<vk::Unique<VkBuffer>> VkBufferSp;
typedef de::SharedPtr<vk::Unique<VkPipeline>> VkPipelineSp;
typedef de::SharedPtr<Allocation> AllocationSp;

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

VkImageAspectFlags aspectFlagsForFormat(VkFormat vkformat)
{
    const tcu::TextureFormat format(mapVkFormat(vkformat));
    VkImageAspectFlags aspectFlags =
        ((tcu::hasDepthComponent(format.order) ? static_cast<vk::VkImageAspectFlags>(vk::VK_IMAGE_ASPECT_DEPTH_BIT) :
                                                 0u) |
         (tcu::hasStencilComponent(format.order) ?
              static_cast<vk::VkImageAspectFlags>(vk::VK_IMAGE_ASPECT_STENCIL_BIT) :
              0u));
    return aspectFlags;
}

enum VerifyBuffer
{
    VB_DEPTH = 0,
    VB_STENCIL
};

struct TestConfig
{
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t imageLayers;
    uint32_t viewLayers;
    uint32_t resolveBaseLayer;
    VkRect2D renderArea;
    VkImageAspectFlags aspectFlag;
    uint32_t sampleCount;
    VkResolveModeFlagBits depthResolveMode;
    VkResolveModeFlagBits stencilResolveMode;
    VerifyBuffer verifyBuffer;
    VkClearDepthStencilValue clearValue;
    float depthExpectedValue;
    uint8_t stencilExpectedValue;
    bool separateDepthStencilLayouts;
    bool unusedResolve;
    tcu::Maybe<VkFormat> compatibleFormat;
    bool sampleMask;

    VkFormat resolveFormat() const
    {
        return compatibleFormat ? compatibleFormat.get() : format;
    }

    VkImageAspectFlags resolveAspectFlags() const
    {
        return aspectFlagsForFormat(resolveFormat());
    }
};

// Auxiliar class to group depth formats by compatibility in bit size and format. Note there is at most one alternative format for
// each given format as of the time this comment is being written, and the alternative (compatible) format for a given format can
// only remove aspects but not add them. That is, we cannot use a depth/stencil attachment to resolve a depth-only attachment.
//
// See:
//    * VUID-VkSubpassDescriptionDepthStencilResolve-pDepthStencilResolveAttachment-03181
//    * VUID-VkSubpassDescriptionDepthStencilResolve-pDepthStencilResolveAttachment-03182
class DepthCompatibilityManager
{
public:
    DepthCompatibilityManager() : m_compatibleFormats()
    {
        m_compatibleFormats[VK_FORMAT_D32_SFLOAT_S8_UINT] = VK_FORMAT_D32_SFLOAT;
        m_compatibleFormats[VK_FORMAT_D16_UNORM_S8_UINT]  = VK_FORMAT_D16_UNORM;
        m_compatibleFormats[VK_FORMAT_D24_UNORM_S8_UINT]  = VK_FORMAT_X8_D24_UNORM_PACK32;
    }

    VkFormat getAlternativeFormat(VkFormat format) const
    {
        const auto itr = m_compatibleFormats.find(format);
        if (itr != end(m_compatibleFormats))
            return itr->second;
        return VK_FORMAT_UNDEFINED;
    }

private:
    std::map<VkFormat, VkFormat> m_compatibleFormats;
};

float get16bitDepthComponent(uint8_t *pixelPtr)
{
    uint16_t *value = reinterpret_cast<uint16_t *>(pixelPtr);
    return static_cast<float>(*value) / 65535.0f;
}

float get24bitDepthComponent(uint8_t *pixelPtr)
{
    const bool littleEndian = (DE_ENDIANNESS == DE_LITTLE_ENDIAN);
    uint32_t value          = (((uint32_t)pixelPtr[0]) << (!littleEndian * 16u)) | (((uint32_t)pixelPtr[1]) << 8u) |
                     (((uint32_t)pixelPtr[2]) << (littleEndian * 16u));
    return static_cast<float>(value) / 16777215.0f;
}

float get32bitDepthComponent(uint8_t *pixelPtr)
{
    return *(reinterpret_cast<float *>(pixelPtr));
}

class DepthStencilResolveTest : public TestInstance
{
public:
    DepthStencilResolveTest(Context &context, TestConfig config);
    virtual ~DepthStencilResolveTest(void);

    virtual tcu::TestStatus iterate(void);

protected:
    bool isFeaturesSupported(void);
    bool isSupportedFormat(Context &context, VkFormat format) const;
    VkSampleCountFlagBits sampleCountBitFromSampleCount(uint32_t count) const;

    VkImageSp createImage(VkFormat vkformat, uint32_t sampleCount, VkImageUsageFlags additionalUsage = 0u);
    AllocationSp createImageMemory(VkImageSp image);
    VkImageViewSp createImageView(VkImageSp image, VkFormat vkformat, uint32_t baseArrayLayer);
    AllocationSp createBufferMemory(void);
    VkBufferSp createBuffer(void);

    Move<VkRenderPass> createRenderPass(VkFormat vkformat, uint32_t renderPassNo);
    Move<VkRenderPass> createRenderPassCompatible(void);
    Move<VkFramebuffer> createFramebuffer(VkRenderPass renderPass, VkImageViewSp multisampleImageView,
                                          VkImageViewSp singlesampleImageView);
    Move<VkPipelineLayout> createRenderPipelineLayout(void);
    Move<VkPipeline> createRenderPipeline(VkRenderPass renderPass, uint32_t renderPassNo,
                                          VkPipelineLayout renderPipelineLayout);

    void submit(void);
    bool verifyDepth(void);
    bool verifyStencil(void);

protected:
    const TestConfig m_config;
    const bool m_featureSupported;

    const InstanceInterface &m_vki;
    const DeviceInterface &m_vkd;
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;

    const Unique<VkCommandPool> m_commandPool;

    VkImageSp m_multisampleImage;
    AllocationSp m_multisampleImageMemory;
    VkImageViewSp m_multisampleImageView;
    VkImageSp m_singlesampleImage;
    AllocationSp m_singlesampleImageMemory;
    VkImageViewSp m_singlesampleImageView;
    VkBufferSp m_buffer;
    AllocationSp m_bufferMemory;

    uint32_t m_numRenderPasses;
    std::vector<Move<VkRenderPass>> m_renderPass;
    Unique<VkRenderPass> m_renderPassCompatible;
    Move<VkFramebuffer> m_framebuffer;
    Unique<VkPipelineLayout> m_renderPipelineLayout;
    std::vector<Move<VkPipeline>> m_renderPipeline;
};

DepthStencilResolveTest::DepthStencilResolveTest(Context &context, TestConfig config)
    : TestInstance(context)
    , m_config(config)
    , m_featureSupported(isFeaturesSupported())
    , m_vki(context.getInstanceInterface())
    , m_vkd(context.getDeviceInterface())
    , m_device(context.getDevice())
    , m_physicalDevice(context.getPhysicalDevice())

    , m_commandPool(createCommandPool(context.getDeviceInterface(), context.getDevice(),
                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))

    , m_multisampleImage(createImage(m_config.format, m_config.sampleCount, VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    , m_multisampleImageMemory(createImageMemory(m_multisampleImage))
    , m_multisampleImageView(createImageView(m_multisampleImage, m_config.format, 0u))

    , m_singlesampleImage(createImage(
          m_config.resolveFormat(), 1,
          (VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
           (config.unusedResolve ? static_cast<vk::VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) : 0u))))
    , m_singlesampleImageMemory(createImageMemory(m_singlesampleImage))
    , m_singlesampleImageView(createImageView(m_singlesampleImage, m_config.resolveFormat(), m_config.resolveBaseLayer))

    , m_buffer(createBuffer())
    , m_bufferMemory(createBufferMemory())

    , m_numRenderPasses((m_config.verifyBuffer == VB_DEPTH || !m_config.sampleMask) ? 1u : m_config.sampleCount)
    , m_renderPassCompatible(createRenderPassCompatible())
    , m_renderPipelineLayout(createRenderPipelineLayout())
{
    for (uint32_t i = 0; i < m_numRenderPasses; i++)
    {
        m_renderPass.push_back(createRenderPass(m_config.format, i));
        m_renderPipeline.push_back(createRenderPipeline(*m_renderPass[i], i, *m_renderPipelineLayout));
    }
    m_framebuffer = createFramebuffer(m_config.compatibleFormat ? *m_renderPassCompatible : *m_renderPass[0],
                                      m_multisampleImageView, m_singlesampleImageView);
}

DepthStencilResolveTest::~DepthStencilResolveTest(void)
{
}

bool DepthStencilResolveTest::isFeaturesSupported()
{
    m_context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");
    if (m_config.imageLayers > 1)
        m_context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_config.separateDepthStencilLayouts)
        m_context.requireDeviceFunctionality("VK_KHR_separate_depth_stencil_layouts");

    VkPhysicalDeviceDepthStencilResolveProperties dsResolveProperties;
    deMemset(&dsResolveProperties, 0, sizeof(VkPhysicalDeviceDepthStencilResolveProperties));
    dsResolveProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
    dsResolveProperties.pNext = nullptr;

    VkPhysicalDeviceProperties2 deviceProperties;
    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties.pNext = &dsResolveProperties;

    // perform query to get supported float control properties
    const VkPhysicalDevice physicalDevice          = m_context.getPhysicalDevice();
    const vk::InstanceInterface &instanceInterface = m_context.getInstanceInterface();
    instanceInterface.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

    // check if both modes are supported
    const auto &depthResolveMode   = m_config.depthResolveMode;
    const auto &stencilResolveMode = m_config.stencilResolveMode;

    if ((depthResolveMode != VK_RESOLVE_MODE_NONE) &&
        !(depthResolveMode & dsResolveProperties.supportedDepthResolveModes))
        TCU_THROW(NotSupportedError, "Depth resolve mode not supported");

    if ((stencilResolveMode != VK_RESOLVE_MODE_NONE) &&
        !(stencilResolveMode & dsResolveProperties.supportedStencilResolveModes))
        TCU_THROW(NotSupportedError, "Stencil resolve mode not supported");

    // Check independent resolve support.
    const auto tcuFormat  = mapVkFormat(m_config.format);
    const auto hasDepth   = tcu::hasDepthComponent(tcuFormat.order);
    const auto hasStencil = tcu::hasStencilComponent(tcuFormat.order);

    if (hasDepth && hasStencil)
    {
        if (depthResolveMode == stencilResolveMode)
            ;
        else if (depthResolveMode == VK_RESOLVE_MODE_NONE || stencilResolveMode == VK_RESOLVE_MODE_NONE)
        {
            if (!dsResolveProperties.independentResolveNone)
                TCU_THROW(NotSupportedError, "independentResolveNone not supported");
        }
        else
        {
            if (!dsResolveProperties.independentResolve)
                TCU_THROW(NotSupportedError, "independentResolve not supported");
        }
    }

    // Check alternative format support if needed.
    if (m_config.compatibleFormat)
    {
        if (!isSupportedFormat(m_context, m_config.compatibleFormat.get()))
            TCU_THROW(NotSupportedError, "Alternative image format for compatibility test not supported");
    }

    return true;
}

VkSampleCountFlagBits DepthStencilResolveTest::sampleCountBitFromSampleCount(uint32_t count) const
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

VkImageSp DepthStencilResolveTest::createImage(VkFormat vkformat, uint32_t sampleCount,
                                               VkImageUsageFlags additionalUsage)
{
    const tcu::TextureFormat format(mapVkFormat(m_config.format));
    const VkImageTiling imageTiling(VK_IMAGE_TILING_OPTIMAL);
    VkSampleCountFlagBits sampleCountBit(sampleCountBitFromSampleCount(sampleCount));
    VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalUsage;

    VkImageFormatProperties imageFormatProperties;
    if (m_vki.getPhysicalDeviceImageFormatProperties(m_physicalDevice, m_config.format, VK_IMAGE_TYPE_2D, imageTiling,
                                                     usage, 0u,
                                                     &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Format not supported");
    }
    if (imageFormatProperties.sampleCounts < sampleCount)
    {
        TCU_THROW(NotSupportedError, "Sample count not supported");
    }
    if (imageFormatProperties.maxArrayLayers < m_config.imageLayers)
    {
        TCU_THROW(NotSupportedError, "Layers count not supported");
    }

    const VkExtent3D imageExtent = {m_config.width, m_config.height, 1u};

    if (!(tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order)))
        TCU_THROW(NotSupportedError, "Format can't be used as depth/stencil attachment");

    if (imageFormatProperties.maxExtent.width < imageExtent.width ||
        imageFormatProperties.maxExtent.height < imageExtent.height ||
        ((imageFormatProperties.sampleCounts & sampleCountBit) == 0) ||
        imageFormatProperties.maxArrayLayers < m_config.imageLayers)
    {
        TCU_THROW(NotSupportedError, "Image type not supported");
    }

    const VkImageCreateInfo pCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                           nullptr,
                                           0u,
                                           VK_IMAGE_TYPE_2D,
                                           vkformat,
                                           imageExtent,
                                           1u,
                                           m_config.imageLayers,
                                           sampleCountBit,
                                           imageTiling,
                                           usage,
                                           VK_SHARING_MODE_EXCLUSIVE,
                                           0u,
                                           nullptr,
                                           VK_IMAGE_LAYOUT_UNDEFINED};

    return safeSharedPtr(new Unique<VkImage>(vk::createImage(m_vkd, m_device, &pCreateInfo)));
}

AllocationSp DepthStencilResolveTest::createImageMemory(VkImageSp image)
{
    Allocator &allocator = m_context.getDefaultAllocator();

    de::MovePtr<Allocation> allocation(
        allocator.allocate(getImageMemoryRequirements(m_vkd, m_device, **image), MemoryRequirement::Any));
    VK_CHECK(m_vkd.bindImageMemory(m_device, **image, allocation->getMemory(), allocation->getOffset()));
    return safeSharedPtr(allocation.release());
}

VkImageViewSp DepthStencilResolveTest::createImageView(VkImageSp image, VkFormat vkformat, uint32_t baseArrayLayer)
{
    const VkImageSubresourceRange range = {aspectFlagsForFormat(vkformat), 0u, 1u, baseArrayLayer, m_config.viewLayers};

    const VkImageViewCreateInfo pCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0u,
        **image,
        (m_config.viewLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
        vkformat,
        makeComponentMappingRGBA(),
        range,
    };
    return safeSharedPtr(new Unique<VkImageView>(vk::createImageView(m_vkd, m_device, &pCreateInfo)));
}

Move<VkRenderPass> DepthStencilResolveTest::createRenderPass(VkFormat vkformat, uint32_t renderPassNo)
{
    const VkSampleCountFlagBits samples(sampleCountBitFromSampleCount(m_config.sampleCount));

    VkImageLayout layout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkImageLayout stencilLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkImageLayout finalLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkImageLayout stencilFinalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    if (m_config.separateDepthStencilLayouts)
    {
        if (m_config.verifyBuffer == VB_DEPTH)
        {
            layout        = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            stencilLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else
        {
            layout        = VK_IMAGE_LAYOUT_GENERAL;
            stencilLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        }
    }

    if (renderPassNo != m_numRenderPasses - 1)
    {
        finalLayout        = layout;
        stencilFinalLayout = stencilLayout;
    }

    const VkAttachmentDescriptionStencilLayout multisampleAttachmentStencilLayout = {
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT,         // VkStructureType sType;
        nullptr,                                                         // const void* pNext;
        (renderPassNo == 0) ? VK_IMAGE_LAYOUT_UNDEFINED : stencilLayout, // VkImageLayout initialLayout;
        stencilFinalLayout,
    };
    const AttachmentDescription2 multisampleAttachment // VkAttachmentDescription2
        (
            // VkStructureType sType;
            m_config.separateDepthStencilLayouts ? &multisampleAttachmentStencilLayout : nullptr, // const void* pNext;
            0u,              // VkAttachmentDescriptionFlags flags;
            m_config.format, // VkFormat format;
            samples,         // VkSampleCountFlagBits samples;
            (renderPassNo == 0) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                  VK_ATTACHMENT_LOAD_OP_LOAD, // VkAttachmentLoadOp loadOp;
            (m_numRenderPasses > 1) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            (renderPassNo == 0) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                  VK_ATTACHMENT_LOAD_OP_LOAD, // VkAttachmentLoadOp stencilLoadOp;
            (m_numRenderPasses > 1) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            (renderPassNo == 0) ? VK_IMAGE_LAYOUT_UNDEFINED : layout, // VkImageLayout initialLayout;
            finalLayout                                               // VkImageLayout finalLayout;
        );
    const VkAttachmentReferenceStencilLayout multisampleAttachmentRefStencilLayout = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        stencilLayout                                          // VkImageLayout stencilLayout;
    };
    const AttachmentReference2 multisampleAttachmentRef // VkAttachmentReference2
        (
            // VkStructureType sType;
            m_config.separateDepthStencilLayouts ? &multisampleAttachmentRefStencilLayout :
                                                   nullptr, // const void* pNext;
            0u,                                             // uint32_t attachment;
            layout,                                         // VkImageLayout layout;
            m_config.aspectFlag                             // VkImageAspectFlags aspectMask;
        );

    vk::VkImageLayout singleSampleInitialLayout =
        (m_config.unusedResolve ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED);
    vk::VkImageLayout singleSampleStencilInitialLayout =
        (m_config.unusedResolve ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED);
    if (renderPassNo != 0)
    {
        singleSampleInitialLayout        = layout;
        singleSampleStencilInitialLayout = stencilLayout;
    }

    const VkAttachmentDescriptionStencilLayout singlesampleAttachmentStencilLayout = {
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT, // VkStructureType sType;
        nullptr,                                                 // const void* pNext;
        singleSampleStencilInitialLayout,                        // VkImageLayout initialLayout;
        stencilFinalLayout,
    };
    const AttachmentDescription2 singlesampleAttachment // VkAttachmentDescription2
        (
            // VkStructureType sType;
            m_config.separateDepthStencilLayouts ? &singlesampleAttachmentStencilLayout : nullptr, // const void* pNext;
            0u,                           // VkAttachmentDescriptionFlags flags;
            vkformat,                     // VkFormat format;
            VK_SAMPLE_COUNT_1_BIT,        // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,  // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE, // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_CLEAR,  // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_STORE, // VkAttachmentStoreOp stencilStoreOp;
            singleSampleInitialLayout,    // VkImageLayout initialLayout;
            finalLayout                   // VkImageLayout finalLayout;
        );

    const VkAttachmentReferenceStencilLayout singlesampleAttachmentRefStencilLayout = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        stencilLayout                                          // VkImageLayout stencilLayout;
    };
    const AttachmentReference2 singlesampleAttachmentRef // VkAttachmentReference2
        (
            // VkStructureType sType;
            m_config.separateDepthStencilLayouts ? &singlesampleAttachmentRefStencilLayout :
                                                   nullptr, // const void* pNext;
            ((m_config.unusedResolve || renderPassNo != m_numRenderPasses - 1) ? VK_ATTACHMENT_UNUSED :
                                                                                 1u), // uint32_t attachment;
            layout,                                                                   // VkImageLayout layout;
            aspectFlagsForFormat(vkformat)                                            // VkImageAspectFlags aspectMask;
        );

    std::vector<AttachmentDescription2> attachments;
    attachments.push_back(multisampleAttachment);
    attachments.push_back(singlesampleAttachment);

    VkSubpassDescriptionDepthStencilResolve dsResolveDescription = {
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,
        nullptr,                     // const void* pNext;
        m_config.depthResolveMode,   // VkResolveModeFlagBits depthResolveMode;
        m_config.stencilResolveMode, // VkResolveModeFlagBits stencilResolveMode;
        &singlesampleAttachmentRef   // VkAttachmentReference2 pDepthStencilResolveAttachment;
    };

    const SubpassDescription2 subpass // VkSubpassDescription2
        (
            // VkStructureType sType;
            renderPassNo == m_numRenderPasses - 1 ? &dsResolveDescription : nullptr, // const void* pNext;
            (VkSubpassDescriptionFlags)0,                                            // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
            0u,                              // uint32_t viewMask;
            0u,                              // uint32_t inputAttachmentCount;
            nullptr,                         // const VkAttachmentReference2* pInputAttachments;
            0u,                              // uint32_t colorAttachmentCount;
            nullptr,                         // const VkAttachmentReference2* pColorAttachments;
            nullptr,                         // const VkAttachmentReference2* pResolveAttachments;
            &multisampleAttachmentRef,       // const VkAttachmentReference2* pDepthStencilAttachment;
            0u,                              // uint32_t preserveAttachmentCount;
            nullptr                          // const uint32_t* pPreserveAttachments;
        );

    const RenderPassCreateInfo2 renderPassCreator // VkRenderPassCreateInfo2
        (
            // VkStructureType sType;
            nullptr,                      // const void* pNext;
            (VkRenderPassCreateFlags)0u,  // VkRenderPassCreateFlags flags;
            (uint32_t)attachments.size(), // uint32_t attachmentCount;
            &attachments[0],              // const VkAttachmentDescription2* pAttachments;
            1u,                           // uint32_t subpassCount;
            &subpass,                     // const VkSubpassDescription2* pSubpasses;
            0u,                           // uint32_t dependencyCount;
            nullptr,                      // const VkSubpassDependency2* pDependencies;
            0u,                           // uint32_t correlatedViewMaskCount;
            nullptr                       // const uint32_t* pCorrelatedViewMasks;
        );

    return renderPassCreator.createRenderPass(m_vkd, m_device);
}

// Checks format support.
// Note: we need the context because this is called from the constructor only after m_config has been set.
bool DepthStencilResolveTest::isSupportedFormat(Context &context, VkFormat format) const
{
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        (m_config.unusedResolve ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : static_cast<vk::VkImageUsageFlagBits>(0u));
    VkImageFormatProperties props;

    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();
    const auto formatCheck    = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D,
                                                                           VK_IMAGE_TILING_OPTIMAL, usage, 0u, &props);

    return (formatCheck == VK_SUCCESS);
}

Move<VkRenderPass> DepthStencilResolveTest::createRenderPassCompatible(void)
{
    // Early exit if we are not testing compatibility.
    if (!m_config.compatibleFormat)
        return {};

    return createRenderPass(m_config.compatibleFormat.get(), 0);
}

Move<VkFramebuffer> DepthStencilResolveTest::createFramebuffer(VkRenderPass renderPass,
                                                               VkImageViewSp multisampleImageView,
                                                               VkImageViewSp singlesampleImageView)
{
    std::vector<VkImageView> attachments;
    attachments.push_back(**multisampleImageView);
    attachments.push_back(**singlesampleImageView);

    const VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                nullptr,
                                                0u,

                                                renderPass,
                                                (uint32_t)attachments.size(),
                                                &attachments[0],

                                                m_config.width,
                                                m_config.height,
                                                m_config.viewLayers};

    return vk::createFramebuffer(m_vkd, m_device, &createInfo);
}

Move<VkPipelineLayout> DepthStencilResolveTest::createRenderPipelineLayout(void)
{
    VkPushConstantRange pushConstant = {VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 4u};

    uint32_t pushConstantRangeCount          = 0u;
    VkPushConstantRange *pPushConstantRanges = nullptr;
    if (m_config.verifyBuffer == VB_STENCIL)
    {
        pushConstantRangeCount = 1u;
        pPushConstantRanges    = &pushConstant;
    }

    const VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                   nullptr,
                                                   (vk::VkPipelineLayoutCreateFlags)0,

                                                   0u,
                                                   nullptr,

                                                   pushConstantRangeCount,
                                                   pPushConstantRanges};

    return vk::createPipelineLayout(m_vkd, m_device, &createInfo);
}

Move<VkPipeline> DepthStencilResolveTest::createRenderPipeline(VkRenderPass renderPass, uint32_t renderPassNo,
                                                               VkPipelineLayout renderPipelineLayout)
{
    const bool testingStencil                    = (m_config.verifyBuffer == VB_STENCIL);
    const vk::BinaryCollection &binaryCollection = m_context.getBinaryCollection();

    const Unique<VkShaderModule> vertexShaderModule(
        createShaderModule(m_vkd, m_device, binaryCollection.get("quad-vert"), 0u));
    const Unique<VkShaderModule> fragmentShaderModule(
        createShaderModule(m_vkd, m_device, binaryCollection.get("quad-frag"), 0u));
    const Move<VkShaderModule> geometryShaderModule(
        m_config.imageLayers == 1 ? Move<VkShaderModule>() :
                                    createShaderModule(m_vkd, m_device, binaryCollection.get("quad-geom"), 0u));

    const VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineVertexInputStateCreateFlags)0u,

        0u,
        nullptr,

        0u,
        nullptr};
    const tcu::UVec2 view(m_config.width, m_config.height);
    const std::vector<VkViewport> viewports(1, makeViewport(view));
    const std::vector<VkRect2D> scissors(1, m_config.renderArea);
    const VkSampleMask samplemask[2] = {renderPassNo < 32 ? (1u << renderPassNo) : 0,
                                        renderPassNo < 32 ? 0 : (1u << (renderPassNo - 32))};

    const VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr,  (VkPipelineMultisampleStateCreateFlags)0u,

        sampleCountBitFromSampleCount(m_config.sampleCount),      VK_FALSE, 0.0f,
        (m_config.sampleMask) ? &samplemask[0] : nullptr,         VK_FALSE, VK_FALSE,
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineDepthStencilStateCreateFlags)0u,

        VK_TRUE, // depthTestEnable
        VK_TRUE,
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        testingStencil, // stencilTestEnable
        {
            VK_STENCIL_OP_REPLACE, // failOp
            VK_STENCIL_OP_REPLACE, // passOp
            VK_STENCIL_OP_REPLACE, // depthFailOp
            VK_COMPARE_OP_ALWAYS,  // compareOp
            0xFFu,                 // compareMask
            0xFFu,                 // writeMask
            1                      // reference
        },
        {VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 1},
        0.0f,
        1.0f};

    std::vector<VkDynamicState> dynamicState;
    dynamicState.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType                      sType;
        nullptr,                                              // const void*                          pNext;
        (VkPipelineDynamicStateCreateFlags)0u,                // VkPipelineDynamicStateCreateFlags    flags;
        static_cast<uint32_t>(dynamicState.size()),           // uint32_t                             dynamicStateCount;
        &dynamicState[0]                                      // const VkDynamicState*                pDynamicStates;
    };

    return makeGraphicsPipeline(
        m_vkd,                // const DeviceInterface&                        vk
        m_device,             // const VkDevice                                device
        renderPipelineLayout, // const VkPipelineLayout                        pipelineLayout
        *vertexShaderModule,  // const VkShaderModule                          vertexShaderModule
        VK_NULL_HANDLE,       // const VkShaderModule                          tessellationControlShaderModule
        VK_NULL_HANDLE,       // const VkShaderModule                          tessellationEvalShaderModule
        m_config.imageLayers == 1 ?
            VK_NULL_HANDLE :
            *geometryShaderModule,           // const VkShaderModule                          geometryShaderModule
        *fragmentShaderModule,               // const VkShaderModule                          fragmentShaderModule
        renderPass,                          // const VkRenderPass                            renderPass
        viewports,                           // const std::vector<VkViewport>&                viewports
        scissors,                            // const std::vector<VkRect2D>&                  scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
        0u,                                  // const uint32_t                                subpass
        0u,                                  // const uint32_t                                patchControlPoints
        &vertexInputState,                   // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
        nullptr,            // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
        &multisampleState,  // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
        &depthStencilState, // const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
        nullptr,            // const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
        testingStencil ? &dynamicStateCreateInfo :
                         nullptr); // const VkPipelineDynamicStateCreateInfo*       dynamicStateCreateInfo
}

AllocationSp DepthStencilResolveTest::createBufferMemory(void)
{
    Allocator &allocator = m_context.getDefaultAllocator();
    de::MovePtr<Allocation> allocation(
        allocator.allocate(getBufferMemoryRequirements(m_vkd, m_device, **m_buffer), MemoryRequirement::HostVisible));
    VK_CHECK(m_vkd.bindBufferMemory(m_device, **m_buffer, allocation->getMemory(), allocation->getOffset()));
    return safeSharedPtr(allocation.release());
}

VkBufferSp DepthStencilResolveTest::createBuffer(void)
{
    const VkBufferUsageFlags bufferUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const tcu::TextureFormat textureFormat(mapVkFormat(m_config.format));
    const VkDeviceSize pixelSize(textureFormat.getPixelSize());
    const VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                           nullptr,
                                           0u,

                                           m_config.width * m_config.height * m_config.imageLayers * pixelSize,
                                           bufferUsage,

                                           VK_SHARING_MODE_EXCLUSIVE,
                                           0u,
                                           nullptr};
    return safeSharedPtr(new Unique<VkBuffer>(vk::createBuffer(m_vkd, m_device, &createInfo)));
}

void DepthStencilResolveTest::submit(void)
{
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());

    // When the depth/stencil resolve attachment is unused, it needs to be cleared outside
    // the render pass so it has the expected values.
    if (m_config.unusedResolve)
    {
        const Unique<VkCommandBuffer> commandBuffer(
            allocateCommandBuffer(m_vkd, m_device, *m_commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const vk::VkImageSubresourceRange imageRange = {
            m_config.resolveAspectFlags(), 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS,
        };
        const vk::VkImageMemoryBarrier preBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,

            // src and dst access masks.
            0,
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,

            // old and new layouts.
            vk::VK_IMAGE_LAYOUT_UNDEFINED,
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,

            **m_singlesampleImage,
            imageRange,
        };
        const vk::VkImageMemoryBarrier postBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,

            // src and dst access masks.
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,
            0,

            // old and new layouts.
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,

            **m_singlesampleImage,
            imageRange,
        };

        vk::beginCommandBuffer(m_vkd, commandBuffer.get());
        m_vkd.cmdPipelineBarrier(commandBuffer.get(), vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preBarrier);
        m_vkd.cmdClearDepthStencilImage(commandBuffer.get(), **m_singlesampleImage,
                                        vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_config.clearValue, 1u,
                                        &imageRange);
        m_vkd.cmdPipelineBarrier(commandBuffer.get(), vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u,
                                 &postBarrier);
        vk::endCommandBuffer(m_vkd, commandBuffer.get());

        vk::submitCommandsAndWait(m_vkd, m_device, m_context.getUniversalQueue(), commandBuffer.get());
    }

    const Unique<VkCommandBuffer> commandBuffer(
        allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const RenderpassSubpass2::SubpassBeginInfo subpassBeginInfo(nullptr, VK_SUBPASS_CONTENTS_INLINE);
    const RenderpassSubpass2::SubpassEndInfo subpassEndInfo(nullptr);

    beginCommandBuffer(vkd, *commandBuffer);
    bool testingDepth = (m_config.verifyBuffer == VB_DEPTH);
    if (testingDepth)
    {
        {
            VkClearValue clearValues[2];
            clearValues[0].depthStencil = m_config.clearValue;
            clearValues[1].depthStencil = m_config.clearValue;

            const VkRenderPassBeginInfo beginInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                nullptr,

                (m_config.compatibleFormat ? *m_renderPassCompatible : *m_renderPass[0]),
                *m_framebuffer,

                {{0u, 0u}, {m_config.width, m_config.height}},

                2u,
                clearValues};
            RenderpassSubpass2::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
        }

        // Render
        vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline[0]);
        vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
        RenderpassSubpass2::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);
    }
    else
    {
        // Stencil
        for (uint32_t i = 0; i < m_config.sampleCount; i++)
        {
            if (i == 0 || m_config.sampleMask)
            {
                if (i > 0)
                {
                    // If this is not the first renderpass, add a barrier to
                    // ensure we observe the store_op -> load_op.
                    const VkImageMemoryBarrier barrier = {
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,

                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,

                        m_config.separateDepthStencilLayouts ? VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL :
                                                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        m_config.separateDepthStencilLayouts ? VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL :
                                                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,

                        VK_QUEUE_FAMILY_IGNORED,
                        VK_QUEUE_FAMILY_IGNORED,

                        **m_multisampleImage,
                        {(m_config.separateDepthStencilLayouts) ? VkImageAspectFlags(VK_IMAGE_ASPECT_STENCIL_BIT) :
                                                                  aspectFlagsForFormat(m_config.format),
                         0u, 1u, 0u, m_config.viewLayers}};

                    vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, 0, 0, 0, 1u, &barrier);
                }

                VkClearValue clearValues[2];
                clearValues[0].depthStencil = m_config.clearValue;
                clearValues[1].depthStencil = m_config.clearValue;

                const VkRenderPassBeginInfo beginInfo = {
                    VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                    nullptr,

                    (m_config.compatibleFormat ? *m_renderPassCompatible : *m_renderPass[i]),
                    *m_framebuffer,

                    {{0u, 0u}, {m_config.width, m_config.height}},

                    2u,
                    clearValues};

                RenderpassSubpass2::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
            }
            // For stencil we can set reference value for just one sample at a time
            // so we need to do as many passes as there are samples, first half
            // of samples is initialized with 1 and second half with 255
            const uint32_t halfOfSamples = m_config.sampleCount >> 1;

            uint32_t stencilReference = 1 + 254 * (i >= halfOfSamples);
            vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                *m_renderPipeline[m_config.sampleMask ? i : 0]);
            vkd.cmdPushConstants(*commandBuffer, *m_renderPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(i),
                                 &i);
            vkd.cmdSetStencilReference(*commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, stencilReference);
            vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
            if (i == m_config.sampleCount - 1 || m_config.sampleMask)
                RenderpassSubpass2::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);
        }
    }

    // Memory barriers between rendering and copying
    {
        const VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,

            // Note: as per the spec, depth/stencil *resolve* operations are synchronized using the color attachment write access.
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,

            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,

            **m_singlesampleImage,
            {(m_config.separateDepthStencilLayouts) ?
                 VkImageAspectFlags(testingDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT) :
                 m_config.resolveAspectFlags(),
             0u, 1u, 0u, m_config.viewLayers}};

        vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                               0u, nullptr, 0u, nullptr, 1u, &barrier);
    }

    // Copy image memory to buffers
    const VkBufferImageCopy region = {
        0u,
        0u,
        0u,
        {
            VkImageAspectFlags(testingDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT),
            0u,
            0u,
            m_config.viewLayers,
        },
        {0u, 0u, 0u},
        {m_config.width, m_config.height, 1u}};

    vkd.cmdCopyImageToBuffer(*commandBuffer, **m_singlesampleImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_buffer,
                             1u, &region);

    // Memory barriers between copies and host access
    {
        const VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                               nullptr,

                                               VK_ACCESS_TRANSFER_WRITE_BIT,
                                               VK_ACCESS_HOST_READ_BIT,

                                               VK_QUEUE_FAMILY_IGNORED,
                                               VK_QUEUE_FAMILY_IGNORED,

                                               **m_buffer,
                                               0u,
                                               VK_WHOLE_SIZE};

        vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                               nullptr, 1u, &barrier, 0u, nullptr);
    }

    endCommandBuffer(vkd, *commandBuffer);

    submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *commandBuffer);
}

bool DepthStencilResolveTest::verifyDepth(void)
{
    // Invalidate allocation before attempting to read buffer memory.
    invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), *m_bufferMemory);

    uint32_t layerSize   = m_config.width * m_config.height;
    uint32_t valuesCount = layerSize * m_config.viewLayers;
    uint8_t *pixelPtr    = static_cast<uint8_t *>(m_bufferMemory->getHostPtr());

    const DeviceInterface &vkd(m_context.getDeviceInterface());
    invalidateMappedMemoryRange(vkd, m_context.getDevice(), m_bufferMemory->getMemory(), m_bufferMemory->getOffset(),
                                VK_WHOLE_SIZE);

    float expectedValue = m_config.depthExpectedValue;
    if (m_config.depthResolveMode == VK_RESOLVE_MODE_NONE || m_config.unusedResolve)
        expectedValue = m_config.clearValue.depth;

    // depth data in buffer is tightly packed, ConstPixelBufferAccess
    // coludn't be used for depth value extraction as it cant interpret
    // formats containing just depth component

    typedef float (*DepthComponentGetterFn)(uint8_t *);
    VkFormat format                          = m_config.format;
    DepthComponentGetterFn getDepthComponent = &get16bitDepthComponent;
    uint32_t pixelStep                       = 2;
    float epsilon                            = 0.002f;

    if ((format == VK_FORMAT_X8_D24_UNORM_PACK32) || (format == VK_FORMAT_D24_UNORM_S8_UINT))
    {
        getDepthComponent = &get24bitDepthComponent;
        pixelStep         = 4;
    }
    else if ((format == VK_FORMAT_D32_SFLOAT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT))
    {
        getDepthComponent = &get32bitDepthComponent;
        pixelStep         = 4;
    }

    for (uint32_t valueIndex = 0; valueIndex < valuesCount; valueIndex++)
    {
        float depth = (*getDepthComponent)(pixelPtr);
        pixelPtr += pixelStep;

        // check if pixel data is outside of render area
        int32_t layerIndex   = valueIndex / layerSize;
        int32_t inLayerIndex = valueIndex % layerSize;
        int32_t x            = inLayerIndex % m_config.width;
        int32_t y            = (inLayerIndex - x) / m_config.width;
        int32_t x1           = m_config.renderArea.offset.x;
        int32_t y1           = m_config.renderArea.offset.y;
        int32_t x2           = x1 + m_config.renderArea.extent.width;
        int32_t y2           = y1 + m_config.renderArea.extent.height;
        if ((x < x1) || (x >= x2) || (y < y1) || (y >= y2))
        {
            // verify that outside of render area there are clear values
            float error = deFloatAbs(depth - m_config.clearValue.depth);
            if (error > epsilon)
            {
                m_context.getTestContext().getLog()
                    << TestLog::Message << "(" << x << ", " << y << ", layer: " << layerIndex
                    << ") is outside of render area but depth value is: " << depth << " (expected "
                    << m_config.clearValue.depth << ")" << TestLog::EndMessage;
                return false;
            }

            // value is correct, go to next one
            continue;
        }

        float error = deFloatAbs(depth - expectedValue);
        if (error > epsilon)
        {
            m_context.getTestContext().getLog()
                << TestLog::Message << "At (" << x << ", " << y << ", layer: " << layerIndex
                << ") depth value is: " << depth << " expected: " << expectedValue << TestLog::EndMessage;
            return false;
        }
    }
    m_context.getTestContext().getLog() << TestLog::Message << "Depth value is " << expectedValue
                                        << TestLog::EndMessage;

    return true;
}

bool DepthStencilResolveTest::verifyStencil(void)
{
    // Invalidate allocation before attempting to read buffer memory.
    invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), *m_bufferMemory);

    uint32_t layerSize   = m_config.width * m_config.height;
    uint32_t valuesCount = layerSize * m_config.viewLayers;
    uint8_t *pixelPtr    = static_cast<uint8_t *>(m_bufferMemory->getHostPtr());

    const DeviceInterface &vkd(m_context.getDeviceInterface());
    invalidateMappedMemoryRange(vkd, m_context.getDevice(), m_bufferMemory->getMemory(), m_bufferMemory->getOffset(),
                                VK_WHOLE_SIZE);

    // when stencil is tested we are discarding invocations and
    // because of that depth and stencil need to be tested separately

    uint8_t expectedValue = m_config.stencilExpectedValue;
    if (m_config.stencilResolveMode == VK_RESOLVE_MODE_NONE || m_config.unusedResolve)
        expectedValue = static_cast<uint8_t>(m_config.clearValue.stencil);

    for (uint32_t valueIndex = 0; valueIndex < valuesCount; valueIndex++)
    {
        uint8_t stencil      = *pixelPtr++;
        int32_t layerIndex   = valueIndex / layerSize;
        int32_t inLayerIndex = valueIndex % layerSize;
        int32_t x            = inLayerIndex % m_config.width;
        int32_t y            = (inLayerIndex - x) / m_config.width;
        int32_t x1           = m_config.renderArea.offset.x;
        int32_t y1           = m_config.renderArea.offset.y;
        int32_t x2           = x1 + m_config.renderArea.extent.width;
        int32_t y2           = y1 + m_config.renderArea.extent.height;
        if ((x < x1) || (x >= x2) || (y < y1) || (y >= y2))
        {
            if (stencil != m_config.clearValue.stencil)
            {
                m_context.getTestContext().getLog()
                    << TestLog::Message << "(" << x << ", " << y << ", layer: " << layerIndex
                    << ") is outside of render area but stencil value is: " << stencil << " (expected "
                    << m_config.clearValue.stencil << ")" << TestLog::EndMessage;
                return false;
            }

            // value is correct, go to next one
            continue;
        }

        if (stencil != expectedValue)
        {
            m_context.getTestContext().getLog()
                << TestLog::Message << "At (" << x << ", " << y << ", layer: " << layerIndex
                << ") stencil value is: " << static_cast<uint32_t>(stencil)
                << " expected: " << static_cast<uint32_t>(expectedValue) << TestLog::EndMessage;
            return false;
        }
    }
    m_context.getTestContext().getLog() << TestLog::Message << "Stencil value is "
                                        << static_cast<uint32_t>(expectedValue) << TestLog::EndMessage;

    return true;
}

tcu::TestStatus DepthStencilResolveTest::iterate(void)
{
    submit();

    bool result = false;
    if (m_config.verifyBuffer == VB_DEPTH)
        result = verifyDepth();
    else
        result = verifyStencil();

    if (result)
        return tcu::TestStatus::pass("Pass");
    return tcu::TestStatus::fail("Fail");
}

struct Programs
{
    void init(vk::SourceCollections &dst, TestConfig config) const
    {
        // geometry shader is only needed in multi-layer framebuffer resolve tests
        if (config.imageLayers > 1)
        {
            const uint32_t layerCount = 3;

            std::ostringstream src;
            src << "#version 450\n"
                << "highp float;\n"
                << "\n"
                << "layout(triangles) in;\n"
                << "layout(triangle_strip, max_vertices = " << 3 * 2 * layerCount << ") out;\n"
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
                << "    for (int layerNdx = 0; layerNdx < " << layerCount << "; ++layerNdx) {\n"
                << "        for(int vertexNdx = 0; vertexNdx < gl_in.length(); vertexNdx++) {\n"
                << "            gl_Position = gl_in[vertexNdx].gl_Position;\n"
                << "            gl_Layer    = layerNdx;\n"
                << "            EmitVertex();\n"
                << "        };\n"
                << "        EndPrimitive();\n"
                << "    };\n"
                << "}\n";

            dst.glslSources.add("quad-geom") << glu::GeometrySource(src.str());
        }

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

        if (config.verifyBuffer == VB_DEPTH)
        {
            dst.glslSources.add("quad-frag") << glu::FragmentSource(
                "#version 450\n"
                "precision highp float;\n"
                "precision highp int;\n"
                "void main (void)\n"
                "{\n"
                "  float sampleIndex = float(gl_SampleID);\n"          // sampleIndex is integer in range <0, 63>
                "  float valueIndex = round(mod(sampleIndex, 4.0));\n" // limit possible depth values - count to 4
                "  float value = valueIndex + 2.0;\n"                  // value is one of [2, 3, 4, 5]
                "  value = round(exp2(value));\n"                      // value is one of [4, 8, 16, 32]
                "  bool condition = (int(value) == 8);\n"            // select second sample value (to make it smallest)
                "  value = round(value - float(condition) * 6.0);\n" // value is one of [4, 2, 16, 32]
                "  gl_FragDepth = value / 100.0;\n"                  // sample depth is one of [0.04, 0.02, 0.16, 0.32]
                "}\n");
        }
        else
        {
            if (config.sampleMask)
            {
                dst.glslSources.add("quad-frag") << glu::FragmentSource("#version 450\n"
                                                                        "precision highp float;\n"
                                                                        "precision highp int;\n"
                                                                        "void main (void)\n"
                                                                        "{\n"
                                                                        "  gl_FragDepth = 0.5;\n"
                                                                        "}\n");
            }
            else
            {
                dst.glslSources.add("quad-frag") << glu::FragmentSource("#version 450\n"
                                                                        "precision highp float;\n"
                                                                        "precision highp int;\n"
                                                                        "layout(push_constant) uniform PushConstant {\n"
                                                                        "  highp int sampleID;\n"
                                                                        "} pushConstants;\n"
                                                                        "void main (void)\n"
                                                                        "{\n"
                                                                        "  if(gl_SampleID != pushConstants.sampleID)\n"
                                                                        "    discard;\n"
                                                                        "  gl_FragDepth = 0.5;\n"
                                                                        "}\n");
            }
        }
    }
};

void checkSupport(Context &context)
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

enum class MiscTestType
{
    PROPERTIES = 0,
    RESOLVE_STENCIL_ASPECT_THAT_IS_NOT_PRESENT,
    RESOLVE_DEPTH_ASPECT_THAT_IS_NOT_PRESENT,
};

class PropertiesTestInstance : public vkt::TestInstance
{
public:
    PropertiesTestInstance(Context &context) : vkt::TestInstance(context)
    {
    }
    virtual ~PropertiesTestInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);
};

tcu::TestStatus PropertiesTestInstance::iterate(void)
{
    vk::VkPhysicalDeviceDepthStencilResolveProperties dsrProperties;
    dsrProperties.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
    dsrProperties.pNext = nullptr;

    vk::VkPhysicalDeviceProperties2 properties2;
    properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &dsrProperties;

    m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties2);

#ifndef CTS_USES_VULKANSC
    if ((dsrProperties.supportedDepthResolveModes & vk::VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) == 0)
        TCU_FAIL("supportedDepthResolveModes does not include VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR");

    if ((dsrProperties.supportedStencilResolveModes & vk::VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) == 0)
        TCU_FAIL("supportedStencilResolveModes does not include VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR");
#endif // CTS_USES_VULKANCTS

    if ((dsrProperties.supportedStencilResolveModes & vk::VK_RESOLVE_MODE_AVERAGE_BIT) != 0)
        TCU_FAIL("supportedStencilResolveModes includes forbidden VK_RESOLVE_MODE_AVERAGE_BIT_KHR");

    if (dsrProperties.independentResolve == VK_TRUE && dsrProperties.independentResolveNone != VK_TRUE)
        TCU_FAIL("independentResolve supported but independentResolveNone not supported");

    return tcu::TestStatus::pass("Pass");
}

class ResolveNonPresentAspectTestInstance : public vkt::TestInstance
{
public:
    ResolveNonPresentAspectTestInstance(Context &context, MiscTestType testType)
        : vkt::TestInstance(context)
        , m_testType(testType)
    {
    }
    virtual ~ResolveNonPresentAspectTestInstance(void) = default;

    virtual tcu::TestStatus iterate(void);

protected:
    Move<VkRenderPass> createDepthPass(bool enableDepthStencilWrite, VkFormat format, VkImageAspectFlags imageAspect,
                                       VkResolveModeFlagBits depthResolveMode,
                                       VkResolveModeFlagBits stencilResolveMode) const;

private:
    MiscTestType m_testType;
};

Move<VkRenderPass> ResolveNonPresentAspectTestInstance::createDepthPass(bool enableDepthStencilWrite, VkFormat format,
                                                                        VkImageAspectFlags imageAspect,
                                                                        VkResolveModeFlagBits depthResolveMode,
                                                                        VkResolveModeFlagBits stencilResolveMode) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    VkAttachmentReference2 multisampleAttachmentRef  = initVulkanStructure();
    multisampleAttachmentRef.layout                  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    multisampleAttachmentRef.aspectMask              = imageAspect;
    VkAttachmentReference2 singlesampleAttachmentRef = multisampleAttachmentRef;
    singlesampleAttachmentRef.attachment             = 1;

    VkSubpassDescriptionDepthStencilResolve dsResolveDescription{
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE, nullptr,
        depthResolveMode,          // VkResolveModeFlagBits            depthResolveMode
        stencilResolveMode,        // VkResolveModeFlagBits            stencilResolveMode
        &singlesampleAttachmentRef // VkAttachmentReference2            pDepthStencilResolveAttachment
    };

    VkSubpassDescription2 subpassDescription   = initVulkanStructure();
    subpassDescription.pNext                   = &dsResolveDescription;
    subpassDescription.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.pDepthStencilAttachment = &multisampleAttachmentRef;

    VkAttachmentDescription2 attachments[]{
        {
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, nullptr,
            0u,                                              // VkAttachmentDescriptionFlags        flags
            format,                                          // VkFormat                            format
            VK_SAMPLE_COUNT_4_BIT,                           // VkSampleCountFlagBits            samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // VkImageLayout                    finalLayout
        },
        {
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, nullptr,
            0u,                                  // VkAttachmentDescriptionFlags        flags
            format,                              // VkFormat                            format
            VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits            samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,         // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_STORE,        // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_CLEAR,         // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_STORE,        // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL // VkImageLayout                    finalLayout
        }};

    VkRenderPassCreateInfo2 renderPassInfo = initVulkanStructure();
    renderPassInfo.attachmentCount         = 2u;
    renderPassInfo.pAttachments            = attachments;
    renderPassInfo.subpassCount            = 1u;
    renderPassInfo.pSubpasses              = &subpassDescription;

    if (!enableDepthStencilWrite)
    {
        // when we are not writing to DS we create renderpass that will just do resolve

        attachments[0].loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[0].finalLayout   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        attachments[1].loadOp        = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    return createRenderPass2(vk, device, &renderPassInfo);
}

tcu::TestStatus ResolveNonPresentAspectTestInstance::iterate(void)
{
    // This code is used to generate two tests: for depth resolve mode and for stencil resolve mode
    // When testing depth three depth-only images are created (no stencil aspect) - two single sampled, one 4xMSAA
    // Triangle is rendered to the msaa image using renderpassA objects
    // Msaa image is resolved to the single sampled image using VkSubpassDescriptionDepthStencilResolve
    // - Set the stencil resolve mode to a supported resolve mode that isn't NONE
    // - Set the depth resolve mode to NONE if possible
    // RenderpassB is used to verify that the image rendered correctly in the the msaa image

    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkDevice device                 = m_context.getDevice();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    Allocator &memAlloc                   = m_context.getDefaultAllocator();
    const uint32_t queueFamilyIndex       = m_context.getUniversalQueueFamilyIndex();

    const bool tryResolvingStencil = (m_testType == MiscTestType::RESOLVE_STENCIL_ASPECT_THAT_IS_NOT_PRESENT);
    const uint32_t renderSize      = 16;
    const VkExtent3D extent        = makeExtent3D(renderSize, renderSize, 1u);

    // When testing resolving non-existing depth aspect we set the depth resolve mode to SAMPLE_ZERO.
    VkFormat testFormat                   = VK_FORMAT_S8_UINT;
    VkResolveModeFlagBits usedResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

    const VkImageAspectFlags imageAspect =
        tryResolvingStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
    const VkImageSubresourceRange srr  = makeImageSubresourceRange(imageAspect, 0u, 1u, 0u, 1u);
    const VkImageSubresourceLayers srl = makeImageSubresourceLayers(imageAspect, 0u, 0u, 1u);
    const std::vector<VkViewport> viewports{makeViewport(renderSize, renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(renderSize, renderSize)};

    if (tryResolvingStencil)
    {
        VkFormat depthFormat(VK_FORMAT_D16_UNORM);
        const VkFormatFeatureFlags requirements(VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
        const auto d16Properties(getPhysicalDeviceFormatProperties(vki, physicalDevice, depthFormat));

        if ((d16Properties.optimalTilingFeatures & requirements) != requirements)
            depthFormat = VK_FORMAT_D32_SFLOAT;

        testFormat = depthFormat;
    }

    // Create three images - one 4xMSAA and two single sampled
    VkImageUsageFlags imageUsage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //    VkStructureType            sType
        nullptr,                             //    const void*                pNext
        0u,                                  //    VkImageCreateFlags        flags
        VK_IMAGE_TYPE_2D,                    //    VkImageType                imageType
        testFormat,                          //    VkFormat                format
        extent,                              //    VkExtent3D                extent
        1u,                                  //    uint32_t                mipLevels
        1u,                                  //    uint32_t                arrayLayers
        VK_SAMPLE_COUNT_4_BIT,               //    VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,             //    VkImageTiling            tiling
        imageUsage,                          //    VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,           //    VkSharingMode            sharingMode
        0u,                                  //    uint32_t                queueFamilyIndexCount
        nullptr,                             //    const uint32_t*            pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,           //    VkImageLayout            initialLayout
    };
    ImageWithMemory multisampledImage(vk, device, memAlloc, imageCreateInfo, MemoryRequirement::Any);
    Move<VkImageView> multisampledImageView(
        makeImageView(vk, device, *multisampledImage, VK_IMAGE_VIEW_TYPE_2D, testFormat, srr));
    ImageWithBuffer singlesampledImageA(vk, device, memAlloc, extent, testFormat, imageUsage, VK_IMAGE_TYPE_2D, srr);
    ImageWithBuffer singlesampledImageB(vk, device, memAlloc, extent, testFormat, imageUsage, VK_IMAGE_TYPE_2D, srr);

    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    VkPipelineMultisampleStateCreateInfo multisampleState       = initVulkanStructure();
    multisampleState.rasterizationSamples                       = VK_SAMPLE_COUNT_4_BIT;
    multisampleState.minSampleShading                           = 1.0f;

    // define DepthStencilState so that we can write to depth and stencil attachments
    const VkStencilOpState stencilOpState{
        VK_STENCIL_OP_KEEP,                // VkStencilOp                                failOp
        VK_STENCIL_OP_INCREMENT_AND_CLAMP, // VkStencilOp                                passOp
        VK_STENCIL_OP_KEEP,                // VkStencilOp                                depthFailOp
        VK_COMPARE_OP_ALWAYS,              // VkCompareOp                                compareOp
        0xffu,                             // uint32_t                                    compareMask
        0xffu,                             // uint32_t                                    writeMask
        0                                  // uint32_t                                    reference
    };
    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                    // const void*                                pNext
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags    flags
        tryResolvingStencil,   // VkBool32                                    depthTestEnable
        tryResolvingStencil,   // VkBool32                                    depthWriteEnable
        VK_COMPARE_OP_GREATER, // VkCompareOp                                depthCompareOp
        VK_FALSE,              // VkBool32                                    depthBoundsTestEnable
        !tryResolvingStencil,  // VkBool32                                    stencilTestEnable
        stencilOpState,        // VkStencilOpState                            front
        stencilOpState,        // VkStencilOpState                            back
        0.0f,                  // float                                    minDepthBounds
        1.0f,                  // float                                    maxDepthBounds
    };

    VkClearValue clearValues[2];
    clearValues[0] = makeClearValueDepthStencil(0.0f, 1u); // clear value for mssa image
    clearValues[1] = makeClearValueDepthStencil(0.2f, 2u); // after resolve in renderpassA clear value should remain

    const VkBufferImageCopy copyRegion{
        0u,        // VkDeviceSize                bufferOffset
        0u,        // uint32_t                    bufferRowLength
        0u,        // uint32_t                    bufferImageHeight
        srl,       // VkImageSubresourceLayers    imageSubresource
        {0, 0, 0}, // VkOffset3D                imageOffset
        extent     // VkExtent3D                imageExtent
    };

    const auto rect = makeRect2D(renderSize, renderSize);
    const auto inbetweanMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    const auto beforeCopyMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    const auto pipelineLayout = makePipelineLayout(vk, device);
    const auto vertModule     = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    const auto fragModule     = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"));

    // create renderpass, framebuffer and pipeline that will be used to resolve with writes enabled.
    VkImageView imageViewsRaw[] = {*multisampledImageView, singlesampledImageA.getImageView()};
    const auto renderPassA      = createDepthPass(true, testFormat, imageAspect, usedResolveMode, usedResolveMode);
    const auto framebufferA     = makeFramebuffer(vk, device, *renderPassA, 2, imageViewsRaw, renderSize, renderSize);
    const auto pipelineA =
        makeGraphicsPipeline(vk, device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             *fragModule, *renderPassA, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0,
                             &vertexInputState, 0, &multisampleState, &depthStencilState);

    // create renderpass, framebufer and pipeline that will be used to resolve with writes disabled and loads.
    depthStencilState.depthWriteEnable = false;
    imageViewsRaw[1]                   = singlesampledImageB.getImageView();
    const auto renderPassB  = createDepthPass(false, testFormat, imageAspect, usedResolveMode, usedResolveMode);
    const auto framebufferB = makeFramebuffer(vk, device, *renderPassB, 2, imageViewsRaw, renderSize, renderSize);
    const auto pipelineB =
        makeGraphicsPipeline(vk, device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             *fragModule, *renderPassB, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0,
                             &vertexInputState, 0, &multisampleState, &depthStencilState);

    const auto cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    // render and try to resolve to aspect that is non present in the image - this should not crash
    beginRenderPass(vk, *cmdBuffer, *renderPassA, *framebufferA, rect, 2, clearValues);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineA);
    vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0u, 1u, &inbetweanMemoryBarrier, 0, 0, 0, 0);

    // resolve once again but this time we want existing aspect
    beginRenderPass(vk, *cmdBuffer, *renderPassB, *framebufferB, rect);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineB);
    vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          1u, &beforeCopyMemoryBarrier, 0, 0, 0, 0);

    // read singlesampled images to buffers
    vk.cmdCopyImageToBuffer(*cmdBuffer, singlesampledImageA.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            singlesampledImageA.getBuffer(), 1u, &copyRegion);
    vk.cmdCopyImageToBuffer(*cmdBuffer, singlesampledImageB.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            singlesampledImageB.getBuffer(), 1u, &copyRegion);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    auto &log                     = m_context.getTestContext().getLog();
    const auto &bufferAllocationA = singlesampledImageA.getBufferAllocation();
    const auto &bufferAllocationB = singlesampledImageB.getBufferAllocation();
    invalidateAlloc(vk, device, bufferAllocationA);
    invalidateAlloc(vk, device, bufferAllocationB);

    tcu::ConstPixelBufferAccess outA(mapVkFormat(testFormat), renderSize, renderSize, 1,
                                     bufferAllocationA.getHostPtr());
    tcu::ConstPixelBufferAccess outB(mapVkFormat(testFormat), renderSize, renderSize, 1,
                                     bufferAllocationB.getHostPtr());

    if (m_testType == MiscTestType::RESOLVE_STENCIL_ASPECT_THAT_IS_NOT_PRESENT)
    {
        const float expectedA = 0.60f;
        const float expectedB = 0.60f;
        const float epsilon   = 0.02f;

        // just check values in four bottom fragments (we rendered triangle)
        for (uint32_t x = 0; x < 2; ++x)
        {
            for (uint32_t y = renderSize - 1; y > renderSize - 3; --y)
            {
                // we resolved to sample 0 and can expect value set in shader
                float value = outA.getPixDepth(x, y);
                if (deFloatAbs(value - expectedA) > epsilon)
                {
                    log << tcu::TestLog::Message
                        << "Wrong value after resolving non-existing stencil aspect in renderpassA"
                           " - expected depth to contain: "
                        << expectedA << " got: " << value << " at (" << x << ", " << y << ")"
                        << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }

                // check if image resolved in renderpassB contains proper depth
                value = outB.getPixDepth(x, y);
                if (deFloatAbs(value - expectedB) > epsilon)
                {
                    log << tcu::TestLog::Message << "Wrong value after resolving depth in renderpassB - expected "
                        << expectedB << " got: " << value << " at (" << x << ", " << y << ")"
                        << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
            }
        }
    }
    else
    {
        const int expectedA = 2;
        const int expectedB = 3;

        // just check values in four bottom fragments (we rendered triangle)
        for (uint32_t x = 0; x < 2; ++x)
        {
            for (uint32_t y = renderSize - 1; y > renderSize - 3; --y)
            {
                // we resolved to sample 0 and can expect value set in shader
                int value = outA.getPixStencil(x, y);
                if (value != expectedA)
                {
                    log << tcu::TestLog::Message
                        << "Wrong value after resolving non-existing depth aspect in renderpassA"
                           " - expected stencil to contain: "
                        << expectedA << " got: " << value << " at (" << x << ", " << y << ")"
                        << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }

                // check if image resolved in renderpassB contains proper stencil
                value = outB.getPixStencil(x, y);
                if (value != expectedB)
                {
                    log << tcu::TestLog::Message << "Wrong value after resolving stencil in renderpassB - expected "
                        << expectedB << " got: " << value << " at (" << x << ", " << y << ")"
                        << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class MiscTestCase : public vkt::TestCase
{
public:
    MiscTestCase(tcu::TestContext &testCtx, const std::string &name, MiscTestType testType)
        : vkt::TestCase(testCtx, name)
        , m_testType(testType)
    {
    }
    virtual ~MiscTestCase(void) = default;

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    MiscTestType m_testType;
};

void MiscTestCase::checkSupport(Context &context) const
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const VkFormatFeatureFlags requirements =
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;

    context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");

    if (m_testType == MiscTestType::RESOLVE_STENCIL_ASPECT_THAT_IS_NOT_PRESENT)
    {
        // When testing resolving stencil we deliberately use depth only format
        const auto d16Properties = getPhysicalDeviceFormatProperties(vki, physicalDevice, VK_FORMAT_D16_UNORM);
        const auto d32Properties = getPhysicalDeviceFormatProperties(vki, physicalDevice, VK_FORMAT_D32_SFLOAT);

        if (((d16Properties.optimalTilingFeatures & requirements) != requirements) ||
            ((d32Properties.optimalTilingFeatures & requirements) != requirements))
            TCU_THROW(NotSupportedError, "Required depth format properties not supported");
    }
    else if (m_testType == MiscTestType::RESOLVE_DEPTH_ASPECT_THAT_IS_NOT_PRESENT)
    {
        // When testing resolving depth we deliberately use stencil only format
        const auto s8Properties = getPhysicalDeviceFormatProperties(vki, physicalDevice, VK_FORMAT_S8_UINT);
        if ((s8Properties.optimalTilingFeatures & requirements) != requirements)
            TCU_THROW(NotSupportedError, "Required stencil format properties not supported");
    }
}

void MiscTestCase::initPrograms(SourceCollections &programCollection) const
{
    if (m_testType == MiscTestType::PROPERTIES)
        return;

    programCollection.glslSources.add("vert")
        << glu::VertexSource("#version 450\n"
                             "void main (void)\n"
                             "{\n"
                             "  const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
                             "  const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
                             "  gl_Position = vec4(x, y, 0.6, 1.0);\n"
                             "}\n");

    programCollection.glslSources.add("frag") << glu::FragmentSource("#version 450\n"
                                                                     "void main (void)\n"
                                                                     "{\n"
                                                                     "}\n");
}

TestInstance *MiscTestCase::createInstance(Context &context) const
{
    if (m_testType == MiscTestType::RESOLVE_STENCIL_ASPECT_THAT_IS_NOT_PRESENT)
        return new ResolveNonPresentAspectTestInstance(context, m_testType);
    if (m_testType == MiscTestType::RESOLVE_DEPTH_ASPECT_THAT_IS_NOT_PRESENT)
        return new ResolveNonPresentAspectTestInstance(context, m_testType);
    return new PropertiesTestInstance(context);
}

void initTests(tcu::TestCaseGroup *group)
{
    typedef InstanceFactory1WithSupport<DepthStencilResolveTest, TestConfig, FunctionSupport0, Programs>
        DSResolveTestInstance;

    struct FormatData
    {
        VkFormat format;
        const char *name;
        bool hasDepth;
        bool hasStencil;
    };
    FormatData formats[] = {
        {VK_FORMAT_D16_UNORM, "d16_unorm", true, false},
        {VK_FORMAT_X8_D24_UNORM_PACK32, "x8_d24_unorm_pack32", true, false},
        {VK_FORMAT_D32_SFLOAT, "d32_sfloat", true, false},
        {VK_FORMAT_S8_UINT, "s8_uint", false, true},
        {VK_FORMAT_D16_UNORM_S8_UINT, "d16_unorm_s8_uint", true, true},
        {VK_FORMAT_D24_UNORM_S8_UINT, "d24_unorm_s8_uint", true, true},
        {VK_FORMAT_D32_SFLOAT_S8_UINT, "d32_sfloat_s8_uint", true, true},
    };

    struct ResolveModeData
    {
        VkResolveModeFlagBits flag;
        std::string name;
    };
    ResolveModeData resolveModes[] = {
        {VK_RESOLVE_MODE_NONE, "none"},           {VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, "zero"},
        {VK_RESOLVE_MODE_AVERAGE_BIT, "average"}, {VK_RESOLVE_MODE_MIN_BIT, "min"},
        {VK_RESOLVE_MODE_MAX_BIT, "max"},
    };

    struct ImageTestData
    {
        const char *groupName;
        uint32_t width;
        uint32_t height;
        uint32_t imageLayers;
        VkRect2D renderArea;
        VkClearDepthStencilValue clearValue;
    };

    // NOTE: tests cant be executed for 1D and 3D images:
    // 1D images are not tested because acording to specification sampleCounts
    // will be set to VK_SAMPLE_COUNT_1_BIT when type is not VK_IMAGE_TYPE_2D
    // 3D images are not tested because VkFramebufferCreateInfo specification
    // states that: each element of pAttachments that is a 2D or 2D array image
    // view taken from a 3D image must not be a depth/stencil format
    ImageTestData imagesTestData[] = {
        {"image_2d_32_32", 32, 32, 1, {{0, 0}, {32, 32}}, {0.000f, 0x00}},
        {"image_2d_8_32", 8, 32, 1, {{1, 1}, {6, 30}}, {0.123f, 0x01}},
        {"image_2d_49_13", 49, 13, 1, {{10, 5}, {20, 8}}, {1.000f, 0x05}},
        {"image_2d_5_1", 5, 1, 1, {{0, 0}, {5, 1}}, {0.500f, 0x00}},
        {"image_2d_17_1", 17, 1, 1, {{1, 0}, {15, 1}}, {0.789f, 0xfa}},
    };
    const uint32_t sampleCounts[]       = {2u, 4u, 8u, 16u, 32u, 64u};
    const float depthExpectedValue[][6] = {
        // 2 samples    4            8            16            32            64
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},            // RESOLVE_MODE_NONE - expect clear value
        {0.04f, 0.04f, 0.04f, 0.04f, 0.04f, 0.04f},      // RESOLVE_MODE_SAMPLE_ZERO_BIT
        {0.03f, 0.135f, 0.135f, 0.135f, 0.135f, 0.135f}, // RESOLVE_MODE_AVERAGE_BIT
        {0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f},      // RESOLVE_MODE_MIN_BIT
        {0.04f, 0.32f, 0.32f, 0.32f, 0.32f, 0.32f},      // RESOLVE_MODE_MAX_BIT
    };
    const uint8_t stencilExpectedValue[][6] = {
        // 2 samples    4        8        16        32        64
        {0u, 0u, 0u, 0u, 0u, 0u},             // RESOLVE_MODE_NONE - expect clear value
        {1u, 1u, 1u, 1u, 1u, 1u},             // RESOLVE_MODE_SAMPLE_ZERO_BIT
        {0u, 0u, 0u, 0u, 0u, 0u},             // RESOLVE_MODE_AVERAGE_BIT - not supported
        {1u, 1u, 1u, 1u, 1u, 1u},             // RESOLVE_MODE_MIN_BIT
        {255u, 255u, 255u, 255u, 255u, 255u}, // RESOLVE_MODE_MAX_BIT
    };

    const DepthCompatibilityManager compatManager;

    tcu::TestContext &testCtx(group->getTestContext());

    // Misc tests.
    {
        // Miscellaneous depth/stencil resolve tests
        de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc"));
        // Check reported depth/stencil resolve properties
        miscGroup->addChild(new MiscTestCase(testCtx, "properties", MiscTestType::PROPERTIES));
        // Test resolving aspects that aren't present
        miscGroup->addChild(new MiscTestCase(testCtx, "resolve_stencil_aspect_that_is_not_present",
                                             MiscTestType::RESOLVE_STENCIL_ASPECT_THAT_IS_NOT_PRESENT));
        miscGroup->addChild(new MiscTestCase(testCtx, "resolve_depth_aspect_that_is_not_present",
                                             MiscTestType::RESOLVE_DEPTH_ASPECT_THAT_IS_NOT_PRESENT));
        group->addChild(miscGroup.release());
    }

    // iterate over image data
    for (uint32_t imageDataNdx = 0; imageDataNdx < DE_LENGTH_OF_ARRAY(imagesTestData); imageDataNdx++)
    {
        ImageTestData imageData = imagesTestData[imageDataNdx];

        // create test group for image data
        de::MovePtr<tcu::TestCaseGroup> imageGroup(new tcu::TestCaseGroup(testCtx, imageData.groupName));

        // iterate over sampleCounts
        for (size_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
        {
            const uint32_t sampleCount(sampleCounts[sampleCountNdx]);
            const std::string sampleName("samples_" + de::toString(sampleCount));

            // create test group for sample count
            de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, sampleName.c_str()));

            // iterate over depth/stencil formats
            for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
            {
                const FormatData &formatData = formats[formatNdx];
                VkFormat format              = formatData.format;
                const char *formatName       = formatData.name;
                const bool hasDepth          = formatData.hasDepth;
                const bool hasStencil        = formatData.hasStencil;
                VkImageAspectFlags aspectFlags =
                    (hasDepth * VK_IMAGE_ASPECT_DEPTH_BIT) | (hasStencil * VK_IMAGE_ASPECT_STENCIL_BIT);
                const int separateLayoutsLoopCount = (hasDepth && hasStencil) ? 2 : 1;

                for (int separateDepthStencilLayouts = 0; separateDepthStencilLayouts < separateLayoutsLoopCount;
                     ++separateDepthStencilLayouts)
                {
                    const bool useSeparateDepthStencilLayouts = bool(separateDepthStencilLayouts);
                    const std::string groupName =
                        std::string(formatName) + ((useSeparateDepthStencilLayouts) ? "_separate_layouts" : "");

                    // create test group for format
                    de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));

                    // iterate over depth resolve modes
                    for (size_t depthResolveModeNdx = 0; depthResolveModeNdx < DE_LENGTH_OF_ARRAY(resolveModes);
                         depthResolveModeNdx++)
                    {
                        // iterate over stencil resolve modes
                        for (size_t stencilResolveModeNdx = 0; stencilResolveModeNdx < DE_LENGTH_OF_ARRAY(resolveModes);
                             stencilResolveModeNdx++)
                        {
                            for (int unusedIdx = 0; unusedIdx < 2; ++unusedIdx)
                            {
                                // there is no average resolve mode for stencil - go to next iteration
                                ResolveModeData &sResolve = resolveModes[stencilResolveModeNdx];
                                if (sResolve.flag == VK_RESOLVE_MODE_AVERAGE_BIT)
                                    continue;

                                // if pDepthStencilResolveAttachment is not NULL and does not have the value VK_ATTACHMENT_UNUSED,
                                // depthResolveMode and stencilResolveMode must not both be VK_RESOLVE_MODE_NONE_KHR
                                ResolveModeData &dResolve = resolveModes[depthResolveModeNdx];
                                if ((dResolve.flag == VK_RESOLVE_MODE_NONE) && (sResolve.flag == VK_RESOLVE_MODE_NONE))
                                    continue;

                                // If there is no depth, the depth resolve mode should be NONE, or
                                // match the stencil resolve mode.
                                if (!hasDepth && (dResolve.flag != VK_RESOLVE_MODE_NONE) &&
                                    (dResolve.flag != sResolve.flag))
                                    continue;

                                // If there is no stencil, the stencil resolve mode should be NONE, or
                                // match the depth resolve mode.
                                if (!hasStencil && (sResolve.flag != VK_RESOLVE_MODE_NONE) &&
                                    (dResolve.flag != sResolve.flag))
                                    continue;

                                const bool unusedResolve = (unusedIdx > 0);

                                std::string baseName = "depth_" + dResolve.name + "_stencil_" + sResolve.name;
                                if (unusedResolve)
                                    baseName += "_unused_resolve";

                                if (hasDepth)
                                {
                                    std::string name     = baseName + "_testing_depth";
                                    const char *testName = name.c_str();
                                    float expectedValue  = depthExpectedValue[depthResolveModeNdx][sampleCountNdx];

                                    const TestConfig testConfig = {format,
                                                                   imageData.width,
                                                                   imageData.height,
                                                                   1u,
                                                                   1u,
                                                                   0u,
                                                                   imageData.renderArea,
                                                                   aspectFlags,
                                                                   sampleCount,
                                                                   dResolve.flag,
                                                                   sResolve.flag,
                                                                   VB_DEPTH,
                                                                   imageData.clearValue,
                                                                   expectedValue,
                                                                   0u,
                                                                   useSeparateDepthStencilLayouts,
                                                                   unusedResolve,
                                                                   tcu::Nothing,
                                                                   false};
                                    formatGroup->addChild(
                                        new DSResolveTestInstance(testCtx, testName, testConfig, checkSupport));

                                    if (sampleCountNdx == 0 && imageDataNdx == 0 &&
                                        dResolve.flag != VK_RESOLVE_MODE_NONE)
                                    {
                                        const auto compatibleFormat = compatManager.getAlternativeFormat(format);

                                        if (compatibleFormat != VK_FORMAT_UNDEFINED)
                                        {
                                            std::string compatibilityTestName        = "compatibility_" + name;
                                            TestConfig compatibilityTestConfig       = testConfig;
                                            compatibilityTestConfig.compatibleFormat = tcu::just(compatibleFormat);

                                            formatGroup->addChild(
                                                new DSResolveTestInstance(testCtx, compatibilityTestName.c_str(),
                                                                          compatibilityTestConfig, checkSupport));
                                        }
                                    }
                                }
                                if (hasStencil)
                                {
                                    std::string name      = baseName + "_testing_stencil";
                                    const char *testName  = name.c_str();
                                    uint8_t expectedValue = stencilExpectedValue[stencilResolveModeNdx][sampleCountNdx];

                                    const TestConfig testConfig = {format,
                                                                   imageData.width,
                                                                   imageData.height,
                                                                   1u,
                                                                   1u,
                                                                   0u,
                                                                   imageData.renderArea,
                                                                   aspectFlags,
                                                                   sampleCount,
                                                                   dResolve.flag,
                                                                   sResolve.flag,
                                                                   VB_STENCIL,
                                                                   imageData.clearValue,
                                                                   0.0f,
                                                                   expectedValue,
                                                                   useSeparateDepthStencilLayouts,
                                                                   unusedResolve,
                                                                   tcu::Nothing,
                                                                   false};
                                    formatGroup->addChild(
                                        new DSResolveTestInstance(testCtx, testName, testConfig, checkSupport));

                                    if (dResolve.flag == VK_RESOLVE_MODE_SAMPLE_ZERO_BIT)
                                    {
                                        std::string samplemaskTestName  = name + "_samplemask";
                                        TestConfig samplemaskTestConfig = testConfig;
                                        samplemaskTestConfig.sampleMask = true;
                                        formatGroup->addChild(new DSResolveTestInstance(
                                            testCtx, samplemaskTestName.c_str(), samplemaskTestConfig, checkSupport));
                                    }

                                    // All formats with stencil and depth aspects have incompatible formats and sizes in the depth
                                    // aspect, so their only alternative is the VK_FORMAT_S8_UINT format. Finally, that stencil-only
                                    // format has no compatible formats that can be used.
                                    if (sampleCountNdx == 0 && imageDataNdx == 0 && hasDepth &&
                                        sResolve.flag != VK_RESOLVE_MODE_NONE)
                                    {
                                        std::string compatibilityTestName        = "compatibility_" + name;
                                        TestConfig compatibilityTestConfig       = testConfig;
                                        compatibilityTestConfig.compatibleFormat = tcu::just(VK_FORMAT_S8_UINT);

                                        formatGroup->addChild(
                                            new DSResolveTestInstance(testCtx, compatibilityTestName.c_str(),
                                                                      compatibilityTestConfig, checkSupport));
                                    }
                                }
                            }
                        }
                    }
                    sampleGroup->addChild(formatGroup.release());
                }
            }

            imageGroup->addChild(sampleGroup.release());
        }

        group->addChild(imageGroup.release());
    }

    {
        // layered texture tests are done for all stencil modes and depth modes - not all combinations
        // Test checks if all layer are resolved in multi-layered framebuffer and if we can have a framebuffer
        // which starts at a layer other than zero. Both parts are tested together by rendering to layers
        // 4-6 and resolving to layers 1-3.
        ImageTestData layeredTextureTestData = {"image_2d_16_64_6", 16, 64, 6, {{10, 10}, {6, 54}}, {1.0f, 0x0}};

        de::MovePtr<tcu::TestCaseGroup> imageGroup(new tcu::TestCaseGroup(testCtx, layeredTextureTestData.groupName));

        for (size_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
        {
            const uint32_t sampleCount(sampleCounts[sampleCountNdx]);
            const std::string sampleName("samples_" + de::toString(sampleCount));

            // create test group for sample count
            de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, sampleName.c_str()));

            // iterate over depth/stencil formats
            for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
            {
                const FormatData &formatData = formats[formatNdx];
                VkFormat format              = formatData.format;
                const char *formatName       = formatData.name;
                const bool hasDepth          = formatData.hasDepth;
                const bool hasStencil        = formatData.hasStencil;
                VkImageAspectFlags aspectFlags =
                    (hasDepth * VK_IMAGE_ASPECT_DEPTH_BIT) | (hasStencil * VK_IMAGE_ASPECT_STENCIL_BIT);
                const int separateLayoutsLoopCount = (hasDepth && hasStencil) ? 2 : 1;

                for (int separateDepthStencilLayouts = 0; separateDepthStencilLayouts < separateLayoutsLoopCount;
                     ++separateDepthStencilLayouts)
                {
                    const bool useSeparateDepthStencilLayouts = bool(separateDepthStencilLayouts);
                    const std::string groupName =
                        std::string(formatName) + ((useSeparateDepthStencilLayouts) ? "_separate_layouts" : "");

                    // create test group for format
                    de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));

                    for (size_t resolveModeNdx = 0; resolveModeNdx < DE_LENGTH_OF_ARRAY(resolveModes); resolveModeNdx++)
                    {
                        for (int unusedIdx = 0; unusedIdx < 2; ++unusedIdx)
                        {
                            ResolveModeData &mode = resolveModes[resolveModeNdx];

                            const bool unusedResolve       = (unusedIdx > 0);
                            const std::string unusedSuffix = (unusedResolve ? "_unused_resolve" : "");

                            if (!hasStencil && mode.flag == VK_RESOLVE_MODE_NONE)
                                continue;

                            if (!hasDepth && mode.flag == VK_RESOLVE_MODE_NONE)
                                continue;

                            if (hasDepth)
                            {
                                std::string name            = "depth_" + mode.name + unusedSuffix;
                                const char *testName        = name.c_str();
                                float expectedValue         = depthExpectedValue[resolveModeNdx][sampleCountNdx];
                                const TestConfig testConfig = {format,
                                                               layeredTextureTestData.width,
                                                               layeredTextureTestData.height,
                                                               layeredTextureTestData.imageLayers,
                                                               3u,
                                                               0u,
                                                               layeredTextureTestData.renderArea,
                                                               aspectFlags,
                                                               sampleCount,
                                                               mode.flag,
                                                               VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
                                                               VB_DEPTH,
                                                               layeredTextureTestData.clearValue,
                                                               expectedValue,
                                                               0u,
                                                               useSeparateDepthStencilLayouts,
                                                               unusedResolve,
                                                               tcu::Nothing,
                                                               false};
                                formatGroup->addChild(
                                    new DSResolveTestInstance(testCtx, testName, testConfig, checkSupport));
                            }

                            // there is no average resolve mode for stencil - go to next iteration
                            if (mode.flag == VK_RESOLVE_MODE_AVERAGE_BIT)
                                continue;

                            if (hasStencil)
                            {
                                std::string name            = "stencil_" + mode.name + unusedSuffix;
                                const char *testName        = name.c_str();
                                uint8_t expectedValue       = stencilExpectedValue[resolveModeNdx][sampleCountNdx];
                                const TestConfig testConfig = {format,
                                                               layeredTextureTestData.width,
                                                               layeredTextureTestData.height,
                                                               layeredTextureTestData.imageLayers,
                                                               3u,
                                                               0u,
                                                               layeredTextureTestData.renderArea,
                                                               aspectFlags,
                                                               sampleCount,
                                                               VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
                                                               mode.flag,
                                                               VB_STENCIL,
                                                               layeredTextureTestData.clearValue,
                                                               0.0f,
                                                               expectedValue,
                                                               useSeparateDepthStencilLayouts,
                                                               unusedResolve,
                                                               tcu::Nothing,
                                                               false};
                                formatGroup->addChild(
                                    new DSResolveTestInstance(testCtx, testName, testConfig, checkSupport));
                            }
                        }
                    }
                    sampleGroup->addChild(formatGroup.release());
                }
            }
            imageGroup->addChild(sampleGroup.release());
        }

        group->addChild(imageGroup.release());
    }
}

} // namespace

tcu::TestCaseGroup *createRenderPass2DepthStencilResolveTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "depth_stencil_resolve", initTests);
}

} // namespace vkt
