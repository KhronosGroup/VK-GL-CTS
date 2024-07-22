/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \brief Binding shader access tests
 *//*--------------------------------------------------------------------*/

#include "vktBindingStagesTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

namespace vkt
{
namespace BindingModel
{

namespace
{

vk::VkImageCreateInfo makeImageCreateInfo(const vk::VkFormat format, const tcu::IVec2 &size,
                                          vk::VkImageUsageFlags usage)
{
    const vk::VkImageCreateInfo imageParams = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                 // const void* pNext;
        (vk::VkImageCreateFlags)0,               // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                                  // VkFormat format;
        vk::makeExtent3D(size.x(), size.y(), 1), // VkExtent3D extent;
        1u,                                      // uint32_t mipLevels;
        1u,                                      // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                                   // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                      // uint32_t queueFamilyIndexCount;
        DE_NULL,                                 // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    return imageParams;
}

class StagesTestInstance : public TestInstance
{
public:
    StagesTestInstance(Context &context, vk::VkDescriptorType descriptorType)
        : vkt::TestInstance(context)
        , m_descriptorType(descriptorType)
    {
    }
    ~StagesTestInstance(void)
    {
    }
    tcu::TestStatus iterate(void);

private:
    const vk::VkDescriptorType m_descriptorType;
};

tcu::TestStatus StagesTestInstance::iterate(void)
{
    const auto &vk           = m_context.getDeviceInterface();
    const auto device        = m_context.getDevice();
    vk::Allocator &allocator = m_context.getDefaultAllocator();
    const auto queueIndex    = m_context.getUniversalQueueFamilyIndex();
    const auto queue         = m_context.getUniversalQueue();

    const auto cmdPool   = vk::makeCommandPool(vk, device, queueIndex);
    const auto cmdBuffer = allocateCommandBuffer(vk, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const auto readDescriptorPool(vk::DescriptorPoolBuilder()
                                      .addType(m_descriptorType)
                                      .build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const auto writeDescriptorPool(vk::DescriptorPoolBuilder()
                                       .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                                       .build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const auto readDescriptorSetLayout(
        vk::DescriptorSetLayoutBuilder()
            .addSingleBinding(m_descriptorType, vk::VK_SHADER_STAGE_FRAGMENT_BIT | vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));
    const auto writeDescriptorSetLayout(
        vk::DescriptorSetLayoutBuilder()
            .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              vk::VK_SHADER_STAGE_FRAGMENT_BIT | vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));
    const auto pipelineLayout =
        vk::makePipelineLayout(vk, device, {*readDescriptorSetLayout, *writeDescriptorSetLayout});
    const auto readDescriptorSet(makeDescriptorSet(vk, device, *readDescriptorPool, *readDescriptorSetLayout));
    const auto writeDescriptorSet(makeDescriptorSet(vk, device, *writeDescriptorPool, *writeDescriptorSetLayout));

    const auto subresourceRange = vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    de::MovePtr<vk::BufferWithMemory> readBuffer;
    de::MovePtr<vk::ImageWithMemory> readImage;
    vk::Move<vk::VkImageView> readImageView;

    const vk::VkSamplerCreateInfo samplerCreateInfo = {
        vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        DE_NULL,                                     // const void* pNext;
        0u,                                          // VkSamplerCreateFlags flags;
        vk::VK_FILTER_LINEAR,                        // VkFilter magFilter;
        vk::VK_FILTER_LINEAR,                        // VkFilter minFilter;
        vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU;
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV;
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
        0.0f,                                        // float mipLodBias;
        VK_FALSE,                                    // VkBool32 anisotropyEnable;
        1.0f,                                        // float maxAnisotropy;
        VK_FALSE,                                    // VkBool32 compareEnable;
        vk::VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
        0.0f,                                        // float minLod;
        1.0f,                                        // float maxLod;
        vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        VK_FALSE                                     // VkBool32 unnormalizedCoordinates;
    };

    const auto sampler = vk::createSampler(vk, device, &samplerCreateInfo);

    if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        m_descriptorType == vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        vk::VkBufferUsageFlags usage = m_descriptorType == vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ?
                                           vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT :
                                           vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        vk::VkBufferCreateInfo readBufferCreateInfo = {
            vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                  // const void* pNext;
            (vk::VkBufferCreateFlags)0u,              // VkBufferCreateFlags flags;
            sizeof(float) * 4u,                       // VkDeviceSize size;
            usage,                                    // VkBufferUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                       // uint32_t queueFamilyIndexCount;
            DE_NULL,                                  // const uint32_t* pQueueFamilyIndices;
        };
        readBuffer = de::MovePtr<vk::BufferWithMemory>(
            new vk::BufferWithMemory(vk, device, allocator, readBufferCreateInfo, vk::MemoryRequirement::HostVisible));

        vk::VkDescriptorBufferInfo readBufferInfo = {
            **readBuffer,  // VkBuffer buffer;
            0u,            // VkDeviceSize offset;
            VK_WHOLE_SIZE, // VkDeviceSize range;
        };

        vk::VkWriteDescriptorSet readDescriptorWrite = {
            vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
            DE_NULL,                                    // const void* pNext;
            *readDescriptorSet,                         // VkDescriptorSet dstSet;
            0u,                                         // uint32_t dstBinding;
            0u,                                         // uint32_t dstArrayElement;
            1u,                                         // uint32_t descriptorCount;
            m_descriptorType,                           // VkDescriptorType descriptorType;
            DE_NULL,                                    // const VkDescriptorImageInfo* pImageInfo;
            &readBufferInfo,                            // const VkDescriptorBufferInfo* pBufferInfo;
            DE_NULL,                                    // const VkBufferView* pTexelBufferView;
        };
        vk.updateDescriptorSets(device, 1u, &readDescriptorWrite, 0u, DE_NULL);

        const auto &readAlloc  = readBuffer->getAllocation();
        const auto readDataPtr = reinterpret_cast<float *>(readAlloc.getHostPtr()) + readAlloc.getOffset();
        for (uint32_t i = 0; i < 4; ++i)
            readDataPtr[i] = float(i) + 1.0f;
        vk::flushAlloc(vk, device, readAlloc);
    }
    else
    {
        vk::VkImageCreateInfo imageCreateInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                              // VkStructureType sType;
            DE_NULL,                                                              // const void* pNext;
            (vk::VkImageCreateFlags)0u,                                           // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                                                 // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,                                         // VkFormat format;
            {4u, 4u, 1u},                                                         // VkExtent3D extent;
            1u,                                                                   // uint32_t mipLevels;
            1u,                                                                   // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                                            // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                                          // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_SAMPLED_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                        // VkSharingMode sharingMode;
            0u,                                                                   // uint32_t queueFamilyIndexCount;
            DE_NULL,                       // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
        };
        readImage = de::MovePtr<vk::ImageWithMemory>(
            new vk::ImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any));

        vk::VkImageViewCreateInfo imageViewCreateInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                      // const void* pNext;
            (vk::VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
            **readImage,                                  // VkImage image;
            vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat format;
            {
                vk::VK_COMPONENT_SWIZZLE_R, // VkComponentSwizzle r;
                vk::VK_COMPONENT_SWIZZLE_G, // VkComponentSwizzle g;
                vk::VK_COMPONENT_SWIZZLE_B, // VkComponentSwizzle b;
                vk::VK_COMPONENT_SWIZZLE_A  // VkComponentSwizzle a;
            },                              // VkComponentMapping  components;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                            // uint32_t baseMipLevel;
                1u,                            // uint32_t levelCount;
                0u,                            // uint32_t baseArrayLayer;
                1u                             // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        readImageView = vk::createImageView(vk, device, &imageViewCreateInfo);

        vk::VkDescriptorImageInfo readImageInfo = {
            *sampler,                                     // VkSampler sampler;
            *readImageView,                               // VkImageView imageView;
            vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // VkImageLayout imageLayout;
        };

        vk::VkWriteDescriptorSet readDescriptorWrite = {
            vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
            DE_NULL,                                    // const void* pNext;
            *readDescriptorSet,                         // VkDescriptorSet dstSet;
            0u,                                         // uint32_t dstBinding;
            0u,                                         // uint32_t dstArrayElement;
            1u,                                         // uint32_t descriptorCount;
            m_descriptorType,                           // VkDescriptorType descriptorType;
            &readImageInfo,                             // const VkDescriptorImageInfo* pImageInfo;
            DE_NULL,                                    // const VkDescriptorBufferInfo* pBufferInfo;
            DE_NULL,                                    // const VkBufferView* pTexelBufferView;
        };
        vk.updateDescriptorSets(device, 1u, &readDescriptorWrite, 0u, DE_NULL);

        vk::VkDeviceSize bufferSize = 4u * 4u * 4u;

        vk::VkBufferCreateInfo readBufferCreateInfo = {
            vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                  // const void* pNext;
            (vk::VkBufferCreateFlags)0u,              // VkBufferCreateFlags flags;
            bufferSize,                               // VkDeviceSize size;
            vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                       // uint32_t queueFamilyIndexCount;
            DE_NULL,                                  // const uint32_t* pQueueFamilyIndices;
        };
        readBuffer = de::MovePtr<vk::BufferWithMemory>(
            new vk::BufferWithMemory(vk, device, allocator, readBufferCreateInfo, vk::MemoryRequirement::HostVisible));

        const auto &readAlloc  = readBuffer->getAllocation();
        const auto readDataPtr = reinterpret_cast<uint8_t *>(readAlloc.getHostPtr()) + readAlloc.getOffset();
        for (uint32_t i = 0; i < 4 * 4 * 4; ++i)
            readDataPtr[i] = (uint8_t)(((i + 1) % 4) * 64 - 1);
        vk::flushAlloc(vk, device, readAlloc);

        const auto copyCmdBuffer =
            allocateCommandBuffer(vk, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        vk::beginCommandBuffer(vk, *copyCmdBuffer);
        vk::VkBufferImageCopy region = {
            0u,                                          // VkDeviceSize bufferOffset;
            0u,                                          // uint32_t bufferRowLength;
            0u,                                          // uint32_t bufferImageHeight;
            {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u}, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                                   // VkOffset3D imageOffset;
            {4u, 4u, 1u},                                // VkExtent3D imageExtent;
        };
        vk::VkImageMemoryBarrier preImageMemoryBarrier = vk::makeImageMemoryBarrier(
            vk::VK_ACCESS_NONE, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED,
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **readImage, subresourceRange);
        vk::VkImageMemoryBarrier postImageMemoryBarrier = vk::makeImageMemoryBarrier(
            vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, **readImage, subresourceRange);
        vk.cmdPipelineBarrier(*copyCmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preImageMemoryBarrier);
        vk.cmdCopyBufferToImage(*copyCmdBuffer, **readBuffer, **readImage, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                                &region);
        vk.cmdPipelineBarrier(*copyCmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                              vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u,
                              0u, DE_NULL, 0u, DE_NULL, 1u, &postImageMemoryBarrier);
        vk::endCommandBuffer(vk, *copyCmdBuffer);
        vk::submitCommandsAndWait(vk, device, queue, *copyCmdBuffer);
    }

    vk::VkBufferCreateInfo writeBufferCreateInfo = {
        vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                  // const void* pNext;
        (vk::VkBufferCreateFlags)0u,              // VkBufferCreateFlags flags;
        sizeof(float) * 4u,                       // VkDeviceSize size;
        vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,   // VkBufferUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        0u,                                       // uint32_t queueFamilyIndexCount;
        DE_NULL,                                  // const uint32_t* pQueueFamilyIndices;
    };
    const auto writeBuffer = de::MovePtr<vk::BufferWithMemory>(
        new vk::BufferWithMemory(vk, device, allocator, writeBufferCreateInfo, vk::MemoryRequirement::HostVisible));

    vk::VkDescriptorBufferInfo writeBufferInfo = {
        **writeBuffer, // VkBuffer buffer;
        0u,            // VkDeviceSize offset;
        VK_WHOLE_SIZE, // VkDeviceSize range;
    };
    ;

    vk::VkWriteDescriptorSet writeDescriptorWrite = {
        vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
        DE_NULL,                                    // const void* pNext;
        *writeDescriptorSet,                        // VkDescriptorSet dstSet;
        0u,                                         // uint32_t dstBinding;
        0u,                                         // uint32_t dstArrayElement;
        1u,                                         // uint32_t descriptorCount;
        vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      // VkDescriptorType descriptorType;
        DE_NULL,                                    // const VkDescriptorImageInfo* pImageInfo;
        &writeBufferInfo,                           // const VkDescriptorBufferInfo* pBufferInfo;
        DE_NULL,                                    // const VkBufferView* pTexelBufferView;
    };
    vk.updateDescriptorSets(device, 1u, &writeDescriptorWrite, 0u, DE_NULL);

    const auto renderSize            = tcu::IVec2(32, 32);
    const auto colorFormat           = vk::VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorSubresourceRange = makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorImage(
        makeImage(vk, device,
                  makeImageCreateInfo(colorFormat, renderSize,
                                      vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
    const auto colorImageAlloc(bindImage(vk, device, allocator, *colorImage, vk::MemoryRequirement::Any));
    const auto colorImageView(
        makeImageView(vk, device, *colorImage, vk::VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

    const auto renderPass(makeRenderPass(vk, device, colorFormat));
    const auto framebuffer(
        vk::makeFramebuffer(vk, device, *renderPass, *colorImageView, renderSize.x(), renderSize.y()));

    const auto vertexModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
    const auto fragmentModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
    const auto compModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
    std::vector<vk::VkViewport> viewports = {
        vk::VkViewport{0.0f, 0.0f, (float)renderSize.x(), (float)renderSize.y(), 0.0f, 1.0f}};
    std::vector<vk::VkRect2D> scissors = {vk::VkRect2D{{0, 0}, {(uint32_t)renderSize.x(), (uint32_t)renderSize.y()}}};
    vk::VkPipelineVertexInputStateCreateInfo vertexInputState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                       // const void* pNext;
        (vk::VkPipelineVertexInputStateCreateFlags)0u,                 // VkPipelineVertexInputStateCreateFlags flags;
        0u,                                                            // uint32_t vertexBindingDescriptionCount;
        DE_NULL, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        0u,      // uint32_t vertexAttributeDescriptionCount;
        DE_NULL, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };
    const auto pipeline =
        vk::makeGraphicsPipeline(vk, device, *pipelineLayout, *vertexModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                 VK_NULL_HANDLE, *fragmentModule, *renderPass, 0u, &vertexInputState);
    const auto computePipeline = vk::makeComputePipeline(vk, device, *pipelineLayout, *compModule);

    vk::VkDescriptorSet descriptorSets[] = {*readDescriptorSet, *writeDescriptorSet};

    vk::VkBindDescriptorSetsInfoKHR bindDescriptorSetsInfo = {
        vk::VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO_KHR,                // VkStructureType sType;
        DE_NULL,                                                            // const void* pNext;
        vk::VK_SHADER_STAGE_FRAGMENT_BIT | vk::VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags stageFlags;
        *pipelineLayout,                                                    // VkPipelineLayout layout;
        0u,                                                                 // uint32_t firstSet;
        2u,                                                                 // uint32_t descriptorSetCount;
        descriptorSets,                                                     // const VkDescriptorSet* pDescriptorSets;
        0u,                                                                 // uint32_t dynamicOffsetCount;
        DE_NULL,                                                            // const uint32_t* pDynamicOffsets;
    };

    const vk::VkDeviceSize colorOutputBufferSize =
        renderSize.x() * renderSize.y() * tcu::getPixelSize(vk::mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM));
    de::MovePtr<vk::BufferWithMemory> colorOutputBuffer = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
        vk, device, allocator, vk::makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        vk::MemoryRequirement::HostVisible));

    vk::VkClearValue clearValue = vk::makeClearValueColor({0.0f, 0.0f, 0.0f, 0.0f});

    vk::VkRenderPassBeginInfo renderPassBegin = {
        vk::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                   // VkStructureType sType;
        DE_NULL,                                                        // const void* pNext;
        *renderPass,                                                    // VkRenderPass renderPass;
        *framebuffer,                                                   // VkFramebuffer framebuffer;
        {{0, 0}, {(uint32_t)renderSize.x(), (uint32_t)renderSize.y()}}, // VkRect2D renderArea;
        1u,                                                             // uint32_t clearValueCount;
        &clearValue,                                                    // const VkClearValue* pClearValues;
    };

    vk::beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdBindDescriptorSets2(*cmdBuffer, &bindDescriptorSetsInfo);

    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBegin, vk::VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
    vk.cmdEndRenderPass(*cmdBuffer);
    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vk.cmdDispatch(*cmdBuffer, 4, 1, 1);

    vk::VkImageMemoryBarrier imageMemoryBarrier =
        vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT,
                                   vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorImage, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageMemoryBarrier);

    vk::VkBufferImageCopy region = {
        0u,                                                       // VkDeviceSize bufferOffset;
        0u,                                                       // uint32_t bufferRowLength;
        0u,                                                       // uint32_t bufferImageHeight;
        {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},              // VkImageSubresourceLayers imageSubresource;
        {0u, 0u, 0u},                                             // VkOffset3D imageOffset;
        {(uint32_t)renderSize.x(), (uint32_t)renderSize.y(), 1u}, // VkExtent3D imageExtent;
    };
    vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **colorOutputBuffer, 1u,
                            &region);

    vk::endCommandBuffer(vk, *cmdBuffer);

    vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, writeBuffer->getAllocation());
    const auto &writeAlloc  = writeBuffer->getAllocation();
    const auto writeDataPtr = reinterpret_cast<float *>(writeAlloc.getHostPtr()) + writeAlloc.getOffset();

    for (uint32_t i = 0; i < 4; ++i)
    {
        if (std::abs(writeDataPtr[i] - (float(i) + 1.0f)) >= 0.02f)
        {
            return tcu::TestStatus::fail("Fail");
        }
    }

    tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(
        tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), renderSize.x(), renderSize.y(), 1,
        (const void *)colorOutputBuffer.get()->getAllocation().getHostPtr());

    for (int y = 0; y < resultBuffer.getHeight(); y++)
    {
        for (int x = 0; x < resultBuffer.getWidth(); x++)
        {
            const auto pixel = resultBuffer.getPixel(x, y);
            for (int i = 0; i < 4; ++i)
            {
                if (std::abs(pixel[i] - float(i + 1) / 4.0f) >= 0.02f)
                {
                    return tcu::TestStatus::fail("Fail");
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class StagesTestCase : public TestCase
{
public:
    StagesTestCase(tcu::TestContext &context, const char *name, vk::VkDescriptorType descriptorType)
        : vkt::TestCase(context, name)
        , m_descriptorType(descriptorType)
    {
    }
    virtual ~StagesTestCase(void)
    {
    }
    virtual TestInstance *createInstance(Context &context) const
    {
        return new StagesTestInstance(context, m_descriptorType);
    }
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    void checkSupport(Context &context) const;

private:
    const vk::VkDescriptorType m_descriptorType;
};

void StagesTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream comp;
    comp << "#version 450\n"
         << "\n";
    if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
    {
        comp << "layout(set = 0, binding = 0) buffer readBuffer{\n"
             << "    float readValues[];\n"
             << "};\n";
    }
    else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        comp << "layout(set = 0, binding = 0) uniform readBuffer{\n"
             << "    vec4 readValues;\n"
             << "};\n";
    }
    else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    {
        comp << "layout(set = 0, binding = 0) uniform sampler2D readImage;\n";
    }
    comp << "layout(set = 1, binding = 0) buffer writeBuffer{\n"
         << "    float writeValues[];\n"
         << "};\n"
         << "\n"
         << "void main (void) {\n";
    if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        m_descriptorType == vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        comp << "    writeValues[gl_GlobalInvocationID.x] = readValues[gl_GlobalInvocationID.x];\n";
    }
    else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    {
        comp << "    writeValues[gl_GlobalInvocationID.x] = texture(readImage, vec2(0.0f))[gl_GlobalInvocationID.x] * "
                "4.0f;\n";
    }
    comp << "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());

    std::ostringstream vert;
    vert << "#version 450\n"
         << "\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(float(gl_VertexIndex & 1) * 2.0f - 1.0f, float((gl_VertexIndex >> 1) & 1) * 2.0f - "
            "1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 450\n"
         << "\n";
    if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
    {
        frag << "layout(set = 0, binding = 0) buffer readBuffer{\n"
             << "    float readValues[];\n"
             << "};\n";
    }
    else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        frag << "layout(set = 0, binding = 0) uniform readBuffer{\n"
             << "    vec4 readValues;\n"
             << "};\n";
    }
    else if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    {
        frag << "layout(set = 0, binding = 0) uniform sampler2D readImage;\n";
    }
    frag << "layout(location = 0) out vec4 color;\n"
         << "void main (void) {\n";
    if (m_descriptorType == vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        m_descriptorType == vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        frag << "    color = vec4(readValues[0] / 4.0f, readValues[1] / 4.0f, readValues[2] / 4.0f, readValues[3] / "
                "4.0f);\n";
    }
    else
    {
        frag << "    color = texture(readImage, vec2(0.5f));\n";
    }
    frag << "}\n";

    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void StagesTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_maintenance6");
}

} // namespace

tcu::TestCaseGroup *createStagesTests(tcu::TestContext &testCtx)
{

    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
        testCtx, "stages", "Update stages from different pipeline bind points with the same call"));

    constexpr struct DescriptorTypeTest
    {
        vk::VkDescriptorType descriptorType;
        const char *name;
    } descriptorTypeTests[] = {
        {vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "storage_buffer"},
        {
            vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            "uniform_buffer",
        },
        {
            vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            "combined_image_sampler",
        },
    };

    for (const auto &descriptorTypeTest : descriptorTypeTests)
    {
        group->addChild(new StagesTestCase(testCtx, descriptorTypeTest.name, descriptorTypeTest.descriptorType));
    }

    return group.release();
}

} // namespace BindingModel
} // namespace vkt
