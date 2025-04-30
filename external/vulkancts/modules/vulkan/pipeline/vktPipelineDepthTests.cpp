/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Depth Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDepthTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

enum class DepthClipControlCase
{
    DISABLED       = 0, // No depth clip control.
    NORMAL         = 1, // Depth clip control with static viewport.
    NORMAL_W       = 2, // Depth clip control with static viewport and .w different from 1.0f
    BEFORE_STATIC  = 3, // Set dynamic viewport state, then bind a static pipeline.
    BEFORE_DYNAMIC = 4, // Set dynamic viewport state, bind dynamic pipeline.
    BEFORE_TWO_DYNAMICS =
        5, // Set dynamic viewport state, bind dynamic pipeline with [0,1] view volume, then bind dynamic pipeline with [-1,1] view volume.
    AFTER_DYNAMIC = 6, // Bind dynamic pipeline, then set dynamic viewport state.
};

bool isSupportedDepthStencilFormat(const InstanceInterface &instanceInterface, VkPhysicalDevice device, VkFormat format)
{
    VkFormatProperties formatProps;

    instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

    return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u;
}

tcu::TestStatus testSupportsDepthStencilFormat(Context &context, VkFormat format)
{
    DE_ASSERT(vk::isDepthStencilFormat(format));

    if (isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), format))
        return tcu::TestStatus::pass("Format can be used in depth/stencil attachment");
    else
        return tcu::TestStatus::fail("Unsupported depth/stencil attachment format");
}

tcu::TestStatus testSupportsAtLeastOneDepthStencilFormat(Context &context, const std::vector<VkFormat> formats)
{
    std::ostringstream supportedFormatsMsg;
    bool pass = false;

    DE_ASSERT(!formats.empty());

    for (size_t formatNdx = 0; formatNdx < formats.size(); formatNdx++)
    {
        const VkFormat format = formats[formatNdx];

        DE_ASSERT(vk::isDepthStencilFormat(format));

        if (isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), format))
        {
            pass = true;
            supportedFormatsMsg << vk::getFormatName(format);

            if (formatNdx < formats.size() - 1)
                supportedFormatsMsg << ", ";
        }
    }

    if (pass)
        return tcu::TestStatus::pass(std::string("Supported depth/stencil formats: ") + supportedFormatsMsg.str());
    else
        return tcu::TestStatus::fail("All depth/stencil formats are unsupported");
}

class DepthTest : public vkt::TestCase
{
public:
    enum
    {
        QUAD_COUNT = 4
    };

    static const float quadDepths[QUAD_COUNT];
    static const float quadDepthsMinusOneToOne[QUAD_COUNT];
    static const float quadWs[QUAD_COUNT];

    DepthTest(tcu::TestContext &testContext, const std::string &name,
              const PipelineConstructionType pipelineConstructionType, const VkFormat depthFormat,
              const VkCompareOp depthCompareOps[QUAD_COUNT], const bool separateDepthStencilLayouts,
              const VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
              const bool depthBoundsTestEnable = false, const float depthBoundsMin = 0.0f,
              const float depthBoundsMax = 1.0f, const bool depthTestEnable = true,
              const bool stencilTestEnable = false, const bool depthAttachmentBound = true,
              const bool colorAttachmentEnable = true, const bool hostVisible = false,
              const tcu::UVec2 renderSize                 = tcu::UVec2(32, 32),
              const DepthClipControlCase depthClipControl = DepthClipControlCase::DISABLED,
              const bool useGeneralLayout                 = false);
    virtual ~DepthTest(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_depthFormat;
    const bool m_separateDepthStencilLayouts;
    VkPrimitiveTopology m_primitiveTopology;
    const bool m_depthBoundsTestEnable;
    const float m_depthBoundsMin;
    const float m_depthBoundsMax;
    const bool m_depthTestEnable;
    const bool m_stencilTestEnable;
    const bool m_depthAttachmentBound;
    const bool m_colorAttachmentEnable;
    const bool m_hostVisible;
    const tcu::UVec2 m_renderSize;
    const DepthClipControlCase m_depthClipControl;
    const bool m_useGeneralLayout;
    VkCompareOp m_depthCompareOps[QUAD_COUNT];
};

class DepthTestInstance : public vkt::TestInstance
{
public:
    DepthTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                      const VkFormat depthFormat, const VkCompareOp depthCompareOps[DepthTest::QUAD_COUNT],
                      const bool separateDepthStencilLayouts, const VkPrimitiveTopology primitiveTopology,
                      const bool depthBoundsTestEnable, const float depthBoundsMin, const float depthBoundsMax,
                      const bool depthTestEnable, const bool stencilTestEnable, const bool depthAttachmentBound,
                      const bool colorAttachmentEnable, const bool hostVisible, const tcu::UVec2 renderSize,
                      const DepthClipControlCase depthClipControl, const bool useGeneralLayout);

    virtual ~DepthTestInstance(void);
    virtual tcu::TestStatus iterate(void);

private:
    tcu::TestStatus verifyImage(void);

private:
    VkCompareOp m_depthCompareOps[DepthTest::QUAD_COUNT];
    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;
    const VkFormat m_depthFormat;
    const bool m_separateDepthStencilLayouts;
    VkPrimitiveTopology m_primitiveTopology;
    const bool m_depthBoundsTestEnable;
    const float m_depthBoundsMin;
    const float m_depthBoundsMax;
    const bool m_depthTestEnable;
    const bool m_stencilTestEnable;
    const bool m_depthAttachmentBound;
    const bool m_colorAttachmentEnable;
    const bool m_hostVisible;
    const DepthClipControlCase m_depthClipControl;
    const bool m_useGeneralLayout;
    VkImageSubresourceRange m_depthImageSubresourceRange;

    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImage> m_depthImage;
    de::MovePtr<Allocation> m_depthImageAlloc;
    Move<VkImageView> m_colorAttachmentView;
    Move<VkImageView> m_depthAttachmentView;
    RenderPassWrapper m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModule;

    Move<VkBuffer> m_vertexBuffer;
    std::vector<Vertex4RGBA> m_vertices;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    Move<VkBuffer> m_altVertexBuffer;
    std::vector<Vertex4RGBA> m_altVertices;
    de::MovePtr<Allocation> m_altVertexBufferAlloc;

    PipelineLayoutWrapper m_pipelineLayout;
    GraphicsPipelineWrapper m_graphicsPipelines[DepthTest::QUAD_COUNT];
    GraphicsPipelineWrapper m_altGraphicsPipelines[DepthTest::QUAD_COUNT];

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

const float DepthTest::quadDepths[QUAD_COUNT] = {0.1f, 0.0f, 0.3f, 0.2f};

// Depth values suitable for the depth range of -1..1.
const float DepthTest::quadDepthsMinusOneToOne[QUAD_COUNT] = {-0.8f, -1.0f, 0.6f, 0.2f};

const float DepthTest::quadWs[QUAD_COUNT] = {2.0f, 1.25f, 0.5f, 0.25f};

DepthTest::DepthTest(tcu::TestContext &testContext, const std::string &name,
                     const PipelineConstructionType pipelineConstructionType, const VkFormat depthFormat,
                     const VkCompareOp depthCompareOps[QUAD_COUNT], const bool separateDepthStencilLayouts,
                     const VkPrimitiveTopology primitiveTopology, const bool depthBoundsTestEnable,
                     const float depthBoundsMin, const float depthBoundsMax, const bool depthTestEnable,
                     const bool stencilTestEnable, const bool depthAttachmentBound, const bool colorAttachmentEnable,
                     const bool hostVisible, const tcu::UVec2 renderSize, const DepthClipControlCase depthClipControl,
                     const bool useGeneralLayout)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_depthFormat(depthFormat)
    , m_separateDepthStencilLayouts(separateDepthStencilLayouts)
    , m_primitiveTopology(primitiveTopology)
    , m_depthBoundsTestEnable(depthBoundsTestEnable)
    , m_depthBoundsMin(depthBoundsMin)
    , m_depthBoundsMax(depthBoundsMax)
    , m_depthTestEnable(depthTestEnable)
    , m_stencilTestEnable(stencilTestEnable)
    , m_depthAttachmentBound(depthAttachmentBound)
    , m_colorAttachmentEnable(colorAttachmentEnable)
    , m_hostVisible(hostVisible)
    , m_renderSize(renderSize)
    , m_depthClipControl(depthClipControl)
    , m_useGeneralLayout(useGeneralLayout)
{
    deMemcpy(m_depthCompareOps, depthCompareOps, sizeof(VkCompareOp) * QUAD_COUNT);
}

DepthTest::~DepthTest(void)
{
}

void DepthTest::checkSupport(Context &context) const
{
    if (m_depthBoundsTestEnable)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_BOUNDS);

    if (m_depthAttachmentBound &&
        !isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_depthFormat))
        throw tcu::NotSupportedError(std::string("Unsupported depth/stencil format: ") + getFormatName(m_depthFormat));

    if (m_separateDepthStencilLayouts &&
        !context.isDeviceFunctionalitySupported("VK_KHR_separate_depth_stencil_layouts"))
        TCU_THROW(NotSupportedError, "VK_KHR_separate_depth_stencil_layouts is not supported");

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
    if (m_depthClipControl != DepthClipControlCase::DISABLED &&
        !context.isDeviceFunctionalitySupported("VK_EXT_depth_clip_control"))
        TCU_THROW(NotSupportedError, "VK_EXT_depth_clip_control is not supported");
#endif // CTS_USES_VULKANSC
}

TestInstance *DepthTest::createInstance(Context &context) const
{
    return new DepthTestInstance(context, m_pipelineConstructionType, m_depthFormat, m_depthCompareOps,
                                 m_separateDepthStencilLayouts, m_primitiveTopology, m_depthBoundsTestEnable,
                                 m_depthBoundsMin, m_depthBoundsMax, m_depthTestEnable, m_stencilTestEnable,
                                 m_depthAttachmentBound, m_colorAttachmentEnable, m_hostVisible, m_renderSize,
                                 m_depthClipControl, m_useGeneralLayout);
}

void DepthTest::initPrograms(SourceCollections &programCollection) const
{
    if (m_colorAttachmentEnable)
    {
        programCollection.glslSources.add("color_vert")
            << glu::VertexSource("#version 310 es\n"
                                 "layout(location = 0) in vec4 position;\n"
                                 "layout(location = 1) in vec4 color;\n"
                                 "layout(location = 0) out highp vec4 vtxColor;\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "    gl_Position = position;\n"
                                 "    gl_PointSize = 1.0f;\n"
                                 "    vtxColor = color;\n"
                                 "}\n");

        programCollection.glslSources.add("color_frag")
            << glu::FragmentSource("#version 310 es\n"
                                   "layout(location = 0) in highp vec4 vtxColor;\n"
                                   "layout(location = 0) out highp vec4 fragColor;\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    fragColor = vtxColor;\n"
                                   "}\n");
    }
    else
    {
        programCollection.glslSources.add("color_vert") << glu::VertexSource("#version 310 es\n"
                                                                             "layout(location = 0) in vec4 position;\n"
                                                                             "layout(location = 1) in vec4 color;\n"
                                                                             "void main (void)\n"
                                                                             "{\n"
                                                                             "    gl_Position = position;\n"
                                                                             "    gl_PointSize = 1.0f;\n"
                                                                             "}\n");
    }
}

DepthTestInstance::DepthTestInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, const VkFormat depthFormat,
    const VkCompareOp depthCompareOps[DepthTest::QUAD_COUNT], const bool separateDepthStencilLayouts,
    const VkPrimitiveTopology primitiveTopology, const bool depthBoundsTestEnable, const float depthBoundsMin,
    const float depthBoundsMax, const bool depthTestEnable, const bool stencilTestEnable,
    const bool depthAttachmentBound, const bool colorAttachmentEnable, const bool hostVisible,
    const tcu::UVec2 renderSize, const DepthClipControlCase depthClipControl, const bool useGeneralLayout)
    : vkt::TestInstance(context)
    , m_renderSize(renderSize)
    , m_colorFormat(colorAttachmentEnable ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_UNDEFINED)
    , m_depthFormat(depthFormat)
    , m_separateDepthStencilLayouts(separateDepthStencilLayouts)
    , m_primitiveTopology(primitiveTopology)
    , m_depthBoundsTestEnable(depthBoundsTestEnable)
    , m_depthBoundsMin(depthBoundsMin)
    , m_depthBoundsMax(depthBoundsMax)
    , m_depthTestEnable(depthTestEnable)
    , m_stencilTestEnable(stencilTestEnable)
    , m_depthAttachmentBound(depthAttachmentBound)
    , m_colorAttachmentEnable(colorAttachmentEnable)
    , m_hostVisible(hostVisible)
    , m_depthClipControl(depthClipControl)
    , m_useGeneralLayout(useGeneralLayout)
    , m_graphicsPipelines{{context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType}}
    , m_altGraphicsPipelines{{context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                              context.getDevice(), m_context.getDeviceExtensions(), pipelineConstructionType},
                             {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                              context.getDevice(), m_context.getDeviceExtensions(), pipelineConstructionType},
                             {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                              context.getDevice(), m_context.getDeviceExtensions(), pipelineConstructionType},
                             {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                              context.getDevice(), m_context.getDeviceExtensions(), pipelineConstructionType}}
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    const bool hasDepthClipControl                = (m_depthClipControl != DepthClipControlCase::DISABLED);
    const bool useAltGraphicsPipelines            = (m_depthClipControl == DepthClipControlCase::BEFORE_TWO_DYNAMICS ||
                                          m_depthClipControl == DepthClipControlCase::NORMAL_W);
    const bool useAltVertices                     = m_depthClipControl == DepthClipControlCase::NORMAL_W;

    const VkImageLayout colorLayout =
        m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkImageLayout dsLayout =
        m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Copy depth operators
    deMemcpy(m_depthCompareOps, depthCompareOps, sizeof(VkCompareOp) * DepthTest::QUAD_COUNT);

    // Create color image
    if (m_colorAttachmentEnable)
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            nullptr,                                                               // const void* pNext;
            0u,                                                                    // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
            m_colorFormat,                                                         // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u},                              // VkExtent3D extent;
            1u,                                                                    // uint32_t mipLevels;
            1u,                                                                    // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            1u,                                                                    // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
        };

        m_colorImage = createImage(vk, vkDevice, &colorImageParams);

        // Allocate and bind color image memory
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));
    }

    // bind depth attachment or depth format should be undefined.
    DE_ASSERT(m_depthAttachmentBound || m_depthFormat == VK_FORMAT_UNDEFINED);

    // Create depth image
    if (m_depthAttachmentBound)
    {
        const VkImageCreateInfo depthImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,      // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                         // VkImageType imageType;
            m_depthFormat,                            // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u}, // VkExtent3D extent;
            1u,                                       // uint32_t mipLevels;
            1u,                                       // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                  // VkImageTiling tiling;
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                                     // VkSharingMode sharingMode;
            1u,                        // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
        };

        m_depthImage = createImage(vk, vkDevice, &depthImageParams);

        // Allocate and bind depth image memory
        auto memReqs = MemoryRequirement::Local | MemoryRequirement::HostVisible;
#ifdef CTS_USES_VULKANSC
        try
#endif // CTS_USES_VULKANSC
        {
            m_depthImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_depthImage),
                                                  m_hostVisible ? memReqs : MemoryRequirement::Any);
#ifdef CTS_USES_VULKANSC
        }
        catch (const tcu::NotSupportedError &)
        {
            // For VulkanSC, let this allocation fall back to any memory, to
            // avoid object counting getting out of sync between main and subprocess.
            m_depthImageAlloc =
                memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_depthImage), MemoryRequirement::Any);
#endif // CTS_USES_VULKANSC
        }
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_depthImage, m_depthImageAlloc->getMemory(),
                                    m_depthImageAlloc->getOffset()));

        const VkImageAspectFlags aspect = (mapVkFormat(m_depthFormat).order == tcu::TextureFormat::DS ?
                                               VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT :
                                               VK_IMAGE_ASPECT_DEPTH_BIT);
        m_depthImageSubresourceRange =
            makeImageSubresourceRange(aspect, 0u, depthImageParams.mipLevels, 0u, depthImageParams.arrayLayers);
    }

    // Create color attachment view
    if (m_colorAttachmentEnable)
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkImageViewCreateFlags flags;
            *m_colorImage,                              // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
            m_colorFormat,                              // VkFormat format;
            componentMappingRGBA,                       // VkComponentMapping components;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    // Create depth attachment view
    if (m_depthAttachmentBound)
    {
        const VkImageViewCreateInfo depthAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageViewCreateFlags flags;
            *m_depthImage,                            // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            m_depthFormat,                            // VkFormat format;
            componentMappingRGBA,                     // VkComponentMapping components;
            m_depthImageSubresourceRange,             // VkImageSubresourceRange subresourceRange;
        };

        m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
    }

    // Create render pass
    m_renderPass = RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat, m_depthFormat,
                                     VK_ATTACHMENT_LOAD_OP_CLEAR, colorLayout, dsLayout, colorLayout, dsLayout);

    // Create framebuffer
    {
        std::vector<VkImage> images;
        std::vector<VkImageView> attachmentBindInfos;

        if (m_colorAttachmentEnable)
        {
            images.push_back(*m_colorImage);
            attachmentBindInfos.push_back(*m_colorAttachmentView);
        }

        if (m_depthAttachmentBound)
        {
            images.push_back(*m_depthImage);
            attachmentBindInfos.push_back(*m_depthAttachmentView);
        }

        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            (uint32_t)attachmentBindInfos.size(),      // uint32_t attachmentCount;
            attachmentBindInfos.data(),                // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, images);
    }

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = PipelineLayoutWrapper(pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
    }

    // Shader modules
    m_vertexShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    if (m_colorAttachmentEnable)
        m_fragmentShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

    const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
    const std::vector<VkViewport> badViewports{makeViewport(0.0f, 0.0f, static_cast<float>(m_renderSize.x()) / 2.0f,
                                                            static_cast<float>(m_renderSize.y()) / 2.0f, 1.0f, 0.0f)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};
    const bool dynamicViewport =
        (static_cast<int>(m_depthClipControl) > static_cast<int>(DepthClipControlCase::BEFORE_STATIC));

    // Create pipeline
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription{
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBA),        // uint32_t strideInBytes;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate inputRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2]{
            {
                0u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                0u                             // uint32_t offset;
            },
            {
                1u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                offsetof(Vertex4RGBA, color),  // uint32_t offset;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t vertexAttributeDescriptionCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr,             // const void*                                    pNext
            0u,                  // VkPipelineInputAssemblyStateCreateFlags        flags
            m_primitiveTopology, // VkPrimitiveTopology                            topology
            VK_FALSE             // VkBool32                                        primitiveRestartEnable
        };

        VkPipelineDepthStencilStateCreateInfo depthStencilStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
            m_depthTestEnable,                                          // VkBool32 depthTestEnable;
            true,                                                       // VkBool32 depthWriteEnable;
            VK_COMPARE_OP_LESS,                                         // VkCompareOp depthCompareOp;
            m_depthBoundsTestEnable,                                    // VkBool32 depthBoundsTestEnable;
            m_stencilTestEnable,                                        // VkBool32 stencilTestEnable;
            // VkStencilOpState front;
            {
                VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
                VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
                VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
                VK_COMPARE_OP_NEVER, // VkCompareOp compareOp;
                0u,                  // uint32_t compareMask;
                0u,                  // uint32_t writeMask;
                0u,                  // uint32_t reference;
            },
            // VkStencilOpState back;
            {
                VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
                VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
                VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
                VK_COMPARE_OP_NEVER, // VkCompareOp compareOp;
                0u,                  // uint32_t compareMask;
                0u,                  // uint32_t writeMask;
                0u,                  // uint32_t reference;
            },
            m_depthBoundsMin, // float minDepthBounds;
            m_depthBoundsMax, // float maxDepthBounds;
        };

        // Make sure rasterization is not disabled when the fragment shader is missing.
        const vk::VkPipelineRasterizationStateCreateInfo rasterizationStateParams{
            vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                        // const void* pNext;
            0u,                                  // VkPipelineRasterizationStateCreateFlags flags;
            VK_FALSE,                            // VkBool32 depthClampEnable;
            VK_FALSE,                            // VkBool32 rasterizerDiscardEnable;
            vk::VK_POLYGON_MODE_FILL,            // VkPolygonMode polygonMode;
            vk::VK_CULL_MODE_NONE,               // VkCullModeFlags cullMode;
            vk::VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace frontFace;
            VK_FALSE,                            // VkBool32 depthBiasEnable;
            0.0f,                                // float depthBiasConstantFactor;
            0.0f,                                // float depthBiasClamp;
            0.0f,                                // float depthBiasSlopeFactor;
            1.0f,                                // float lineWidth;
        };

        PipelineViewportDepthClipControlCreateInfoWrapper depthClipControlWrapper;
        PipelineViewportDepthClipControlCreateInfoWrapper depthClipControl01Wrapper;

#ifndef CTS_USES_VULKANSC
        VkPipelineViewportDepthClipControlCreateInfoEXT depthClipControlCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT, // VkStructureType sType;
            nullptr,                                                                // const void* pNext;
            VK_TRUE,                                                                // VkBool32 negativeOneToOne;
        };
        if (hasDepthClipControl)
            depthClipControlWrapper.ptr = &depthClipControlCreateInfo;

        // Using the range 0,1 in the structure.
        VkPipelineViewportDepthClipControlCreateInfoEXT depthClipControlCreateInfo01{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT, // VkStructureType sType;
            nullptr,                                                                // const void* pNext;
            VK_FALSE,                                                               // VkBool32 negativeOneToOne;
        };
        depthClipControl01Wrapper.ptr = &depthClipControlCreateInfo01;
#endif // CTS_USES_VULKANSC

        // Dynamic viewport if needed.
        std::vector<VkDynamicState> dynamicStates;

        if (m_depthClipControl == DepthClipControlCase::BEFORE_DYNAMIC ||
            m_depthClipControl == DepthClipControlCase::BEFORE_TWO_DYNAMICS ||
            m_depthClipControl == DepthClipControlCase::AFTER_DYNAMIC)
        {
            dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        }

        const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                              // const void* pNext;
            0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
            static_cast<uint32_t>(dynamicStates.size()),          // uint32_t dynamicStateCount;
            de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
        };

        const vk::VkPipelineColorBlendAttachmentState blendState{
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        uint32_t colorAttachmentCount = (m_colorFormat != VK_FORMAT_UNDEFINED) ? 1u : 0u;

        const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr,                 // const void*                                    pNext
            0u,                      // VkPipelineColorBlendStateCreateFlags            flags
            VK_FALSE,                // VkBool32                                        logicOpEnable
            VK_LOGIC_OP_CLEAR,       // VkLogicOp                                    logicOp
            colorAttachmentCount,    // uint32_t                                        attachmentCount
            &blendState,             // const VkPipelineColorBlendAttachmentState*    pAttachments
            {0.0f, 0.0f, 0.0f, 0.0f} // float                                        blendConstants[4]
        };

        for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
        {
            depthStencilStateParams.depthCompareOp = depthCompareOps[quadNdx];

            m_graphicsPipelines[quadNdx]
                .setDefaultMultisampleState()
                .setDefaultColorBlendState()
                .setViewportStatePnext(depthClipControlWrapper.ptr)
                .setDynamicState(&dynamicStateCreateInfo)
                .setupVertexInputState(&vertexInputStateParams, &inputAssemblyStateParams)
                .setupPreRasterizationShaderState((dynamicViewport ? badViewports : viewports), scissors,
                                                  m_pipelineLayout, *m_renderPass, 0u, m_vertexShaderModule,
                                                  &rasterizationStateParams)
                .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule,
                                          &depthStencilStateParams)
                .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateCreateInfo)
                .setMonolithicPipelineLayout(m_pipelineLayout)
                .buildPipeline();

            if (useAltGraphicsPipelines)
            {
                if (m_depthClipControl == DepthClipControlCase::NORMAL_W)
                {
                    m_altGraphicsPipelines[quadNdx]
                        .setDefaultMultisampleState()
                        .setDefaultColorBlendState()
                        .setViewportStatePnext(depthClipControl01Wrapper.ptr)
                        .setDynamicState(&dynamicStateCreateInfo)
                        .setupVertexInputState(&vertexInputStateParams, &inputAssemblyStateParams)
                        .setupPreRasterizationShaderState((dynamicViewport ? badViewports : viewports), scissors,
                                                          m_pipelineLayout, *m_renderPass, 0u, m_vertexShaderModule,
                                                          &rasterizationStateParams)
                        .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule,
                                                  &depthStencilStateParams)
                        .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateCreateInfo)
                        .setMonolithicPipelineLayout(m_pipelineLayout)
                        .buildPipeline();
                }
                else
                {
                    m_altGraphicsPipelines[quadNdx]
                        .setDefaultMultisampleState()
                        .setDefaultColorBlendState()
                        .setViewportStatePnext(depthClipControl01Wrapper.ptr)
                        .setDynamicState(&dynamicStateCreateInfo)
                        .setupVertexInputState(&vertexInputStateParams)
                        .setupPreRasterizationShaderState((dynamicViewport ? badViewports : viewports), scissors,
                                                          m_pipelineLayout, *m_renderPass, 0u, m_vertexShaderModule,
                                                          &rasterizationStateParams)
                        .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule,
                                                  &depthStencilStateParams)
                        .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateCreateInfo)
                        .setMonolithicPipelineLayout(m_pipelineLayout)
                        .buildPipeline();
                }
            }
        }
    }

    // Create vertex buffer
    {
        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            1024u,                                // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_vertices          = createOverlappingQuads();
        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        if (useAltVertices)
        {
            m_altVertices          = createOverlappingQuads();
            m_altVertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
            m_altVertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_altVertexBuffer),
                                                       MemoryRequirement::HostVisible);

            VK_CHECK(vk.bindBufferMemory(vkDevice, *m_altVertexBuffer, m_altVertexBufferAlloc->getMemory(),
                                         m_altVertexBufferAlloc->getOffset()));
        }

        // Adjust depths
        for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
            for (int vertexNdx = 0; vertexNdx < 6; vertexNdx++)
            {
                m_vertices[quadNdx * 6 + vertexNdx].position.z() =
                    (hasDepthClipControl ? DepthTest::quadDepthsMinusOneToOne[quadNdx] :
                                           DepthTest::quadDepths[quadNdx]);
                if (m_depthClipControl == DepthClipControlCase::NORMAL_W)
                {
                    const float w = DepthTest::quadWs[quadNdx];
                    m_vertices[quadNdx * 6 + vertexNdx].position.x() *= w;
                    m_vertices[quadNdx * 6 + vertexNdx].position.y() *= w;
                    m_vertices[quadNdx * 6 + vertexNdx].position.z() *= w;
                    m_vertices[quadNdx * 6 + vertexNdx].position.w() = w;
                }
                if (useAltVertices)
                {
                    m_altVertices[quadNdx * 6 + vertexNdx].position = m_vertices[quadNdx * 6 + vertexNdx].position;
                    float z = m_altVertices[quadNdx * 6 + vertexNdx].position.z();
                    float w = m_altVertices[quadNdx * 6 + vertexNdx].position.w();
                    if (depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_NOT_EQUAL ||
                        depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_LESS ||
                        depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_LESS_OR_EQUAL)
                    {
                        z += 0.01f;
                    }
                    else if (depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_GREATER ||
                             depthCompareOps[quadNdx] == vk::VK_COMPARE_OP_GREATER_OR_EQUAL)
                    {
                        z -= 0.01f;
                    }
                    m_altVertices[quadNdx * 6 + vertexNdx].position.z() = (z + w) * 0.5f;
                }
            }

        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);

        if (useAltVertices)
        {
            deMemcpy(m_altVertexBufferAlloc->getHostPtr(), m_altVertices.data(),
                     m_altVertices.size() * sizeof(Vertex4RGBA));
            flushAlloc(vk, vkDevice, *m_altVertexBufferAlloc);
        }
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    {
        std::vector<VkClearValue> attachmentClearValues;

        if (m_colorAttachmentEnable)
            attachmentClearValues.push_back(defaultClearValue(m_colorFormat));

        if (m_depthAttachmentBound)
        {
            attachmentClearValues.push_back(defaultClearValue(m_depthFormat));
        }

        const VkImageMemoryBarrier colorBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType            sType;
            nullptr,                                    // const void*                pNext;
            (VkAccessFlags)0,                           // VkAccessFlags              srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags              dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout              oldLayout;
            colorLayout,                                // VkImageLayout              newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                   srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                   dstQueueFamilyIndex;
            *m_colorImage,                              // VkImage                    image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange    subresourceRange;
        };

        VkImageSubresourceRange depthBarrierSubresourceRange = m_depthImageSubresourceRange;
        VkImageLayout newLayout                              = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        if (m_separateDepthStencilLayouts)
        {
            depthBarrierSubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            newLayout                               = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        }
        if (m_useGeneralLayout)
            newLayout = dsLayout;

        const VkImageMemoryBarrier depthBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,       // VkStructureType            sType;
            nullptr,                                      // const void*                pNext;
            (VkAccessFlags)0,                             // VkAccessFlags              srcAccessMask;
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // VkAccessFlags              dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                    // VkImageLayout              oldLayout;
            newLayout,                                    // VkImageLayout              newLayout;
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                   srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                   dstQueueFamilyIndex;
            *m_depthImage,                                // VkImage                    image;
            depthBarrierSubresourceRange,                 // VkImageSubresourceRange    subresourceRange;
        };

        std::vector<VkImageMemoryBarrier> imageLayoutBarriers;

        if (m_colorAttachmentEnable)
            imageLayoutBarriers.push_back(colorBarrier);

        if (m_depthAttachmentBound)
        {
            imageLayoutBarriers.push_back(depthBarrier);
        }

        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                              (VkDependencyFlags)0, 0u, nullptr, 0u, nullptr, (uint32_t)imageLayoutBarriers.size(),
                              imageLayoutBarriers.data());

        m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
                           (uint32_t)attachmentClearValues.size(), attachmentClearValues.data());

        const VkDeviceSize quadOffset = (m_vertices.size() / DepthTest::QUAD_COUNT) * sizeof(Vertex4RGBA);

        for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
        {
            VkDeviceSize vertexBufferOffset = quadOffset * quadNdx;

            if (m_depthClipControl == DepthClipControlCase::NORMAL_W &&
                depthCompareOps[quadNdx] != vk::VK_COMPARE_OP_NEVER)
            {
                m_altGraphicsPipelines[quadNdx].bind(*m_cmdBuffer);
                vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_altVertexBuffer.get(), &vertexBufferOffset);
                vk.cmdDraw(*m_cmdBuffer, (uint32_t)(m_altVertices.size() / DepthTest::QUAD_COUNT), 1, 0, 0);
            }

            if (m_depthClipControl == DepthClipControlCase::BEFORE_STATIC ||
                m_depthClipControl == DepthClipControlCase::BEFORE_DYNAMIC ||
                m_depthClipControl == DepthClipControlCase::BEFORE_TWO_DYNAMICS)
            {
                if (vk::isConstructionTypeShaderObject(pipelineConstructionType))
                {
#ifndef CTS_USES_VULKANSC
                    vk.cmdSetViewportWithCount(*m_cmdBuffer, 1u, viewports.data());
#else
                    vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, 1u, viewports.data());
#endif
                }
                else
                {
                    vk.cmdSetViewport(*m_cmdBuffer, 0u, 1u, viewports.data());
                }
            }

            if (m_depthClipControl == DepthClipControlCase::BEFORE_TWO_DYNAMICS)
                m_altGraphicsPipelines[quadNdx].bind(*m_cmdBuffer);
            m_graphicsPipelines[quadNdx].bind(*m_cmdBuffer);

            if (m_depthClipControl == DepthClipControlCase::AFTER_DYNAMIC)
            {
                if (vk::isConstructionTypeShaderObject(pipelineConstructionType))
                {
#ifndef CTS_USES_VULKANSC
                    vk.cmdSetViewportWithCount(*m_cmdBuffer, 1u, viewports.data());
#else
                    vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, 1u, viewports.data());
#endif
                }
                else
                {
                    vk.cmdSetViewport(*m_cmdBuffer, 0u, 1u, viewports.data());
                }
            }

            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdDraw(*m_cmdBuffer, (uint32_t)(m_vertices.size() / DepthTest::QUAD_COUNT), 1, 0, 0);
        }

        m_renderPass.end(vk, *m_cmdBuffer);
        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

DepthTestInstance::~DepthTestInstance(void)
{
}

tcu::TestStatus DepthTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyImage();
}

tcu::TestStatus DepthTestInstance::verifyImage(void)
{
    const tcu::TextureFormat tcuColorFormat = mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM);
    const tcu::TextureFormat tcuDepthFormat =
        m_depthAttachmentBound ? mapVkFormat(m_depthFormat) : tcu::TextureFormat();

    const ColorVertexShader vertexShader;
    const ColorFragmentShader fragmentShader(tcuColorFormat, tcuDepthFormat,
                                             (m_depthClipControl != DepthClipControlCase::DISABLED));
    const rr::Program program(&vertexShader, &fragmentShader);
    ReferenceRenderer refRenderer(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
    bool colorCompareOk = false;
    bool depthCompareOk = false;

    // Render reference image
    {
        for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
        {
            // Set depth state
            rr::RenderState renderState(refRenderer.getViewportState(),
                                        m_context.getDeviceProperties().limits.subPixelPrecisionBits);
            renderState.fragOps.depthTestEnabled = m_depthTestEnable;
            renderState.fragOps.depthFunc        = mapVkCompareOp(m_depthCompareOps[quadNdx]);
            if (m_depthBoundsTestEnable)
            {
                renderState.fragOps.depthBoundsTestEnabled = true;
                renderState.fragOps.minDepthBound          = m_depthBoundsMin;
                renderState.fragOps.maxDepthBound          = m_depthBoundsMax;
            }

            refRenderer.draw(
                renderState, mapVkPrimitiveTopology(m_primitiveTopology),
                std::vector<Vertex4RGBA>(m_vertices.begin() + quadNdx * 6, m_vertices.begin() + (quadNdx + 1) * 6));
        }
    }

    // Compare color result with reference image
    if (m_colorAttachmentEnable)
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice vkDevice         = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::MovePtr<tcu::TextureLevel> result = readColorAttachment(
            vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize,
            m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        colorCompareOk = tcu::intThresholdPositionDeviationCompare(
            m_context.getTestContext().getLog(), "IntImageCompare", "Image comparison", refRenderer.getAccess(),
            result->getAccess(), tcu::UVec4(2, 2, 2, 2), tcu::IVec3(1, 1, 0), true, tcu::COMPARE_LOG_RESULT);
    }
    else
    {
        colorCompareOk = true;
    }

    // Compare depth result with reference image
    if (m_depthAttachmentBound)
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice vkDevice         = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::MovePtr<tcu::TextureLevel> result = readDepthAttachment(
            vk, vkDevice, queue, queueFamilyIndex, allocator, *m_depthImage, m_depthFormat, m_renderSize,
            m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        {
            de::MovePtr<tcu::TextureLevel> convertedReferenceLevel;
            tcu::Maybe<tcu::TextureFormat> convertedFormat;

            if (refRenderer.getDepthStencilAccess().getFormat().type == tcu::TextureFormat::UNSIGNED_INT_24_8_REV)
            {
                convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT24);
            }
            else if (refRenderer.getDepthStencilAccess().getFormat().type == tcu::TextureFormat::UNSIGNED_INT_16_8_8)
            {
                convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
            }
            else if (refRenderer.getDepthStencilAccess().getFormat().type ==
                     tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV)
            {
                convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
            }

            if (convertedFormat)
            {
                convertedReferenceLevel = de::MovePtr<tcu::TextureLevel>(
                    new tcu::TextureLevel(*convertedFormat, refRenderer.getDepthStencilAccess().getSize().x(),
                                          refRenderer.getDepthStencilAccess().getSize().y()));
                tcu::copy(convertedReferenceLevel->getAccess(), refRenderer.getDepthStencilAccess());
            }

            float depthThreshold = 0.0f;

            if (tcu::getTextureChannelClass(result->getFormat().type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
            {
                const tcu::IVec4 formatBits = tcu::getTextureFormatBitDepth(result->getFormat());
                depthThreshold              = 1.0f / static_cast<float>((1 << formatBits[0]) - 1);
            }
            else if (tcu::getTextureChannelClass(result->getFormat().type) == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
            {
                depthThreshold = 0.0000001f;
            }
            else
                TCU_FAIL("unrecognized format type class");

            depthCompareOk = tcu::floatThresholdCompare(
                m_context.getTestContext().getLog(), "DepthImageCompare", "Depth image comparison",
                convertedReferenceLevel ? convertedReferenceLevel->getAccess() : refRenderer.getDepthStencilAccess(),
                result->getAccess(), tcu::Vec4(depthThreshold, 0.0f, 0.0f, 0.0f), tcu::COMPARE_LOG_RESULT);
        }
    }
    else
    {
        depthCompareOk = true;
    }

    if (colorCompareOk && depthCompareOk)
        return tcu::TestStatus::pass("Result image matches reference");
    else
        return tcu::TestStatus::fail("Image mismatch");
}

std::string getFormatCaseName(const VkFormat format)
{
    const std::string fullName = getFormatName(format);

    DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

    return de::toLower(fullName.substr(10));
}

std::string getTopologyName(const VkPrimitiveTopology topology)
{
    const std::string fullName = getPrimitiveTopologyName(topology);

    DE_ASSERT(de::beginsWith(fullName, "VK_PRIMITIVE_TOPOLOGY_"));

    return de::toLower(fullName.substr(22));
}

std::string getCompareOpsName(const VkCompareOp quadDepthOps[DepthTest::QUAD_COUNT])
{
    std::ostringstream name;

    for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
    {
        const std::string fullOpName = getCompareOpName(quadDepthOps[quadNdx]);

        DE_ASSERT(de::beginsWith(fullOpName, "VK_COMPARE_OP_"));

        name << de::toLower(fullOpName.substr(14));

        if (quadNdx < DepthTest::QUAD_COUNT - 1)
            name << "_";
    }

    return name.str();
}

//
// Tests that do a layout change on the transfer queue for a depth/stencil image.
// This seems to create issues in some implementations.
//

struct TransferLayoutChangeParams
{
    TransferLayoutChangeParams(VkImageAspectFlags aspects_)
        : aspects(aspects_)
        , clearColor(0.0f, 0.0f, 0.0f, 1.0f)
        , geometryColor(0.0f, 0.0f, 1.0f, 1.0f)
        , clearDepth(0.0f)
        , geometryDepth(1.0f)
        , clearStencil(0u)
        , geometryStencil(255u)
    {
        // Make sure aspects only includes depth and/or stencil bits.
        const auto validBits   = (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        const auto invalidBits = ~validBits;
        DE_ASSERT((aspects_ & invalidBits) == 0u);
        DE_UNREF(invalidBits); // For release builds.
    }

    VkImageAspectFlags aspects;

    const tcu::Vec4 clearColor;
    const tcu::Vec4 geometryColor;
    const float clearDepth;
    const float geometryDepth;
    const uint32_t clearStencil;
    const uint32_t geometryStencil;
};

using TransferLayoutChangeParamsPtr = de::SharedPtr<TransferLayoutChangeParams>;

void transferLayoutChangePrograms(SourceCollections &dst, TransferLayoutChangeParamsPtr params)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "const float geometryDepth = " << params->geometryDepth << ";\n"
         << "const vec4 vertices[] = vec4[](\n"
         << "    vec4(-1.0, -1.0, geometryDepth, 1.0),\n"
         << "    vec4(-1.0,  3.0, geometryDepth, 1.0),\n"
         << "    vec4( 3.0, -1.0, geometryDepth, 1.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vertices[gl_VertexIndex % 3];\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4" << params->geometryColor << ";\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void transferLayoutChangeSupportCheck(Context &context, TransferLayoutChangeParamsPtr)
{
    // Will throw NotSupportedError if the queue does not exist.
    context.getTransferQueue();
}

// Find a suitable format for the depth/stencil buffer.
VkFormat chooseDepthStencilFormat(const InstanceInterface &vki, VkPhysicalDevice physDev)
{
    // The spec mandates support for one of these two formats.
    const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};

    // We will read from
    const auto requiredFeatures = (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
    const auto chosenFormat     = findFirstSupportedFormat(vki, physDev, requiredFeatures, ImageFeatureType::OPTIMAL,
                                                           std::begin(candidates), std::end(candidates));

    if (chosenFormat == VK_FORMAT_UNDEFINED)
        TCU_FAIL("No suitable depth/stencil format found");

    return chosenFormat;
}

tcu::TestStatus transferLayoutChangeTest(Context &context, TransferLayoutChangeParamsPtr params)
{
    const auto ctx         = context.getContextCommonData();
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto dsFormat    = chooseDepthStencilFormat(ctx.vki, ctx.physicalDevice);
    const auto xferUsage   = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | xferUsage);
    const auto dsUsage     = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | xferUsage);
    const tcu::IVec3 fbExtent(1, 1, 1);
    const auto fbExtentVk      = makeExtent3D(fbExtent);
    const auto dsAspects       = (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    const bool useDepth        = static_cast<bool>(params->aspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    const bool useStencil      = static_cast<bool>(params->aspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    const auto dsUsedAspects   = params->aspects;
    const auto dsUnusedAspects = (dsAspects & (~dsUsedAspects));

    const VkPipelineStageFlags xferStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const VkPipelineStageFlags dsStage =
        (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, fbExtentVk, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);
    const auto colorSRR = makeDefaultImageSubresourceRange();

    // Depth/stencil buffers (regular and staging).
    const VkImageCreateInfo dsCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        dsFormat,
        fbExtentVk,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        dsUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory dsBuffer(ctx.vkd, ctx.device, ctx.allocator, dsCreateInfo, MemoryRequirement::Any);
    ImageWithMemory dsStagingBuffer(ctx.vkd, ctx.device, ctx.allocator, dsCreateInfo, MemoryRequirement::Any);

    const auto dsSRR       = makeImageSubresourceRange(dsAspects, 0u, 1u, 0u, 1u);
    const auto dsView      = makeImageView(ctx.vkd, ctx.device, *dsBuffer, VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSRR);
    const auto dsUsedSRR   = makeImageSubresourceRange(dsUsedAspects, 0u, 1u, 0u, 1u);
    const auto dsUnusedSRR = makeImageSubresourceRange(dsUnusedAspects, 0u, 1u, 0u, 1u);
    const auto dsUsedSRL   = makeImageSubresourceLayers(dsUsedAspects, 0u, 0u, 1u);

    // Render pass and framebuffer. We'll clear the images from the transfer queue, so we load them here.
    const std::vector<VkImageView> fbViews{colorBuffer.getImageView(), *dsView};

    const std::vector<VkAttachmentDescription> attachmentDescriptions{
        makeAttachmentDescription(0u, colorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
                                  VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        makeAttachmentDescription(0u, dsFormat, VK_SAMPLE_COUNT_1_BIT,
                                  (useDepth ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
                                  (useDepth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE),
                                  (useStencil ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
                                  (useStencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE),
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
    };

    const std::vector<VkAttachmentReference> attachmentReferences{
        makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
    };

    const VkSubpassDescription subpassDescription = {
        0u,      VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 1u, &attachmentReferences.at(0u),
        nullptr, &attachmentReferences.at(1u),    0u, nullptr,
    };

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(attachmentDescriptions),
        de::dataOrNull(attachmentDescriptions),
        1u,
        &subpassDescription,
        0u,
        nullptr,
    };

    const auto renderPass  = createRenderPass(ctx.vkd, ctx.device, &renderPassCreateInfo);
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews),
                                             de::dataOrNull(fbViews), fbExtentVk.width, fbExtentVk.height);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Pipeline.
    const auto &binaries      = context.getBinaryCollection();
    const auto vertModule     = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule     = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    // Enable depth/stencil tests depending on the used aspects.
    const auto stencilOp = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP,
                                              VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, params->geometryStencil);

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        useDepth,
        useDepth,
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        useStencil,
        stencilOp,
        stencilOp,
        0.0f,
        1.0f,
    };

    const auto pipeline = makeGraphicsPipeline(
        ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputStateCreateInfo,
        nullptr, nullptr, &depthStencilStateCreateInfo);

    const auto xferQFIndex = context.getTransferQueueFamilyIndex();
    const auto xferQueue   = context.getTransferQueue();

    // Clear staging image on the universal queue first.
    // This is needed because we can't run vkCmdClearDepthStencilImage on the transfer queue due to VUID-vkCmdClearDepthStencilImage-commandBuffer-cmdpool.
    // Likewise, we cannot use vkCmdCopyBufferToImage as a workaround due to VUID-vkCmdCopyBufferToImage-commandBuffer-07737.
    // So we use a staging image that we clear on the universal queue, and then use vkCmdCopyImage on the transfer queue, which is legal.
    CommandPoolWithBuffer stagingCmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto stagingCmdBuffer = *stagingCmd.cmdBuffer;
    beginCommandBuffer(ctx.vkd, stagingCmdBuffer);
    {
        const auto preClearBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *dsStagingBuffer, dsSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, stagingCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, xferStage,
                                      &preClearBarrier);

        const auto dsClearValue = makeClearValueDepthStencil(params->clearDepth, params->clearStencil);
        ctx.vkd.cmdClearDepthStencilImage(stagingCmdBuffer, *dsStagingBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          &dsClearValue.depthStencil, 1u, &dsSRR);
    }

    // Barrier to transition layout and ownership of the staging image.
    const auto qfotStagingBarrier = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dsStagingBuffer, dsSRR, ctx.qfIndex, xferQFIndex);
    cmdPipelineImageMemoryBarrier(ctx.vkd, stagingCmdBuffer, xferStage, xferStage, &qfotStagingBarrier);

    endCommandBuffer(ctx.vkd, stagingCmdBuffer);

    CommandPoolWithBuffer xferCmd(ctx.vkd, ctx.device, xferQFIndex);
    const auto xferCmdBuffer = *xferCmd.cmdBuffer;
    beginCommandBuffer(ctx.vkd, xferCmdBuffer);

    // Acquire ownership of the staging image.
    cmdPipelineImageMemoryBarrier(ctx.vkd, xferCmdBuffer, xferStage, xferStage, &qfotStagingBarrier);

    // Transition layout of the final depth/stencil buffer before the copy.
    // This is the key and motivation to write this test: here we are transitioning the layout of a depth/stencil
    // image using the transfer queue.
    {
        const auto preCopyBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *dsBuffer, dsUsedSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, xferCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, xferStage,
                                      &preCopyBarrier);
    }
    {
        const auto zeroOffset        = makeOffset3D(0, 0, 0);
        const VkImageCopy copyRegion = {
            dsUsedSRL, zeroOffset, dsUsedSRL, zeroOffset, fbExtentVk,
        };
        ctx.vkd.cmdCopyImage(xferCmdBuffer, *dsStagingBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dsBuffer,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
    }
    const VkAccessFlags dsAccess =
        (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    const auto qfotBarrier = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, dsAccess, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, *dsBuffer, dsUsedSRR, xferQFIndex, ctx.qfIndex);

    // We need to zero-out some bitflags in the barrier and command because (a) it's recommended by the spec and (b)
    // because we can only include stages supported by the queue, and the xfer queue does not support the dst stage.
    auto qfotBarrierRelease          = qfotBarrier;
    qfotBarrierRelease.dstAccessMask = 0u;

    auto qfotBarrierAcquire          = qfotBarrier;
    qfotBarrierAcquire.srcAccessMask = 0u;

    // Release depth/stencil image to the universal queue.
    cmdPipelineImageMemoryBarrier(ctx.vkd, xferCmdBuffer, xferStage, 0u, &qfotBarrierRelease);

    endCommandBuffer(ctx.vkd, xferCmdBuffer);

    // Use the depth/stencil attachment normally in the universal queue.
    CommandPoolWithBuffer useCmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto drawCmdBuffer = *useCmd.cmdBuffer;
    beginCommandBuffer(ctx.vkd, drawCmdBuffer);

    // Acquire depth/stencil image.
    cmdPipelineImageMemoryBarrier(ctx.vkd, drawCmdBuffer, 0u, dsStage, &qfotBarrierAcquire);

    // Clear color image.
    {
        const auto preClearBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.getImage(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, drawCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, xferStage,
                                      &preClearBarrier);

        const auto colorClearValue = makeClearValueColorVec4(params->clearColor);
        ctx.vkd.cmdClearColorImage(drawCmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &colorClearValue.color, 1u, &colorSRR);

        const auto colorAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        const auto postClearBarrier =
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, colorAccess, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(), colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, drawCmdBuffer, xferStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      &postClearBarrier);
    }

    // Transition pending depth/stencil aspect if needed.
    if (dsUnusedAspects != 0u)
    {
        const auto extraBarrier =
            makeImageMemoryBarrier(0u, dsAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, *dsBuffer, dsUnusedSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, drawCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dsStage,
                                      &extraBarrier);
    }

    beginRenderPass(ctx.vkd, drawCmdBuffer, *renderPass, *framebuffer, scissors.at(0u));
    ctx.vkd.cmdBindPipeline(drawCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    ctx.vkd.cmdDraw(drawCmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(ctx.vkd, drawCmdBuffer);

    // Copy color, depth and stencil aspects to buffers.
    const auto fbExtent2D = fbExtent.swizzle(0, 1);
    copyImageToBuffer(ctx.vkd, drawCmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent2D);

    const auto tcuColorFormat   = mapVkFormat(colorFormat);
    const auto tcuDepthFormat   = getDepthCopyFormat(dsFormat);
    const auto tcuStencilFormat = getStencilCopyFormat(dsFormat);
    const auto pixelCount       = fbExtent.x() * fbExtent.y() * fbExtent.z();

    const auto depthVerifBufferSize = static_cast<VkDeviceSize>(pixelCount * tcu::getPixelSize(tcuDepthFormat));
    const auto depthVerifBufferInfo = makeBufferCreateInfo(depthVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory depthVerifBuffer(ctx.vkd, ctx.device, ctx.allocator, depthVerifBufferInfo,
                                      MemoryRequirement::HostVisible);

    const auto stencilVerifBufferSize = static_cast<VkDeviceSize>(pixelCount * tcu::getPixelSize(tcuStencilFormat));
    const auto stencilVerifBufferInfo = makeBufferCreateInfo(stencilVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory stencilVerifBuffer(ctx.vkd, ctx.device, ctx.allocator, stencilVerifBufferInfo,
                                        MemoryRequirement::HostVisible);

    copyImageToBuffer(ctx.vkd, drawCmdBuffer, *dsBuffer, *depthVerifBuffer, fbExtent2D, dsAccess,
                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_DEPTH_BIT,
                      VK_IMAGE_ASPECT_DEPTH_BIT);
    copyImageToBuffer(ctx.vkd, drawCmdBuffer, *dsBuffer, *stencilVerifBuffer, fbExtent2D, dsAccess,
                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_STENCIL_BIT,
                      VK_IMAGE_ASPECT_STENCIL_BIT);

    endCommandBuffer(ctx.vkd, drawCmdBuffer);

    // Two semaphores: from universal queue to transfer queue, and from transfer queue to universal queue.
    const auto uniToXferSem = createSemaphore(ctx.vkd, ctx.device);
    const auto xferToUniSem = createSemaphore(ctx.vkd, ctx.device);

    const VkSubmitInfo stagingSubmitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0u, nullptr, nullptr, 1u, &stagingCmdBuffer, 1u, &uniToXferSem.get(),
    };
    VK_CHECK(ctx.vkd.queueSubmit(ctx.queue, 1u, &stagingSubmitInfo, VK_NULL_HANDLE));

    const VkSubmitInfo xferSubmitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1u, &uniToXferSem.get(), &xferStage, 1u, &xferCmdBuffer, 1u,
        &xferToUniSem.get(),
    };
    VK_CHECK(ctx.vkd.queueSubmit(xferQueue, 1u, &xferSubmitInfo, VK_NULL_HANDLE));

    const VkSubmitInfo drawSubmitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1u, &xferToUniSem.get(), &dsStage, 1u, &drawCmdBuffer, 0u, nullptr,
    };
    const auto fence = createFence(ctx.vkd, ctx.device);
    VK_CHECK(ctx.vkd.queueSubmit(ctx.queue, 1u, &drawSubmitInfo, *fence));
    waitForFence(ctx.vkd, ctx.device, *fence);

    auto &log = context.getTestContext().getLog();

    // Check color buffer.
    {
        invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());

        tcu::TextureLevel refLevel(tcuColorFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
        tcu::PixelBufferAccess refAccess = refLevel.getAccess();
        tcu::clear(refAccess, params->geometryColor);

        tcu::ConstPixelBufferAccess resAccess(tcuColorFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

        const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

        if (!tcu::floatThresholdCompare(log, "Color", "", refAccess, resAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
            TCU_FAIL("Unexpected results in color buffer; check log for details;");
    }

    if (useDepth)
    {
        invalidateAlloc(ctx.vkd, ctx.device, depthVerifBuffer.getAllocation());

        tcu::TextureLevel refLevel(tcuDepthFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
        tcu::PixelBufferAccess refAccess = refLevel.getAccess();
        tcu::clearDepth(refAccess, params->geometryDepth);

        tcu::ConstPixelBufferAccess resAccess(tcuDepthFormat, fbExtent, depthVerifBuffer.getAllocation().getHostPtr());

        if (!tcu::dsThresholdCompare(log, "Depth", "", refAccess, resAccess, 0.0f, tcu::COMPARE_LOG_ON_ERROR))
            TCU_FAIL("Unexpected results in depth buffer; check log for details;");
    }

    if (useStencil)
    {
        invalidateAlloc(ctx.vkd, ctx.device, stencilVerifBuffer.getAllocation());

        tcu::TextureLevel refLevel(tcuStencilFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
        tcu::PixelBufferAccess refAccess = refLevel.getAccess();
        tcu::clearStencil(refAccess, static_cast<int>(params->geometryStencil));

        tcu::ConstPixelBufferAccess resAccess(tcuStencilFormat, fbExtent,
                                              stencilVerifBuffer.getAllocation().getHostPtr());

        if (!tcu::dsThresholdCompare(log, "Stencil", "", refAccess, resAccess, 0.0f, tcu::COMPARE_LOG_ON_ERROR))
            TCU_FAIL("Unexpected results in stencil buffer; check log for details;");
    }

    return tcu::TestStatus::pass("Pass");
}

// Tests for render passes which only update the depth buffer (i.e. deph-only prepasses or postpasses).
//
// When doing a depth pre-pass we'll:
// * Clear depth to 1.0.
// * Draw with depth 0.0 on the left side.
// * Clear color to 0,0,0,1.
// * Draw with depth to 0.5 on the whole framebuffer.
// * Only the right side should be drawn, so the end result is:
//   * Color attachment: left clear, right with color.
//   * Depth attachment: 0.0, 0.5 in the left and right sides, respectively.
//
// When doing a depth post-pass:
// * Clear depth and color to 1.0 and 0,0,0,1 respectively.
// * Draw on the left with depth 0.0.
// * Do not clear again, just load.
// * Draw full-screen with depth 0.5.
// * Only the right side should be drawn on the second draw, so the end result is:
//   * Color attachment: left with color, right clear.
//   * Depth attachment: 0.0, 0.5 in the left and right sides, respectively.

enum class DepthOnlyType
{
    SEPARATE_RENDER_PASSES = 0,
    SUBPASSES,
    DYNAMIC_RENDERING,
};

struct DepthOnlyParams
{
    PipelineConstructionType pipelineConstructionType;
    DepthOnlyType depthOnlyType;
    bool prepass; // true: pre-pass, false: post-pass.

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(8, 8, 1);
    }

    VkFormat getColorFormat() const
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    VkFormat getDepthFormat() const
    {
        return VK_FORMAT_D16_UNORM;
    }

    tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    tcu::Vec4 getGeomColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
};

class DepthOnlyInstance : public vkt::TestInstance
{
public:
    DepthOnlyInstance(Context &context, const DepthOnlyParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~DepthOnlyInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    DepthOnlyParams m_params;
};

class DepthOnlyCase : public vkt::TestCase
{
public:
    DepthOnlyCase(tcu::TestContext &testCtx, const std::string &name, const DepthOnlyParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DepthOnlyCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new DepthOnlyInstance(context, m_params);
    }

protected:
    DepthOnlyParams m_params;
};

void DepthOnlyCase::checkSupport(Context &context) const
{
    const auto ctx = context.getContextCommonData();

    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.pipelineConstructionType);

    if (m_params.depthOnlyType == DepthOnlyType::DYNAMIC_RENDERING)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

void DepthOnlyCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // We will work with a quad from (0,0) to (1,1) and apply a scale and offset to make it cover whichever part of the
    // screen we want. We will also overwrite the depth with a given value.
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (push_constant, std430) uniform PushConstantBlock {\n"
         << "    vec2 scale;\n"
         << "    vec2 offset;\n"
         << "    float depth;\n"
         << "} pc;\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(inPos.xy * pc.scale + pc.offset, pc.depth, 1.0);\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4" << m_params.getGeomColor() << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus DepthOnlyInstance::iterate(void)
{
    const auto ctx         = m_context.getContextCommonData();
    const auto extent      = m_params.getExtent();
    const auto extentVk    = makeExtent3D(extent);
    const auto colorFormat = m_params.getColorFormat();
    const auto depthFormat = m_params.getDepthFormat();
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto depthUsage  = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto imageType   = VK_IMAGE_TYPE_2D;
    const auto colorSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto depthSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL    = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto depthSRL    = makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u);
    const auto sampleCount = VK_SAMPLE_COUNT_1_BIT;
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto topology    = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    // These are used for several barriers and dependencies.
    const auto colorAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    const auto colorStage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto depthAccess =
        (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    const auto depthStage = (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

    // Color and depth buffers.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, colorFormat, colorUsage, imageType,
                                colorSRR);
    ImageWithBuffer depthBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, depthFormat, depthUsage, imageType,
                                depthSRR);

    Move<VkRenderPass> depthOnlyRenderPass;
    Move<VkRenderPass> depthColorRenderPass;
    Move<VkRenderPass> commonRenderPass;

    Move<VkFramebuffer> depthOnlyFramebuffer;
    Move<VkFramebuffer> depthColorFramebuffer;
    Move<VkFramebuffer> commonFramebuffer;

#ifndef CTS_USES_VULKANSC
    VkPipelineRenderingCreateInfo depthOnlyRenderingInfo  = initVulkanStructure();
    VkPipelineRenderingCreateInfo depthColorRenderingInfo = initVulkanStructure();
#endif // CTS_USES_VULKANSC

    if (m_params.depthOnlyType == DepthOnlyType::SEPARATE_RENDER_PASSES)
    {
        if (m_params.prepass)
        {
            // The first render pass will clear attachments and use only a depth attachment.
            depthOnlyRenderPass = makeRenderPass(ctx.vkd, ctx.device, VK_FORMAT_UNDEFINED, depthFormat);

            // The second render pass will load the depth attachment and clear the color attachment.
            const std::vector<VkAttachmentDescription> attDescs{
                makeAttachmentDescription(0u, colorFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                          VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                          VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
                makeAttachmentDescription(
                    0u, depthFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
            };

            const std::vector<VkAttachmentReference> attRefs{
                makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
                makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
            };

            const std::vector<VkSubpassDescription> subpasses{
                makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &attRefs.at(0u), nullptr, &attRefs.at(1u), 0u,
                                       nullptr),
            };

            const VkRenderPassCreateInfo rpInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                nullptr,
                0u,
                de::sizeU32(attDescs),
                de::dataOrNull(attDescs),
                de::sizeU32(subpasses),
                de::dataOrNull(subpasses),
                0u,
                nullptr,
            };

            depthColorRenderPass = createRenderPass(ctx.vkd, ctx.device, &rpInfo);
        }
        else
        {
            // The first render pass will clear both the color and depth attachments.
            depthColorRenderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat, depthFormat);

            // The second render pass will load and use the depth attachment only.
            depthOnlyRenderPass =
                makeRenderPass(ctx.vkd, ctx.device, VK_FORMAT_UNDEFINED, depthFormat, VK_ATTACHMENT_LOAD_OP_LOAD);
        }

        {
            const std::vector<VkImageView> imageViews{depthBuffer.getImageView()};
            depthOnlyFramebuffer = makeFramebuffer(ctx.vkd, ctx.device, *depthOnlyRenderPass, de::sizeU32(imageViews),
                                                   de::dataOrNull(imageViews), extentVk.width, extentVk.height);
        }
        {
            const std::vector<VkImageView> imageViews{colorBuffer.getImageView(), depthBuffer.getImageView()};
            depthColorFramebuffer = makeFramebuffer(ctx.vkd, ctx.device, *depthColorRenderPass, de::sizeU32(imageViews),
                                                    de::dataOrNull(imageViews), extentVk.width, extentVk.height);
        }
    }
    else if (m_params.depthOnlyType == DepthOnlyType::SUBPASSES)
    {
        const std::vector<VkAttachmentDescription> attDescs{
            makeAttachmentDescription(0u, colorFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                      VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                      VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
            makeAttachmentDescription(0u, depthFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                      VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                      VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
        };

        const VkSubpassDependency subpassDependency = {
            0u, 1u, depthStage, depthStage, depthAccess, depthAccess, VK_DEPENDENCY_BY_REGION_BIT,
        };

        if (m_params.prepass)
        {
            // The first subpass will use only a depth attachment.
            const std::vector<VkAttachmentReference> depthOnlySubpassRefs{
                makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
            };

            // The second one will use both color and depth.
            const std::vector<VkAttachmentReference> finalSubpassRefs{
                makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
                makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
            };

            const std::vector<VkSubpassDescription> subpasses{
                makeSubpassDescription(0u, bindPoint, 0u, nullptr, 0u, nullptr, nullptr, &depthOnlySubpassRefs.at(0u),
                                       0u, nullptr),
                makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &finalSubpassRefs.at(0u), nullptr,
                                       &finalSubpassRefs.at(1u), 0u, nullptr),
            };

            const VkRenderPassCreateInfo rpInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                nullptr,
                0u,
                de::sizeU32(attDescs),
                de::dataOrNull(attDescs),
                de::sizeU32(subpasses),
                de::dataOrNull(subpasses),
                1u,
                &subpassDependency,
            };

            commonRenderPass = createRenderPass(ctx.vkd, ctx.device, &rpInfo);
        }
        else
        {
            // The first subpass will use both color and depth,
            const std::vector<VkAttachmentReference> depthColorRefs{
                makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
                makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
            };

            // The second one will use depth only.
            const std::vector<VkAttachmentReference> depthOnlySubpassRefs{
                makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
            };

            const std::vector<VkSubpassDescription> subpasses{
                makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &depthColorRefs.at(0u), nullptr,
                                       &depthColorRefs.at(1u), 0u, nullptr),
                makeSubpassDescription(0u, bindPoint, 0u, nullptr, 0u, nullptr, nullptr, &depthOnlySubpassRefs.at(0u),
                                       0u, nullptr),
            };

            const VkRenderPassCreateInfo rpInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                nullptr,
                0u,
                de::sizeU32(attDescs),
                de::dataOrNull(attDescs),
                de::sizeU32(subpasses),
                de::dataOrNull(subpasses),
                1u,
                &subpassDependency,
            };

            commonRenderPass = createRenderPass(ctx.vkd, ctx.device, &rpInfo);
        }

        {
            const std::vector<VkImageView> imageViews{colorBuffer.getImageView(), depthBuffer.getImageView()};
            commonFramebuffer = makeFramebuffer(ctx.vkd, ctx.device, *commonRenderPass, de::sizeU32(imageViews),
                                                de::dataOrNull(imageViews), extentVk.width, extentVk.height);
        }
    }
#ifndef CTS_USES_VULKANSC
    else if (m_params.depthOnlyType == DepthOnlyType::DYNAMIC_RENDERING)
    {
        depthOnlyRenderingInfo = VkPipelineRenderingCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0u,
            0u,
            nullptr,
            depthFormat,
            VK_FORMAT_UNDEFINED,
        };

        depthColorRenderingInfo = VkPipelineRenderingCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0u,
            1u,
            &colorFormat,
            depthFormat,
            VK_FORMAT_UNDEFINED,
        };
    }
#endif // CTS_USES_VULKANSC
    else
        DE_ASSERT(false);

    // Prepare the pipelines.
    struct PushConstants
    {
        tcu::Vec2 scale;
        tcu::Vec2 offset;
        float depth;
    };

    const auto pcSize   = DE_SIZEOF32(PushConstants);
    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, ctx.vkd, ctx.device, VK_NULL_HANDLE,
                                               &pcRange);

    const VkPipelineRasterizationStateCreateInfo pipelineRasterizationInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_FALSE,
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_TRUE, // Enable depth test.
        VK_TRUE, // Enable depth writes.
        VK_COMPARE_OP_LESS,
        VK_FALSE,
        VK_FALSE,
        makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0u, 0u, 0u),
        makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0u, 0u, 0u),
        0.0f,
        0.0f,
    };

    const VkPipelineColorBlendStateCreateInfo depthOnlyColorBlendingState = initVulkanStructure();
    const VkPipelineColorBlendAttachmentState depthColorAttState          = {
        VK_FALSE,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_OP_ADD,
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT),
    };
    const VkPipelineColorBlendStateCreateInfo depthColorColorBlendingState = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_LOGIC_OP_CLEAR,
        1u,
        &depthColorAttState,
        {0.0f, 0.0f, 0.0f, 0.0f},
    };

    const auto &binaries = m_context.getBinaryCollection();
    const ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    const ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    const bool useSubpasses = (m_params.depthOnlyType == DepthOnlyType::SUBPASSES);
#ifndef CTS_USES_VULKANSC
    const bool useDynamicRendering = (m_params.depthOnlyType == DepthOnlyType::DYNAMIC_RENDERING);
#endif // CTS_USES_VULKANSC

    const auto depthOnlyPipelineRenderPass = (useSubpasses ? *commonRenderPass : *depthOnlyRenderPass);
    const auto depthOnlyPipelineSubpass    = (useSubpasses ? (m_params.prepass ? 0u : 1u) : 0u);

    const auto depthColorPipelineRenderPass = (useSubpasses ? *commonRenderPass : *depthColorRenderPass);
    const auto depthColorPipelineSubpass    = (useSubpasses ? (m_params.prepass ? 1u : 0u) : 0u);

    PipelineRenderingCreateInfoWrapper depthOnlyRenderingInfoPtr;
    PipelineRenderingCreateInfoWrapper depthColorRenderingInfoPtr;

#ifndef CTS_USES_VULKANSC
    if (useDynamicRendering)
    {
        depthOnlyRenderingInfoPtr.ptr  = &depthOnlyRenderingInfo;
        depthColorRenderingInfoPtr.ptr = &depthColorRenderingInfo;
    }
#endif // CTS_USES_VULKANSC

    GraphicsPipelineWrapper depthOnlyPipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                              m_context.getDeviceExtensions(), m_params.pipelineConstructionType);
    depthOnlyPipeline.setDefaultVertexInputState(true)
        .setDefaultTopology(topology)
        .setDefaultMultisampleState()
        .setupVertexInputState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, depthOnlyPipelineRenderPass,
                                          depthOnlyPipelineSubpass, vertShader, &pipelineRasterizationInfo,
                                          ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr, nullptr,
                                          depthOnlyRenderingInfoPtr)
        .setupFragmentShaderState(pipelineLayout, depthOnlyPipelineRenderPass, depthOnlyPipelineSubpass,
                                  ShaderWrapper(), &depthStencilState)
        .setupFragmentOutputState(depthOnlyPipelineRenderPass, depthOnlyPipelineSubpass, &depthOnlyColorBlendingState)
        .buildPipeline();

    GraphicsPipelineWrapper depthColorPipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                               m_context.getDeviceExtensions(), m_params.pipelineConstructionType);
    depthColorPipeline.setDefaultVertexInputState(true)
        .setDefaultTopology(topology)
        .setDefaultMultisampleState()
        .setupVertexInputState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, depthColorPipelineRenderPass,
                                          depthColorPipelineSubpass, vertShader, &pipelineRasterizationInfo,
                                          ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr, nullptr,
                                          depthColorRenderingInfoPtr)
        .setupFragmentShaderState(pipelineLayout, depthColorPipelineRenderPass, depthColorPipelineSubpass, fragShader,
                                  &depthStencilState)
        .setupFragmentOutputState(depthColorPipelineRenderPass, depthColorPipelineSubpass,
                                  &depthColorColorBlendingState)
        .buildPipeline();

    // Quad and vertex buffer.
    std::vector<tcu::Vec4> vertices;
    vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f);
    vertices.emplace_back(0.0f, 1.0f, 0.0f, 1.0f);
    vertices.emplace_back(1.0f, 0.0f, 0.0f, 1.0f);
    vertices.emplace_back(1.0f, 1.0f, 0.0f, 1.0f);

    const auto vertexBufferSize  = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertexBufferInfo  = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &bufferAlloc = vertexBuffer.getAllocation();
        memcpy(bufferAlloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
    }
    const VkDeviceSize vertexBufferOffset = 0ull;

    const auto depthBack  = 1.0f;
    const auto depthMid   = 0.5f;
    const auto depthFront = 0.0f;

    const auto depthClearValue = makeClearValueDepthStencil(depthBack, 0u);
    const auto colorClearValue = makeClearValueColor(m_params.getClearColor());

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    // Closure to draw on the left with a given depth.
    const auto drawLeft = [&](const float depth)
    {
        const PushConstants pcValues{
            tcu::Vec2(1.0f, 2.0f),
            tcu::Vec2(-1.0f, -1.0f),
            depth,
        };
        ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &pcValues);
        ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    };

    // Closure to draw full screen with a given depth.
    const auto drawFull = [&](const float depth)
    {
        const PushConstants pcValues{
            tcu::Vec2(2.0f, 2.0f),
            tcu::Vec2(-1.0f, -1.0f),
            depth,
        };
        ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &pcValues);
        ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
#ifndef CTS_USES_VULKANSC
    if (useDynamicRendering)
    {
        // Move images to the right layouts.
        {
            const std::vector<VkImageMemoryBarrier> barriers{
                makeImageMemoryBarrier(0u, colorAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(), colorSRR),
                makeImageMemoryBarrier(0u, depthAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depthBuffer.getImage(),
                                       depthSRR),
            };

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          (colorStage | depthStage), de::dataOrNull(barriers), barriers.size());
        }

        if (m_params.prepass)
        {
            const VkRenderingAttachmentInfo depthOnlyAttachmentInfo = {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                nullptr,
                depthBuffer.getImageView(),
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_RESOLVE_MODE_NONE,
                VK_NULL_HANDLE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                depthClearValue,
            };

            const VkRenderingInfo depthOnlyBeginRendering = {
                VK_STRUCTURE_TYPE_RENDERING_INFO, nullptr, 0u, scissors.at(0u), 1u, 0u, 0u, nullptr,
                &depthOnlyAttachmentInfo,         nullptr,
            };

            ctx.vkd.cmdBeginRendering(cmdBuffer, &depthOnlyBeginRendering);
            depthOnlyPipeline.bind(cmdBuffer);
            // Draw with depthFront on the left side.
            drawLeft(depthFront);
            ctx.vkd.cmdEndRendering(cmdBuffer);

            // Sync writes to depth attachment before the next render pass.
            {
                const auto barrier = makeMemoryBarrier(depthAccess, depthAccess);
                cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, depthStage, depthStage, &barrier);
            }

            const std::vector<VkRenderingAttachmentInfo> depthColorAttachmentInfos{
                VkRenderingAttachmentInfo{
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    nullptr,
                    colorBuffer.getImageView(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_RESOLVE_MODE_NONE,
                    VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    colorClearValue,
                },
                VkRenderingAttachmentInfo{
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, nullptr, depthBuffer.getImageView(),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_RESOLVE_MODE_NONE, VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
                    depthClearValue, // Not used.
                },
            };

            const VkRenderingInfo depthColorBeginRendering = {
                VK_STRUCTURE_TYPE_RENDERING_INFO,
                nullptr,
                0u,
                scissors.at(0u),
                1u,
                0u,
                1u,
                &depthColorAttachmentInfos.at(0u),
                &depthColorAttachmentInfos.at(1u),
                nullptr,
            };

            ctx.vkd.cmdBeginRendering(cmdBuffer, &depthColorBeginRendering);
            depthColorPipeline.bind(cmdBuffer);
            // Draw with depthMid on the whole framebuffer.
            drawFull(depthMid);
            ctx.vkd.cmdEndRendering(cmdBuffer);
        }
        else
        {
            const std::vector<VkRenderingAttachmentInfo> depthColorAttachmentInfos{
                VkRenderingAttachmentInfo{
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    nullptr,
                    colorBuffer.getImageView(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_RESOLVE_MODE_NONE,
                    VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    colorClearValue,
                },
                VkRenderingAttachmentInfo{
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    nullptr,
                    depthBuffer.getImageView(),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_RESOLVE_MODE_NONE,
                    VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    depthClearValue,
                },
            };

            const VkRenderingInfo depthColorBeginRendering = {
                VK_STRUCTURE_TYPE_RENDERING_INFO,
                nullptr,
                0u,
                scissors.at(0u),
                1u,
                0u,
                1u,
                &depthColorAttachmentInfos.at(0u),
                &depthColorAttachmentInfos.at(1u),
                nullptr,
            };

            ctx.vkd.cmdBeginRendering(cmdBuffer, &depthColorBeginRendering);
            depthColorPipeline.bind(cmdBuffer);
            // Draw with depthFront on the left side.
            drawLeft(depthFront);
            ctx.vkd.cmdEndRendering(cmdBuffer);

            // Sync writes to depth attachment before the next render pass.
            {
                const auto barrier = makeMemoryBarrier(depthAccess, depthAccess);
                cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, depthStage, depthStage, &barrier);
            }

            const VkRenderingAttachmentInfo depthOnlyAttachmentInfo = {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                nullptr,
                depthBuffer.getImageView(),
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_RESOLVE_MODE_NONE,
                VK_NULL_HANDLE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ATTACHMENT_LOAD_OP_LOAD,
                VK_ATTACHMENT_STORE_OP_STORE,
                depthClearValue, // Not used.
            };

            const VkRenderingInfo depthOnlyBeginRendering = {
                VK_STRUCTURE_TYPE_RENDERING_INFO, nullptr, 0u, scissors.at(0u), 1u, 0u, 0u, nullptr,
                &depthOnlyAttachmentInfo,         nullptr,
            };

            ctx.vkd.cmdBeginRendering(cmdBuffer, &depthOnlyBeginRendering);
            depthOnlyPipeline.bind(cmdBuffer);
            // Draw with depthMid on the whole framebuffer.
            drawFull(depthMid);
            ctx.vkd.cmdEndRendering(cmdBuffer);
        }
    }
    else
#endif // CTS_USES_VULKANSC
        if (useSubpasses)
        {
            const std::vector<VkClearValue> clearValues{
                colorClearValue,
                depthClearValue,
            };
            beginRenderPass(ctx.vkd, cmdBuffer, *commonRenderPass, *commonFramebuffer, scissors.at(0u),
                            de::sizeU32(clearValues), de::dataOrNull(clearValues));

            if (m_params.prepass)
            {
                depthOnlyPipeline.bind(cmdBuffer);
                // Draw with depthFront on the left side.
                drawLeft(depthFront);
                ctx.vkd.cmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
                depthColorPipeline.bind(cmdBuffer);
                // Draw with depthMid on the whole framebuffer.
                drawFull(depthMid);
            }
            else
            {
                depthColorPipeline.bind(cmdBuffer);
                // Draw with depthFront on the left side.
                drawLeft(depthFront);
                ctx.vkd.cmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
                depthOnlyPipeline.bind(cmdBuffer);
                // Draw with depthMid on the whole framebuffer.
                drawFull(depthMid);
            }

            ctx.vkd.cmdEndRenderPass(cmdBuffer);
        }
        else if (m_params.depthOnlyType == DepthOnlyType::SEPARATE_RENDER_PASSES)
        {
            if (m_params.prepass)
            {
                beginRenderPass(ctx.vkd, cmdBuffer, *depthOnlyRenderPass, *depthOnlyFramebuffer, scissors.at(0u),
                                depthClearValue);
                depthOnlyPipeline.bind(cmdBuffer);
                // Draw with depthFront on the left side.
                drawLeft(depthFront);
                endRenderPass(ctx.vkd, cmdBuffer);

                // Sync writes to depth attachment before the next render pass.
                {
                    const auto barrier = makeMemoryBarrier(depthAccess, depthAccess);
                    cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, depthStage, depthStage, &barrier);
                }

                beginRenderPass(ctx.vkd, cmdBuffer, *depthColorRenderPass, *depthColorFramebuffer, scissors.at(0u),
                                colorClearValue);
                depthColorPipeline.bind(cmdBuffer);
                // Draw with depthMid on the whole framebuffer.
                drawFull(depthMid);
                endRenderPass(ctx.vkd, cmdBuffer);
            }
            else
            {
                const std::vector<VkClearValue> clearValues{
                    colorClearValue,
                    depthClearValue,
                };
                beginRenderPass(ctx.vkd, cmdBuffer, *depthColorRenderPass, *depthColorFramebuffer, scissors.at(0u),
                                de::sizeU32(clearValues), de::dataOrNull(clearValues));
                depthColorPipeline.bind(cmdBuffer);
                // Draw with depthFront on the left side.
                drawLeft(depthFront);
                endRenderPass(ctx.vkd, cmdBuffer);

                // Sync writes to depth attachment before the next render pass.
                {
                    const auto barrier = makeMemoryBarrier(depthAccess, depthAccess);
                    cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, depthStage, depthStage, &barrier);
                }

                beginRenderPass(ctx.vkd, cmdBuffer, *depthOnlyRenderPass, *depthOnlyFramebuffer, scissors.at(0u));
                depthOnlyPipeline.bind(cmdBuffer);
                // Draw with depthMid on the whole framebuffer.
                drawFull(depthMid);
                endRenderPass(ctx.vkd, cmdBuffer);
            }
        }
        else
            DE_ASSERT(false);

    // Copy depth and color images to host-visible buffers.
    {
        const auto copyAccess = VK_ACCESS_TRANSFER_READ_BIT;
        const auto copyStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;

        const std::vector<VkImageMemoryBarrier> preCopyBarriers{
            makeImageMemoryBarrier(colorAccess, copyAccess, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getImage(), colorSRR),
            makeImageMemoryBarrier(depthAccess, copyAccess, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depthBuffer.getImage(), depthSRR),
        };
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, (colorStage | depthStage), copyStage,
                                      de::dataOrNull(preCopyBarriers), preCopyBarriers.size());

        {
            const auto copyRegion = makeBufferImageCopy(extentVk, colorSRL);
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         colorBuffer.getBuffer(), 1u, &copyRegion);
        }
        {
            const auto copyRegion = makeBufferImageCopy(extentVk, depthSRL);
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, depthBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         depthBuffer.getBuffer(), 1u, &copyRegion);
        }

        const auto postCopyBarrier = makeMemoryBarrier(copyAccess, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, copyStage, VK_PIPELINE_STAGE_HOST_BIT, &postCopyBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    auto &colorAlloc = colorBuffer.getBufferAllocation();
    auto &depthAlloc = depthBuffer.getBufferAllocation();

    invalidateAlloc(ctx.vkd, ctx.device, colorAlloc);
    invalidateAlloc(ctx.vkd, ctx.device, depthAlloc);

    const auto colorTcuFormat = mapVkFormat(colorFormat);
    const auto depthTcuFormat = mapVkFormat(depthFormat);

    tcu::ConstPixelBufferAccess colorResAccess(colorTcuFormat, extent, colorAlloc.getHostPtr());
    tcu::ConstPixelBufferAccess depthResAccess(depthTcuFormat, extent, depthAlloc.getHostPtr());

    tcu::TextureLevel colorRefLevel(colorTcuFormat, extent.x(), extent.y(), extent.z());
    tcu::TextureLevel depthRefLevel(depthTcuFormat, extent.x(), extent.y(), extent.z());

    auto colorRefAccess = colorRefLevel.getAccess();
    auto depthRefAccess = depthRefLevel.getAccess();

    const auto geomColor  = m_params.getGeomColor();
    const auto clearColor = m_params.getClearColor();

    const auto half = extent / tcu::IVec3(2, 1, 1);

    auto colorLeft  = tcu::getSubregion(colorRefAccess, 0, 0, half.x(), half.y());
    auto colorRight = tcu::getSubregion(colorRefAccess, half.x(), 0, half.x(), half.y());

    auto depthLeft  = tcu::getSubregion(depthRefAccess, 0, 0, half.x(), half.y());
    auto depthRight = tcu::getSubregion(depthRefAccess, half.x(), 0, half.x(), half.y());

    // Depth: see test mechanism description for the rationale.
    tcu::clearDepth(depthLeft, depthFront);
    tcu::clearDepth(depthRight, depthMid);

    // Color: idem.
    tcu::clear(colorLeft, (m_params.prepass ? clearColor : geomColor));
    tcu::clear(colorRight, (m_params.prepass ? geomColor : clearColor));

    bool colorOk = true;
    bool depthOk = true;

    auto &log = m_context.getTestContext().getLog();

    const tcu::Vec4 colorThreshold(0.0f, 0.0f, 0.0f, 0.0f);
    const float depthThreshold = 0.000025f; // 1/65535 < 0.000025 < 2/65535

    colorOk = tcu::floatThresholdCompare(log, "Color", "", colorRefAccess, colorResAccess, colorThreshold,
                                         tcu::COMPARE_LOG_ON_ERROR);
    depthOk = tcu::dsThresholdCompare(log, "Depth", "", depthRefAccess, depthResAccess, depthThreshold,
                                      tcu::COMPARE_LOG_ON_ERROR);

    if (!(colorOk && depthOk))
        TCU_FAIL("Unexpected results found in color/depth buffers; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDepthTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType)
{
    const auto genFormatTests =
        (!vk::isConstructionTypeShaderObject(pipelineConstructionType) ||
         pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV);

    const VkFormat depthFormats[] = {VK_FORMAT_D16_UNORM,         VK_FORMAT_X8_D24_UNORM_PACK32,
                                     VK_FORMAT_D32_SFLOAT,        VK_FORMAT_D16_UNORM_S8_UINT,
                                     VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT};

    // Each entry configures the depth compare operators of QUAD_COUNT quads.
    // All entries cover pair-wise combinations of compare operators.
    const VkCompareOp depthOps[][DepthTest::QUAD_COUNT] = {
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL,
         VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL,
         VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_NOT_EQUAL,
         VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL,
         VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NOT_EQUAL},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL,
         VK_COMPARE_OP_LESS_OR_EQUAL},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS},
        {VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NEVER},
        {VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_EQUAL},
        {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_GREATER_OR_EQUAL},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS},
        {VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER}};

    const bool colorAttachmentEnabled[] = {true, false};

    const VkPrimitiveTopology primitiveTopologies[] = {
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    de::MovePtr<tcu::TestCaseGroup> depthTests(new tcu::TestCaseGroup(testCtx, "depth"));
    de::MovePtr<tcu::TestCaseGroup> noColorAttachmentTests(new tcu::TestCaseGroup(testCtx, "nocolor"));

    // Tests for format features
    if (!isConstructionTypeLibrary(pipelineConstructionType) &&
        !isConstructionTypeShaderObject(pipelineConstructionType))
    {
        de::MovePtr<tcu::TestCaseGroup> formatFeaturesTests(new tcu::TestCaseGroup(testCtx, "format_features"));

        // Formats that must be supported in all implementations
        addFunctionCase(formatFeaturesTests.get(), "support_d16_unorm", testSupportsDepthStencilFormat,
                        VK_FORMAT_D16_UNORM);

        // Sets where at least one of the formats must be supported
        const VkFormat depthOnlyFormats[]    = {VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT};
        const VkFormat depthStencilFormats[] = {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT};

        addFunctionCase(
            formatFeaturesTests.get(), "support_d24_unorm_or_d32_sfloat", testSupportsAtLeastOneDepthStencilFormat,
            std::vector<VkFormat>(depthOnlyFormats, depthOnlyFormats + DE_LENGTH_OF_ARRAY(depthOnlyFormats)));

        addFunctionCase(
            formatFeaturesTests.get(), "support_d24_unorm_s8_uint_or_d32_sfloat_s8_uint",
            testSupportsAtLeastOneDepthStencilFormat,
            std::vector<VkFormat>(depthStencilFormats, depthStencilFormats + DE_LENGTH_OF_ARRAY(depthStencilFormats)));

        depthTests->addChild(formatFeaturesTests.release());
    }

    for (uint32_t colorAttachmentEnabledIdx = 0; colorAttachmentEnabledIdx < DE_LENGTH_OF_ARRAY(colorAttachmentEnabled);
         colorAttachmentEnabledIdx++)
    {
        const bool colorEnabled = colorAttachmentEnabled[colorAttachmentEnabledIdx];

        // Tests for format and compare operators
        if (genFormatTests)
        {
            // Uses different depth formats
            de::MovePtr<tcu::TestCaseGroup> formatTests(new tcu::TestCaseGroup(testCtx, "format"));

            for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(depthFormats); formatNdx++)
            {
                const bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(depthFormats[formatNdx]).order);
                const bool hasStencil = tcu::hasStencilComponent(mapVkFormat(depthFormats[formatNdx]).order);
                const int separateLayoutsLoopCount = (hasDepth && hasStencil) ? 2 : 1;

                for (int separateDepthStencilLayouts = 0; separateDepthStencilLayouts < separateLayoutsLoopCount;
                     ++separateDepthStencilLayouts)
                {
                    const bool useSeparateDepthStencilLayouts = bool(separateDepthStencilLayouts);

                    de::MovePtr<tcu::TestCaseGroup> formatTest(
                        new tcu::TestCaseGroup(testCtx, (getFormatCaseName(depthFormats[formatNdx]) +
                                                         ((useSeparateDepthStencilLayouts) ? "_separate_layouts" : ""))
                                                            .c_str()));
                    // Combines depth compare operators
                    de::MovePtr<tcu::TestCaseGroup> compareOpsTests(new tcu::TestCaseGroup(testCtx, "compare_ops"));

                    for (size_t topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(primitiveTopologies); topologyNdx++)
                    {
                        const std::string topologyName = getTopologyName(primitiveTopologies[topologyNdx]) + "_";
                        for (size_t opsNdx = 0; opsNdx < DE_LENGTH_OF_ARRAY(depthOps); opsNdx++)
                        {
                            compareOpsTests->addChild(new DepthTest(
                                testCtx, topologyName + getCompareOpsName(depthOps[opsNdx]), pipelineConstructionType,
                                depthFormats[formatNdx], depthOps[opsNdx], useSeparateDepthStencilLayouts,
                                primitiveTopologies[topologyNdx], false, 0.0f, 1.0f));

                            compareOpsTests->addChild(new DepthTest(
                                testCtx, topologyName + getCompareOpsName(depthOps[opsNdx]) + "_depth_bounds_test",
                                pipelineConstructionType, depthFormats[formatNdx], depthOps[opsNdx],
                                useSeparateDepthStencilLayouts, primitiveTopologies[topologyNdx], true, 0.1f, 0.25f,
                                true, false, true, colorEnabled));

                            // Use general layout for some of the test cases
                            if ((formatNdx + opsNdx) % 10 == 0)
                            {
                                compareOpsTests->addChild(
                                    new DepthTest(testCtx,
                                                  topologyName + getCompareOpsName(depthOps[opsNdx]) +
                                                      "_depth_bounds_test_general_layout",
                                                  pipelineConstructionType, depthFormats[formatNdx], depthOps[opsNdx],
                                                  useSeparateDepthStencilLayouts, primitiveTopologies[topologyNdx],
                                                  true, 0.1f, 0.25f, true, false, true, colorEnabled, false,
                                                  tcu::UVec2(32, 32), DepthClipControlCase::DISABLED, true));
                            }
                        }
                    }
                    // Special VkPipelineDepthStencilStateCreateInfo known to have issues
                    {
                        const VkCompareOp depthOpsSpecial[DepthTest::QUAD_COUNT] = {
                            VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER};

                        compareOpsTests->addChild(new DepthTest(
                            testCtx, "never_zerodepthbounds_depthdisabled_stencilenabled", pipelineConstructionType,
                            depthFormats[formatNdx], depthOpsSpecial, useSeparateDepthStencilLayouts,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true, 0.0f, 0.0f, false, true, true, colorEnabled));
                    }
                    formatTest->addChild(compareOpsTests.release());

                    // Test case with depth test enabled, but depth write disabled
                    de::MovePtr<tcu::TestCaseGroup> depthTestDisabled(
                        new tcu::TestCaseGroup(testCtx, "depth_test_disabled"));
                    {
                        const VkCompareOp depthOpsDepthTestDisabled[DepthTest::QUAD_COUNT] = {
                            VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS};
                        depthTestDisabled->addChild(new DepthTest(
                            testCtx, "depth_write_enabled", pipelineConstructionType, depthFormats[formatNdx],
                            depthOpsDepthTestDisabled, useSeparateDepthStencilLayouts,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false, /* depthBoundsTestEnable */
                            0.0f,                                       /* depthBoundMin*/
                            1.0f,                                       /* depthBoundMax*/
                            false,                                      /* depthTestEnable */
                            false,                                      /* stencilTestEnable */
                            true,                                       /* depthAttachmentBound */
                            colorEnabled /* colorAttachmentEnable */));
                    }
                    formatTest->addChild(depthTestDisabled.release());

                    // Test case with depth buffer placed in local memory
                    de::MovePtr<tcu::TestCaseGroup> hostVisibleTests(new tcu::TestCaseGroup(testCtx, "host_visible"));
                    {
                        const VkCompareOp hostVisibleOps[DepthTest::QUAD_COUNT] = {
                            VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS};

                        // Depth buffer placed in local memory
                        hostVisibleTests->addChild(
                            new DepthTest(testCtx, "local_memory_depth_buffer", pipelineConstructionType,
                                          depthFormats[formatNdx], hostVisibleOps, useSeparateDepthStencilLayouts,
                                          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false, /* depthBoundsTestEnable */
                                          0.0f,                                       /* depthBoundMin*/
                                          1.0f,                                       /* depthBoundMax*/
                                          true,                                       /* depthTestEnable */
                                          false,                                      /* stencilTestEnable */
                                          true,                                       /* depthAttachmentBound */
                                          colorEnabled,                               /* colorAttachmentEnable */
                                          true,                                       /* hostVisible */
                                          tcu::UVec2(256, 256) /*renderSize*/));
                    }

                    formatTest->addChild(hostVisibleTests.release());
                    formatTests->addChild(formatTest.release());
                }
            }

            if (colorEnabled)
                depthTests->addChild(formatTests.release());
            else
                noColorAttachmentTests->addChild(formatTests.release());
        }
    }
    if (genFormatTests)
        depthTests->addChild(noColorAttachmentTests.release());

    // no depth attachment bound test.
    if (!vk::isConstructionTypeShaderObject(pipelineConstructionType))
    {
        de::MovePtr<tcu::TestCaseGroup> depthBoundTestNoDepthAttachment(
            new tcu::TestCaseGroup(testCtx, "no_depth_attachment"));
        {
            const VkCompareOp depthOpsDepthTestDisabled[DepthTest::QUAD_COUNT] = {
                VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS};
            depthBoundTestNoDepthAttachment->addChild(new DepthTest(
                testCtx, "depth_bound_test", pipelineConstructionType, VK_FORMAT_UNDEFINED, depthOpsDepthTestDisabled,
                false, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true, /* depthBoundsTestEnable */
                0.1f,                                             /* depthBoundMin*/
                0.2f,                                             /* depthBoundMax*/
                false,                                            /* depthTestEnable */
                false,                                            /* stencilTestEnable */
                false,                                            /* depthAttachmentBound */
                true /* colorAttachmentEnable */));
        }
        depthTests->addChild(depthBoundTestNoDepthAttachment.release());
    }

#ifndef CTS_USES_VULKANSC
    de::MovePtr<tcu::TestCaseGroup> depthClipControlTests(new tcu::TestCaseGroup(testCtx, "depth_clip_control"));
    {
        const VkCompareOp compareOps[] = {VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_LESS};

        const struct
        {
            const DepthClipControlCase viewportCase;
            const std::string suffix;
        } kViewportCases[] = {
            {DepthClipControlCase::NORMAL, ""},
            {DepthClipControlCase::NORMAL_W, "_different_w"},
            {DepthClipControlCase::BEFORE_STATIC, "_viewport_before_static"},
            {DepthClipControlCase::BEFORE_DYNAMIC, "_viewport_before_dynamic"},
            {DepthClipControlCase::BEFORE_TWO_DYNAMICS, "_viewport_before_two_dynamic"},
            {DepthClipControlCase::AFTER_DYNAMIC, "_viewport_after_dynamic"},
        };

        for (const auto &viewportCase : kViewportCases)
            for (const auto &format : depthFormats)
                for (const auto &compareOp : compareOps)
                {
                    std::string testName = getFormatCaseName(format) + "_" +
                                           de::toLower(std::string(getCompareOpName(compareOp)).substr(14)) +
                                           viewportCase.suffix;

                    const VkCompareOp ops[DepthTest::QUAD_COUNT] = {compareOp, compareOp, compareOp, compareOp};
                    depthClipControlTests->addChild(new DepthTest(testCtx, testName, pipelineConstructionType, format,
                                                                  ops, false, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                                  false, 0.0f, 1.0f, true, false, true, true, false,
                                                                  tcu::UVec2(32, 32), viewportCase.viewportCase));
                }
    }
    depthTests->addChild(depthClipControlTests.release());
#endif // CTS_USES_VULKANSC

    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        de::MovePtr<tcu::TestCaseGroup> xferLayoutGroup(new tcu::TestCaseGroup(testCtx, "xfer_queue_layout"));

        const std::vector<VkImageAspectFlags> aspectCases{
            (VK_IMAGE_ASPECT_DEPTH_BIT),
            (VK_IMAGE_ASPECT_STENCIL_BIT),
            (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT),
        };

        for (const auto &aspects : aspectCases)
        {
            const std::string testName = std::string("aspect") +
                                         (((aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0u) ? "_depth" : "") +
                                         (((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0u) ? "_stencil" : "");

            de::SharedPtr<TransferLayoutChangeParams> params(new TransferLayoutChangeParams(aspects));

            addFunctionCaseWithPrograms(xferLayoutGroup.get(), testName, transferLayoutChangeSupportCheck,
                                        transferLayoutChangePrograms, transferLayoutChangeTest, params);
        }

        depthTests->addChild(xferLayoutGroup.release());
    }

    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ||
        pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY ||
        pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV)
    {
        de::MovePtr<tcu::TestCaseGroup> depthOnlyGroup(new tcu::TestCaseGroup(testCtx, "depth_only"));

        const struct
        {
            DepthOnlyType depthOnlyType;
            const char *name;
        } depthOnlyTypeCases[] = {
            {DepthOnlyType::SEPARATE_RENDER_PASSES, "separate_render_passes"},
            {DepthOnlyType::SUBPASSES, "subpasses"},
#ifndef CTS_USES_VULKANSC
            {DepthOnlyType::DYNAMIC_RENDERING, "dynamic_rendering"},
#endif // CTS_USES_VULKANSC
        };

        for (const auto &depthOnlyTypeCase : depthOnlyTypeCases)
        {
            if (depthOnlyTypeCase.depthOnlyType != DepthOnlyType::DYNAMIC_RENDERING &&
                isConstructionTypeShaderObject(pipelineConstructionType))
                continue;

            for (const bool prepass : {false, true})
            {
                const DepthOnlyParams params{pipelineConstructionType, depthOnlyTypeCase.depthOnlyType, prepass};
                const auto testName = std::string(depthOnlyTypeCase.name) + (prepass ? "_prepass" : "_postpass");
                depthOnlyGroup->addChild(new DepthOnlyCase(testCtx, testName, params));
            }
        }

        depthTests->addChild(depthOnlyGroup.release());
    }

    return depthTests.release();
}

} // namespace pipeline
} // namespace vkt
