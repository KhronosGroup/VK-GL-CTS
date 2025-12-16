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
 * \brief Stencil Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktPipelineUniqueRandomIterator.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkFormatLists.hpp"
#include "vkBarrierUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deMemory.h"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

bool isSupportedDepthStencilFormat(const InstanceInterface &instanceInterface, VkPhysicalDevice device, VkFormat format)
{
    VkFormatProperties formatProps;

    instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

    return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

class StencilOpStateUniqueRandomIterator : public UniqueRandomIterator<VkStencilOpState>
{
public:
    StencilOpStateUniqueRandomIterator(int seed);
    virtual ~StencilOpStateUniqueRandomIterator(void)
    {
    }
    virtual VkStencilOpState getIndexedValue(uint32_t index);

private:
    // Pre-calculated constants
    const static uint32_t s_stencilOpsLength;
    const static uint32_t s_stencilOpsLength2;
    const static uint32_t s_stencilOpsLength3;
    const static uint32_t s_compareOpsLength;

    // Total number of cross-combinations of (stencilFailOp x stencilPassOp x stencilDepthFailOp x stencilCompareOp)
    const static uint32_t s_totalStencilOpStates;
};

class StencilTest : public vkt::TestCase
{
public:
    enum
    {
        QUAD_COUNT = 4
    };

    struct StencilStateConfig
    {
        uint32_t frontReadMask;
        uint32_t frontWriteMask;
        uint32_t frontRef;

        uint32_t backReadMask;
        uint32_t backWriteMask;
        uint32_t backRef;
    };

    const static StencilStateConfig s_stencilStateConfigs[QUAD_COUNT];
    const static float s_quadDepths[QUAD_COUNT];

    StencilTest(tcu::TestContext &testContext, const std::string &name,
                PipelineConstructionType pipelineConstructionType, VkFormat stencilFormat,
                const VkStencilOpState &stencilOpStateFront, const VkStencilOpState &stencilOpStateBack,
                const bool colorAttachmentEnable, const bool separateDepthStencilLayouts, const bool useGeneralLayout);
    virtual ~StencilTest(void) = default;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    PipelineConstructionType m_pipelineConstructionType;
    VkFormat m_stencilFormat;
    const VkStencilOpState m_stencilOpStateFront;
    const VkStencilOpState m_stencilOpStateBack;
    const bool m_colorAttachmentEnable;
    const bool m_separateDepthStencilLayouts;
    const bool m_useGeneralLayout;
};

class StencilTestInstance : public vkt::TestInstance
{
public:
    StencilTestInstance(Context &context, PipelineConstructionType pipelineConstructionType, VkFormat stencilFormat,
                        const VkStencilOpState &stencilOpStatesFront, const VkStencilOpState &stencilOpStatesBack,
                        const bool colorAttachmentEnable, const bool separateDepthStencilLayouts,
                        const bool useGeneralLayout);
    virtual ~StencilTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void);

private:
    tcu::TestStatus verifyImage(void);

    VkStencilOpState m_stencilOpStateFront;
    VkStencilOpState m_stencilOpStateBack;
    const bool m_colorAttachmentEnable;
    const bool m_separateDepthStencilLayouts;
    const bool m_useGeneralLayout;
    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;
    const VkFormat m_stencilFormat;
    VkImageSubresourceRange m_stencilImageSubresourceRange;

    VkImageCreateInfo m_colorImageCreateInfo;
    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImage> m_stencilImage;
    de::MovePtr<Allocation> m_stencilImageAlloc;
    Move<VkImageView> m_colorAttachmentView;
    Move<VkImageView> m_stencilAttachmentView;
    RenderPassWrapper m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModule;

    Move<VkBuffer> m_vertexBuffer;
    std::vector<Vertex4RGBA> m_vertices;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    PipelineLayoutWrapper m_pipelineLayout;
    GraphicsPipelineWrapper m_graphicsPipelines[StencilTest::QUAD_COUNT];

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

const VkStencilOp stencilOps[] = {VK_STENCIL_OP_KEEP,
                                  VK_STENCIL_OP_ZERO,
                                  VK_STENCIL_OP_REPLACE,
                                  VK_STENCIL_OP_INCREMENT_AND_CLAMP,
                                  VK_STENCIL_OP_DECREMENT_AND_CLAMP,
                                  VK_STENCIL_OP_INVERT,
                                  VK_STENCIL_OP_INCREMENT_AND_WRAP,
                                  VK_STENCIL_OP_DECREMENT_AND_WRAP};

const VkCompareOp compareOps[] = {VK_COMPARE_OP_NEVER,
                                  VK_COMPARE_OP_LESS,
                                  VK_COMPARE_OP_EQUAL,
                                  VK_COMPARE_OP_LESS_OR_EQUAL,
                                  VK_COMPARE_OP_GREATER,
                                  VK_COMPARE_OP_NOT_EQUAL,
                                  VK_COMPARE_OP_GREATER_OR_EQUAL,
                                  VK_COMPARE_OP_ALWAYS};

// StencilOpStateUniqueRandomIterator

const uint32_t StencilOpStateUniqueRandomIterator::s_stencilOpsLength     = DE_LENGTH_OF_ARRAY(stencilOps);
const uint32_t StencilOpStateUniqueRandomIterator::s_stencilOpsLength2    = s_stencilOpsLength * s_stencilOpsLength;
const uint32_t StencilOpStateUniqueRandomIterator::s_stencilOpsLength3    = s_stencilOpsLength2 * s_stencilOpsLength;
const uint32_t StencilOpStateUniqueRandomIterator::s_compareOpsLength     = DE_LENGTH_OF_ARRAY(compareOps);
const uint32_t StencilOpStateUniqueRandomIterator::s_totalStencilOpStates = s_stencilOpsLength3 * s_compareOpsLength;

StencilOpStateUniqueRandomIterator::StencilOpStateUniqueRandomIterator(int seed)
    : UniqueRandomIterator<VkStencilOpState>(s_totalStencilOpStates, s_totalStencilOpStates, seed)
{
}

VkStencilOpState StencilOpStateUniqueRandomIterator::getIndexedValue(uint32_t index)
{
    const uint32_t stencilCompareOpIndex    = index / s_stencilOpsLength3;
    const uint32_t stencilCompareOpSeqIndex = stencilCompareOpIndex * s_stencilOpsLength3;

    const uint32_t stencilDepthFailOpIndex    = (index - stencilCompareOpSeqIndex) / s_stencilOpsLength2;
    const uint32_t stencilDepthFailOpSeqIndex = stencilDepthFailOpIndex * s_stencilOpsLength2;

    const uint32_t stencilPassOpIndex =
        (index - stencilCompareOpSeqIndex - stencilDepthFailOpSeqIndex) / s_stencilOpsLength;
    const uint32_t stencilPassOpSeqIndex = stencilPassOpIndex * s_stencilOpsLength;

    const uint32_t stencilFailOpIndex =
        index - stencilCompareOpSeqIndex - stencilDepthFailOpSeqIndex - stencilPassOpSeqIndex;

    const VkStencilOpState stencilOpState = {
        stencilOps[stencilFailOpIndex],      // VkStencilOp failOp;
        stencilOps[stencilPassOpIndex],      // VkStencilOp passOp;
        stencilOps[stencilDepthFailOpIndex], // VkStencilOp depthFailOp;
        compareOps[stencilCompareOpIndex],   // VkCompareOp compareOp;
        0x0,                                 // uint32_t compareMask;
        0x0,                                 // uint32_t writeMask;
        0x0                                  // uint32_t reference;
    };

    return stencilOpState;
}

// StencilTest

const StencilTest::StencilStateConfig StencilTest::s_stencilStateConfigs[QUAD_COUNT] = {
    //    frontReadMask    frontWriteMask        frontRef        backReadMask    backWriteMask    backRef
    {0xFF, 0xFF, 0xAB, 0xF0, 0xFF, 0xFF},
    {0xFF, 0xF0, 0xCD, 0xF0, 0xF0, 0xEF},
    {0xF0, 0x0F, 0xEF, 0xFF, 0x0F, 0xCD},
    {0xF0, 0x01, 0xFF, 0xFF, 0x01, 0xAB}};

const float StencilTest::s_quadDepths[QUAD_COUNT] = {0.1f, 0.0f, 0.3f, 0.2f};

StencilTest::StencilTest(tcu::TestContext &testContext, const std::string &name,
                         PipelineConstructionType pipelineConstructionType, VkFormat stencilFormat,
                         const VkStencilOpState &stencilOpStateFront, const VkStencilOpState &stencilOpStateBack,
                         const bool colorAttachmentEnable, const bool separateDepthStencilLayouts,
                         const bool useGeneralLayout)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_stencilFormat(stencilFormat)
    , m_stencilOpStateFront(stencilOpStateFront)
    , m_stencilOpStateBack(stencilOpStateBack)
    , m_colorAttachmentEnable(colorAttachmentEnable)
    , m_separateDepthStencilLayouts(separateDepthStencilLayouts)
    , m_useGeneralLayout(useGeneralLayout)
{
}

void StencilTest::checkSupport(Context &context) const
{
    if (!isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_stencilFormat))
        throw tcu::NotSupportedError(std::string("Unsupported depth/stencil format: ") +
                                     getFormatName(m_stencilFormat));

    if (m_separateDepthStencilLayouts &&
        !context.isDeviceFunctionalitySupported("VK_KHR_separate_depth_stencil_layouts"))
        TCU_THROW(NotSupportedError, "VK_KHR_separate_depth_stencil_layouts is not supported");

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().separateStencilMaskRef)
        TCU_THROW(
            NotSupportedError,
            "VK_KHR_portability_subset: Separate stencil mask references are not supported by this implementation");
#endif // CTS_USES_VULKANSC
}

TestInstance *StencilTest::createInstance(Context &context) const
{
    return new StencilTestInstance(context, m_pipelineConstructionType, m_stencilFormat, m_stencilOpStateFront,
                                   m_stencilOpStateBack, m_colorAttachmentEnable, m_separateDepthStencilLayouts,
                                   m_useGeneralLayout);
}

void StencilTest::initPrograms(SourceCollections &sourceCollections) const
{
    if (m_colorAttachmentEnable)
    {
        sourceCollections.glslSources.add("color_vert")
            << glu::VertexSource("#version 310 es\n"
                                 "layout(location = 0) in vec4 position;\n"
                                 "layout(location = 1) in vec4 color;\n"
                                 "layout(location = 0) out highp vec4 vtxColor;\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "    gl_Position = position;\n"
                                 "    vtxColor = color;\n"
                                 "}\n");

        sourceCollections.glslSources.add("color_frag")
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
        sourceCollections.glslSources.add("color_vert") << glu::VertexSource("#version 310 es\n"
                                                                             "layout(location = 0) in vec4 position;\n"
                                                                             "layout(location = 1) in vec4 color;\n"
                                                                             "void main (void)\n"
                                                                             "{\n"
                                                                             "    gl_Position = position;\n"
                                                                             "}\n");
    }
}

// StencilTestInstance

StencilTestInstance::StencilTestInstance(Context &context, PipelineConstructionType pipelineConstructionType,
                                         VkFormat stencilFormat, const VkStencilOpState &stencilOpStateFront,
                                         const VkStencilOpState &stencilOpStateBack, const bool colorAttachmentEnable,
                                         const bool separateDepthStencilLayouts, const bool useGeneralLayout)
    : vkt::TestInstance(context)
    , m_stencilOpStateFront(stencilOpStateFront)
    , m_stencilOpStateBack(stencilOpStateBack)
    , m_colorAttachmentEnable(colorAttachmentEnable)
    , m_separateDepthStencilLayouts(separateDepthStencilLayouts)
    , m_useGeneralLayout(useGeneralLayout)
    , m_renderSize(32, 32)
    , m_colorFormat(colorAttachmentEnable ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_UNDEFINED)
    , m_stencilFormat(stencilFormat)
    , m_graphicsPipelines{{context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                          {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                           context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType}}
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

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
            &queueFamilyIndex,        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout initialLayout;
        };

        m_colorImageCreateInfo = colorImageParams;
        m_colorImage           = createImage(vk, vkDevice, &m_colorImageCreateInfo);

        // Allocate and bind color image memory
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));
    }

    // Create stencil image
    {
        const VkImageUsageFlags usageFlags =
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        const VkImageCreateInfo stencilImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,      // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                         // VkImageType imageType;
            m_stencilFormat,                          // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u}, // VkExtent3D extent;
            1u,                                       // uint32_t mipLevels;
            1u,                                       // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                  // VkImageTiling tiling;
            usageFlags,                               // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                // VkSharingMode sharingMode;
            1u,                                       // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,                        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED                 // VkImageLayout initialLayout;
        };

        m_stencilImage = createImage(vk, vkDevice, &stencilImageParams);

        // Allocate and bind stencil image memory
        m_stencilImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_stencilImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_stencilImage, m_stencilImageAlloc->getMemory(),
                                    m_stencilImageAlloc->getOffset()));

        const VkImageAspectFlags aspect = (mapVkFormat(m_stencilFormat).order == tcu::TextureFormat::DS ?
                                               VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT :
                                               VK_IMAGE_ASPECT_STENCIL_BIT);
        m_stencilImageSubresourceRange =
            makeImageSubresourceRange(aspect, 0u, stencilImageParams.mipLevels, 0u, stencilImageParams.arrayLayers);
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

    // Create stencil attachment view
    {
        const VkImageViewCreateInfo stencilAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageViewCreateFlags flags;
            *m_stencilImage,                          // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            m_stencilFormat,                          // VkFormat format;
            componentMappingRGBA,                     // VkComponentMapping components;
            m_stencilImageSubresourceRange,           // VkImageSubresourceRange subresourceRange;
        };

        m_stencilAttachmentView = createImageView(vk, vkDevice, &stencilAttachmentViewParams);
    }

    // Create render pass
    VkImageLayout colorLayout = m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkImageLayout dsLayout =
        m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    m_renderPass = RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat, m_stencilFormat,
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

        images.push_back(*m_stencilImage);
        attachmentBindInfos.push_back(*m_stencilAttachmentView);

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

    m_vertexShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    if (m_colorAttachmentEnable)
        m_fragmentShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

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
                0u                             // uint32_t offsetInBytes;
            },
            {
                1u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                offsetof(Vertex4RGBA, color),  // uint32_t offsetInBytes;
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

        const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
        const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

        const bool isDepthEnabled = (vk::mapVkFormat(m_stencilFormat).order != tcu::TextureFormat::S);

        VkPipelineDepthStencilStateCreateInfo depthStencilStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
            isDepthEnabled,                                             // VkBool32 depthTestEnable;
            isDepthEnabled,                                             // VkBool32 depthWriteEnable;
            VK_COMPARE_OP_LESS,                                         // VkCompareOp depthCompareOp;
            false,                                                      // VkBool32 depthBoundsTestEnable;
            true,                                                       // VkBool32 stencilTestEnable;
            m_stencilOpStateFront,                                      // VkStencilOpState front;
            m_stencilOpStateBack,                                       // VkStencilOpState back;
            0.0f,                                                       // float minDepthBounds;
            1.0f                                                        // float maxDepthBounds;
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
        const vk::VkPipelineColorBlendStateCreateInfo colorBlendStateParams{
            vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr,                           // const void*                                    pNext
            0u,                                // VkPipelineColorBlendStateCreateFlags            flags
            VK_FALSE,                          // VkBool32                                        logicOpEnable
            vk::VK_LOGIC_OP_CLEAR,             // VkLogicOp                                    logicOp
            m_colorAttachmentEnable ? 1u : 0u, // uint32_t                                        attachmentCount
            &blendState,                       // const VkPipelineColorBlendAttachmentState*    pAttachments
            {1.0f, 1.0f, 1.0f, 1.0f}           // float                                        blendConstants[4]
        };

        // Setup different stencil masks and refs in each quad
        for (int quadNdx = 0; quadNdx < StencilTest::QUAD_COUNT; quadNdx++)
        {
            const StencilTest::StencilStateConfig &config = StencilTest::s_stencilStateConfigs[quadNdx];
            VkStencilOpState &front                       = depthStencilStateParams.front;
            VkStencilOpState &back                        = depthStencilStateParams.back;

            front.compareMask = config.frontReadMask;
            front.writeMask   = config.frontWriteMask;
            front.reference   = config.frontRef;

            back.compareMask = config.backReadMask;
            back.writeMask   = config.backWriteMask;
            back.reference   = config.backRef;

            m_graphicsPipelines[quadNdx]
                .setDefaultRasterizerDiscardEnable(!m_colorAttachmentEnable)
                .setDefaultMultisampleState()
                .setupVertexInputState(&vertexInputStateParams)
                .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPass, 0u,
                                                  m_vertexShaderModule, &rasterizationStateParams)
                .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule,
                                          &depthStencilStateParams)
                .setupFragmentOutputState(*m_renderPass, 0,
                                          (m_colorAttachmentEnable ? &colorBlendStateParams : nullptr))
                .setMonolithicPipelineLayout(m_pipelineLayout)
                .buildPipeline();
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

        // Adjust depths
        for (int quadNdx = 0; quadNdx < 4; quadNdx++)
            for (int vertexNdx = 0; vertexNdx < 6; vertexNdx++)
                m_vertices[quadNdx * 6 + vertexNdx].position.z() = StencilTest::s_quadDepths[quadNdx];

        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    {
        const VkImageLayout attachmentLayout =
            m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const VkImageMemoryBarrier colorImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType            sType;
            nullptr,                                    // const void*                pNext;
            (VkAccessFlags)0,                           // VkAccessFlags              srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags              dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout              oldLayout;
            attachmentLayout,                           // VkImageLayout              newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                   srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                   dstQueueFamilyIndex;
            *m_colorImage,                              // VkImage                    image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange    subresourceRange;
        };

        VkImageSubresourceRange stencilImageBarrierSubresourceRange = m_stencilImageSubresourceRange;
        VkImageLayout newLayout                                     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        if (m_separateDepthStencilLayouts)
        {
            stencilImageBarrierSubresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
            newLayout                                      = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        }
        if (m_useGeneralLayout)
            newLayout = VK_IMAGE_LAYOUT_GENERAL;

        const VkImageMemoryBarrier stencilImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,       // VkStructureType            sType;
            nullptr,                                      // const void*                pNext;
            (VkAccessFlags)0,                             // VkAccessFlags              srcAccessMask;
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // VkAccessFlags              dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                    // VkImageLayout              oldLayout;
            newLayout,                                    // VkImageLayout              newLayout;
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                   srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                      // uint32_t                   dstQueueFamilyIndex;
            *m_stencilImage,                              // VkImage                    image;
            stencilImageBarrierSubresourceRange,          // VkImageSubresourceRange    subresourceRange;
        };

        std::vector<VkClearValue> attachmentClearValues;
        std::vector<VkImageMemoryBarrier> imageLayoutBarriers;

        if (m_colorAttachmentEnable)
        {
            attachmentClearValues.push_back(defaultClearValue(m_colorFormat));
            imageLayoutBarriers.push_back(colorImageBarrier);
        }

        attachmentClearValues.push_back(defaultClearValue(m_stencilFormat));
        imageLayoutBarriers.push_back(stencilImageBarrier);

        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                              (VkDependencyFlags)0, 0u, nullptr, 0u, nullptr, (uint32_t)imageLayoutBarriers.size(),
                              imageLayoutBarriers.data());

        m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
                           (uint32_t)attachmentClearValues.size(), attachmentClearValues.data());

        const VkDeviceSize quadOffset = (m_vertices.size() / StencilTest::QUAD_COUNT) * sizeof(Vertex4RGBA);

        for (int quadNdx = 0; quadNdx < StencilTest::QUAD_COUNT; quadNdx++)
        {
            VkDeviceSize vertexBufferOffset = quadOffset * quadNdx;

            m_graphicsPipelines[quadNdx].bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdDraw(*m_cmdBuffer, (uint32_t)(m_vertices.size() / StencilTest::QUAD_COUNT), 1, 0, 0);
        }

        m_renderPass.end(vk, *m_cmdBuffer);
        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

tcu::TestStatus StencilTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyImage();
}

tcu::TestStatus StencilTestInstance::verifyImage(void)
{
    const tcu::TextureFormat tcuColorFormat   = mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM);
    const tcu::TextureFormat tcuStencilFormat = mapVkFormat(m_stencilFormat);
    const ColorVertexShader vertexShader;
    const ColorFragmentShader fragmentShader(tcuColorFormat, tcuStencilFormat);
    const rr::Program program(&vertexShader, &fragmentShader);
    ReferenceRenderer refRenderer(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuStencilFormat, &program);
    bool colorCompareOk   = false;
    bool stencilCompareOk = false;

    // Render reference image
    {
        // Set depth state
        rr::RenderState renderState(refRenderer.getViewportState(),
                                    m_context.getDeviceProperties().limits.subPixelPrecisionBits);

        renderState.fragOps.depthTestEnabled   = true;
        renderState.fragOps.depthFunc          = mapVkCompareOp(VK_COMPARE_OP_LESS);
        renderState.fragOps.stencilTestEnabled = true;

        rr::StencilState &refStencilFront = renderState.fragOps.stencilStates[rr::FACETYPE_FRONT];
        rr::StencilState &refStencilBack  = renderState.fragOps.stencilStates[rr::FACETYPE_BACK];

        refStencilFront.sFail  = mapVkStencilOp(m_stencilOpStateFront.failOp);
        refStencilFront.dpFail = mapVkStencilOp(m_stencilOpStateFront.depthFailOp);
        refStencilFront.dpPass = mapVkStencilOp(m_stencilOpStateFront.passOp);
        refStencilFront.func   = mapVkCompareOp(m_stencilOpStateFront.compareOp);

        refStencilBack.sFail  = mapVkStencilOp(m_stencilOpStateBack.failOp);
        refStencilBack.dpPass = mapVkStencilOp(m_stencilOpStateBack.passOp);
        refStencilBack.dpFail = mapVkStencilOp(m_stencilOpStateBack.depthFailOp);
        refStencilBack.func   = mapVkCompareOp(m_stencilOpStateBack.compareOp);

        // Reverse winding of vertices, as Vulkan screen coordinates start at upper left
        std::vector<Vertex4RGBA> cwVertices(m_vertices);
        for (size_t vertexNdx = 0; vertexNdx < cwVertices.size() - 2; vertexNdx += 3)
        {
            const Vertex4RGBA cwVertex1 = cwVertices[vertexNdx + 1];

            cwVertices[vertexNdx + 1] = cwVertices[vertexNdx + 2];
            cwVertices[vertexNdx + 2] = cwVertex1;
        }

        for (int quadNdx = 0; quadNdx < StencilTest::QUAD_COUNT; quadNdx++)
        {
            refStencilFront.ref       = (int)StencilTest::s_stencilStateConfigs[quadNdx].frontRef;
            refStencilFront.compMask  = StencilTest::s_stencilStateConfigs[quadNdx].frontReadMask;
            refStencilFront.writeMask = StencilTest::s_stencilStateConfigs[quadNdx].frontWriteMask;

            refStencilBack.ref       = (int)StencilTest::s_stencilStateConfigs[quadNdx].backRef;
            refStencilBack.compMask  = StencilTest::s_stencilStateConfigs[quadNdx].backReadMask;
            refStencilBack.writeMask = StencilTest::s_stencilStateConfigs[quadNdx].backWriteMask;

            refRenderer.draw(
                renderState, rr::PRIMITIVETYPE_TRIANGLES,
                std::vector<Vertex4RGBA>(cwVertices.begin() + quadNdx * 6, cwVertices.begin() + (quadNdx + 1) * 6));
        }
    }

    // Compare result with reference image
    if (m_colorAttachmentEnable)
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice vkDevice         = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::UniquePtr<tcu::TextureLevel> result(
            readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat,
                                m_renderSize,
                                m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .release());

        colorCompareOk = tcu::intThresholdPositionDeviationCompare(
            m_context.getTestContext().getLog(), "IntImageCompare", "Image comparison", refRenderer.getAccess(),
            result->getAccess(), tcu::UVec4(2, 2, 2, 2), tcu::IVec3(1, 1, 0), true, tcu::COMPARE_LOG_RESULT);
    }
    else
    {
        colorCompareOk = true;
    }

    // Compare stencil result with reference image
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice vkDevice         = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::UniquePtr<tcu::TextureLevel> result(
            readStencilAttachment(
                vk, vkDevice, queue, queueFamilyIndex, allocator, *m_stencilImage, m_stencilFormat, m_renderSize,
                m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .release());

        {
            const tcu::PixelBufferAccess stencilAccess(
                tcu::getEffectiveDepthStencilAccess(refRenderer.getDepthStencilAccess(), tcu::Sampler::MODE_STENCIL));
            stencilCompareOk = tcu::intThresholdPositionDeviationCompare(
                m_context.getTestContext().getLog(), "StencilImageCompare", "Stencil image comparison", stencilAccess,
                result->getAccess(), tcu::UVec4(2, 2, 2, 2), tcu::IVec3(1, 1, 0), true, tcu::COMPARE_LOG_RESULT);
        }
    }

    if (colorCompareOk && stencilCompareOk)
        return tcu::TestStatus::pass("Result image matches reference");
    else
        return tcu::TestStatus::fail("Image mismatch");
}

struct NoStencilAttachmentParams
{
    PipelineConstructionType pipelineConstructionType;
    VkFormat format;
    bool dynamicRendering;
    bool dynamicEnable;

    bool useDynamicRendering(void) const
    {
        return (dynamicRendering || isConstructionTypeShaderObject(pipelineConstructionType));
    }
};

class NoStencilAttachmentCase : public vkt::TestCase
{
public:
    NoStencilAttachmentCase(tcu::TestContext &testCtx, const std::string &name, const NoStencilAttachmentParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~NoStencilAttachmentCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    static void getImageCreateInfo(VkImageCreateInfo *createInfo, const VkFormat format);
    static tcu::Vec4 getClearColor(void);
    static tcu::Vec4 getGeometryColor(void);

protected:
    const NoStencilAttachmentParams m_params;
};

class NoStencilAttachmentInstance : public vkt::TestInstance
{
public:
    NoStencilAttachmentInstance(Context &context, const NoStencilAttachmentParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~NoStencilAttachmentInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const NoStencilAttachmentParams m_params;
};

void NoStencilAttachmentCase::getImageCreateInfo(VkImageCreateInfo *createInfo_, const VkFormat format)
{
    const VkImageUsageFlags mainUsage = (isDepthStencilFormat(format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT :
                                                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const VkImageUsageFlags usage     = (mainUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    *createInfo_ = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        makeExtent3D(32u, 32u, 1u),          // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
}

tcu::Vec4 NoStencilAttachmentCase::getClearColor(void)
{
    return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

tcu::Vec4 NoStencilAttachmentCase::getGeometryColor(void)
{
    return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
}

void NoStencilAttachmentCase::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    checkPipelineConstructionRequirements(vki, physicalDevice, m_params.pipelineConstructionType);

    if (m_params.dynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    if (m_params.dynamicEnable && !isConstructionTypeShaderObject(m_params.pipelineConstructionType))
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

    VkImageCreateInfo createInfo;
    VkImageFormatProperties imgFormatProperties;

    getImageCreateInfo(&createInfo, m_params.format);

    const auto result = vki.getPhysicalDeviceImageFormatProperties(
        physicalDevice, createInfo.format, createInfo.imageType, createInfo.tiling, createInfo.usage, createInfo.flags,
        &imgFormatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Format does not support the required features");

    VK_CHECK(result);
}

void NoStencilAttachmentCase::initPrograms(SourceCollections &dst) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "vec2 positions[3] = vec2[](\n"
         << "    vec2(-1.0, -1.0),\n"
         << "    vec2( 3.0, -1.0),\n"
         << "    vec2(-1.0,  3.0)\n"
         << ");\n"
         << "layout (push_constant, std430) uniform PushConstantBlock {\n"
         << "    float depth;\n"
         << "} pc;\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(positions[gl_VertexIndex % 3], pc.depth, 1.0);\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *NoStencilAttachmentCase::createInstance(Context &context) const
{
    return new NoStencilAttachmentInstance(context, m_params);
}

// The goal here is indicating the stencil attachment is not present, either because there is no stencil aspect in the depth/stencil
// format or because (when using dynamic rendering) the stencil attachment and format are null/unused respectively. Despite this
// setup, in the depth/stencil pipeline state or dynamic state, we will indicate the stencil test is enabled.
//
// According to the spec, if there is no stencil attachment, the stencil test should not modify coverage and should be effectively
// disabled.
tcu::TestStatus NoStencilAttachmentInstance::iterate(void)
{
    const auto &ctx        = m_context.getContextCommonData();
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuColorFmt = mapVkFormat(colorFormat);
    const auto tcuDSFmt    = mapVkFormat(m_params.format);
    const auto clearColor  = NoStencilAttachmentCase::getClearColor();
    const auto geomColor   = NoStencilAttachmentCase::getGeometryColor();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
    const auto dataStages     = VK_SHADER_STAGE_VERTEX_BIT;
    const float clearDepth    = 0.5f;
    const float geomDepth     = 0.75f;
    const uint32_t stencilClr = 255u;
    const uint32_t stencilRef = 128u;
    const bool useDR          = m_params.useDynamicRendering();

    // Formats used to verify the different aspects.
    const bool hasDepth   = tcu::hasDepthComponent(tcuDSFmt.order);
    const bool hasStencil = tcu::hasStencilComponent(tcuDSFmt.order);

    DE_ASSERT(hasDepth);

    const auto depthCopyFormat   = (hasDepth ? getDepthCopyFormat(m_params.format) : tcu::TextureFormat());
    const auto stencilCopyFormat = (hasStencil ? getStencilCopyFormat(m_params.format) : tcu::TextureFormat());

    VkImageCreateInfo colorCreateInfo;
    VkImageCreateInfo dsCreateInfo;

    NoStencilAttachmentCase::getImageCreateInfo(&colorCreateInfo, colorFormat);
    NoStencilAttachmentCase::getImageCreateInfo(&dsCreateInfo, m_params.format);

    const auto &vkExtent  = colorCreateInfo.extent;
    const auto fbExtent   = tcu::IVec3(static_cast<int>(vkExtent.width), static_cast<int>(vkExtent.height),
                                       static_cast<int>(vkExtent.depth));
    const auto pixelCount = fbExtent.x() * fbExtent.y() * fbExtent.z();
    const auto colorSRR   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, colorCreateInfo.mipLevels, 0u,
                                                      colorCreateInfo.arrayLayers);
    const auto dsSRR      = makeImageSubresourceRange(getImageAspectFlags(tcuDSFmt), 0u, dsCreateInfo.mipLevels, 0u,
                                                      dsCreateInfo.arrayLayers);
    const auto colorSRL   = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, colorCreateInfo.arrayLayers);
    const auto depthSRL   = makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, dsCreateInfo.arrayLayers);
    const auto stencilSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, dsCreateInfo.arrayLayers);

    // Color buffer with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, colorCreateInfo.extent, colorCreateInfo.format,
                                colorCreateInfo.usage, colorCreateInfo.imageType, colorSRR, colorCreateInfo.arrayLayers,
                                colorCreateInfo.samples, colorCreateInfo.tiling, colorCreateInfo.mipLevels,
                                colorCreateInfo.sharingMode);

    ImageWithMemory dsBuffer(ctx.vkd, ctx.device, ctx.allocator, dsCreateInfo, MemoryRequirement::Any);
    const auto dsView =
        makeImageView(ctx.vkd, ctx.device, dsBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, dsCreateInfo.format, dsSRR);

    // Verification buffers for depth/stencil.
    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

    BufferWithMemoryPtr depthVerifBuffer;
    BufferWithMemoryPtr stencilVerifBuffer;

    VkDeviceSize depthVerifBufferSize   = 0ull;
    VkDeviceSize stencilVerifBufferSize = 0ull;

    if (hasDepth)
    {
        depthVerifBufferSize  = static_cast<VkDeviceSize>(tcu::getPixelSize(depthCopyFormat) * pixelCount);
        const auto createInfo = makeBufferCreateInfo(depthVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        depthVerifBuffer.reset(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::HostVisible));
    }

    if (hasStencil)
    {
        stencilVerifBufferSize = static_cast<VkDeviceSize>(tcu::getPixelSize(stencilCopyFormat) * pixelCount);
        const auto createInfo  = makeBufferCreateInfo(stencilVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        stencilVerifBuffer.reset(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::HostVisible));
    }

    const std::vector<VkImageView> fbViews{colorBuffer.getImageView(), *dsView};

    // Push constants.
    const auto pcSize  = static_cast<uint32_t>(sizeof(geomDepth));
    const auto pcRange = makePushConstantRange(dataStages, 0u, pcSize);

    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, ctx.vkd, ctx.device, VK_NULL_HANDLE,
                                               &pcRange);

    const auto renderPass  = (useDR ? Move<VkRenderPass>() :
                                      makeRenderPass(ctx.vkd, ctx.device, colorCreateInfo.format, dsCreateInfo.format));
    const auto framebuffer = (useDR ? Move<VkFramebuffer>() :
                                      makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews),
                                                      de::dataOrNull(fbViews), colorCreateInfo.extent.width,
                                                      colorCreateInfo.extent.height, colorCreateInfo.arrayLayers));

    // Modules.
    const auto &binaries = m_context.getBinaryCollection();
    const ShaderWrapper vertModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const ShaderWrapper fragModule(ctx.vkd, ctx.device, binaries.get("frag"));
    const ShaderWrapper nullModule;

    const std::vector<VkViewport> viewports(1u, makeViewport(colorCreateInfo.extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(colorCreateInfo.extent));

    // Depth/stencil state: note how, despite not having a stencil attachment, we enable the stencil test in a way that will not
    // preserve the stencil clear value no matter if the test passes or not.
    const auto stencilOpState = makeStencilOpState(VK_STENCIL_OP_ZERO, VK_STENCIL_OP_DECREMENT_AND_CLAMP,
                                                   VK_STENCIL_OP_INVERT, VK_COMPARE_OP_EQUAL, 0xFFu, 0xFFu, stencilRef);

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        VK_TRUE,                                                    // VkBool32 depthTestEnable;
        VK_TRUE,                                                    // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_GREATER,                                      // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        (m_params.dynamicEnable ? VK_FALSE : VK_TRUE),              // VkBool32 stencilTestEnable;
        stencilOpState,                                             // VkStencilOpState front;
        stencilOpState,                                             // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        1.0f,                                                       // float maxDepthBounds;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

#ifndef CTS_USES_VULKANSC
    // When using dynamic rendering, we'll signal the lack of a stencil attachment using VK_FORMAT_UNDEFINED for the stencil format.
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, // VkStructureType sType;
        nullptr,                                          // const void* pNext;
        0u,                                               // uint32_t viewMask;
        1u,                                               // uint32_t colorAttachmentCount;
        &colorCreateInfo.format,                          // const VkFormat* pColorAttachmentFormats;
        dsCreateInfo.format,                              // VkFormat depthAttachmentFormat;
        VK_FORMAT_UNDEFINED,                              // VkFormat stencilAttachmentFormat;
    };
#endif // CTS_USES_VULKANSC

    PipelineRenderingCreateInfoWrapper pipelineRenderingCreateInfoPtr(
#ifndef CTS_USES_VULKANSC
        &pipelineRenderingCreateInfo
#else
        nullptr
#endif // CTS_USES_VULKANSC
    );

    std::vector<VkDynamicState> dynamicStates;
    if (m_params.dynamicEnable)
        dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT);

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
        de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
        de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
    };

    GraphicsPipelineWrapper pipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                            m_context.getDeviceExtensions(), m_params.pipelineConstructionType);

    pipelineWrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setDynamicState(&dynamicStateCreateInfo)
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule, nullptr,
                                          nullModule, nullModule, nullModule, nullptr, nullptr,
                                          pipelineRenderingCreateInfoPtr)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, &depthStencilStateCreateInfo)
        .setupFragmentOutputState(*renderPass, 0u)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const std::vector<VkClearValue> clearValues{
        makeClearValueColor(clearColor),
        makeClearValueDepthStencil(clearDepth, stencilClr),
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    if (useDR)
    {
        // Transition image layouts and clear images, then begin rendering.
        const std::vector<VkImageMemoryBarrier> preClearBarriers{
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.getImage(), colorSRR),

            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dsBuffer.get(), dsSRR),
        };

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, de::dataOrNull(preClearBarriers),
                                      de::sizeU32(preClearBarriers));

        // We want to use these clears instead of beginRendering clears to make sure the stencil aspect of the image is cleared too when present.
        ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &clearValues.at(0u).color, 1u, &colorSRR);
        ctx.vkd.cmdClearDepthStencilImage(cmdBuffer, dsBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          &clearValues.at(1u).depthStencil, 1u, &dsSRR);

        const std::vector<VkImageMemoryBarrier> postClearBarriers{
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                   (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   colorBuffer.getImage(), colorSRR),

            makeImageMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, dsBuffer.get(),
                dsSRR),
        };

        const auto srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const auto dstStage = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(postClearBarriers),
                                      de::sizeU32(postClearBarriers));

#ifndef CTS_USES_VULKANSC
        const VkRenderingAttachmentInfo colorAttInfo = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,     // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            colorBuffer.getImageView(),                      // VkImageView imageView;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout imageLayout;
            VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
            VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
            VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
            VK_ATTACHMENT_LOAD_OP_LOAD,                      // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
            /*unused*/ makeClearValueColor(tcu::Vec4(0.0f)), // VkClearValue clearValue;
        };

        const VkRenderingAttachmentInfo depthAttInfo = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,      // VkStructureType sType;
            nullptr,                                          // const void* pNext;
            *dsView,                                          // VkImageView imageView;
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout imageLayout;
            VK_RESOLVE_MODE_NONE,                             // VkResolveModeFlagBits resolveMode;
            VK_NULL_HANDLE,                                   // VkImageView resolveImageView;
            VK_IMAGE_LAYOUT_UNDEFINED,                        // VkImageLayout resolveImageLayout;
            VK_ATTACHMENT_LOAD_OP_LOAD,                       // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp storeOp;
            /*unused*/ makeClearValueColor(tcu::Vec4(0.0f)),  // VkClearValue clearValue;
        };

        const VkRenderingInfo renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO, // VkStructureType sType;
            nullptr,                          // const void* pNext;
            0u,                               // VkRenderingFlags flags;
            scissors.at(0u),                  // VkRect2D renderArea;
            colorCreateInfo.arrayLayers,      // uint32_t layerCount;
            0u,                               // uint32_t viewMask;
            1u,                               // uint32_t colorAttachmentCount;
            &colorAttInfo,                    // const VkRenderingAttachmentInfo* pColorAttachments;
            &depthAttInfo,                    // const VkRenderingAttachmentInfo* pDepthAttachment;
            nullptr,                          // const VkRenderingAttachmentInfo* pStencilAttachment;
        };

        ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
#else
        DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
    }
    else
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), de::sizeU32(clearValues),
                        de::dataOrNull(clearValues));

    pipelineWrapper.bind(cmdBuffer);
    ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, dataStages, 0u, pcSize, &geomDepth);
    if (m_params.dynamicEnable)
    {
#ifndef CTS_USES_VULKANSC
        ctx.vkd.cmdSetStencilTestEnable(cmdBuffer, VK_TRUE);
#else
        ctx.vkd.cmdSetStencilTestEnableEXT(cmdBuffer, VK_TRUE);
#endif // CTS_USES_VULKANSC
    }
    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);

    if (useDR)
    {
#ifndef CTS_USES_VULKANSC
        endRendering(ctx.vkd, cmdBuffer);
#else
        DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
    }
    else
        endRenderPass(ctx.vkd, cmdBuffer);

    {
        const std::vector<VkImageMemoryBarrier> imgMemoryBarriers{
            makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   colorBuffer.getImage(), colorSRR),

            makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dsBuffer.get(), dsSRR),
        };

        const auto srcStages = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        const auto dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, de::dataOrNull(imgMemoryBarriers),
                                      de::sizeU32(imgMemoryBarriers));

        const auto colorRegion = makeBufferImageCopy(vkExtent, colorSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     colorBuffer.getBuffer(), 1u, &colorRegion);

        if (hasDepth)
        {
            const auto depthRegion = makeBufferImageCopy(vkExtent, depthSRL);
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, dsBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         depthVerifBuffer->get(), 1u, &depthRegion);
        }

        if (hasStencil)
        {
            const auto stencilRegion = makeBufferImageCopy(vkExtent, stencilSRL);
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, dsBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         stencilVerifBuffer->get(), 1u, &stencilRegion);
        }

        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color output.
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuColorFmt, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuColorFmt, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, geomColor);

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "ResultColor", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    // Verify depth/stencil if available.
    if (hasDepth)
    {
        const auto &allocation = depthVerifBuffer->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, allocation);

        tcu::PixelBufferAccess resultDepth(depthCopyFormat, fbExtent, allocation.getHostPtr());
        tcu::TextureLevel referenceDepth(depthCopyFormat, fbExtent.x(), fbExtent.y());
        auto refDepthAccess = referenceDepth.getAccess();

        tcu::clearDepth(refDepthAccess, geomDepth);
        const float depthThreshold =
            0.000025f; // Should be good enough for D16, D24 and D32 given the depth values we're using.

        if (!tcu::dsThresholdCompare(log, "ResultDepth", "", refDepthAccess, resultDepth, depthThreshold,
                                     tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Unexpected depth in result buffer; check log for details");
    }
    if (hasStencil)
    {
        const auto &allocation = stencilVerifBuffer->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, allocation);

        tcu::PixelBufferAccess resultStencil(stencilCopyFormat, fbExtent, allocation.getHostPtr());
        tcu::TextureLevel referenceStencil(stencilCopyFormat, fbExtent.x(), fbExtent.y());
        auto refStencilAccess = referenceStencil.getAccess();

        tcu::clearStencil(refStencilAccess, stencilClr);
        const float stencilThreshold = 0.0f; // This is actually not used for stencil.

        if (!tcu::dsThresholdCompare(log, "ResultStencil", "", refStencilAccess, resultStencil, stencilThreshold,
                                     tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Unexpected stencil value in result buffer; check log for details");
    }

    return tcu::TestStatus::pass("Pass");
}

// Utilities for test names

const char *getShortName(VkStencilOp stencilOp)
{
    switch (stencilOp)
    {
    case VK_STENCIL_OP_KEEP:
        return "keep";
    case VK_STENCIL_OP_ZERO:
        return "zero";
    case VK_STENCIL_OP_REPLACE:
        return "repl";
    case VK_STENCIL_OP_INCREMENT_AND_CLAMP:
        return "incc";
    case VK_STENCIL_OP_DECREMENT_AND_CLAMP:
        return "decc";
    case VK_STENCIL_OP_INVERT:
        return "inv";
    case VK_STENCIL_OP_INCREMENT_AND_WRAP:
        return "wrap";
    case VK_STENCIL_OP_DECREMENT_AND_WRAP:
        return "decw";

    default:
        DE_FATAL("Invalid VkStencilOpState value");
    }
    return nullptr;
}

std::string getFormatCaseName(VkFormat format)
{
    const std::string fullName = getFormatName(format);

    DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

    return de::toLower(fullName.substr(10));
}

} // namespace

tcu::TestCaseGroup *createStencilTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType)
{
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(compareOps) == 8);
    DE_STATIC_ASSERT(vk::VK_COMPARE_OP_LAST == 8);

    static const char *compareOpNames[8] = {"comp_never",
                                            "comp_less",
                                            "comp_equal",
                                            "comp_less_or_equal",
                                            "comp_greater",
                                            "comp_not_equal",
                                            "comp_greater_or_equal",
                                            "comp_always"};

    // Stencil tests
    de::MovePtr<tcu::TestCaseGroup> stencilTests(new tcu::TestCaseGroup(testCtx, "stencil"));
    // Stencil tests with no color attachment
    de::MovePtr<tcu::TestCaseGroup> noColorAttachmentTests(new tcu::TestCaseGroup(testCtx, "nocolor"));
    const bool colorAttachmentEnabled[] = {true, false};

    for (uint32_t colorAttachmentEnabledIdx = 0; colorAttachmentEnabledIdx < DE_LENGTH_OF_ARRAY(colorAttachmentEnabled);
         colorAttachmentEnabledIdx++)
    {
        const bool colorEnabled = colorAttachmentEnabled[colorAttachmentEnabledIdx];
        de::MovePtr<tcu::TestCaseGroup> formatTests(new tcu::TestCaseGroup(testCtx, "format"));
        StencilOpStateUniqueRandomIterator stencilOpItr(123);

        for (auto format : formats::stencilFormats)
        {
            const bool hasDepth                = tcu::hasDepthComponent(mapVkFormat(format).order);
            const bool hasStencil              = tcu::hasStencilComponent(mapVkFormat(format).order);
            const int separateLayoutsLoopCount = (hasDepth && hasStencil) ? 2 : 1;

            for (int separateDepthStencilLayouts = 0; separateDepthStencilLayouts < separateLayoutsLoopCount;
                 ++separateDepthStencilLayouts)
            {
                const bool useSeparateDepthStencilLayouts = bool(separateDepthStencilLayouts);

                de::MovePtr<tcu::TestCaseGroup> formatTest(new tcu::TestCaseGroup(
                    testCtx, (getFormatCaseName(format) + ((useSeparateDepthStencilLayouts) ? "_separate_layouts" : ""))
                                 .c_str()));

                de::MovePtr<tcu::TestCaseGroup> stencilStateTests;
                {
                    std::ostringstream desc;
                    desc << "Draws 4 quads with the following depths and dynamic stencil states: ";

                    for (int quadNdx = 0; quadNdx < StencilTest::QUAD_COUNT; quadNdx++)
                    {
                        const StencilTest::StencilStateConfig &stencilConfig =
                            StencilTest::s_stencilStateConfigs[quadNdx];

                        desc << "(" << quadNdx << ") "
                             << "z = " << StencilTest::s_quadDepths[quadNdx] << ", "
                             << "frontReadMask = " << stencilConfig.frontReadMask << ", "
                             << "frontWriteMask = " << stencilConfig.frontWriteMask << ", "
                             << "frontRef = " << stencilConfig.frontRef << ", "
                             << "backReadMask = " << stencilConfig.backReadMask << ", "
                             << "backWriteMask = " << stencilConfig.backWriteMask << ", "
                             << "backRef = " << stencilConfig.backRef;
                    }

                    stencilStateTests = de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "states"));
                }

                stencilOpItr.reset();

                for (uint32_t failOpNdx = 0u; failOpNdx < DE_LENGTH_OF_ARRAY(stencilOps); failOpNdx++)
                {
                    const std::string failOpName = std::string("fail_") + getShortName(stencilOps[failOpNdx]);
                    de::MovePtr<tcu::TestCaseGroup> failOpTest(new tcu::TestCaseGroup(testCtx, failOpName.c_str()));

                    for (uint32_t passOpNdx = 0u; passOpNdx < DE_LENGTH_OF_ARRAY(stencilOps); passOpNdx++)
                    {
                        const std::string passOpName = std::string("pass_") + getShortName(stencilOps[passOpNdx]);
                        de::MovePtr<tcu::TestCaseGroup> passOpTest(new tcu::TestCaseGroup(testCtx, passOpName.c_str()));

                        for (uint32_t dFailOpNdx = 0u; dFailOpNdx < DE_LENGTH_OF_ARRAY(stencilOps); dFailOpNdx++)
                        {
                            const std::string dFailOpName =
                                std::string("dfail_") + getShortName(stencilOps[dFailOpNdx]);
                            de::MovePtr<tcu::TestCaseGroup> dFailOpTest(
                                new tcu::TestCaseGroup(testCtx, dFailOpName.c_str()));

                            for (uint32_t compareOpNdx = 0u; compareOpNdx < DE_LENGTH_OF_ARRAY(compareOps);
                                 compareOpNdx++)
                            {
                                // Iterate front set of stencil state in ascending order
                                const VkStencilOpState stencilStateFront = {
                                    stencilOps[failOpNdx],    // failOp
                                    stencilOps[passOpNdx],    // passOp
                                    stencilOps[dFailOpNdx],   // depthFailOp
                                    compareOps[compareOpNdx], // compareOp
                                    0x0,                      // compareMask
                                    0x0,                      // writeMask
                                    0x0                       // reference
                                };

                                // Iterate back set of stencil state in random order
                                const VkStencilOpState stencilStateBack = stencilOpItr.next();
                                const std::string caseName              = compareOpNames[compareOpNdx];

                                de::MovePtr<tcu::TestCaseGroup> layoutTest(
                                    new tcu::TestCaseGroup(testCtx, caseName.c_str()));

                                for (uint32_t layoutNdx = 0u; layoutNdx < 2; layoutNdx++)
                                {
                                    bool useGeneralLayout = layoutNdx == 1;
                                    if (useGeneralLayout)
                                    {
                                        if (failOpNdx > 2 || passOpNdx > 2 || dFailOpNdx > 2 || compareOpNdx > 2)
                                        {
                                            continue;
                                        }
                                    }
                                    const char *layoutName = useGeneralLayout ? "general" : "any";
                                    layoutTest->addChild(new StencilTest(testCtx, layoutName, pipelineConstructionType,
                                                                         format, stencilStateFront, stencilStateBack,
                                                                         colorEnabled, useSeparateDepthStencilLayouts,
                                                                         useGeneralLayout));
                                }
                                dFailOpTest->addChild(layoutTest.release());
                            }
                            passOpTest->addChild(dFailOpTest.release());
                        }
                        failOpTest->addChild(passOpTest.release());
                    }
                    stencilStateTests->addChild(failOpTest.release());
                }

                formatTest->addChild(stencilStateTests.release());
                formatTests->addChild(formatTest.release());
            }
        }

        if (colorEnabled)
            stencilTests->addChild(formatTests.release());
        else
            noColorAttachmentTests->addChild(formatTests.release());
    }

    stencilTests->addChild(noColorAttachmentTests.release());

    // Tests attempting to enable the stencil test while not using a stencil attachment.
    const auto isNoStencilRelevantVariant =
        (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ||
         pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV ||
         pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY);
    if (isNoStencilRelevantVariant)
    {
        de::MovePtr<tcu::TestCaseGroup> noStencilAttGroup(new tcu::TestCaseGroup(testCtx, "no_stencil_att"));

        for (const auto dynamicRendering : {false, true})
        {
            if (!dynamicRendering && isConstructionTypeShaderObject(pipelineConstructionType))
                continue;

#ifdef CTS_USES_VULKANSC
            if (dynamicRendering)
                continue;
#endif

            const char *renderingGroupName = (dynamicRendering ? "dynamic_rendering" : "render_passes");
            de::MovePtr<tcu::TestCaseGroup> renderingGroup(new tcu::TestCaseGroup(testCtx, renderingGroupName));

            for (const auto dynamicEnable : {false, true})
            {
                const char *enableGroupName = (dynamicEnable ? "dynamic_enable" : "static_enable");
                de::MovePtr<tcu::TestCaseGroup> dynEnableGroup(new tcu::TestCaseGroup(testCtx, enableGroupName));

                for (const auto depthComponentFormat : formats::depthFormats)
                {
                    // When using classic render passes, we cannot indicate a separate stencil format and image.
                    const auto tcuFormat = mapVkFormat(depthComponentFormat);
                    if (!dynamicRendering && tcu::hasStencilComponent(tcuFormat.order))
                        continue;

                    const NoStencilAttachmentParams params{
                        pipelineConstructionType,
                        depthComponentFormat,
                        dynamicRendering,
                        dynamicEnable,
                    };
                    const std::string testName = getFormatCaseName(depthComponentFormat);
                    dynEnableGroup->addChild(new NoStencilAttachmentCase(testCtx, testName, params));
                }

                renderingGroup->addChild(dynEnableGroup.release());
            }

            noStencilAttGroup->addChild(renderingGroup.release());
        }

        stencilTests->addChild(noStencilAttGroup.release());
    }

    return stencilTests.release();
}

} // namespace pipeline
} // namespace vkt
