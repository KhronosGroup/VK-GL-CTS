/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Tests attachments unused by subpasses
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassUnusedAttachmentTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "vktRenderPassTestsUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuPlatform.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include <cstring>
#include <set>
#include <sstream>
#include <vector>

namespace vkt
{
namespace renderpass
{

using namespace vk;

namespace
{

struct TestParams
{
    VkAttachmentLoadOp loadOp;
    VkAttachmentStoreOp storeOp;
    VkAttachmentLoadOp stencilLoadOp;
    VkAttachmentStoreOp stencilStoreOp;
    SharedGroupParams groupParams;
};

struct Vertex4RGBA
{
    tcu::Vec4 position;
    tcu::Vec4 color;
};

std::vector<Vertex4RGBA> createQuad(void)
{
    std::vector<Vertex4RGBA> vertices;

    const float size = 0.8f;
    const tcu::Vec4 color(0.2f, 0.3f, 0.1f, 1.0f);
    const Vertex4RGBA lowerLeftVertex  = {tcu::Vec4(-size, -size, 0.0f, 1.0f), color};
    const Vertex4RGBA lowerRightVertex = {tcu::Vec4(size, -size, 0.0f, 1.0f), color};
    const Vertex4RGBA upperLeftVertex  = {tcu::Vec4(-size, size, 0.0f, 1.0f), color};
    const Vertex4RGBA upperRightVertex = {tcu::Vec4(size, size, 0.0f, 1.0f), color};

    vertices.push_back(lowerLeftVertex);
    vertices.push_back(lowerRightVertex);
    vertices.push_back(upperLeftVertex);
    vertices.push_back(upperLeftVertex);
    vertices.push_back(lowerRightVertex);
    vertices.push_back(upperRightVertex);

    return vertices;
}

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass(const DeviceInterface &vk, VkDevice vkDevice, const TestParams testParams)
{
    const VkImageAspectFlags aspectMask =
        testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY ? 0 : VK_IMAGE_ASPECT_COLOR_BIT;
    const AttachmentDesc attachmentDescriptions[] = {
        // Result attachment
        AttachmentDesc(nullptr,                                 // const void*                        pNext
                       (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
                       VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
                       VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits            samples
                       VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
                       VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
                       VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
                       VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
                       VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                    finalLayout
                       ),
        // Unused attachment
        AttachmentDesc(nullptr,                                  // const void*                        pNext
                       (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags        flags
                       VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat                            format
                       VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits            samples
                       testParams.loadOp,                        // VkAttachmentLoadOp                loadOp
                       testParams.storeOp,                       // VkAttachmentStoreOp                storeOp
                       testParams.stencilLoadOp,                 // VkAttachmentLoadOp                stencilLoadOp
                       testParams.stencilStoreOp,                // VkAttachmentStoreOp                stencilStoreOp
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout                    initialLayout
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout                    finalLayout
                       ),
        // Input attachment
        AttachmentDesc(nullptr,                                 // const void*                        pNext
                       (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
                       VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
                       VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits            samples
                       VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
                       VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
                       VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
                       VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
                       VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                    finalLayout
                       )};

    // Note: Attachment 1 is not referenced by any subpass.
    const AttachmentRef resultAttachmentRefSubpass0(
        nullptr,                                  // const void*            pNext
        2u,                                       // uint32_t                attachment
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
        aspectMask                                // VkImageAspectFlags    aspectMask
    );

    const AttachmentRef resultAttachmentRefSubpass1(
        nullptr,                                  // const void*            pNext
        0u,                                       // uint32_t                attachment
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
        aspectMask                                // VkImageAspectFlags    aspectMask
    );

    const AttachmentRef inputAttachmentRefSubpass1(
        nullptr,                                  // const void*            pNext
        2u,                                       // uint32_t                attachment
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // VkImageLayout        layout
        aspectMask                                // VkImageAspectFlags    aspectMask
    );

    const SubpassDesc subpassDescriptions[] = {
        SubpassDesc(nullptr,
                    (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags        flags
                    VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint                pipelineBindPoint
                    0u,                              // uint32_t                            viewMask
                    0u,                              // uint32_t                            inputAttachmentCount
                    nullptr,                         // const VkAttachmentReference*        pInputAttachments
                    1u,                              // uint32_t                            colorAttachmentCount
                    &resultAttachmentRefSubpass0,    // const VkAttachmentReference*        pColorAttachments
                    nullptr,                         // const VkAttachmentReference*        pResolveAttachments
                    nullptr,                         // const VkAttachmentReference*        pDepthStencilAttachment
                    0u,                              // uint32_t                            preserveAttachmentCount
                    nullptr                          // const uint32_t*                    pPreserveAttachments
                    ),
        SubpassDesc(nullptr,
                    (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags        flags
                    VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint                pipelineBindPoint
                    0u,                              // uint32_t                            viewMask
                    1u,                              // uint32_t                            inputAttachmentCount
                    &inputAttachmentRefSubpass1,     // const VkAttachmentReference*        pInputAttachments
                    1u,                              // uint32_t                            colorAttachmentCount
                    &resultAttachmentRefSubpass1,    // const VkAttachmentReference*        pColorAttachments
                    nullptr,                         // const VkAttachmentReference*        pResolveAttachments
                    nullptr,                         // const VkAttachmentReference*        pDepthStencilAttachment
                    0u,                              // uint32_t                            preserveAttachmentCount
                    nullptr                          // const uint32_t*                    pPreserveAttachments
                    )};

    const SubpassDep subpassDependency(
        nullptr,                                       // const void*                pNext
        0u,                                            // uint32_t                    srcSubpass
        1u,                                            // uint32_t                    dstSubpass
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags        srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // VkPipelineStageFlags        dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags            srcAccessMask
        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,           // VkAccessFlags            dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT,                   // VkDependencyFlags        dependencyFlags
        0u                                             // int32_t                    viewOffset
    );

    const RenderPassCreateInfo renderPassInfo(nullptr,                    // const void*                        pNext
                                              (VkRenderPassCreateFlags)0, // VkRenderPassCreateFlags            flags
                                              3u, // uint32_t                            attachmentCount
                                              attachmentDescriptions, // const VkAttachmentDescription*    pAttachments
                                              2u,                  // uint32_t                            subpassCount
                                              subpassDescriptions, // const VkSubpassDescription*        pSubpasses
                                              1u,                 // uint32_t                            dependencyCount
                                              &subpassDependency, // const VkSubpassDependency*        pDependencies
                                              0u,     // uint32_t                            correlatedViewMaskCount
                                              nullptr // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(vk, vkDevice);
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
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

#ifndef CTS_USES_VULKANSC
void beginSecondaryCmdBuffer(const DeviceInterface &vk, VkCommandBuffer secCmdBuffer,
                             const void *additionalInheritanceRenderingInfo)
{
    VkCommandBufferUsageFlags usageFlags(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    const std::vector<VkFormat> colorAttachmentFormats(3, VK_FORMAT_R8G8B8A8_UNORM);

    const VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        additionalInheritanceRenderingInfo,                              // const void* pNext;
        0u,                                                              // VkRenderingFlagsKHR flags;
        0u,                                                              // uint32_t viewMask;
        3u,                                                              // uint32_t colorAttachmentCount;
        colorAttachmentFormats.data(),                                   // const VkFormat* pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        VK_SAMPLE_COUNT_1_BIT,                                           // VkSampleCountFlagBits rasterizationSamples;
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
#endif // CTS_USES_VULKANSC

class UnusedAttachmentTest : public vkt::TestCase
{
public:
    UnusedAttachmentTest(tcu::TestContext &testContext, const std::string &name, const TestParams &testParams);
    virtual ~UnusedAttachmentTest(void) = default;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    void checkSupport(Context &context) const;

private:
    const TestParams m_testParams;
};

class UnusedAttachmentTestInstance : public vkt::TestInstance
{
public:
    UnusedAttachmentTestInstance(Context &context, const TestParams &testParams);
    virtual ~UnusedAttachmentTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void);

protected:
    template <typename RenderpassSubpass>
    void createCommandBuffer(const DeviceInterface &vk, VkDevice vkDevice);
    void createCommandBufferDynamicRendering(const DeviceInterface &vk, VkDevice vkDevice);

#ifndef CTS_USES_VULKANSC
    void preRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void inbetweenRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
#endif // CTS_USES_VULKANSC
    void drawFirstSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void drawSecondSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);

private:
    tcu::TestStatus verifyImage(void);

    const TestParams m_testParams;
    const tcu::UVec2 m_renderSize;

    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorAttachmentView;

    Move<VkImage> m_unusedImage;
    de::MovePtr<Allocation> m_unusedImageAlloc;
    Move<VkImageView> m_unusedAttachmentView;

    Move<VkImage> m_inputImage;
    de::MovePtr<Allocation> m_inputImageAlloc;
    Move<VkImageView> m_inputAttachmentView;
    VkImageLayout m_inputImageReadLayout;

    Move<VkDescriptorSetLayout> m_descriptorSetLayoutSubpass0;
    Move<VkDescriptorSetLayout> m_descriptorSetLayoutSubpass1;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSetSubpass1;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModuleSubpass0;
    ShaderWrapper m_fragmentShaderModuleSubpass1;

    Move<VkBuffer> m_vertexBuffer;
    std::vector<Vertex4RGBA> m_vertices;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    Move<VkBuffer> m_backingBuffer;
    de::MovePtr<Allocation> m_backingBufferAlloc;

    PipelineLayoutWrapper m_pipelineLayoutSubpass0;
    PipelineLayoutWrapper m_pipelineLayoutSubpass1;
    GraphicsPipelineWrapper m_graphicsPipelineSubpass0;
    GraphicsPipelineWrapper m_graphicsPipelineSubpass1;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkCommandBuffer> m_secCmdBuffer;
};

UnusedAttachmentTest::UnusedAttachmentTest(tcu::TestContext &testContext, const std::string &name,
                                           const TestParams &testParams)
    : vkt::TestCase(testContext, name)
    , m_testParams(testParams)
{
}

TestInstance *UnusedAttachmentTest::createInstance(Context &context) const
{
    return new UnusedAttachmentTestInstance(context, m_testParams);
}

void UnusedAttachmentTest::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_testParams.groupParams->pipelineConstructionType);
    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");
}

void UnusedAttachmentTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream fragmentSource;

    sourceCollections.glslSources.add("color_vert")
        << glu::VertexSource("#version 450\n"
                             "layout(location = 0) in highp vec4 position;\n"
                             "layout(location = 1) in highp vec4 color;\n"
                             "layout(location = 0) out highp vec4 vtxColor;\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_Position = position;\n"
                             "    vtxColor = color;\n"
                             "}\n");

    sourceCollections.glslSources.add("color_frag_sb0")
        << glu::FragmentSource("#version 450\n"
                               "layout(location = 0) in highp vec4 vtxColor;\n"
                               "layout(location = 0) out highp vec4 fragColor;\n"
                               "void main (void)\n"
                               "{\n"
                               "    fragColor = vtxColor;\n"
                               "}\n");

    sourceCollections.glslSources.add("color_frag_sb1") << glu::FragmentSource(
        "#version 450\n"
        "layout(location = 0) in highp vec4 vtxColor;\n"
        "layout(location = 0) out highp vec4 fragColor;\n"
        "layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColor;"
        "void main (void)\n"
        "{\n"
        "    fragColor = subpassLoad(inputColor) + vtxColor;\n"
        "}\n");
}

UnusedAttachmentTestInstance::UnusedAttachmentTestInstance(Context &context, const TestParams &testParams)
    : vkt::TestInstance(context)
    , m_testParams(testParams)
    , m_renderSize(32u, 32u)
    , m_inputImageReadLayout(chooseInputImageLayout(testParams.groupParams))
    , m_vertices(createQuad())
    , m_graphicsPipelineSubpass0(context.getInstanceInterface(), context.getDeviceInterface(),
                                 context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(),
                                 m_testParams.groupParams->pipelineConstructionType)
    , m_graphicsPipelineSubpass1(context.getInstanceInterface(), context.getDeviceInterface(),
                                 context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(),
                                 m_testParams.groupParams->pipelineConstructionType)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

    // Create color image
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            nullptr,                                                               // const void* pNext;
            0u,                                                                    // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
            VK_FORMAT_R8G8B8A8_UNORM,                                              // VkFormat format;
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

        m_colorImage = createImage(vk, vkDevice, &colorImageParams);

        // Allocate and bind color image memory
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));
    }

    // Create image which is not used by any subpass
    {
        const VkImageCreateInfo unusedImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,      // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                         // VkImageType imageType;
            VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u}, // VkExtent3D extent;
            1u,                                       // uint32_t mipLevels;
            1u,                                       // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                  // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
            1u,                                  // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,                   // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
        };

        m_unusedImage = createImage(vk, vkDevice, &unusedImageParams);

        // Allocate and bind unused image memory
        VkMemoryRequirements memoryRequirements = getImageMemoryRequirements(vk, vkDevice, *m_unusedImage);

        m_unusedImageAlloc = memAlloc.allocate(memoryRequirements, MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_unusedImage, m_unusedImageAlloc->getMemory(),
                                    m_unusedImageAlloc->getOffset()));

        // Clear image with specific value to verify the contents don't change
        {
            const VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Move<VkCommandPool> cmdPool;
            Move<VkCommandBuffer> cmdBuffer;

            VkClearValue clearValue;
            clearValue.color.float32[0] = 0.1f;
            clearValue.color.float32[1] = 0.2f;
            clearValue.color.float32[2] = 0.3f;
            clearValue.color.float32[3] = 0.4f;

            // Create command pool and buffer
            cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
            cmdBuffer = allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            const VkImageMemoryBarrier preImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                0u,                                     // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                *m_unusedImage,                         // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    aspectMask, // VkImageAspect aspect;
                    0u,         // uint32_t baseMipLevel;
                    1u,         // uint32_t mipLevels;
                    0u,         // uint32_t baseArraySlice;
                    1u          // uint32_t arraySize;
                }};

            const VkImageMemoryBarrier postImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType sType;
                nullptr,                                  // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,             // VkAccessFlags srcAccessMask;
                VK_ACCESS_SHADER_READ_BIT,                // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,     // VkImageLayout oldLayout;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                  // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                  // uint32_t dstQueueFamilyIndex;
                *m_unusedImage,                           // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    aspectMask, // VkImageAspect aspect;
                    0u,         // uint32_t baseMipLevel;
                    1u,         // uint32_t mipLevels;
                    0u,         // uint32_t baseArraySlice;
                    1u          // uint32_t arraySize;
                }};

            const VkImageSubresourceRange clearRange = {
                aspectMask, // VkImageAspectFlags aspectMask;
                0u,         // uint32_t baseMipLevel;
                1u,         // uint32_t levelCount;
                0u,         // uint32_t baseArrayLayer;
                1u          // uint32_t layerCount;
            };

            // Clear image
            beginCommandBuffer(vk, *cmdBuffer);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &preImageBarrier);
            vk.cmdClearColorImage(*cmdBuffer, *m_unusedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color,
                                  1, &clearRange);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);
            endCommandBuffer(vk, *cmdBuffer);

            submitCommandsAndWait(vk, vkDevice, m_context.getUniversalQueue(), cmdBuffer.get());
        }
    }

    // Create input image
    {
        const VkImageCreateInfo inputImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                       // VkStructureType sType;
            nullptr,                                                                   // const void* pNext;
            0u,                                                                        // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                          // VkImageType imageType;
            VK_FORMAT_R8G8B8A8_UNORM,                                                  // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u},                                  // VkExtent3D extent;
            1u,                                                                        // uint32_t mipLevels;
            1u,                                                                        // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                     // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                                   // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                                 // VkSharingMode sharingMode;
            1u,                       // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout initialLayout;
        };

        m_inputImage = createImage(vk, vkDevice, &inputImageParams);

        // Allocate and bind input image memory
        m_inputImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_inputImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_inputImage, m_inputImageAlloc->getMemory(),
                                    m_inputImageAlloc->getOffset()));
    }

    // Create color attachment view
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkImageViewCreateFlags flags;
            *m_colorImage,                              // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
            VK_FORMAT_R8G8B8A8_UNORM,                   // VkFormat format;
            componentMappingRGBA,                       // VkChannelMapping channels;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    // Create unused attachment view
    {
        const VkImageViewCreateInfo unusedAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkImageViewCreateFlags flags;
            *m_unusedImage,                             // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
            VK_FORMAT_R8G8B8A8_UNORM,                   // VkFormat format;
            componentMappingRGBA,                       // VkChannelMapping channels;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_unusedAttachmentView = createImageView(vk, vkDevice, &unusedAttachmentViewParams);
    }

    // Create input attachment view
    {
        const VkImageViewCreateInfo inputAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkImageViewCreateFlags flags;
            *m_inputImage,                              // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
            VK_FORMAT_R8G8B8A8_UNORM,                   // VkFormat format;
            componentMappingRGBA,                       // VkChannelMapping channels;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_inputAttachmentView = createImageView(vk, vkDevice, &inputAttachmentViewParams);
    }

    // Create render pass
    if (testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        m_renderPass = createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1,
                                        SubpassDependency1, RenderPassCreateInfo1>(vk, vkDevice, testParams);
    else if (testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        m_renderPass = createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                        SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, testParams);
    else
        m_renderPass = Move<VkRenderPass>();

    // Create framebuffer if renderpass handle is valid
    if (*m_renderPass != VK_NULL_HANDLE)
    {
        const VkImageView imageViews[] = {*m_colorAttachmentView, *m_unusedAttachmentView, *m_inputAttachmentView};

        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            3u,                                        // uint32_t attachmentCount;
            imageViews,                                // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
    }

    // Create pipeline layout for subpass 0
    {
        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutParams = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType
            nullptr,                                             // const void*                            pNext
            0u,                                                  // VkDescriptorSetLayoutCreateFlags        flags
            0u,                                                  // uint32_t                                bindingCount
            nullptr                                              // const VkDescriptorSetLayoutBinding*    pBindings
        };
        m_descriptorSetLayoutSubpass0 = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);
        m_pipelineLayoutSubpass0 = PipelineLayoutWrapper(testParams.groupParams->pipelineConstructionType, vk, vkDevice,
                                                         *m_descriptorSetLayoutSubpass0);
    }

    // Create pipeline layout for subpass 1
    {
        const VkDescriptorSetLayoutBinding layoutBinding = {
            0u,                                  // uint32_t binding;
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType descriptorType;
            1u,                                  // uint32_t descriptorCount;
            VK_SHADER_STAGE_FRAGMENT_BIT,        // VkShaderStageFlags stageFlags;
            nullptr                              // const VkSampler* pImmutableSamplers;
        };

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutParams = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType
            nullptr,                                             // const void*                            pNext
            0u,                                                  // VkDescriptorSetLayoutCreateFlags        flags
            1u,                                                  // uint32_t                                bindingCount
            &layoutBinding                                       // const VkDescriptorSetLayoutBinding*    pBindings
        };
        m_descriptorSetLayoutSubpass1 = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);
        m_pipelineLayoutSubpass1 = PipelineLayoutWrapper(testParams.groupParams->pipelineConstructionType, vk, vkDevice,
                                                         *m_descriptorSetLayoutSubpass1);
    }

    // Update descriptor set
    {
        const VkDescriptorPoolSize descriptorPoolSize = {
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType type;
            1u                                   // uint32_t descriptorCount;
        };

        const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType                sType
            nullptr,                                           // const void*                    pNext
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags    flags
            1u,                                                // uint32_t                        maxSets
            1u,                                                // uint32_t                        poolSizeCount
            &descriptorPoolSize                                // const VkDescriptorPoolSize*    pPoolSizes
        };

        m_descriptorPool = createDescriptorPool(vk, vkDevice, &descriptorPoolCreateInfo);

        const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                    sType
            nullptr,                                        // const void*                        pNext
            *m_descriptorPool,                              // VkDescriptorPool                    descriptorPool
            1u,                                             // uint32_t                            descriptorSetCount
            &m_descriptorSetLayoutSubpass1.get(),           // const VkDescriptorSetLayout*        pSetLayouts
        };

        m_descriptorSetSubpass1 = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

        const VkDescriptorImageInfo inputImageInfo = {
            VK_NULL_HANDLE,         // VkSampler sampler;
            *m_inputAttachmentView, // VkImageView imageView;
            m_inputImageReadLayout  // VkImageLayout imageLayout;
        };

        const VkWriteDescriptorSet descriptorWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            *m_descriptorSetSubpass1,               // VkDescriptorSet dstSet;
            0u,                                     // uint32_t dstBinding;
            0u,                                     // uint32_t dstArrayElement;
            1u,                                     // uint32_t descriptorCount;
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,    // VkDescriptorType descriptorType;
            &inputImageInfo,                        // const VkDescriptorImageInfo* pImageInfo;
            nullptr,                                // const VkDescriptorBufferInfo* pBufferInfo;
            nullptr                                 // const VkBufferView* pTexelBufferView;
        };

        vk.updateDescriptorSets(vkDevice, 1u, &descriptorWrite, 0u, nullptr);
    }

    m_vertexShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    m_fragmentShaderModuleSubpass0 =
        ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag_sb0"), 0);
    m_fragmentShaderModuleSubpass1 =
        ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag_sb1"), 0);

    // Create pipelines
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBA),        // uint32_t strideInBytes;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate inputRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
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
                (uint32_t)(sizeof(float) * 4), // uint32_t offset;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t vertexAttributeDescriptionCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
        deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
        colorBlendAttachmentState.colorWriteMask = 0xF;

        uint32_t colorAttachmentsCount = (*m_renderPass == VK_NULL_HANDLE) ? 3u : 1u;
        const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(colorAttachmentsCount,
                                                                                          colorBlendAttachmentState);
        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
        colorBlendStateCreateInfo.attachmentCount                     = uint32_t(colorBlendAttachmentStates.size());
        colorBlendStateCreateInfo.pAttachments                        = colorBlendAttachmentStates.data();

        PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
        RenderingAttachmentLocationInfoWrapper renderingAttachmentLocationInfoWrapper;
        RenderingInputAttachmentIndexInfoWrapper renderingInputAttachmentIndexInfoWrapper;
        const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
        const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

#ifndef CTS_USES_VULKANSC
        uint32_t colorAttachmentLocationsSubpass0[]{VK_ATTACHMENT_UNUSED, VK_ATTACHMENT_UNUSED, 0};
        uint32_t colorAttachmentLocationsSubpass1[]{0, VK_ATTACHMENT_UNUSED, VK_ATTACHMENT_UNUSED};
        VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo = initVulkanStructure();
        renderingAttachmentLocationInfo.colorAttachmentCount                 = colorAttachmentsCount;
        renderingAttachmentLocationInfo.pColorAttachmentLocations            = colorAttachmentLocationsSubpass0;

        uint32_t colorAttachmentInputIndices[]{VK_ATTACHMENT_UNUSED, VK_ATTACHMENT_UNUSED, 0};
        VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo{
            VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
            nullptr,
            colorAttachmentsCount,       // uint32_t                    colorAttachmentCount
            colorAttachmentInputIndices, // const uint32_t*            pColorAttachmentInputIndices
            nullptr,                     // uint32_t                    depthInputAttachmentIndex
            nullptr,                     // uint32_t                    stencilInputAttachmentIndex
        };

        const std::vector<VkFormat> colorAttachmentFormats(colorAttachmentsCount, VK_FORMAT_R8G8B8A8_UNORM);
        VkPipelineRenderingCreateInfo renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                          nullptr,
                                                          0u,
                                                          3u,
                                                          colorAttachmentFormats.data(),
                                                          VK_FORMAT_UNDEFINED,
                                                          VK_FORMAT_UNDEFINED};

        if (*m_renderPass == VK_NULL_HANDLE)
        {
            renderingCreateInfoWrapper.ptr               = &renderingCreateInfo;
            renderingAttachmentLocationInfoWrapper.ptr   = &renderingAttachmentLocationInfo;
            renderingInputAttachmentIndexInfoWrapper.ptr = &renderingInputAttachmentIndexInfo;
        }
#endif // CTS_USES_VULKANSC

        m_graphicsPipelineSubpass0.setDefaultMultisampleState()
            .setDefaultDepthStencilState()
            .setDefaultRasterizationState()
            .setupVertexInputState(&vertexInputStateParams)
            .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayoutSubpass0, *m_renderPass, 0u,
                                              m_vertexShaderModule, 0u, ShaderWrapper(), ShaderWrapper(),
                                              ShaderWrapper(), nullptr, nullptr, renderingCreateInfoWrapper)
            .setupFragmentShaderState(m_pipelineLayoutSubpass0, *m_renderPass, 0u, m_fragmentShaderModuleSubpass0)
            .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateCreateInfo, 0, VK_NULL_HANDLE, {},
                                      renderingAttachmentLocationInfoWrapper)
            .setMonolithicPipelineLayout(m_pipelineLayoutSubpass0)
            .buildPipeline();

#ifndef CTS_USES_VULKANSC
        renderingAttachmentLocationInfo.pColorAttachmentLocations = colorAttachmentLocationsSubpass1;
#endif // CTS_USES_VULKANSC

        m_graphicsPipelineSubpass1.setDefaultMultisampleState()
            .setDefaultDepthStencilState()
            .setDefaultRasterizationState()
            .setupVertexInputState(&vertexInputStateParams)
            .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayoutSubpass1, *m_renderPass, 1u,
                                              m_vertexShaderModule, 0u, ShaderWrapper(), ShaderWrapper(),
                                              ShaderWrapper(), nullptr, nullptr, renderingCreateInfoWrapper)
            .setupFragmentShaderState(m_pipelineLayoutSubpass1, *m_renderPass, 1u, m_fragmentShaderModuleSubpass1, 0, 0,
                                      0, VK_NULL_HANDLE, {}, renderingInputAttachmentIndexInfoWrapper)
            .setupFragmentOutputState(*m_renderPass, 1u, &colorBlendStateCreateInfo, 0, VK_NULL_HANDLE, {},
                                      renderingAttachmentLocationInfoWrapper)
            .setMonolithicPipelineLayout(m_pipelineLayoutSubpass1)
            .buildPipeline();
    }

    // Create vertex buffer
    {
        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                    // VkStructureType sType;
            nullptr,                                                 // const void* pNext;
            0u,                                                      // VkBufferCreateFlags flags;
            (VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()), // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,                       // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                               // VkSharingMode sharingMode;
            1u,                                                      // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                                        // const uint32_t* pQueueFamilyIndices;
        };

        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        // Upload vertex data
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    if (testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        createCommandBuffer<RenderpassSubpass1>(vk, vkDevice);
    else if (testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        createCommandBuffer<RenderpassSubpass2>(vk, vkDevice);
    else
        createCommandBufferDynamicRendering(vk, vkDevice);
}

template <typename RenderpassSubpass>
void UnusedAttachmentTestInstance::createCommandBuffer(const DeviceInterface &vk, VkDevice vkDevice)
{
    const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(nullptr, VK_SUBPASS_CONTENTS_INLINE);
    const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(nullptr);

    const VkClearValue attachmentClearValues[] = {
        makeClearValueColorF32(0.5f, 0.5f, 0.5f, 1.0f), // color
        makeClearValueColorF32(0.5f, 0.5f, 0.5f, 1.0f), // unused
        makeClearValueColorF32(0.5f, 0.2f, 0.1f, 1.0f)  // input
    };

    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    const VkRenderPassBeginInfo renderPassBeginInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        *m_renderPass,                            // VkRenderPass renderPass;
        *m_framebuffer,                           // VkFramebuffer framebuffer;
        makeRect2D(m_renderSize),                 // VkRect2D renderArea;
        3u,                                       // uint32_t clearValueCount;
        attachmentClearValues                     // const VkClearValue* pClearValues;
    };
    RenderpassSubpass::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfo, &subpassBeginInfo);

    drawFirstSubpass(vk, *m_cmdBuffer);
    vk.cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    drawSecondSubpass(vk, *m_cmdBuffer);

    RenderpassSubpass::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);
    endCommandBuffer(vk, *m_cmdBuffer);
}

void UnusedAttachmentTestInstance::createCommandBufferDynamicRendering(const DeviceInterface &vk, VkDevice vkDevice)
{
#ifndef CTS_USES_VULKANSC
    uint32_t colorAttachmentLocationsSubpass0[]{VK_ATTACHMENT_UNUSED, VK_ATTACHMENT_UNUSED, 0};
    uint32_t colorAttachmentLocationsSubpass1[]{0, VK_ATTACHMENT_UNUSED, VK_ATTACHMENT_UNUSED};
    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo = initVulkanStructure();
    renderingAttachmentLocationInfo.colorAttachmentCount                 = 3u;
    renderingAttachmentLocationInfo.pColorAttachmentLocations            = colorAttachmentLocationsSubpass0;

    uint32_t colorAttachmentInputIndicesSubpass0[]{VK_ATTACHMENT_UNUSED, VK_ATTACHMENT_UNUSED, VK_ATTACHMENT_UNUSED};
    uint32_t colorAttachmentInputIndicesSubpass1[]{VK_ATTACHMENT_UNUSED, VK_ATTACHMENT_UNUSED, 0};
    VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo{
        VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
        nullptr,
        3u,                                  // uint32_t                   colorAttachmentCount
        colorAttachmentInputIndicesSubpass1, // const uint32_t*            pColorAttachmentInputIndices
        nullptr,                             // uint32_t                   depthInputAttachmentIndex
        nullptr,                             // uint32_t                   stencilInputAttachmentIndex
    };

    std::vector<VkRenderingAttachmentInfo> colorAttachments(
        3u,
        {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,   // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            *m_colorAttachmentView,                        // VkImageView imageView;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,      // VkImageLayout imageLayout;
            VK_RESOLVE_MODE_NONE,                          // VkResolveModeFlagBits resolveMode;
            VK_NULL_HANDLE,                                // VkImageView resolveImageView;
            VK_IMAGE_LAYOUT_UNDEFINED,                     // VkImageLayout resolveImageLayout;
            VK_ATTACHMENT_LOAD_OP_CLEAR,                   // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                  // VkAttachmentStoreOp storeOp;
            makeClearValueColorF32(0.5f, 0.5f, 0.5f, 1.0f) // VkClearValue clearValue;
        });

    colorAttachments[1].imageView   = *m_unusedAttachmentView;
    colorAttachments[1].loadOp      = m_testParams.loadOp;
    colorAttachments[1].storeOp     = m_testParams.storeOp;
    colorAttachments[2].imageView   = *m_inputAttachmentView;
    colorAttachments[2].imageLayout = m_inputImageReadLayout;
    colorAttachments[2].clearValue  = makeClearValueColorF32(0.5f, 0.2f, 0.1f, 1.0f);

    VkRenderingInfo renderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        0,                        // VkRenderingFlagsKHR flags;
        makeRect2D(m_renderSize), // VkRect2D renderArea;
        1u,                       // uint32_t layerCount;
        0u,                       // uint32_t viewMask;
        3u,                       // uint32_t colorAttachmentCount;
        colorAttachments.data(),  // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                  // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                  // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
    {
        m_secCmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        VkCommandBuffer secCmdBuffer = *m_secCmdBuffer;

        // record secondary command buffer
        renderingAttachmentLocationInfo.pNext = &renderingInputAttachmentIndexInfo;
        beginSecondaryCmdBuffer(vk, secCmdBuffer, &renderingAttachmentLocationInfo);
        vk.cmdBeginRendering(secCmdBuffer, &renderingInfo);

        renderingAttachmentLocationInfo.pNext                          = nullptr;
        renderingAttachmentLocationInfo.pColorAttachmentLocations      = colorAttachmentLocationsSubpass0;
        renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices = colorAttachmentInputIndicesSubpass0;
        vk.cmdSetRenderingAttachmentLocationsKHR(secCmdBuffer, &renderingAttachmentLocationInfo);
        vk.cmdSetRenderingInputAttachmentIndicesKHR(secCmdBuffer, &renderingInputAttachmentIndexInfo);
        drawFirstSubpass(vk, secCmdBuffer);
        inbetweenRenderCommands(vk, secCmdBuffer);

        renderingAttachmentLocationInfo.pColorAttachmentLocations      = colorAttachmentLocationsSubpass1;
        renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices = colorAttachmentInputIndicesSubpass1;
        vk.cmdSetRenderingAttachmentLocationsKHR(secCmdBuffer, &renderingAttachmentLocationInfo);
        vk.cmdSetRenderingInputAttachmentIndicesKHR(secCmdBuffer, &renderingInputAttachmentIndexInfo);
        drawSecondSubpass(vk, secCmdBuffer);

        vk.cmdEndRendering(secCmdBuffer);
        endCommandBuffer(vk, secCmdBuffer);

        // record primary command buffer
        beginCommandBuffer(vk, *m_cmdBuffer);
        preRenderCommands(vk, *m_cmdBuffer);
        vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &secCmdBuffer);
        endCommandBuffer(vk, *m_cmdBuffer);
    }
    else
    {
        beginCommandBuffer(vk, *m_cmdBuffer, 0u);
        preRenderCommands(vk, *m_cmdBuffer);
        vk.cmdBeginRendering(*m_cmdBuffer, &renderingInfo);

        vk.cmdSetRenderingAttachmentLocationsKHR(*m_cmdBuffer, &renderingAttachmentLocationInfo);
        drawFirstSubpass(vk, *m_cmdBuffer);
        inbetweenRenderCommands(vk, *m_cmdBuffer);

        renderingAttachmentLocationInfo.pColorAttachmentLocations = colorAttachmentLocationsSubpass1;
        vk.cmdSetRenderingAttachmentLocationsKHR(*m_cmdBuffer, &renderingAttachmentLocationInfo);
        vk.cmdSetRenderingInputAttachmentIndicesKHR(*m_cmdBuffer, &renderingInputAttachmentIndexInfo);
        drawSecondSubpass(vk, *m_cmdBuffer);

        vk.cmdEndRendering(*m_cmdBuffer);
        endCommandBuffer(vk, *m_cmdBuffer);
    }
#else
    DE_UNREF(vk);
    DE_UNREF(vkDevice);
#endif // CTS_USES_VULKANSC
}

#ifndef CTS_USES_VULKANSC
void UnusedAttachmentTestInstance::preRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkImageSubresourceRange subresourceRange(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
    const VkImageMemoryBarrier imageBarriers[]{
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *m_colorImage, subresourceRange),
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               m_inputImageReadLayout, *m_inputImage, subresourceRange),
    };

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          0u, 0u, nullptr, 0u, nullptr, 2u, imageBarriers);
}

void UnusedAttachmentTestInstance::inbetweenRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkImageSubresourceRange subresourceRange(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
    VkImageMemoryBarrier imageBarrier(
        makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                               m_inputImageReadLayout, m_inputImageReadLayout, *m_inputImage, subresourceRange));
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 0u, nullptr,
                          1u, &imageBarrier);
}
#endif // CTS_USES_VULKANSC

void UnusedAttachmentTestInstance::drawFirstSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkDeviceSize vertexBufferOffset = 0;
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineSubpass0.getPipeline());
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);
}

void UnusedAttachmentTestInstance::drawSecondSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkDeviceSize vertexBufferOffset = 0;
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineSubpass1.getPipeline());
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutSubpass1, 0, 1,
                             &m_descriptorSetSubpass1.get(), 0, nullptr);
    vk.cmdDraw(cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);
}

tcu::TestStatus UnusedAttachmentTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyImage();
}

tcu::TestStatus UnusedAttachmentTestInstance::verifyImage(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator allocator(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
    de::UniquePtr<tcu::TextureLevel> textureLevelResult(
        pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage,
                                      VK_FORMAT_R8G8B8A8_UNORM, m_renderSize)
            .release());
    const tcu::ConstPixelBufferAccess &resultAccess = textureLevelResult->getAccess();
    de::UniquePtr<tcu::TextureLevel> textureLevelUnused(
        pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_unusedImage,
                                      VK_FORMAT_R8G8B8A8_UNORM, m_renderSize)
            .release());
    const tcu::ConstPixelBufferAccess &unusedAccess = textureLevelUnused->getAccess();
    tcu::TestLog &log                               = m_context.getTestContext().getLog();
    tcu::Vec4 refColor(0.1f, 0.2f, 0.3f, 0.4f);

    // Define helper lambda for checks if color is same as reference
    auto isColorValid = [](const tcu::Vec4 &color, const tcu::Vec4 &reference)
    { return tcu::boolAll(tcu::lessThan(tcu::absDiff(color, reference), tcu::Vec4(0.01f))); };

    // Log images
    log << tcu::TestLog::ImageSet("Result", "Result images")
        << tcu::TestLog::Image("Rendered", "Rendered image", resultAccess)
        << tcu::TestLog::Image("Unused", "Unused image", unusedAccess) << tcu::TestLog::EndImageSet;

    // With renderpass object there could be attachment that is not listed as color attachment
    // for any subpass and in that case it would not be cleared even when op load clear is specified
    // in dynamic renderpass load operation will be done for all specified color attachments because we dont
    // know at vkCmdBeginRendering which color attachments are going to be used and which will be left unused
    if ((m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) &&
        (m_testParams.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) && (m_testParams.storeOp == VK_ATTACHMENT_STORE_OP_STORE))
        refColor = tcu::Vec4(0.5f, 0.5f, 0.5f, 1.0f);

    // Check the unused image data.
    for (int y = 0; y < unusedAccess.getHeight(); y++)
        for (int x = 0; x < unusedAccess.getWidth(); x++)
        {
            if (!isColorValid(unusedAccess.getPixel(x, y), refColor))
                return tcu::TestStatus::fail("Unused image contents has changed.");
        }

    // Check for rendered result. Just a quick check to see if correct color is found at the center of the quad.
    const tcu::Vec4 resultColor = resultAccess.getPixel(resultAccess.getWidth() / 2, resultAccess.getHeight() / 2);
    if (!isColorValid(resultColor, tcu::Vec4(0.4f, 0.6f, 0.2f, 1.0f)))
        return tcu::TestStatus::fail("Result image mismatch");

    return tcu::TestStatus::pass("Pass");
}

std::string loadOpToString(VkAttachmentLoadOp loadOp)
{
    switch (loadOp)
    {
    case VK_ATTACHMENT_LOAD_OP_LOAD:
        return "load";
    case VK_ATTACHMENT_LOAD_OP_CLEAR:
        return "clear";
    case VK_ATTACHMENT_LOAD_OP_DONT_CARE:
        return "dontcare";
    default:
        DE_FATAL("unexpected attachment load op");
        return "";
    }
}

std::string storeOpToString(VkAttachmentStoreOp storeOp)
{
    switch (storeOp)
    {
    case VK_ATTACHMENT_STORE_OP_STORE:
        return "store";
    case VK_ATTACHMENT_STORE_OP_DONT_CARE:
        return "dontcare";
    default:
        DE_FATAL("unexpected attachment store op");
        return "";
    }
}

} // namespace

tcu::TestCaseGroup *createRenderPassUnusedAttachmentTests(tcu::TestContext &testCtx,
                                                          const SharedGroupParams groupParams)
{
    // Unused attachment tests
    de::MovePtr<tcu::TestCaseGroup> unusedAttTests(new tcu::TestCaseGroup(testCtx, "unused_attachment"));

    const VkAttachmentLoadOp loadOps[] = {VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                          VK_ATTACHMENT_LOAD_OP_DONT_CARE};

    const VkAttachmentStoreOp storeOps[] = {VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_DONT_CARE};

    uint32_t stencilLoadOpStartIdx  = 0;
    uint32_t stencilStoreOpStartIdx = 0;
    if (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        // in dynamic rendering cases we dont_care about stencil load/store
        stencilLoadOpStartIdx  = DE_LENGTH_OF_ARRAY(loadOps) - 1;
        stencilStoreOpStartIdx = DE_LENGTH_OF_ARRAY(storeOps) - 1;
    }

    for (uint32_t loadOpIdx = 0; loadOpIdx < DE_LENGTH_OF_ARRAY(loadOps); loadOpIdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> loadOpGroup(
            new tcu::TestCaseGroup(testCtx, (std::string("loadop") + loadOpToString(loadOps[loadOpIdx])).c_str()));

        for (uint32_t storeOpIdx = 0; storeOpIdx < DE_LENGTH_OF_ARRAY(storeOps); storeOpIdx++)
        {
            if (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
            {
                // for dynamic rendering we need to skip all LOAD_OP_DONT_CARE and STORE_OP_DONT_CARE cases
                // because load/store ops are not affected by remapping, thus loadop=DONTCARE
                // permits to initialize unused attachment with random data and storeop=DONTCARE
                // permits to store random data to unused attachment and this is the case on tiling GPUs
                if ((loadOps[loadOpIdx] == VK_ATTACHMENT_LOAD_OP_DONT_CARE) ||
                    (storeOps[storeOpIdx] == VK_ATTACHMENT_STORE_OP_DONT_CARE))
                    continue;
            }

            de::MovePtr<tcu::TestCaseGroup> storeOpGroup(new tcu::TestCaseGroup(
                testCtx, (std::string("storeop") + storeOpToString(storeOps[storeOpIdx])).c_str()));

            for (uint32_t stencilLoadOpIdx = stencilLoadOpStartIdx; stencilLoadOpIdx < DE_LENGTH_OF_ARRAY(loadOps);
                 stencilLoadOpIdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> stencilLoadOpGroup(new tcu::TestCaseGroup(
                    testCtx, (std::string("stencilloadop") + loadOpToString(loadOps[stencilLoadOpIdx])).c_str()));

                for (uint32_t stencilStoreOpIdx = stencilStoreOpStartIdx;
                     stencilStoreOpIdx < DE_LENGTH_OF_ARRAY(storeOps); stencilStoreOpIdx++)
                {
                    TestParams params;
                    const std::string testName =
                        std::string("stencilstoreop") + storeOpToString(storeOps[stencilStoreOpIdx]);

                    params.loadOp         = loadOps[loadOpIdx];
                    params.storeOp        = storeOps[storeOpIdx];
                    params.stencilLoadOp  = loadOps[stencilLoadOpIdx];
                    params.stencilStoreOp = storeOps[stencilStoreOpIdx];
                    params.groupParams    = groupParams;

                    stencilLoadOpGroup->addChild(new UnusedAttachmentTest(testCtx, testName, params));
                }
                storeOpGroup->addChild(stencilLoadOpGroup.release());
            }
            loadOpGroup->addChild(storeOpGroup.release());
        }
        unusedAttTests->addChild(loadOpGroup.release());
    }

    return unusedAttTests.release();
}

} // namespace renderpass
} // namespace vkt
