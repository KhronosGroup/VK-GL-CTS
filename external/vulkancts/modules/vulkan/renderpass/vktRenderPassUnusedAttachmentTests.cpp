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
    RenderingType renderingType;
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
        testParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY ? 0 : VK_IMAGE_ASPECT_COLOR_BIT;
    const AttachmentDesc attachmentDescriptions[] = {
        // Result attachment
        AttachmentDesc(DE_NULL,                                 // const void*                        pNext
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
        AttachmentDesc(DE_NULL,                                  // const void*                        pNext
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
        AttachmentDesc(DE_NULL,                                 // const void*                        pNext
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
        DE_NULL,                                  // const void*            pNext
        2u,                                       // uint32_t                attachment
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
        aspectMask                                // VkImageAspectFlags    aspectMask
    );

    const AttachmentRef resultAttachmentRefSubpass1(
        DE_NULL,                                  // const void*            pNext
        0u,                                       // uint32_t                attachment
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
        aspectMask                                // VkImageAspectFlags    aspectMask
    );

    const AttachmentRef inputAttachmentRefSubpass1(
        DE_NULL,                                  // const void*            pNext
        2u,                                       // uint32_t                attachment
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // VkImageLayout        layout
        aspectMask                                // VkImageAspectFlags    aspectMask
    );

    const SubpassDesc subpassDescriptions[] = {
        SubpassDesc(DE_NULL,
                    (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags        flags
                    VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint                pipelineBindPoint
                    0u,                              // uint32_t                            viewMask
                    0u,                              // uint32_t                            inputAttachmentCount
                    DE_NULL,                         // const VkAttachmentReference*        pInputAttachments
                    1u,                              // uint32_t                            colorAttachmentCount
                    &resultAttachmentRefSubpass0,    // const VkAttachmentReference*        pColorAttachments
                    DE_NULL,                         // const VkAttachmentReference*        pResolveAttachments
                    DE_NULL,                         // const VkAttachmentReference*        pDepthStencilAttachment
                    0u,                              // uint32_t                            preserveAttachmentCount
                    DE_NULL                          // const uint32_t*                    pPreserveAttachments
                    ),
        SubpassDesc(DE_NULL,
                    (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags        flags
                    VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint                pipelineBindPoint
                    0u,                              // uint32_t                            viewMask
                    1u,                              // uint32_t                            inputAttachmentCount
                    &inputAttachmentRefSubpass1,     // const VkAttachmentReference*        pInputAttachments
                    1u,                              // uint32_t                            colorAttachmentCount
                    &resultAttachmentRefSubpass1,    // const VkAttachmentReference*        pColorAttachments
                    DE_NULL,                         // const VkAttachmentReference*        pResolveAttachments
                    DE_NULL,                         // const VkAttachmentReference*        pDepthStencilAttachment
                    0u,                              // uint32_t                            preserveAttachmentCount
                    DE_NULL                          // const uint32_t*                    pPreserveAttachments
                    )};

    const SubpassDep subpassDependency(
        DE_NULL,                                       // const void*                pNext
        0u,                                            // uint32_t                    srcSubpass
        1u,                                            // uint32_t                    dstSubpass
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags        srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // VkPipelineStageFlags        dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags            srcAccessMask
        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,           // VkAccessFlags            dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT,                   // VkDependencyFlags        dependencyFlags
        0u                                             // int32_t                    viewOffset
    );

    const RenderPassCreateInfo renderPassInfo(DE_NULL,                    // const void*                        pNext
                                              (VkRenderPassCreateFlags)0, // VkRenderPassCreateFlags            flags
                                              3u, // uint32_t                            attachmentCount
                                              attachmentDescriptions, // const VkAttachmentDescription*    pAttachments
                                              2u,                  // uint32_t                            subpassCount
                                              subpassDescriptions, // const VkSubpassDescription*        pSubpasses
                                              1u,                 // uint32_t                            dependencyCount
                                              &subpassDependency, // const VkSubpassDependency*        pDependencies
                                              0u,     // uint32_t                            correlatedViewMaskCount
                                              DE_NULL // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(vk, vkDevice);
}

class UnusedAttachmentTest : public vkt::TestCase
{
public:
    UnusedAttachmentTest(tcu::TestContext &testContext, const std::string &name, const TestParams &testParams);
    virtual ~UnusedAttachmentTest(void);
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const TestParams m_testParams;
};

class UnusedAttachmentTestInstance : public vkt::TestInstance
{
public:
    UnusedAttachmentTestInstance(Context &context, const TestParams &testParams);
    virtual ~UnusedAttachmentTestInstance(void);
    virtual tcu::TestStatus iterate(void);
    template <typename RenderpassSubpass>
    void createCommandBuffer(const DeviceInterface &vk, VkDevice vkDevice);

private:
    tcu::TestStatus verifyImage(void);

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

    Move<VkDescriptorSetLayout> m_descriptorSetLayoutSubpass0;
    Move<VkDescriptorSetLayout> m_descriptorSetLayoutSubpass1;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSetSubpass1;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    Move<VkShaderModule> m_vertexShaderModule;
    Move<VkShaderModule> m_fragmentShaderModuleSubpass0;
    Move<VkShaderModule> m_fragmentShaderModuleSubpass1;

    Move<VkBuffer> m_vertexBuffer;
    std::vector<Vertex4RGBA> m_vertices;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    Move<VkBuffer> m_backingBuffer;
    de::MovePtr<Allocation> m_backingBufferAlloc;

    Move<VkPipelineLayout> m_pipelineLayoutSubpass0;
    Move<VkPipelineLayout> m_pipelineLayoutSubpass1;
    Move<VkPipeline> m_graphicsPipelineSubpass0;
    Move<VkPipeline> m_graphicsPipelineSubpass1;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

UnusedAttachmentTest::UnusedAttachmentTest(tcu::TestContext &testContext, const std::string &name,
                                           const TestParams &testParams)
    : vkt::TestCase(testContext, name)
    , m_testParams(testParams)
{
}

UnusedAttachmentTest::~UnusedAttachmentTest(void)
{
}

TestInstance *UnusedAttachmentTest::createInstance(Context &context) const
{
    return new UnusedAttachmentTestInstance(context, m_testParams);
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
    , m_renderSize(32u, 32u)
    , m_vertices(createQuad())
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

    // Check for renderpass2 extension if used
    if (testParams.renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

    // Create color image
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            DE_NULL,                                                               // const void* pNext;
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
            DE_NULL,                                  // const void* pNext;
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
                DE_NULL,                                // const void* pNext;
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
                DE_NULL,                                  // const void* pNext;
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
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1, &preImageBarrier);
            vk.cmdClearColorImage(*cmdBuffer, *m_unusedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color,
                                  1, &clearRange);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                                  (const VkBufferMemoryBarrier *)DE_NULL, 1, &postImageBarrier);
            endCommandBuffer(vk, *cmdBuffer);

            submitCommandsAndWait(vk, vkDevice, m_context.getUniversalQueue(), cmdBuffer.get());
        }
    }

    // Create input image
    {
        const VkImageCreateInfo inputImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                       // VkStructureType sType;
            DE_NULL,                                                                   // const void* pNext;
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
            DE_NULL,                                    // const void* pNext;
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
            DE_NULL,                                    // const void* pNext;
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
            DE_NULL,                                    // const void* pNext;
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
    if (testParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        m_renderPass = createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1,
                                        SubpassDependency1, RenderPassCreateInfo1>(vk, vkDevice, testParams);
    else
        m_renderPass = createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                        SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, testParams);

    // Create framebuffer
    {
        const VkImageView imageViews[] = {*m_colorAttachmentView, *m_unusedAttachmentView, *m_inputAttachmentView};

        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                   // const void* pNext;
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
            DE_NULL,                                             // const void*                            pNext
            0u,                                                  // VkDescriptorSetLayoutCreateFlags        flags
            0u,                                                  // uint32_t                                bindingCount
            DE_NULL                                              // const VkDescriptorSetLayoutBinding*    pBindings
        };
        m_descriptorSetLayoutSubpass0 = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);

        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t setLayoutCount;
            &m_descriptorSetLayoutSubpass0.get(),          // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            DE_NULL                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayoutSubpass0 = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
    }

    // Create pipeline layout for subpass 1
    {
        const VkDescriptorSetLayoutBinding layoutBinding = {
            0u,                                  // uint32_t binding;
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType descriptorType;
            1u,                                  // uint32_t descriptorCount;
            VK_SHADER_STAGE_FRAGMENT_BIT,        // VkShaderStageFlags stageFlags;
            DE_NULL                              // const VkSampler* pImmutableSamplers;
        };

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutParams = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType
            DE_NULL,                                             // const void*                            pNext
            0u,                                                  // VkDescriptorSetLayoutCreateFlags        flags
            1u,                                                  // uint32_t                                bindingCount
            &layoutBinding                                       // const VkDescriptorSetLayoutBinding*    pBindings
        };
        m_descriptorSetLayoutSubpass1 = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);

        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t setLayoutCount;
            &m_descriptorSetLayoutSubpass1.get(),          // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            DE_NULL                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayoutSubpass1 = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
    }

    // Update descriptor set
    {
        const VkDescriptorPoolSize descriptorPoolSize = {
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType type;
            1u                                   // uint32_t descriptorCount;
        };

        const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType                sType
            DE_NULL,                                           // const void*                    pNext
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags    flags
            1u,                                                // uint32_t                        maxSets
            1u,                                                // uint32_t                        poolSizeCount
            &descriptorPoolSize                                // const VkDescriptorPoolSize*    pPoolSizes
        };

        m_descriptorPool = createDescriptorPool(vk, vkDevice, &descriptorPoolCreateInfo);

        const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                    sType
            DE_NULL,                                        // const void*                        pNext
            *m_descriptorPool,                              // VkDescriptorPool                    descriptorPool
            1u,                                             // uint32_t                            descriptorSetCount
            &m_descriptorSetLayoutSubpass1.get(),           // const VkDescriptorSetLayout*        pSetLayouts
        };

        m_descriptorSetSubpass1 = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

        const VkDescriptorImageInfo inputImageInfo = {
            DE_NULL,                                 // VkSampleri sampler;
            *m_inputAttachmentView,                  // VkImageView imageView;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout imageLayout;
        };

        const VkWriteDescriptorSet descriptorWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
            DE_NULL,                                // const void* pNext;
            *m_descriptorSetSubpass1,               // VkDescriptorSet dstSet;
            0u,                                     // uint32_t dstBinding;
            0u,                                     // uint32_t dstArrayElement;
            1u,                                     // uint32_t descriptorCount;
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,    // VkDescriptorType descriptorType;
            &inputImageInfo,                        // const VkDescriptorImageInfo* pImageInfo;
            DE_NULL,                                // const VkDescriptorBufferInfo* pBufferInfo;
            DE_NULL                                 // const VkBufferView* pTexelBufferView;
        };

        vk.updateDescriptorSets(vkDevice, 1u, &descriptorWrite, 0u, DE_NULL);
    }

    m_vertexShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    m_fragmentShaderModuleSubpass0 =
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag_sb0"), 0);
    m_fragmentShaderModuleSubpass1 =
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag_sb1"), 0);

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
            DE_NULL,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t vertexAttributeDescriptionCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const std::vector<VkViewport> viewports(1, makeViewport(m_renderSize));
        const std::vector<VkRect2D> scissors(1, makeRect2D(m_renderSize));

        {
            m_graphicsPipelineSubpass0 = makeGraphicsPipeline(
                vk,                        // const DeviceInterface&                        vk
                vkDevice,                  // const VkDevice                                device
                *m_pipelineLayoutSubpass0, // const VkPipelineLayout                        pipelineLayout
                *m_vertexShaderModule,     // const VkShaderModule                            vertexShaderModule
                DE_NULL,                   // const VkShaderModule                            tessellationControlModule
                DE_NULL,                   // const VkShaderModule                            tessellationEvalModule
                DE_NULL,                   // const VkShaderModule                            geometryShaderModule
                *m_fragmentShaderModuleSubpass0, // const VkShaderModule                            fragmentShaderModule
                *m_renderPass,                   // const VkRenderPass                            renderPass
                viewports,                       // const std::vector<VkViewport>&                viewports
                scissors,                        // const std::vector<VkRect2D>&                    scissors
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                    topology
                0u,                                  // const uint32_t                                subpass
                0u,                                  // const uint32_t                                patchControlPoints
                &vertexInputStateParams); // const VkPipelineVertexInputStateCreateInfo*    vertexInputStateCreateInfo

            m_graphicsPipelineSubpass1 = makeGraphicsPipeline(
                vk,                        // const DeviceInterface&                        vk
                vkDevice,                  // const VkDevice                                device
                *m_pipelineLayoutSubpass1, // const VkPipelineLayout                        pipelineLayout
                *m_vertexShaderModule,     // const VkShaderModule                            vertexShaderModule
                DE_NULL,                   // const VkShaderModule                            tessellationControlModule
                DE_NULL,                   // const VkShaderModule                            tessellationEvalModule
                DE_NULL,                   // const VkShaderModule                            geometryShaderModule
                *m_fragmentShaderModuleSubpass1, // const VkShaderModule                            fragmentShaderModule
                *m_renderPass,                   // const VkRenderPass                            renderPass
                viewports,                       // const std::vector<VkViewport>&                viewports
                scissors,                        // const std::vector<VkRect2D>&                    scissors
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                    topology
                1u,                                  // const uint32_t                                subpass
                0u,                                  // const uint32_t                                patchControlPoints
                &vertexInputStateParams); // const VkPipelineVertexInputStateCreateInfo*    vertexInputStateCreateInfo
        }
    }

    // Create vertex buffer
    {
        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                    // VkStructureType sType;
            DE_NULL,                                                 // const void* pNext;
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
    if (testParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        createCommandBuffer<RenderpassSubpass1>(vk, vkDevice);
    else
        createCommandBuffer<RenderpassSubpass2>(vk, vkDevice);
}

UnusedAttachmentTestInstance::~UnusedAttachmentTestInstance(void)
{
}

template <typename RenderpassSubpass>
void UnusedAttachmentTestInstance::createCommandBuffer(const DeviceInterface &vk, VkDevice vkDevice)
{
    const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
    const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(DE_NULL);

    const VkClearValue attachmentClearValues[] = {
        makeClearValueColorF32(0.5f, 0.5f, 0.5f, 1.0f), // color
        makeClearValueColorF32(0.5f, 0.5f, 0.5f, 1.0f), // unused
        makeClearValueColorF32(0.5f, 0.2f, 0.1f, 1.0f)  // input
    };

    const VkDeviceSize vertexBufferOffset = 0;

    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    const VkRenderPassBeginInfo renderPassBeginInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType sType;
        DE_NULL,                                  // const void* pNext;
        *m_renderPass,                            // VkRenderPass renderPass;
        *m_framebuffer,                           // VkFramebuffer framebuffer;
        makeRect2D(m_renderSize),                 // VkRect2D renderArea;
        3u,                                       // uint32_t clearValueCount;
        attachmentClearValues                     // const VkClearValue* pClearValues;
    };
    RenderpassSubpass::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfo, &subpassBeginInfo);

    vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineSubpass0);
    vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);
    vk.cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineSubpass1);
    vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutSubpass1, 0, 1,
                             &m_descriptorSetSubpass1.get(), 0, DE_NULL);
    vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);

    RenderpassSubpass::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);
    endCommandBuffer(vk, *m_cmdBuffer);
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

    // Log images
    log << tcu::TestLog::ImageSet("Result", "Result images")
        << tcu::TestLog::Image("Rendered", "Rendered image", resultAccess)
        << tcu::TestLog::Image("Unused", "Unused image", unusedAccess) << tcu::TestLog::EndImageSet;

    // Check the unused image data hasn't changed.
    for (int y = 0; y < unusedAccess.getHeight(); y++)
        for (int x = 0; x < unusedAccess.getWidth(); x++)
        {
            const tcu::Vec4 color    = unusedAccess.getPixel(x, y);
            const tcu::Vec4 refColor = tcu::Vec4(0.1f, 0.2f, 0.3f, 0.4f);
            for (uint32_t cpnt = 0; cpnt < 4; cpnt++)
                if (de::abs(color[cpnt] - refColor[cpnt]) > 0.01f)
                    return tcu::TestStatus::fail("Unused image contents has changed.");
        }

    // Check for rendered result. Just a quick check to see if correct color is found at the center of the quad.
    const tcu::Vec4 resultColor = resultAccess.getPixel(resultAccess.getWidth() / 2, resultAccess.getHeight() / 2);
    const tcu::Vec4 refColor    = tcu::Vec4(0.4f, 0.6f, 0.2f, 1.0f);
    for (uint32_t cpnt = 0; cpnt < 4; cpnt++)
        if (de::abs(resultColor[cpnt] - refColor[cpnt]) > 0.01f)
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

tcu::TestCaseGroup *createRenderPassUnusedAttachmentTests(tcu::TestContext &testCtx, const RenderingType renderingType)
{
    // Unused attachment tests
    de::MovePtr<tcu::TestCaseGroup> unusedAttTests(new tcu::TestCaseGroup(testCtx, "unused_attachment"));

    const VkAttachmentLoadOp loadOps[] = {VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                          VK_ATTACHMENT_LOAD_OP_DONT_CARE};

    const VkAttachmentStoreOp storeOps[] = {VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_DONT_CARE};

    for (uint32_t loadOpIdx = 0; loadOpIdx < DE_LENGTH_OF_ARRAY(loadOps); loadOpIdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> loadOpGroup(
            new tcu::TestCaseGroup(testCtx, (std::string("loadop") + loadOpToString(loadOps[loadOpIdx])).c_str()));

        for (uint32_t storeOpIdx = 0; storeOpIdx < DE_LENGTH_OF_ARRAY(storeOps); storeOpIdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> storeOpGroup(new tcu::TestCaseGroup(
                testCtx, (std::string("storeop") + storeOpToString(storeOps[storeOpIdx])).c_str()));

            for (uint32_t stencilLoadOpIdx = 0; stencilLoadOpIdx < DE_LENGTH_OF_ARRAY(loadOps); stencilLoadOpIdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> stencilLoadOpGroup(new tcu::TestCaseGroup(
                    testCtx, (std::string("stencilloadop") + loadOpToString(loadOps[stencilLoadOpIdx])).c_str()));

                for (uint32_t stencilStoreOpIdx = 0; stencilStoreOpIdx < DE_LENGTH_OF_ARRAY(storeOps);
                     stencilStoreOpIdx++)
                {
                    TestParams params;
                    const std::string testName =
                        std::string("stencilstoreop") + storeOpToString(storeOps[stencilStoreOpIdx]);

                    params.loadOp         = loadOps[loadOpIdx];
                    params.storeOp        = storeOps[storeOpIdx];
                    params.stencilLoadOp  = loadOps[stencilLoadOpIdx];
                    params.stencilStoreOp = storeOps[stencilStoreOpIdx];
                    params.renderingType  = renderingType;

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
