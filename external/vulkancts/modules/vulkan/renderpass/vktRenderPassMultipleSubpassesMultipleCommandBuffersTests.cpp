/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Tests vkCmdClearAttachments with unused attachments.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassMultipleSubpassesMultipleCommandBuffersTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include <sstream>
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace vkt
{
namespace renderpass
{

namespace
{

struct Vertex
{
    tcu::Vec4 position;
    tcu::Vec4 color;
};

template <typename T>
inline VkDeviceSize sizeInBytes(const std::vector<T> &vec)
{
    return vec.size() * sizeof(vec[0]);
}

std::vector<Vertex> genVertices(void)
{
    std::vector<Vertex> vectorData;
    const tcu::Vec4 red    = {1.0f, 0.0f, 0.0f, 1.0f};
    const tcu::Vec4 green  = {0.0f, 1.0f, 0.0f, 1.0f};
    const tcu::Vec4 blue   = {0.0f, 0.0f, 1.0f, 1.0f};
    const tcu::Vec4 yellow = {1.0f, 1.0f, 0.0f, 1.0f};

    vectorData.push_back({tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), red});
    vectorData.push_back({tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f), red});
    vectorData.push_back({tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), red});
    vectorData.push_back({tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), red});

    vectorData.push_back({tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f), green});
    vectorData.push_back({tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), green});
    vectorData.push_back({tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), green});
    vectorData.push_back({tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), green});

    vectorData.push_back({tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), blue});
    vectorData.push_back({tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f), blue});
    vectorData.push_back({tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), blue});
    vectorData.push_back({tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), blue});

    vectorData.push_back({tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f), yellow});
    vectorData.push_back({tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), yellow});
    vectorData.push_back({tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), yellow});
    vectorData.push_back({tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), yellow});

    return vectorData;
}

class MultipleSubpassesMultipleCommandBuffersTestInstance : public TestInstance
{
public:
    MultipleSubpassesMultipleCommandBuffersTestInstance(Context &context, const bool useGeneralLayout);
    virtual ~MultipleSubpassesMultipleCommandBuffersTestInstance(void)
    {
    }
    virtual tcu::TestStatus iterate(void);
    void createCommandBuffer(const DeviceInterface &vk, VkDevice vkDevice);

private:
    const bool m_useGeneralLayout;

    static constexpr uint32_t kImageWidth  = 32;
    static constexpr uint32_t kImageHeight = 32;
    const tcu::UVec2 m_renderSize          = {kImageWidth, kImageHeight};

    VkClearValue m_initialColor;
    VkClearValue m_clearColor;

    Move<VkImage> m_colorImageA;
    de::MovePtr<Allocation> m_colorImageAllocA;
    Move<VkImageView> m_colorAttachmentViewA;

    Move<VkImage> m_colorImageB;
    de::MovePtr<Allocation> m_colorImageAllocB;
    Move<VkImageView> m_colorAttachmentViewB;

    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebufferA;
    Move<VkFramebuffer> m_framebufferB;
    Move<VkShaderModule> m_vertexShaderModule;
    Move<VkShaderModule> m_fragmentShaderModule;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkPipeline> m_graphicsPipeline0;
    Move<VkPipeline> m_graphicsPipeline1;
    Move<VkPipeline> m_graphicsPipeline2;
    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBufferA;
    Move<VkCommandBuffer> m_cmdBufferB;

    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferAlloc;
};

class MultipleSubpassesMultipleCommandBuffersTest : public vkt::TestCase
{
public:
    MultipleSubpassesMultipleCommandBuffersTest(tcu::TestContext &testContext, const std::string &name,
                                                const bool useGeneralLayout)
        : vkt::TestCase(testContext, name)
        , m_useGeneralLayout(useGeneralLayout)
    {
    }
    virtual ~MultipleSubpassesMultipleCommandBuffersTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    const bool m_useGeneralLayout;
};

TestInstance *MultipleSubpassesMultipleCommandBuffersTest::createInstance(Context &context) const
{
    return new MultipleSubpassesMultipleCommandBuffersTestInstance(context, m_useGeneralLayout);
}

void MultipleSubpassesMultipleCommandBuffersTest::initPrograms(SourceCollections &sourceCollections) const
{
    // Vertex shader.
    sourceCollections.glslSources.add("vert_shader") << glu::VertexSource("#version 450\n"
                                                                          "layout(location = 0) in vec4 position;\n"
                                                                          "layout(location = 1) in vec4 color;\n"
                                                                          "layout(location = 0) out vec4 vtxColor;\n"
                                                                          "void main (void)\n"
                                                                          "{\n"
                                                                          "\tgl_Position = position;\n"
                                                                          "\tvtxColor = color;\n"
                                                                          "}\n");

    // Fragment shader.
    std::ostringstream fragmentSource;

    fragmentSource << "#version 450\n"
                   << "layout(location = 0) in vec4 vtxColor;\n"
                   << "layout(location = 0) out vec4 fragColor;\n"
                   << "void main (void)\n"
                   << "{\n"
                   << "\tfragColor = vtxColor;\n"
                   << "}\n";

    sourceCollections.glslSources.add("frag_shader") << glu::FragmentSource(fragmentSource.str());
}

// Create a render pass for this use case.
Move<VkRenderPass> createRenderPass(const DeviceInterface &vk, VkDevice vkDevice, const bool useGeneralLayout)
{
    const VkImageLayout attachmentLayout =
        useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Create attachment descriptions.
    const VkAttachmentDescription attachmentDescription = {
        (VkAttachmentDescriptionFlags)0,  // VkAttachmentDescriptionFlags        flags
        VK_FORMAT_R32G32B32A32_SFLOAT,    // VkFormat                            format
        VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits            samples
        VK_ATTACHMENT_LOAD_OP_LOAD,       // VkAttachmentLoadOp                loadOp
        VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp                storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp                stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp                stencilStoreOp
        attachmentLayout,                 // VkImageLayout                    initialLayout
        attachmentLayout                  // VkImageLayout                    finalLayout
    };

    // Mark attachments as used or not depending on the test parameters.
    const VkAttachmentReference attachmentReference{
        0u,               // uint32_t                attachment
        attachmentLayout, // VkImageLayout        layout
    };

    // Create subpass description with the previous color attachment references.
    std::vector<vk::VkSubpassDescription> subpassDescriptions;
    {
        const vk::VkSubpassDescription subpassDescription = {
            (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags        flags
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint                pipelineBindPoint
            0u,                              // uint32_t                            inputAttachmentCount
            nullptr,                         // const VkAttachmentReference*        pInputAttachments
            1u,                              // uint32_t                            colorAttachmentCount
            &attachmentReference,            // const VkAttachmentReference*        pColorAttachments
            nullptr,                         // const VkAttachmentReference*        pResolveAttachments
            nullptr,                         // const VkAttachmentReference*        pDepthStencilAttachment
            0u,                              // uint32_t                            preserveAttachmentCount
            nullptr                          // const uint32_t*                    pPreserveAttachments
        };
        subpassDescriptions.emplace_back(subpassDescription);
        subpassDescriptions.emplace_back(subpassDescription);
        subpassDescriptions.emplace_back(subpassDescription);
    }

    std::vector<vk::VkSubpassDependency> subpassDependencies;
    {
        vk::VkSubpassDependency subpassDependency = {
            0u,                                            // uint32_t                srcSubpass
            1u,                                            // uint32_t                    dstSubpass
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags        srcStageMask
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags        dstStageMask
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags            srcAccessMask
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags            dstAccessMask
            0u                                        // VkDependencyFlags        dependencyFlags
        };
        subpassDependencies.emplace_back(subpassDependency);
        subpassDependency.srcSubpass = 1u;
        subpassDependency.dstSubpass = 2u;
        subpassDependencies.emplace_back(subpassDependency);
    }

    const vk::VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,         // VkStructureType                    sType
        nullptr,                                           // const void*                        pNext
        (VkRenderPassCreateFlags)0,                        // VkRenderPassCreateFlags            flags
        1u,                                                // uint32_t                            attachmentCount
        &attachmentDescription,                            // const VkAttachmentDescription*    pAttachments
        static_cast<uint32_t>(subpassDescriptions.size()), // uint32_t                            subpassCount
        subpassDescriptions.data(),                        // const VkSubpassDescription*        pSubpasses
        static_cast<uint32_t>(subpassDependencies.size()), // uint32_t                            dependencyCount
        subpassDependencies.data(),                        // const VkSubpassDependency*        pDependencies
    };

    return createRenderPass(vk, vkDevice, &renderPassInfo);
}

MultipleSubpassesMultipleCommandBuffersTestInstance::MultipleSubpassesMultipleCommandBuffersTestInstance(
    Context &context, const bool useGeneralLayout)
    : vkt::TestInstance(context)
    , m_useGeneralLayout(useGeneralLayout)
{
    // Initial color for all images.
    m_initialColor.color.float32[0] = 0.0f;
    m_initialColor.color.float32[1] = 0.0f;
    m_initialColor.color.float32[2] = 0.0f;
    m_initialColor.color.float32[3] = 1.0f;

    // Clear color for used attachments.
    m_clearColor.color.float32[0] = 1.0f;
    m_clearColor.color.float32[1] = 1.0f;
    m_clearColor.color.float32[2] = 1.0f;
    m_clearColor.color.float32[3] = 1.0f;

    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
    const VkComponentMapping componentMapping = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                 VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

    // Create color images.
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            0u,                                  // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            VK_FORMAT_R32G32B32A32_SFLOAT,       // VkFormat format;
            {kImageWidth, kImageHeight, 1u},     // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            1u,                                  // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
            1u,                                  // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,                   // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
        };
        // Create, allocate and bind image memory.
        m_colorImageA = createImage(vk, vkDevice, &colorImageParams);
        m_colorImageAllocA =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImageA), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImageA, m_colorImageAllocA->getMemory(),
                                    m_colorImageAllocA->getOffset()));

        m_colorImageB = createImage(vk, vkDevice, &colorImageParams);
        m_colorImageAllocB =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImageB), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImageB, m_colorImageAllocB->getMemory(),
                                    m_colorImageAllocB->getOffset()));

        // Create image view.
        {
            const VkImageViewCreateInfo colorAttachmentViewParamsA = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                0u,                                         // VkImageViewCreateFlags flags;
                *m_colorImageA,                             // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
                VK_FORMAT_R32G32B32A32_SFLOAT,              // VkFormat format;
                componentMapping,                           // VkChannelMapping channels;
                {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
            };
            m_colorAttachmentViewA = createImageView(vk, vkDevice, &colorAttachmentViewParamsA);

            const VkImageViewCreateInfo colorAttachmentViewParamsB = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                0u,                                         // VkImageViewCreateFlags flags;
                *m_colorImageB,                             // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
                VK_FORMAT_R32G32B32A32_SFLOAT,              // VkFormat format;
                componentMapping,                           // VkChannelMapping channels;
                {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
            };
            m_colorAttachmentViewB = createImageView(vk, vkDevice, &colorAttachmentViewParamsB);
        }

        // Clear image and leave it prepared to be used as a color attachment.
        {
            const VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Move<VkCommandPool> cmdPool;
            Move<VkCommandBuffer> cmdBuffer;
            std::vector<VkImageMemoryBarrier> preImageBarriers;
            std::vector<VkMemoryBarrier> postMemoryBarriers;
            std::vector<VkImageMemoryBarrier> postImageBarriers;

            // Create command pool and buffer
            cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
            cmdBuffer = allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            const VkImageLayout layout =
                m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            // From undefined layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
            const VkImageMemoryBarrier preImageBarrierA = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                0u,                                     // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
                layout,                                 // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                *m_colorImageA,                         // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    aspectMask, // VkImageAspect aspect;
                    0u,         // uint32_t baseMipLevel;
                    1u,         // uint32_t mipLevels;
                    0u,         // uint32_t baseArraySlice;
                    1u          // uint32_t arraySize;
                }};

            preImageBarriers.emplace_back(preImageBarrierA);

            const VkMemoryBarrier postMemoryBarrierA = {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER, // VkStructureType	sType;
                nullptr,                          // const void*		pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,     // VkAccessFlags	srcAccessMask;
                VK_ACCESS_SHADER_READ_BIT         // VkAccessFlags	dstAccessMask;
            };

            // From VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
            const VkImageMemoryBarrier postImageBarrierA = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType sType;
                nullptr,                                  // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,             // VkAccessFlags srcAccessMask;
                VK_ACCESS_SHADER_READ_BIT,                // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,     // VkImageLayout oldLayout;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                  // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                  // uint32_t dstQueueFamilyIndex;
                *m_colorImageA,                           // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    aspectMask, // VkImageAspect aspect;
                    0u,         // uint32_t baseMipLevel;
                    1u,         // uint32_t mipLevels;
                    0u,         // uint32_t baseArraySlice;
                    1u          // uint32_t arraySize;
                }};

            if (m_useGeneralLayout)
                postMemoryBarriers.push_back(postMemoryBarrierA);
            else
                postImageBarriers.emplace_back(postImageBarrierA);

            // From undefined layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
            const VkImageMemoryBarrier preImageBarrierB = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                0u,                                     // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
                layout,                                 // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                *m_colorImageB,                         // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    aspectMask, // VkImageAspect aspect;
                    0u,         // uint32_t baseMipLevel;
                    1u,         // uint32_t mipLevels;
                    0u,         // uint32_t baseArraySlice;
                    1u          // uint32_t arraySize;
                }};

            preImageBarriers.emplace_back(preImageBarrierB);

            const VkMemoryBarrier postMemoryBarrierB = {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER, // VkStructureType	sType;
                nullptr,                          // const void*		pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,     // VkAccessFlags	srcAccessMask;
                VK_ACCESS_SHADER_READ_BIT         // VkAccessFlags	dstAccessMask;
            };

            // From VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
            const VkImageMemoryBarrier postImageBarrierB = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType sType;
                nullptr,                                  // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,             // VkAccessFlags srcAccessMask;
                VK_ACCESS_SHADER_READ_BIT,                // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,     // VkImageLayout oldLayout;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                  // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                  // uint32_t dstQueueFamilyIndex;
                *m_colorImageB,                           // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    aspectMask, // VkImageAspect aspect;
                    0u,         // uint32_t baseMipLevel;
                    1u,         // uint32_t mipLevels;
                    0u,         // uint32_t baseArraySlice;
                    1u          // uint32_t arraySize;
                }};

            if (m_useGeneralLayout)
                postMemoryBarriers.push_back(postMemoryBarrierB);
            else
                postImageBarriers.emplace_back(postImageBarrierB);

            const VkImageSubresourceRange clearRange = {
                aspectMask, // VkImageAspectFlags aspectMask;
                0u,         // uint32_t baseMipLevel;
                1u,         // uint32_t levelCount;
                0u,         // uint32_t baseArrayLayer;
                1u          // uint32_t layerCount;
            };

            // Clear image and transfer layout.
            beginCommandBuffer(vk, *cmdBuffer);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                  static_cast<uint32_t>(preImageBarriers.size()), preImageBarriers.data());
            vk.cmdClearColorImage(*cmdBuffer, *m_colorImageA, layout, &m_initialColor.color, 1, &clearRange);
            vk.cmdClearColorImage(*cmdBuffer, *m_colorImageB, layout, &m_initialColor.color, 1, &clearRange);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                  static_cast<uint32_t>(postImageBarriers.size()), postImageBarriers.data());
            endCommandBuffer(vk, *cmdBuffer);

            submitCommandsAndWait(vk, vkDevice, m_context.getUniversalQueue(), cmdBuffer.get());
        }
    }

    // Create render pass.
    m_renderPass = createRenderPass(vk, vkDevice, m_useGeneralLayout);

    // Create framebuffer
    {
        const VkImageView attachmentBindInfosA[1] = {
            *m_colorAttachmentViewA,
        };
        const VkFramebufferCreateInfo framebufferParamsA = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            1u,                                        // uint32_t attachmentCount;
            attachmentBindInfosA,                      // const VkImageView* pAttachments;
            kImageWidth,                               // uint32_t width;
            kImageHeight,                              // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_framebufferA = createFramebuffer(vk, vkDevice, &framebufferParamsA);

        const VkImageView attachmentBindInfosB[1] = {
            *m_colorAttachmentViewB,
        };
        const VkFramebufferCreateInfo framebufferParamsB = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            1u,                                        // uint32_t attachmentCount;
            attachmentBindInfosB,                      // const VkImageView* pAttachments;
            kImageWidth,                               // uint32_t width;
            kImageHeight,                              // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_framebufferB = createFramebuffer(vk, vkDevice, &framebufferParamsB);
    }

    // Create pipeline layout.
    {
        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutParams = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType
            nullptr,                                             // const void*                            pNext
            0u,                                                  // VkDescriptorSetLayoutCreateFlags        flags
            0u,                                                  // uint32_t                                bindingCount
            nullptr                                              // const VkDescriptorSetLayoutBinding*    pBindings
        };
        m_descriptorSetLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);

        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t setLayoutCount;
            &m_descriptorSetLayout.get(),                  // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
    }

    // Create Vertex buffer
    {
        const std::vector<Vertex> vertexValues = genVertices();
        const VkDeviceSize vertexBufferSize    = sizeInBytes(vertexValues);

        const vk::VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                 // VkStructureType        sType
            nullptr,                                                              // const void*            pNext
            0u,                                                                   // VkBufferCreateFlags    flags
            vertexBufferSize,                                                     // VkDeviceSize            size
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags    usage
            VK_SHARING_MODE_EXCLUSIVE,                                            // VkSharingMode        sharingMode
            1u,               // uint32_t                queueFamilyIndexCount
            &queueFamilyIndex // const uint32_t*        pQueueFamilyIndices
        };

        m_vertexBuffer      = createBuffer(vk, vkDevice, &bufferCreateInfo);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));
        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), vertexValues.data(), static_cast<size_t>(vertexBufferSize));
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Vertex buffer description
    const vk::VkVertexInputBindingDescription bindingDescription = {
        0u,                         // uint32_t                binding
        sizeof(Vertex),             // uint32_t                stride
        VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate    inputRate
    };

    std::vector<vk::VkVertexInputAttributeDescription> attributeDescriptions;
    {
        vk::VkVertexInputAttributeDescription attributeDescriptionVertex = {
            0u,                            // uint32_t        location
            0u,                            // uint32_t        binding
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat        format
            offsetof(Vertex, position)     // uint32_t        offset
        };

        vk::VkVertexInputAttributeDescription attributeDescriptionColor = {
            1u,                            // uint32_t        location
            0u,                            // uint32_t        binding
            VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat        format
            offsetof(Vertex, color)        // uint32_t        offset
        };
        attributeDescriptions.emplace_back(attributeDescriptionVertex);
        attributeDescriptions.emplace_back(attributeDescriptionColor);
    }

    const vk::VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                   // const void*                                pNext
        0u,                                                        // VkPipelineVertexInputStateCreateFlags    flags
        1u,                  // uint32_t                                    vertexBindingDescriptionCount
        &bindingDescription, // const VkVertexInputBindingDescription*    pVertexBindingDescriptions
        static_cast<uint32_t>(
            attributeDescriptions
                .size()),             // uint32_t                                    vertexAttributeDescriptionCount
        attributeDescriptions.data(), // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    m_vertexShaderModule   = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert_shader"), 0);
    m_fragmentShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag_shader"), 0);

    // Create pipeline.
    {
        const std::vector<VkViewport> viewports(1, makeViewport(m_renderSize));
        const std::vector<VkRect2D> scissors(1, makeRect2D(m_renderSize));

        const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
            VK_FALSE,                // VkBool32                    blendEnable
            VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcColorBlendFactor
            VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstColorBlendFactor
            VK_BLEND_OP_ADD,         // VkBlendOp                colorBlendOp
            VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcAlphaBlendFactor
            VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstAlphaBlendFactor
            VK_BLEND_OP_ADD,         // VkBlendOp                alphaBlendOp
            VK_COLOR_COMPONENT_R_BIT // VkColorComponentFlags    colorWriteMask
                | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

        const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr,                    // const void*                                    pNext
            0u,                         // VkPipelineColorBlendStateCreateFlags            flags
            VK_FALSE,                   // VkBool32                                        logicOpEnable
            VK_LOGIC_OP_CLEAR,          // VkLogicOp                                    logicOp
            1u,                         // uint32_t                                        attachmentCount
            &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState*    pAttachments
            {0.0f, 0.0f, 0.0f, 0.0f}    // float                                        blendConstants[4]
        };

        m_graphicsPipeline0 = makeGraphicsPipeline(
            vk,                      // const DeviceInterface&                            vk
            vkDevice,                // const VkDevice                                    device
            *m_pipelineLayout,       // const VkPipelineLayout                            pipelineLayout
            *m_vertexShaderModule,   // const VkShaderModule                                vertexShaderModule
            VK_NULL_HANDLE,          // const VkShaderModule                                tessellationControlModule
            VK_NULL_HANDLE,          // const VkShaderModule                                tessellationEvalModule
            VK_NULL_HANDLE,          // const VkShaderModule                                geometryShaderModule
            *m_fragmentShaderModule, // const VkShaderModule                                fragmentShaderModule
            *m_renderPass,           // const VkRenderPass                                renderPass
            viewports,               // const std::vector<VkViewport>&                    viewports
            scissors,                // const std::vector<VkRect2D>&                        scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // const VkPrimitiveTopology                        topology
            0u,                                   // const uint32_t                                    subpass
            0u,                // const uint32_t                                    patchControlPoints
            &vertexInputState, // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
            nullptr,           // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
            nullptr,           // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
            nullptr,           // const VkPipelineDepthStencilStateCreateInfo*        depthStencilStateCreateInfo
            &colorBlendStateCreateInfo); // const VkPipelineColorBlendStateCreateInfo*        colorBlendStateCreateInfo

        m_graphicsPipeline1 = makeGraphicsPipeline(
            vk,                      // const DeviceInterface&                            vk
            vkDevice,                // const VkDevice                                    device
            *m_pipelineLayout,       // const VkPipelineLayout                            pipelineLayout
            *m_vertexShaderModule,   // const VkShaderModule                                vertexShaderModule
            VK_NULL_HANDLE,          // const VkShaderModule                                tessellationControlModule
            VK_NULL_HANDLE,          // const VkShaderModule                                tessellationEvalModule
            VK_NULL_HANDLE,          // const VkShaderModule                                geometryShaderModule
            *m_fragmentShaderModule, // const VkShaderModule                                fragmentShaderModule
            *m_renderPass,           // const VkRenderPass                                renderPass
            viewports,               // const std::vector<VkViewport>&                    viewports
            scissors,                // const std::vector<VkRect2D>&                        scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // const VkPrimitiveTopology                        topology
            1u,                                   // const uint32_t                                    subpass
            0u,                // const uint32_t                                    patchControlPoints
            &vertexInputState, // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
            nullptr,           // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
            nullptr,           // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
            nullptr,           // const VkPipelineDepthStencilStateCreateInfo*        depthStencilStateCreateInfo
            &colorBlendStateCreateInfo); // const VkPipelineColorBlendStateCreateInfo*        colorBlendStateCreateInfo

        m_graphicsPipeline2 = makeGraphicsPipeline(
            vk,                      // const DeviceInterface&                            vk
            vkDevice,                // const VkDevice                                    device
            *m_pipelineLayout,       // const VkPipelineLayout                            pipelineLayout
            *m_vertexShaderModule,   // const VkShaderModule                                vertexShaderModule
            VK_NULL_HANDLE,          // const VkShaderModule                                tessellationControlModule
            VK_NULL_HANDLE,          // const VkShaderModule                                tessellationEvalModule
            VK_NULL_HANDLE,          // const VkShaderModule                                geometryShaderModule
            *m_fragmentShaderModule, // const VkShaderModule                                fragmentShaderModule
            *m_renderPass,           // const VkRenderPass                                renderPass
            viewports,               // const std::vector<VkViewport>&                    viewports
            scissors,                // const std::vector<VkRect2D>&                        scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // const VkPrimitiveTopology                        topology
            2u,                                   // const uint32_t                                    subpass
            0u,                // const uint32_t                                    patchControlPoints
            &vertexInputState, // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
            nullptr,           // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
            nullptr,           // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
            nullptr,           // const VkPipelineDepthStencilStateCreateInfo*        depthStencilStateCreateInfo
            &colorBlendStateCreateInfo); // const VkPipelineColorBlendStateCreateInfo*        colorBlendStateCreateInfo
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    createCommandBuffer(vk, vkDevice);
}

void MultipleSubpassesMultipleCommandBuffersTestInstance::createCommandBuffer(const DeviceInterface &vk,
                                                                              VkDevice vkDevice)
{
    const VkRenderPassBeginInfo renderPassBeginInfoA = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        *m_renderPass,                            // VkRenderPass renderPass;
        *m_framebufferA,                          // VkFramebuffer framebuffer;
        makeRect2D(m_renderSize),                 // VkRect2D renderArea;
        0u,                                       // uint32_t clearValueCount;
        nullptr                                   // const VkClearValue* pClearValues;
    };
    const VkRenderPassBeginInfo renderPassBeginInfoB = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        *m_renderPass,                            // VkRenderPass renderPass;
        *m_framebufferB,                          // VkFramebuffer framebuffer;
        makeRect2D(m_renderSize),                 // VkRect2D renderArea;
        0u,                                       // uint32_t clearValueCount;
        nullptr                                   // const VkClearValue* pClearValues;
    };

    m_cmdBufferA = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    m_cmdBufferB = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const VkClearRect clearRect = {
        {
            // VkRect2D rect;
            {
                0,
                0,
            },                          // VkOffset2D offset;
            {kImageWidth, kImageHeight} // VkExtent2D extent;
        },
        0u, // uint32_t baseArrayLayer;
        1u  // uint32_t layerCount;
    };

    const VkClearAttachment clearAttachment = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
        0u,                        // uint32_t colorAttachment;
        m_clearColor               // VkClearValue clearValue;
    };

    VkDeviceSize vertexBufferOffset = 0u;

    // Command Buffer A will set its own event but wait for the B's event before continuing to the next subpass.
    beginCommandBuffer(vk, *m_cmdBufferA, 0u);
    beginCommandBuffer(vk, *m_cmdBufferB, 0u);
    vk.cmdBeginRenderPass(*m_cmdBufferA, &renderPassBeginInfoA, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdBindPipeline(*m_cmdBufferA, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline0);
    vk.cmdBindVertexBuffers(*m_cmdBufferA, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdClearAttachments(*m_cmdBufferA, 1u, &clearAttachment, 1u, &clearRect);

    vk.cmdBeginRenderPass(*m_cmdBufferB, &renderPassBeginInfoB, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdBindPipeline(*m_cmdBufferB, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline0);
    vk.cmdClearAttachments(*m_cmdBufferB, 1u, &clearAttachment, 1u, &clearRect);
    vk.cmdNextSubpass(*m_cmdBufferB, VK_SUBPASS_CONTENTS_INLINE);

    vk.cmdNextSubpass(*m_cmdBufferA, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdBindPipeline(*m_cmdBufferA, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline1);
    vk.cmdBindVertexBuffers(*m_cmdBufferA, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(*m_cmdBufferA, 4u, 1u, 0u, 0u);

    vertexBufferOffset = 8 * sizeof(Vertex);
    vk.cmdBindPipeline(*m_cmdBufferB, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline1);
    vk.cmdBindVertexBuffers(*m_cmdBufferB, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(*m_cmdBufferB, 4u, 1u, 0u, 0u);
    vk.cmdNextSubpass(*m_cmdBufferB, VK_SUBPASS_CONTENTS_INLINE);

    vertexBufferOffset = 0u;
    vk.cmdNextSubpass(*m_cmdBufferA, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdBindPipeline(*m_cmdBufferA, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline2);
    vk.cmdBindVertexBuffers(*m_cmdBufferA, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(*m_cmdBufferA, 4u, 1u, 4u, 0u);

    vertexBufferOffset = 8 * sizeof(Vertex);
    vk.cmdBindPipeline(*m_cmdBufferB, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline2);
    vk.cmdDraw(*m_cmdBufferB, 4u, 1u, 4u, 0u);
    vk.cmdEndRenderPass(*m_cmdBufferB);
    vk.cmdEndRenderPass(*m_cmdBufferA);
    endCommandBuffer(vk, *m_cmdBufferA);
    endCommandBuffer(vk, *m_cmdBufferB);
}

tcu::TestStatus MultipleSubpassesMultipleCommandBuffersTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator allocator(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

    {
        const Unique<VkFence> fence(createFence(vk, vkDevice));
        std::vector<VkCommandBuffer> commandBuffers;
        commandBuffers.emplace_back(m_cmdBufferA.get());
        commandBuffers.emplace_back(m_cmdBufferB.get());

        const VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,                // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            0u,                                           // uint32_t waitSemaphoreCount;
            nullptr,                                      // const VkSemaphore* pWaitSemaphores;
            nullptr,                                      // const VkPipelineStageFlags* pWaitDstStageMask;
            static_cast<uint32_t>(commandBuffers.size()), // uint32_t commandBufferCount;
            commandBuffers.data(),                        // const VkCommandBuffer* pCommandBuffers;
            0u,                                           // uint32_t signalSemaphoreCount;
            nullptr,                                      // const VkSemaphore* pSignalSemaphores;
        };

        VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
        VK_CHECK(vk.waitForFences(vkDevice, 1u, &fence.get(), true, ~0ull));
    }

    {
        // Colors to compare to.
        const tcu::Vec4 red    = {1.0f, 0.0f, 0.0f, 1.0f};
        const tcu::Vec4 green  = {0.0f, 1.0f, 0.0f, 1.0f};
        const tcu::Vec4 blue   = {0.0f, 0.0f, 1.0f, 1.0f};
        const tcu::Vec4 yellow = {1.0f, 1.0f, 0.0f, 1.0f};

        const VkImageLayout attachmentLayout =
            m_useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // Read result images.
        de::MovePtr<tcu::TextureLevel> imagePixelsA =
            pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImageA,
                                          VK_FORMAT_R32G32B32A32_SFLOAT, m_renderSize, attachmentLayout);
        de::MovePtr<tcu::TextureLevel> imagePixelsB =
            pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImageB,
                                          VK_FORMAT_R32G32B32A32_SFLOAT, m_renderSize, attachmentLayout);

        // Verify pixel colors match.
        const tcu::ConstPixelBufferAccess &imageAccessA = imagePixelsA->getAccess();
        const tcu::ConstPixelBufferAccess &imageAccessB = imagePixelsB->getAccess();

        tcu::TextureLevel referenceImageA(mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT), m_renderSize.x(),
                                          m_renderSize.y());
        tcu::TextureLevel referenceImageB(mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT), m_renderSize.x(),
                                          m_renderSize.y());

        tcu::clear(tcu::getSubregion(referenceImageA.getAccess(), 0u, 0u, imageAccessA.getWidth() / 2,
                                     imageAccessA.getHeight()),
                   red);
        tcu::clear(tcu::getSubregion(referenceImageA.getAccess(), imageAccessA.getWidth() / 2, 0u,
                                     imageAccessA.getWidth() / 2, imageAccessA.getHeight()),
                   green);

        if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                        referenceImageA.getAccess(), imageAccessA, tcu::Vec4(0.02f),
                                        tcu::COMPARE_LOG_RESULT))
            TCU_FAIL("[A] Rendered image is not correct");

        tcu::clear(tcu::getSubregion(referenceImageB.getAccess(), 0u, 0u, imageAccessB.getWidth() / 2,
                                     imageAccessB.getHeight()),
                   blue);
        tcu::clear(tcu::getSubregion(referenceImageB.getAccess(), imageAccessB.getWidth() / 2, 0u,
                                     imageAccessA.getWidth() / 2, imageAccessB.getHeight()),
                   yellow);

        if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                        referenceImageB.getAccess(), imageAccessB, tcu::Vec4(0.02f),
                                        tcu::COMPARE_LOG_RESULT))
            TCU_FAIL("[B] Rendered image is not correct");
    }

    return tcu::TestStatus::pass("Pass");
}
} // namespace

tcu::TestCaseGroup *createRenderPassMultipleSubpassesMultipleCommandBuffersTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(
        new tcu::TestCaseGroup(testCtx, "multiple_subpasses_multiple_command_buffers"));

    testGroup->addChild(new MultipleSubpassesMultipleCommandBuffersTest(testCtx, "test", false));
    testGroup->addChild(new MultipleSubpassesMultipleCommandBuffersTest(testCtx, "test_general_layout", true));

    return testGroup.release();
}

} // namespace renderpass
} // namespace vkt
