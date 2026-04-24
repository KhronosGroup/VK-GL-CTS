/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 LunarG, Inc.
 * Copyright (c) 2025 Google LLC
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
 * \brief Vulkan Query Tests With Discard
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolDiscardTests.hpp"

#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{

namespace QueryPool
{

namespace
{

using namespace vk;
using namespace de;

enum class DiscardType
{
    DISCARD,
    SAMPLE_MASK,
    ALPHA_TO_COVERAGE,
    ALPHA_TO_COVERAGE_DYNAMIC,
};

struct TestParameters
{
    bool earlyFragmentTests;
    bool useDepth;
    bool precise;
    DiscardType discardType;

    bool isAlphaToCoverage() const
    {
        return discardType == DiscardType::ALPHA_TO_COVERAGE || discardType == DiscardType::ALPHA_TO_COVERAGE_DYNAMIC;
    }
};

class QueryPoolDiscardTestInstance : public vkt::TestInstance
{
    const VkExtent2D m_imageSize = {32u, 32u};
    const VkFormat m_colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const VkFormat m_depthFormat = VK_FORMAT_D16_UNORM;
    MovePtr<ImageWithMemory> m_msaaImage;
    MovePtr<ImageWithMemory> m_colorImage;
    MovePtr<ImageWithMemory> m_depthImage;
    Move<VkImageView> m_msaaImageView;
    Move<VkImageView> m_colorImageView;
    Move<VkImageView> m_depthImageView;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkPipeline> m_pipeline;

public:
    QueryPoolDiscardTestInstance(vkt::Context &context, const TestParameters &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }

private:
    void createRenderPass();
    void createPipeline();

    tcu::TestStatus iterate(void);

    const TestParameters m_params;
};

void QueryPoolDiscardTestInstance::createRenderPass()
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    auto &alloc               = m_context.getDefaultAllocator();

    const VkExtent3D imageSize                = {m_imageSize.width, m_imageSize.height, 1u};
    const VkComponentMapping componentMapping = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                                 VK_COMPONENT_SWIZZLE_A};
    const VkImageSubresourceRange colorSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageSubresourceRange depthSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);

    VkImageCreateInfo colorCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType          sType
        nullptr,                                                               // const void*              pNext
        0u,                                                                    // VkImageCreateFlags       flags
        VK_IMAGE_TYPE_2D,                                                      // VkImageType              imageType
        m_colorFormat,                                                         // VkFormat                 format
        imageSize,                                                             // VkExtent3D               extent
        1u,                                                                    // uint32_t                 mipLevels
        1u,                                                                    // uint32_t                 arrayLayers
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling            tiling
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode            sharingMode
        0,                        // uint32_t                 queueFamilyIndexCount
        nullptr,                  // const uint32_t*          pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout            initialLayout
    };

    m_colorImage =
        MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, alloc, colorCreateInfo, MemoryRequirement::Any));

    VkImageViewCreateInfo colorViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        **m_colorImage,                           // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        m_colorFormat,                            // VkFormat format;
        componentMapping,                         // VkComponentMapping components;
        colorSubresourceRange                     // VkImageSubresourceRange subresourceRange;
    };
    m_colorImageView = createImageView(vk, device, &colorViewCreateInfo);

    if (m_params.isAlphaToCoverage())
    {
        colorCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
        m_msaaImage =
            MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, alloc, colorCreateInfo, MemoryRequirement::Any));

        colorViewCreateInfo.image = **m_msaaImage;
        m_msaaImageView           = createImageView(vk, device, &colorViewCreateInfo);
    }

    const VkSampleCountFlagBits depthSampleCount =
        (m_params.isAlphaToCoverage() ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT);

    const VkImageCreateInfo depthCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,         // VkStructureType          sType
        nullptr,                                     // const void*              pNext
        0u,                                          // VkImageCreateFlags       flags
        VK_IMAGE_TYPE_2D,                            // VkImageType              imageType
        m_depthFormat,                               // VkFormat                 format
        imageSize,                                   // VkExtent3D               extent
        1u,                                          // uint32_t                 mipLevels
        1u,                                          // uint32_t                 arrayLayers
        depthSampleCount,                            // VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,                     // VkImageTiling            tiling
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, // VkImageUsageFlags  usage
        VK_SHARING_MODE_EXCLUSIVE,                   // VkSharingMode            sharingMode
        0,                                           // uint32_t                 queueFamilyIndexCount
        nullptr,                                     // const uint32_t*          pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED                    // VkImageLayout            initialLayout
    };

    m_depthImage =
        MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, alloc, depthCreateInfo, MemoryRequirement::Any));
    VkImageViewCreateInfo depthViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        **m_depthImage,                           // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        m_depthFormat,                            // VkFormat format;
        componentMapping,                         // VkComponentMapping components;
        depthSubresourceRange                     // VkImageSubresourceRange subresourceRange;
    };
    m_depthImageView = createImageView(vk, device, &depthViewCreateInfo);

    const VkAttachmentDescription attachmentDescriptions[3] = {
        {
            0u,                                      // VkAttachmentDescriptionFlags    flags;
            m_colorFormat,                           // VkFormat                        format;
            VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits           samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp              loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp             storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp              stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp             stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                   initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                   finalLayout;
        },
        {
            0u,                                              // VkAttachmentDescriptionFlags    flags;
            m_depthFormat,                                   // VkFormat                        format;
            depthSampleCount,                                // VkSampleCountFlagBits           samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp              loadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                // VkAttachmentStoreOp             storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,                 // VkAttachmentLoadOp              stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                // VkAttachmentStoreOp             stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout                   initialLayout;
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // VkImageLayout                   finalLayout;
        },
        {
            0u,                                      // VkAttachmentDescriptionFlags    flags;
            m_colorFormat,                           // VkFormat                        format;
            VK_SAMPLE_COUNT_4_BIT,                   // VkSampleCountFlagBits           samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp              loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp             storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp              stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp             stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                   initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                   finalLayout;
        },
    };

    const VkAttachmentReference colorAttachmentReference = {m_params.isAlphaToCoverage() ? 2u : 0u,
                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference resolveAttachmentReference = {0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference depthAttachmentReference   = {1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference *pResolveAttachmentReference =
        (m_params.isAlphaToCoverage()) ? &resolveAttachmentReference : nullptr;

    const VkSubpassDescription subpassDescription = {
        (VkSubpassDescriptionFlags)0u,   // VkSubpassDescriptionFlags       flags
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint
        0u,                              // uint32_t                        inputAttachmentCount
        nullptr,                         // const VkAttachmentReference*    pInputAttachments
        1u,                              // uint32_t                        colorAttachmentCount
        &colorAttachmentReference,       // const VkAttachmentReference*    pColorAttachments
        pResolveAttachmentReference,     // const VkAttachmentReference*    pResolveAttachments
        &depthAttachmentReference,       // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                              // uint32_t                        preserveAttachmentCount
        nullptr                          // const uint32_t*                 pPreserveAttachments
    };

    const uint32_t attachmentCount = m_params.isAlphaToCoverage() ? 3u : 2u;

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkRenderPassCreateFlags flags;
        attachmentCount,                           // uint32_t attachmentCount;
        attachmentDescriptions,                    // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
        0u,                                        // uint32_t dependencyCount;
        nullptr,                                   // const VkSubpassDependency* pDependencies;
    };
    m_renderPass               = vk::createRenderPass(vk, device, &renderPassInfo);
    VkImageView attachments[3] = {*m_colorImageView, *m_depthImageView, *m_msaaImageView};
    m_framebuffer =
        makeFramebuffer(vk, device, *m_renderPass, attachmentCount, attachments, m_imageSize.width, m_imageSize.height);
}

void QueryPoolDiscardTestInstance::createPipeline()
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(vk, device, binaries.get("vert"));
    const auto fragModule = createShaderModule(vk, device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(m_imageSize));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(m_imageSize));
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    const bool useMsaa                      = m_params.isAlphaToCoverage();
    const VkSampleCountFlagBits sampleCount = useMsaa ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
    VkBool32 alphaToOneCoverage             = makeVkBool(useMsaa);

    const VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        sampleCount,                                              // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        alphaToOneCoverage,                                       // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    const auto stencilOp = makeStencilOpState(VK_STENCIL_OP_ZERO, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_ZERO,
                                              VK_COMPARE_OP_GREATER, 0xFFu, 0xFFu, 128u);
    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        makeVkBool(m_params.useDepth),                              // VkBool32 depthTestEnable;
        makeVkBool(m_params.useDepth),                              // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_LESS,                                         // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        stencilOp,                                                  // VkStencilOpState front;
        stencilOp,                                                  // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        0.0f,                                                       // float maxDepthBounds;
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo = initVulkanStructure();
#ifndef CTS_USES_VULKANSC
    const VkDynamicState dynamicState = VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT;
    if (m_params.discardType == DiscardType::ALPHA_TO_COVERAGE_DYNAMIC)
    {
        dynamicStateInfo.dynamicStateCount = 1u;
        dynamicStateInfo.pDynamicStates    = &dynamicState;
    }
#endif

    m_pipelineLayout = makePipelineLayout(vk, device);
    m_pipeline       = makeGraphicsPipeline(vk, device, *m_pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                            VK_NULL_HANDLE, *fragModule, *m_renderPass, viewports, scissors,
                                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInputState, nullptr,
                                            &multisampleState, &depthStencilState, nullptr, &dynamicStateInfo);
}

tcu::TestStatus QueryPoolDiscardTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = m_context.getUniversalQueue();
    auto &alloc                     = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const Move<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Move<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // sType
        nullptr,                                  // pNext
        0u,                                       // flags
        VK_QUERY_TYPE_OCCLUSION,                  // queryType
        1u,                                       // queryCount
        0u,                                       // pipelineStatistics
    };

    Move<VkQueryPool> queryPool = createQueryPool(vk, device, &queryPoolCreateInfo);

    createRenderPass();
    createPipeline();

    de::MovePtr<BufferWithMemory> colorOutputBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, alloc,
                             makeBufferCreateInfo(m_imageSize.width * m_imageSize.height * sizeof(uint32_t),
                                                  vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                             MemoryRequirement::HostVisible));

    beginCommandBuffer(vk, *cmdBuffer, 0u);
    vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0, 1);
    VkQueryControlFlags controlFlags = m_params.precise ? VK_QUERY_CONTROL_PRECISE_BIT : 0;
    vk.cmdBeginQuery(*cmdBuffer, *queryPool, 0, controlFlags);

    VkClearValue clearValues[3];
    clearValues[0]              = makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    clearValues[1].depthStencil = {1.0f, 0};
    clearValues[2]              = makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(m_imageSize), 3u, clearValues);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
#ifndef CTS_USES_VULKANSC
    if (m_params.discardType == DiscardType::ALPHA_TO_COVERAGE_DYNAMIC)
    {
        vk.cmdSetAlphaToCoverageEnableEXT(*cmdBuffer, VK_TRUE);
    }
#endif
    vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
    endRenderPass(vk, *cmdBuffer);

    vk.cmdEndQuery(*cmdBuffer, *queryPool, 0);

    VkImageMemoryBarrier imageBarrier =
        makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               **m_colorImage, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0u, nullptr, 0u, nullptr, 1u, &imageBarrier);

    VkBufferImageCopy copyRegion =
        makeBufferImageCopy(makeExtent3D(m_imageSize.width, m_imageSize.height, 1u),
                            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
    vk.cmdCopyImageToBuffer(*cmdBuffer, **m_colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **colorOutputBuffer, 1u,
                            &copyRegion);

    const auto bufferBarrier = makeBufferMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT,
                                                       **colorOutputBuffer, 0, VK_WHOLE_SIZE);
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    invalidateAlloc(vk, device, colorOutputBuffer->getAllocation());
    uint32_t queryResult = 0u;
    vk.getQueryPoolResults(device, *queryPool, 0u, 1u, sizeof(uint32_t), &queryResult, sizeof(uint32_t),
                           VK_QUERY_RESULT_WAIT_BIT);

    if (m_params.precise)
    {
        uint32_t expectedOcclusionResult = m_imageSize.width * m_imageSize.height;
        if (!m_params.earlyFragmentTests)
            expectedOcclusionResult /= 2u;
        if (m_params.isAlphaToCoverage())
            expectedOcclusionResult *= 4u;

        if (queryResult != expectedOcclusionResult)
        {
            log << tcu::TestLog::Message << "Expected occlusion query result " << expectedOcclusionResult
                << ", but actual value is " << queryResult << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Unexpected query result");
        }
    }
    else
    {
        if (queryResult == 0)
        {
            log << tcu::TestLog::Message << "Expected occlusion query result 0, but actual value is " << queryResult
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Unexpected query result");
        }
    }

    tcu::ConstPixelBufferAccess resultBuffer =
        tcu::ConstPixelBufferAccess(mapVkFormat(m_colorFormat), m_imageSize.width, m_imageSize.height, 1,
                                    (const void *)colorOutputBuffer->getAllocation().getHostPtr());
    for (uint32_t j = 0u; j < m_imageSize.height; ++j)
    {
        for (uint32_t i = 0u; i < m_imageSize.width; ++i)
        {
            tcu::Vec4 pixel    = resultBuffer.getPixel(i, j);
            tcu::Vec4 expected = ((i & 1) == 1) ? tcu::Vec4(1.0f) : tcu::Vec4(0.0f);
            if (pixel != expected)
            {
                log << tcu::TestLog::Message << "At (" << i << ", " << j << ") expected " << expected
                    << ", but actual value is " << pixel << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class QueryPoolDiscardTestCase : public vkt::TestCase
{
public:
    QueryPoolDiscardTestCase(tcu::TestContext &context, const char *name, const TestParameters &params)
        : TestCase(context, name)
        , m_params(params)
    {
    }

private:
    void checkSupport(vkt::Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new QueryPoolDiscardTestInstance(context, m_params);
    }

    const TestParameters m_params;
};

void QueryPoolDiscardTestCase::checkSupport(vkt::Context &context) const
{
    if (m_params.discardType == DiscardType::ALPHA_TO_COVERAGE_DYNAMIC)
    {
#ifndef CTS_USES_VULKANSC
        const auto &eds3Features = context.getExtendedDynamicState3FeaturesEXT();
        if (!eds3Features.extendedDynamicState3AlphaToCoverageEnable)
            TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToCoverageEnable not supported");
#endif
    }
    if (m_params.precise && !context.getDeviceFeatures().occlusionQueryPrecise)
        TCU_THROW(NotSupportedError, "occlusionQueryPrecise not supported");
    if (m_params.earlyFragmentTests)
    {
        if (!context.getMaintenance5Properties().earlyFragmentSampleMaskTestBeforeSampleCounting)
            TCU_THROW(NotSupportedError, "earlyFragmentSampleMaskTestBeforeSampleCounting not supported");
        if (m_params.isAlphaToCoverage() &&
            !context.getMaintenance5Properties().earlyFragmentMultisampleCoverageAfterSampleCounting)
            TCU_THROW(NotSupportedError, "earlyFragmentMultisampleCoverageAfterSampleCounting not supported");
    }
}

void QueryPoolDiscardTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream vert;
    std::stringstream frag;

    vert << "#version 450\n"
         << "void main() {\n"
         << "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
         << "    gl_Position = vec4(pos * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    frag << "#version 450\n";
    if (m_params.earlyFragmentTests)
        frag << "layout(early_fragment_tests) in;\n";
    frag << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    gl_SampleMask[0] = ~0;\n"
         << "    outColor = vec4(1.0f);\n";
    frag << "    if ((uint(gl_FragCoord.x) & 1u) == 0u) {\n";
    if (m_params.discardType == DiscardType::DISCARD)
        frag << "       discard;\n";
    else if (m_params.discardType == DiscardType::SAMPLE_MASK)
        frag << "       gl_SampleMask[0] = 0;\n";
    else if (m_params.isAlphaToCoverage())
        frag << "       outColor = vec4(1.0f, 1.0f, 1.0f, 0.0f);\n";
    frag << "    }\n"
         << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

} // namespace

tcu::TestCaseGroup *createDiscardTests(tcu::TestContext &testContext)
{
    tcu::TestCaseGroup *discardTests = new tcu::TestCaseGroup(testContext, "discard");

    const bool boolRange[] = {false, true};
    const struct DiscardTests
    {
        DiscardType type;
        const char *name;
    } discardTypes[] = {
        {DiscardType::DISCARD, "discard"},
        {DiscardType::SAMPLE_MASK, "sample_mask"},
        {DiscardType::ALPHA_TO_COVERAGE, "alpha_to_coverage"},
#ifndef CTS_USES_VULKANSC
        {DiscardType::ALPHA_TO_COVERAGE_DYNAMIC, "alpha_to_coverage_dynamic"},
#endif
    };

    for (const bool earlyFragmentTest : boolRange)
    {
        std::string earlyFragmentName = earlyFragmentTest ? "early" : "normal";
        de::MovePtr<tcu::TestCaseGroup> earlyFragmentGroup(
            new tcu::TestCaseGroup(testContext, earlyFragmentName.c_str()));
        for (const bool depth : boolRange)
        {
            std::string depthName = depth ? "with_depth" : "no_depth";
            de::MovePtr<tcu::TestCaseGroup> depthGroup(new tcu::TestCaseGroup(testContext, depthName.c_str()));
            for (const bool precise : boolRange)
            {
                std::string preciseName = precise ? "precise" : "none";
                de::MovePtr<tcu::TestCaseGroup> preciseGroup(new tcu::TestCaseGroup(testContext, preciseName.c_str()));
                for (const auto &discardType : discardTypes)
                {
                    TestParameters params;
                    params.useDepth           = depth;
                    params.precise            = precise;
                    params.discardType        = discardType.type;
                    params.earlyFragmentTests = earlyFragmentTest;
                    preciseGroup->addChild(new QueryPoolDiscardTestCase(testContext, discardType.name, params));
                }
                depthGroup->addChild(preciseGroup.release());
            }
            earlyFragmentGroup->addChild(depthGroup.release());
        }
        discardTests->addChild(earlyFragmentGroup.release());
    }

    return discardTests;
}

} // namespace QueryPool
} // namespace vkt
