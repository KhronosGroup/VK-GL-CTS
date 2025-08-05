/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \file  vktImageGeneralLayoutTests.cpp
 * \brief Image general layout tests
 *//*--------------------------------------------------------------------*/

#include "vktImageGeneralLayoutTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuAstcUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "deRandom.hpp"

namespace vkt
{

using namespace vk;

namespace image
{

enum AstcTestType
{
    TEST_TYPE_COPY_INTO_IMAGE,
    TEST_TYPE_COPY_FROM_IMAGE,
    TEST_TYPE_HOST_COPY_INTO_IMAGE,
    TEST_TYPE_HOST_COPY_FROM_IMAGE,
    TEST_TYPE_SAMPLE_ALIAS,
    TEST_TYPE_LAST
};

struct AstcTestParameters
{
    enum AstcTestType testType;
};

namespace
{

vk::Move<VkSampler> makeSampler(const DeviceInterface &vk, const VkDevice &device)
{
    const VkSamplerCreateInfo samplerInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_NEAREST,
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        VK_FALSE,
        0.0f,
        VK_FALSE,
        VK_COMPARE_OP_NEVER,
        0.0f,
        1.0f,
        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        VK_FALSE,
    };

    return createSampler(vk, device, &samplerInfo);
}

class AstcSampleTestInstance : public TestInstance
{
public:
    AstcSampleTestInstance(Context &context, const AstcTestParameters &parameters)
        : TestInstance(context)
        , m_parameters(parameters)
    {
    }

    void createRenderPassAndFramebuffer(VkFormat format, VkExtent3D imageExtent, VkImageView imageView);
    void createPipeline(VkExtent3D imageExtent, VkSampler sampler, VkImageView imageView);
    void submit(const std::vector<VkCommandBuffer> &commandBuffers);
    tcu::TestStatus iterate(void);

protected:
    const AstcTestParameters m_parameters;
    vk::Move<VkDescriptorSetLayout> descriptorSetLayout;
    vk::Move<vk::VkDescriptorPool> descriptorPool;
    vk::Move<vk::VkDescriptorSet> descriptorSet;
    vk::PipelineLayoutWrapper pipelineLayout;
    de::MovePtr<GraphicsPipelineWrapper> pipeline;

    vk::Move<vk::VkRenderPass> renderPass;
    vk::Move<vk::VkFramebuffer> framebuffer;
};

void AstcSampleTestInstance::createRenderPassAndFramebuffer(VkFormat format, VkExtent3D imageExtent,
                                                            VkImageView imageView)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const vk::VkAttachmentDescription colorAttachmentDescription = {
        (vk::VkAttachmentDescriptionFlags)0u, // VkAttachmentDescriptionFlags    flags
        format,                               // VkFormat                        format
        vk::VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits           samples
        vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              loadOp
        vk::VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp             storeOp
        vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              stencilLoadOp
        vk::VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp             stencilStoreOp
        vk::VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout                   initialLayout
        vk::VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout                   finalLayout
    };

    const vk::VkAttachmentReference colorAttachmentRef = {
        0u,                         // uint32_t         attachment
        vk::VK_IMAGE_LAYOUT_GENERAL // VkImageLayout    layout
    };

    const vk::VkSubpassDescription subpassDescription = {
        (vk::VkSubpassDescriptionFlags)0u,   // VkSubpassDescriptionFlags       flags
        vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint
        0u,                                  // uint32_t                        inputAttachmentCount
        nullptr,                             // const VkAttachmentReference*    pInputAttachments
        1u,                                  // uint32_t                        colorAttachmentCount
        &colorAttachmentRef,                 // const VkAttachmentReference*    pColorAttachments
        nullptr,                             // const VkAttachmentReference*    pResolveAttachments
        nullptr,                             // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                                  // uint32_t                        preserveAttachmentCount
        nullptr                              // const uint32_t*                 pPreserveAttachments
    };

    const vk::VkRenderPassCreateInfo renderPassInfo = {
        vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkRenderPassCreateFlags flags;
        1u,                                            // uint32_t attachmentCount;
        &colorAttachmentDescription,                   // const VkAttachmentDescription* pAttachments;
        1u,                                            // uint32_t subpassCount;
        &subpassDescription,                           // const VkSubpassDescription* pSubpasses;
        0u,                                            // uint32_t dependencyCount;
        nullptr,                                       // const VkSubpassDependency* pDependencies;
    };

    renderPass  = createRenderPass(vk, device, &renderPassInfo);
    framebuffer = makeFramebuffer(vk, device, *renderPass, imageView, imageExtent.width, imageExtent.height);
}

void AstcSampleTestInstance::createPipeline(VkExtent3D imageExtent, VkSampler sampler, VkImageView imageView)
{
    const InstanceInterface &vki              = m_context.getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const DeviceInterface &vk                 = m_context.getDeviceInterface();
    const VkDevice device                     = m_context.getDevice();
    const auto &deviceExtensions              = m_context.getDeviceExtensions();

    vk::ShaderWrapper vert = vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
    vk::ShaderWrapper frag = vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

    DescriptorSetLayoutBuilder descriptorBuilder;
    descriptorBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptorSetLayout = vk::Move<VkDescriptorSetLayout>(descriptorBuilder.build(vk, device));
    pipelineLayout = vk::PipelineLayoutWrapper(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device, *descriptorSetLayout);
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    descriptorPool = poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    descriptorSet  = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

    vk::VkDescriptorImageInfo descriptorSrcImageInfo(
        makeDescriptorImageInfo(sampler, imageView, vk::VK_IMAGE_LAYOUT_GENERAL));
    vk::DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorSrcImageInfo);
    updateBuilder.update(vk, device);

    pipeline = de::MovePtr<GraphicsPipelineWrapper>(new GraphicsPipelineWrapper(
        vki, vk, physicalDevice, device, deviceExtensions, vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC));

    const vk::VkPipelineVertexInputStateCreateInfo vertexInput = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0u, 0u, nullptr, 0u, nullptr,
    };

    const std::vector<vk::VkViewport> viewports{makeViewport(imageExtent)};
    const std::vector<vk::VkRect2D> scissors{makeRect2D(imageExtent)};

    pipeline->setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultDepthStencilState()
        .setDefaultColorBlendState()
        .setupVertexInputState(&vertexInput)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vert)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, frag)
        .setupFragmentOutputState(*renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();
}

void AstcSampleTestInstance::submit(const std::vector<VkCommandBuffer> &commandBuffers)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkQueue queue       = m_context.getUniversalQueue();

    VkSubmitInfo submitInfo;
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext                = nullptr;
    submitInfo.waitSemaphoreCount   = 0u;
    submitInfo.pWaitSemaphores      = nullptr;
    submitInfo.pWaitDstStageMask    = nullptr;
    submitInfo.commandBufferCount   = (uint32_t)commandBuffers.size();
    submitInfo.pCommandBuffers      = commandBuffers.data();
    submitInfo.signalSemaphoreCount = 0u;
    submitInfo.pSignalSemaphores    = nullptr;
    vk.queueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE);
    vk.queueWaitIdle(queue);
}

tcu::TestStatus AstcSampleTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    auto &alloc                     = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const VkExtent3D imageExtent = makeExtent3D(128u, 128u, 1u);
    const VkFormat sampledFormat = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    const VkFormat outputFormat  = VK_FORMAT_R8G8B8A8_UNORM;

    const VkDeviceSize srcBufferSize    = imageExtent.width * imageExtent.height / 8u / 8u * 16u;
    const VkDeviceSize outputBufferSize = imageExtent.width * imageExtent.height * 4u;
    BufferWithMemory outputBuffer       = BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(outputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible);

    std::vector<uint8_t> generatedData;
    auto compressedFormat = vk::mapVkCompressedFormat(sampledFormat);
    tcu::astc::generateBlockCaseTestData(generatedData, compressedFormat, tcu::astc::BLOCK_TEST_TYPE_VOID_EXTENT_LDR);
    const tcu::IVec3 blockPixelSize                       = tcu::getBlockPixelSize(compressedFormat);
    const tcu::TexDecompressionParams decompressionParams = tcu::TexDecompressionParams::AstcMode::ASTCMODE_LDR;
    const tcu::TextureFormat uncompressedFormat           = getUncompressedFormat(compressedFormat);
    const int numBlocks                                   = 128 * 128 / 8 / 8;
    tcu::TextureLevel texture(uncompressedFormat, blockPixelSize.x() * (int)numBlocks, blockPixelSize.y());
    decompress(texture.getAccess(), compressedFormat, generatedData.data(), decompressionParams);

    std::vector<uint8_t> generatedData2(generatedData.begin() + 128, generatedData.end());
    tcu::TextureLevel texture2(uncompressedFormat, blockPixelSize.x() * (int)numBlocks, blockPixelSize.y());
    decompress(texture2.getAccess(), compressedFormat, generatedData2.data(), decompressionParams);

    BufferWithMemory srcBuffer(vk, device, alloc, makeBufferCreateInfo(srcBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                               vk::MemoryRequirement::HostVisible);
    BufferWithMemory srcBuffer2(vk, device, alloc,
                                makeBufferCreateInfo(srcBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                vk::MemoryRequirement::HostVisible);
    BufferWithMemory srcBufferCopy =
        BufferWithMemory(vk, device, alloc, makeBufferCreateInfo(srcBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                         MemoryRequirement::HostVisible);
    uint8_t *srcData = reinterpret_cast<uint8_t *>(srcBuffer.getAllocation().getHostPtr());
    memcpy(srcData, generatedData.data(), (size_t)srcBufferSize);
    uint8_t *srcData2 = reinterpret_cast<uint8_t *>(srcBuffer2.getAllocation().getHostPtr());
    memcpy(srcData2, generatedData2.data(), (size_t)srcBufferSize);

    const auto subresourceRange = makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const vk::VkImageSubresourceLayers subresourceLayers =
        makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const vk::VkComponentMapping componentMapping = makeComponentMappingRGBA();

    VkImageCreateFlags imageCreateFlags = 0u;
    VkFormat imageFormat                = sampledFormat;
    if (m_parameters.testType == TEST_TYPE_SAMPLE_ALIAS)
    {
        imageCreateFlags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT;
        imageFormat      = VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    }

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        imageCreateFlags,
        VK_IMAGE_TYPE_2D,
        imageFormat,
        imageExtent,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vk::ImageWithMemory sampledImage(vk, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any);

    vk::VkImageViewCreateInfo imageViewCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        (vk::VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        *sampledImage,                                // VkImage image;
        vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        sampledFormat,                                // VkFormat format;
        componentMapping,                             // VkComponentMapping components;
        subresourceRange                              // VkImageSubresourceRange subresourceRange;
    };
    const auto sampledImageView = createImageView(vk, device, &imageViewCreateInfo, nullptr);

    const auto sampler = makeSampler(vk, device);

    const VkImageCreateInfo outputImageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        outputFormat,
        imageExtent,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vk::ImageWithMemory outputImage(vk, device, alloc, outputImageCreateInfo, vk::MemoryRequirement::Any);

    vk::VkImageViewCreateInfo outputImageViewCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        (vk::VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        *outputImage,                                 // VkImage image;
        vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        outputFormat,                                 // VkFormat format;
        componentMapping,                             // VkComponentMapping components;
        subresourceRange                              // VkImageSubresourceRange subresourceRange;
    };
    const auto outputImageView = createImageView(vk, device, &outputImageViewCreateInfo, nullptr);

    createRenderPassAndFramebuffer(outputFormat, imageExtent, *outputImageView);
    createPipeline(imageExtent, *sampler, *sampledImageView);

    const auto commandPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));

    const auto cmdBufferInit(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const auto cmdBufferSample(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const auto cmdBufferOp(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const auto cmdBufferFinish(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBufferInit);

    const VkImageMemoryBarrier initialBarrier =
        makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               *sampledImage, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBufferInit, VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                          nullptr, 1, &initialBarrier);

    const VkBufferImageCopy region =
        vk::makeBufferImageCopy(imageExtent, vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
    vk.cmdCopyBufferToImage(*cmdBufferInit, *srcBuffer, *sampledImage, VK_IMAGE_LAYOUT_GENERAL, 1, &region);

    vk::VkMemoryBarrier memoryBarrier = makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBufferInit, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                          &memoryBarrier, 0, nullptr, 0, nullptr);
    endCommandBuffer(vk, *cmdBufferInit);

    beginCommandBuffer(vk, *cmdBufferSample, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    vk::beginRenderPass(vk, *cmdBufferSample, *renderPass, *framebuffer, makeRect2D(imageExtent));
    vk.cmdBindPipeline(*cmdBufferSample, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
    vk.cmdBindDescriptorSets(*cmdBufferSample, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1,
                             &*descriptorSet, 0, nullptr);
    vk.cmdDraw(*cmdBufferSample, 4u, 1u, 0u, 0u);
    vk::endRenderPass(vk, *cmdBufferSample);
    endCommandBuffer(vk, *cmdBufferSample);

    beginCommandBuffer(vk, *cmdBufferOp);
    if (m_parameters.testType == TEST_TYPE_COPY_INTO_IMAGE)
    {
        const auto preBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBufferOp, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                              1u, &preBarrier, 0u, nullptr, 0u, nullptr);
        vk.cmdCopyBufferToImage(*cmdBufferOp, *srcBuffer2, *sampledImage, VK_IMAGE_LAYOUT_GENERAL, 1u, &region);
        const auto postBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBufferOp, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u,
                              1u, &postBarrier, 0u, nullptr, 0u, nullptr);
    }
    else if (m_parameters.testType == TEST_TYPE_COPY_FROM_IMAGE)
    {
        const auto preBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBufferOp, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                              1u, &preBarrier, 0u, nullptr, 0u, nullptr);
        vk.cmdCopyImageToBuffer(*cmdBufferOp, *sampledImage, vk::VK_IMAGE_LAYOUT_GENERAL, *srcBufferCopy, 1u, &region);
        const auto postBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBufferOp, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u,
                              1u, &postBarrier, 0u, nullptr, 0u, nullptr);
    }
    endCommandBuffer(vk, *cmdBufferOp);

    beginCommandBuffer(vk, *cmdBufferFinish);
    vk::VkImageMemoryBarrier postImageBarrier = makeImageMemoryBarrier(
        vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL,
        vk::VK_IMAGE_LAYOUT_GENERAL, *outputImage, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBufferFinish, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                          &postImageBarrier);

    const vk::VkBufferImageCopy copyRegion =
        makeBufferImageCopy(makeExtent3D(imageExtent.width, imageExtent.height, 1u), subresourceLayers);
    vk.cmdCopyImageToBuffer(*cmdBufferFinish, *outputImage, vk::VK_IMAGE_LAYOUT_GENERAL, *outputBuffer, 1u,
                            &copyRegion);

    endCommandBuffer(vk, *cmdBufferFinish);

    if (m_parameters.testType == TEST_TYPE_HOST_COPY_INTO_IMAGE ||
        m_parameters.testType == TEST_TYPE_HOST_COPY_FROM_IMAGE)
    {
#ifndef CTS_USES_VULKANSC
        std::vector<VkCommandBuffer> commandBuffers;
        commandBuffers.push_back(*cmdBufferInit);
        commandBuffers.push_back(*cmdBufferSample);
        submit(commandBuffers);

        if (m_parameters.testType == TEST_TYPE_HOST_COPY_INTO_IMAGE)
        {
            const VkMemoryToImageCopy memoryRegion{
                VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY, // VkStructureType			sType;
                nullptr,                                // const void*				pNext;
                generatedData2.data(),                  // const void*				pHostPointer;
                0u,                                     // uint32_t					memoryRowLength;
                0u,                                     // uint32_t					memoryImageHeight;
                subresourceLayers,                      // VkImageSubresourceLayers	imageSubresource;
                {0, 0, 0},                              // VkOffset3D				imageOffset;
                imageExtent,                            // VkExtent3D				imageExtent;
            };
            const VkCopyMemoryToImageInfo copyMemoryToImageInfo{
                VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO, // VkStructureType				sType;
                nullptr,                                     // const void*					pNext;
                0u,                                          // VkHostImageCopyFlags		    flags;
                *sampledImage,                               // VkImage						dstImage;
                VK_IMAGE_LAYOUT_GENERAL,                     // VkImageLayout				dstImageLayout;
                1u,                                          // uint32_t					    regionCount;
                &memoryRegion,                               // const VkMemoryToImageCopy*	pRegions;
            };
            vk.copyMemoryToImage(device, &copyMemoryToImageInfo);
        }
        else
        {
            const VkImageToMemoryCopy memoryRegion{
                VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY, // VkStructureType			sType;
                nullptr,                                // const void*				pNext;
                generatedData2.data(),                  // void*					pHostPointer;
                0u,                                     // uint32_t					memoryRowLength;
                0u,                                     // uint32_t					memoryImageHeight;
                subresourceLayers,                      // VkImageSubresourceLayers	imageSubresource;
                {0, 0, 0},                              // VkOffset3D				imageOffset;
                imageExtent,                            // VkExtent3D				imageExtent;
            };
            const VkCopyImageToMemoryInfo copyImageToMemoryInfo{
                VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO, // VkStructureType				sType;
                nullptr,                                     // const void*					pNext;
                0u,                                          // VkHostImageCopyFlags		    flags;
                *sampledImage,                               // VkImage						srcImage;
                VK_IMAGE_LAYOUT_GENERAL,                     // VkImageLayout				srcImageLayout;
                1u,                                          // uint32_t					    regionCount;
                &memoryRegion,                               // const VkImageToMemoryCopy*	pRegions;
            };
            vk.copyImageToMemory(device, &copyImageToMemoryInfo);
        }

        commandBuffers.push_back(*cmdBufferSample);
        commandBuffers.push_back(*cmdBufferFinish);
        submit(commandBuffers);
#endif
    }
    else
    {
        std::vector<VkCommandBuffer> commandBuffers;
        commandBuffers.push_back(*cmdBufferInit);
        commandBuffers.push_back(*cmdBufferSample);
        commandBuffers.push_back(*cmdBufferOp);
        commandBuffers.push_back(*cmdBufferSample);
        commandBuffers.push_back(*cmdBufferFinish);
        submit(commandBuffers);
    }

    invalidateAlloc(vk, device, outputBuffer.getAllocation());

    const auto outputData = reinterpret_cast<const uint8_t *>(outputBuffer.getAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess textureAccess;
    if (m_parameters.testType == TEST_TYPE_COPY_INTO_IMAGE || m_parameters.testType == TEST_TYPE_HOST_COPY_INTO_IMAGE)
        textureAccess = texture2.getAccess();
    else
        textureAccess = texture.getAccess();
    const int width       = textureAccess.getWidth();
    const int height      = textureAccess.getHeight();
    const int depth       = textureAccess.getDepth();
    const int numChannels = 4;

    for (int z = 0; z < depth; ++z)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                tcu::Vec4 texturePixelHalf = textureAccess.getPixel(x, y, z);
                float texturePixel[4]      = {
                    static_cast<float>(texturePixelHalf.x()), static_cast<float>(texturePixelHalf.y()),
                    static_cast<float>(texturePixelHalf.z()), static_cast<float>(texturePixelHalf.w())};

                size_t pixelIndex = (z * width * height + y * width + x) * numChannels;
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    float bufferValue   = (float)outputData[pixelIndex + channel] / 255.0f;
                    float textureValue  = texturePixel[channel];
                    const float epsilon = 0.04f;
                    if (std::abs(bufferValue - textureValue) > epsilon)
                    {
                        log << tcu::TestLog::Section("image_result", "image_result")
                            << tcu::LogImage("image", "", textureAccess) << tcu::TestLog::EndSection;

                        log << tcu::TestLog::Message << "At pixel (" << x << ", " << y << ", " << z << ") channel "
                            << channel << " value is expected to be " << textureValue << ", but actual value is "
                            << bufferValue << tcu::TestLog::EndMessage;

                        return tcu::TestStatus::fail("Fail");
                    }
                }
            }
        }
    }

    if (m_parameters.testType == TEST_TYPE_COPY_FROM_IMAGE)
    {
        const auto outputData2 = reinterpret_cast<const uint8_t *>(srcBufferCopy.getAllocation().getHostPtr());
        if (memcmp(srcData, outputData2, (size_t)srcBufferSize) != 0)
        {
            uint32_t fail_print_count = 20;
            for (uint32_t i = 0; i < srcBufferSize; ++i)
            {
                log << tcu::TestLog::Message << "At byte " << i << " source data was " << srcData[i]
                    << ", but result is " << outputData2[i] << tcu::TestLog::EndMessage;
                if (fail_print_count-- == 0)
                {
                    log << tcu::TestLog::Message << "Remaining errors not logged" << tcu::TestLog::EndMessage;
                    break;
                }
            }
            return tcu::TestStatus::fail("Fail");
        }
    }
    else if (m_parameters.testType == TEST_TYPE_HOST_COPY_FROM_IMAGE)
    {
        const auto outputData2 = generatedData2.data();
        if (memcmp(srcData, outputData2, (size_t)srcBufferSize) != 0)
        {
            uint32_t fail_print_count = 20;
            for (uint32_t i = 0; i < srcBufferSize; ++i)
            {
                log << tcu::TestLog::Message << "At byte " << i << " source data was " << srcData[i]
                    << ", but result is " << outputData2[i] << tcu::TestLog::EndMessage;
                if (fail_print_count-- == 0)
                {
                    log << tcu::TestLog::Message << "Remaining errors not logged" << tcu::TestLog::EndMessage;
                    break;
                }
            }
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class AstcSampleCase : public TestCase
{
public:
    AstcSampleCase(tcu::TestContext &testCtx, const std::string &name, const AstcTestParameters &parameters)
        : TestCase(testCtx, name)
        , m_parameters(parameters)
    {
    }
    virtual void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

protected:
    const AstcTestParameters m_parameters;
};

void AstcSampleCase::checkSupport(Context &context) const
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vk           = context.getInstanceInterface();

    context.requireDeviceFunctionality("VK_EXT_astc_decode_mode");
    if (!getPhysicalDeviceFeatures(vk, physicalDevice).textureCompressionASTC_LDR)
        TCU_THROW(NotSupportedError, "textureCompressionASTC_LDR not supported");

    const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(
        context.getInstanceInterface(), context.getPhysicalDevice(), VK_FORMAT_ASTC_8x8_UNORM_BLOCK);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "format feature sample image bit not supported");

    if (m_parameters.testType == TEST_TYPE_HOST_COPY_FROM_IMAGE ||
        m_parameters.testType == TEST_TYPE_HOST_COPY_INTO_IMAGE)
        context.requireDeviceFunctionality("VK_EXT_host_image_copy");
}

TestInstance *AstcSampleCase::createInstance(Context &context) const
{
    return new AstcSampleTestInstance(context, m_parameters);
}

void AstcSampleCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 450\n"
         << "layout (location=0) out vec2 texCoord;\n"
         << "void main()\n"
         << "{\n"
         << "    texCoord = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);"
         << "    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    std::ostringstream frag;
    frag << "#version 450\n"
         << "layout (location=0) out vec4 out_color;\n"
         << "layout (location=0) in vec2 texCoord;\n"
         << "layout (set=0, binding=0) uniform sampler2D combinedSampler;\n"
         << "void main()\n"
         << "{\n"
         << "    out_color = texture(combinedSampler, texCoord);\n"
         << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

struct MemoryBarrierTestParameters
{
    VkShaderStageFlagBits stage;
    bool writeFirst;
    VkAccessFlags2 readAccess;
    VkAccessFlags2 writeAccess;
};

class MemoryBarrierTestInstance : public TestInstance
{
public:
    MemoryBarrierTestInstance(Context &context, const MemoryBarrierTestParameters &parameters)
        : TestInstance(context)
        , m_parameters(parameters)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    const MemoryBarrierTestParameters m_parameters;
};

tcu::TestStatus MemoryBarrierTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    auto &alloc                     = m_context.getDefaultAllocator();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const VkExtent3D imageExtent = makeExtent3D(128u, 128u, 1u);
    const auto componentMapping  = makeComponentMappingRGBA();
    const auto subresourceRange  = makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto subresourceLayers = makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

    const uint32_t bufferCount                     = imageExtent.width * imageExtent.height;
    const uint32_t bufferSize                      = (uint32_t)(bufferCount * sizeof(float));
    de::MovePtr<BufferWithMemory> srcBuffer        = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, alloc,
        makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                    vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));
    de::MovePtr<BufferWithMemory> readOutputBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, alloc,
        makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                             vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));
    de::MovePtr<BufferWithMemory> imageCopyBuffer  = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, alloc,
        makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                              vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));
    std::vector<float> testData(bufferCount);
    de::Random rnd(0x01234);
    for (uint32_t i = 0; i < bufferCount; ++i)
        testData[i] = rnd.getFloat();

    float *data = reinterpret_cast<float *>(srcBuffer->getAllocation().getHostPtr());
    memcpy(data, testData.data(), bufferSize);
    flushAlloc(vk, device, srcBuffer->getAllocation());

    VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT,
        imageExtent,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vk::ImageWithMemory image(vk, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any);
    imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    vk::ImageWithMemory fbImage(vk, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any);

    vk::VkImageViewCreateInfo imageViewCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        (vk::VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        *image,                                       // VkImage image;
        vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        imageCreateInfo.format,                       // VkFormat format;
        componentMapping,                             // VkComponentMapping components;
        subresourceRange                              // VkImageSubresourceRange subresourceRange;
    };
    const auto imageView      = createImageView(vk, device, &imageViewCreateInfo, nullptr);
    imageViewCreateInfo.image = *fbImage;
    const auto fbImageView    = createImageView(vk, device, &imageViewCreateInfo, nullptr);

    const auto sampler = makeSampler(vk, device);

    const vk::VkAttachmentDescription2 colorAttachmentDescription = {
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType					sType;
        nullptr,                                    // const void*						pNext;
        (VkAttachmentDescriptionFlags)0u,           // VkAttachmentDescriptionFlags    flags
        imageCreateInfo.format,                     // VkFormat                        format
        VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits           samples
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // VkAttachmentLoadOp              loadOp
        VK_ATTACHMENT_STORE_OP_STORE,               // VkAttachmentStoreOp             storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // VkAttachmentLoadOp              stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,           // VkAttachmentStoreOp             stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout                   initialLayout
        VK_IMAGE_LAYOUT_GENERAL                     // VkImageLayout                   finalLayout
    };

    const vk::VkAttachmentReference2 colorAttachmentRef = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, // VkStructureType		sType;
        nullptr,                                  // const void*			pNext;
        0u,                                       // uint32_t         attachment
        VK_IMAGE_LAYOUT_GENERAL,                  // VkImageLayout    layout
        VK_IMAGE_ASPECT_COLOR_BIT                 // VkImageAspectFlags aspectMask;
    };

    const VkSubpassDescription2 subpassDescription = {
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, // VkStructureType					sType;
        nullptr,                                 // const void*						pNext;
        (VkSubpassDescriptionFlags)0u,           // VkSubpassDescriptionFlags       flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,         // VkPipelineBindPoint             pipelineBindPoint
        0u,                                      // uint32_t						viewMask;
        0u,                                      // uint32_t                        inputAttachmentCount
        nullptr,                                 // const VkAttachmentReference*    pInputAttachments
        1u,                                      // uint32_t                        colorAttachmentCount
        &colorAttachmentRef,                     // const VkAttachmentReference*    pColorAttachments
        nullptr,                                 // const VkAttachmentReference*    pResolveAttachments
        nullptr,                                 // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                                      // uint32_t                        preserveAttachmentCount
        nullptr                                  // const uint32_t*                 pPreserveAttachments
    };

    VkMemoryBarrier2 memoryBarrier =
        makeMemoryBarrier2(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, m_parameters.writeAccess,
                           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, m_parameters.readAccess);

    VkSubpassDependency2 dependency = {
        VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2, // VkStructureType			sType;
        &memoryBarrier,                         // const void*				pNext;
        0u,                                     // uint32_t				srcSubpass;
        0u,                                     // uint32_t				dstSubpass;
        0u,                                     // VkPipelineStageFlags	srcStageMask;
        0u,                                     // VkPipelineStageFlags	dstStageMask;
        0u,                                     // VkAccessFlags		srcAccessMask;
        0u,                                     // VkAccessFlags		dstAccessMask;
        VK_DEPENDENCY_BY_REGION_BIT,            // VkDependencyFlags	dependencyFlags;
        0                                       //int32_t viewOffset;
    };

    const vk::VkRenderPassCreateInfo2 renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        0u,                                          // VkRenderPassCreateFlags flags;
        1u,                                          // uint32_t attachmentCount;
        &colorAttachmentDescription,                 // const VkAttachmentDescription* pAttachments;
        1u,                                          // uint32_t subpassCount;
        &subpassDescription,                         // const VkSubpassDescription* pSubpasses;
        1u,                                          // uint32_t dependencyCount;
        &dependency,                                 // const VkSubpassDependency* pDependencies;
        0u,                                          // uint32_t correlatedViewMaskCount;
        nullptr                                      // const uint32_t* pCorrelatedViewMasks;
    };

    vk::Move<vk::VkRenderPass> renderPass = createRenderPass2(vk, device, &renderPassInfo);
    vk::Move<vk::VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *fbImageView, imageExtent.width, imageExtent.height);

    vk::Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                              VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                              VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device);

    vk::Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)
            .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

    vk::Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

    VkDescriptorImageInfo imageInfo   = makeDescriptorImageInfo(*sampler, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(**readOutputBuffer, 0u, VK_WHOLE_SIZE);

    DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u),
                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
    updateBuilder.update(vk, device);

    const vk::Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device, 1u, &*descriptorSetLayout);

    vk::Move<VkShaderModule> writeComputeShader =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("write_comp"));
    vk::Move<VkShaderModule> readComputeShader =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("read_comp"));

    vk::Move<VkPipeline> writeComputePipeline = makeComputePipeline(vk, device, *pipelineLayout, *writeComputeShader);
    vk::Move<VkPipeline> readComputePipeline  = makeComputePipeline(vk, device, *pipelineLayout, *readComputeShader);

    vk::Move<VkShaderModule> vertexShader = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    vk::Move<VkShaderModule> writeFragmentShader =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("write_frag"));
    vk::Move<VkShaderModule> readFragmentShader =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("read_frag"));

    std::vector<VkViewport> viewports{makeViewport(imageExtent)};
    std::vector<VkRect2D> scissors{makeRect2D(imageExtent)};

    VkPipelineVertexInputStateCreateInfo vertexInput = vk::initVulkanStructure();

    vk::Move<VkPipeline> writeGraphicsPipeline =
        makeGraphicsPipeline(vk, device, *pipelineLayout, *vertexShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             *writeFragmentShader, *renderPass, viewports, scissors,
                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInput);
    vk::Move<VkPipeline> readGraphicsPipeline = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertexShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *readFragmentShader,
        *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInput);

    const auto commandPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);
    auto imageMemoryBarrier = makeImageMemoryBarrier(0u, vk::VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                     VK_IMAGE_LAYOUT_GENERAL, *image, subresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_NONE, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr,
                          0u, nullptr, 1, &imageMemoryBarrier);
    VkBufferImageCopy bufferImageCopy = makeBufferImageCopy(imageExtent, subresourceLayers);
    vk.cmdCopyBufferToImage(*cmdBuffer, **srcBuffer, *image, VK_IMAGE_LAYOUT_GENERAL, 1u, &bufferImageCopy);

#ifndef CTS_USES_VULKANSC
    {
        auto memoryBarrier2 = makeMemoryBarrier2(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                 m_parameters.stage == VK_SHADER_STAGE_FRAGMENT_BIT ?
                                                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT :
                                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                 m_parameters.readAccess | m_parameters.writeAccess);
        VkDependencyInfo dependencyInfo   = vk::initVulkanStructure();
        dependencyInfo.memoryBarrierCount = 1u;
        dependencyInfo.pMemoryBarriers    = &memoryBarrier2;
        vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);

        if (m_parameters.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            vk::beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissors[0]);
            vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                     &*descriptorSet, 0u, nullptr);
        }
        else
        {
            vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                     &*descriptorSet, 0u, nullptr);
        }
    }
    {
        VkMemoryBarrier2 memoryBarrier2;
        if (m_parameters.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            if (m_parameters.writeFirst)
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *writeGraphicsPipeline);
            else
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *readGraphicsPipeline);

            vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

            memoryBarrier2 = makeMemoryBarrier2(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, m_parameters.writeAccess,
                                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, m_parameters.readAccess);
        }
        else
        {
            if (m_parameters.writeFirst)
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *writeComputePipeline);
            else
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *readComputePipeline);

            vk.cmdDispatch(*cmdBuffer, imageExtent.width, imageExtent.height, 1u);

            memoryBarrier2 = makeMemoryBarrier2(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, m_parameters.writeAccess,
                                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, m_parameters.readAccess);
        }

        VkDependencyInfo dependencyInfo   = vk::initVulkanStructure();
        dependencyInfo.memoryBarrierCount = 1u;
        dependencyInfo.pMemoryBarriers    = &memoryBarrier2;
        dependencyInfo.dependencyFlags    = m_parameters.stage == VK_SHADER_STAGE_FRAGMENT_BIT ?
                                                (VkDependencyFlags)VK_DEPENDENCY_BY_REGION_BIT :
                                                (VkDependencyFlags)0u;
        vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);

        if (m_parameters.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            if (m_parameters.writeFirst)
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *readGraphicsPipeline);
            else
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *writeGraphicsPipeline);

            vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
        }
        else
        {
            if (m_parameters.writeFirst)
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *readComputePipeline);
            else
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *writeComputePipeline);

            vk.cmdDispatch(*cmdBuffer, imageExtent.width, imageExtent.height, 1u);
        }
    }
    {
        if (m_parameters.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
            endRenderPass(vk, *cmdBuffer);

        VkMemoryBarrier2 memoryBarrier2;
        if (m_parameters.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            memoryBarrier2 = makeMemoryBarrier2(
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT);
        }
        else
        {
            memoryBarrier2 = makeMemoryBarrier2(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                m_parameters.writeAccess | m_parameters.readAccess,
                                                VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT);
        }

        VkDependencyInfo dependencyInfo   = initVulkanStructure();
        dependencyInfo.memoryBarrierCount = 1u;
        dependencyInfo.pMemoryBarriers    = &memoryBarrier2;
        vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);

        const VkBufferImageCopy copyRegion = makeBufferImageCopy(imageExtent, subresourceLayers);
        vk.cmdCopyImageToBuffer(*cmdBuffer, *image, VK_IMAGE_LAYOUT_GENERAL, **imageCopyBuffer, 1u, &copyRegion);
        if (m_parameters.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            vk.cmdCopyImageToBuffer(*cmdBuffer, *fbImage, VK_IMAGE_LAYOUT_GENERAL, **readOutputBuffer, 1u, &copyRegion);
        }
    }
#endif
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    {
        tcu::ConstPixelBufferAccess resultCopyBuffer =
            tcu::ConstPixelBufferAccess(mapVkFormat(imageCreateInfo.format), imageExtent.width, imageExtent.height, 1u,
                                        (const void *)readOutputBuffer->getAllocation().getHostPtr());
        for (uint32_t i = 0; i < bufferCount; ++i)
        {
            float value = (reinterpret_cast<const float *>(resultCopyBuffer.getDataPtr()))[i];
            float expected =
                m_parameters.writeFirst ? float((i / imageExtent.width) + (i % imageExtent.width)) : testData[i];
            const float epsilon = 1e-6f;
            if (std::abs(value - expected) > epsilon)
            {
                log << tcu::TestLog::Message << "At index " << i << " result data is " << value
                    << ", but expected value is " << expected << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
        }
    }
    {
        tcu::ConstPixelBufferAccess resultCopyBuffer =
            tcu::ConstPixelBufferAccess(mapVkFormat(imageCreateInfo.format), imageExtent.width, imageExtent.height, 1u,
                                        (const void *)imageCopyBuffer->getAllocation().getHostPtr());
        for (uint32_t i = 0; i < bufferCount; ++i)
        {
            float value         = (reinterpret_cast<const float *>(resultCopyBuffer.getDataPtr()))[i];
            float expected      = float((i / imageExtent.width) + (i % imageExtent.width));
            const float epsilon = 1e-6f;
            if (std::abs(value - expected) > epsilon)
            {
                log << tcu::TestLog::Message << "Framebuffer result: At index " << i << " result data is " << value
                    << ", but expected value is " << expected << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class MemoryBarrierCase : public TestCase
{
public:
    MemoryBarrierCase(tcu::TestContext &testCtx, const std::string &name, const MemoryBarrierTestParameters &parameters)
        : TestCase(testCtx, name)
        , m_parameters(parameters)
    {
    }
    virtual void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

protected:
    const MemoryBarrierTestParameters m_parameters;
};

void MemoryBarrierCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_synchronization2");
}

TestInstance *MemoryBarrierCase::createInstance(Context &context) const
{
    return new MemoryBarrierTestInstance(context, m_parameters);
}

void MemoryBarrierCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 450\n"
         << "layout (location = 0) out vec2 texCoord;\n"
         << "void main()\n"
         << "{\n"
         << "    texCoord = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);"
         << "    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    std::ostringstream write_frag;
    write_frag << "#version 450\n"
               << "layout (location = 0) in vec2 texCoord;\n"
               << "layout (binding = 0, r32f) uniform image2D storageImage;\n"
               << "void main()\n"
               << "{\n"
               << "    ivec2 coord = ivec2(texCoord * 128.0f);\n"
               << "    vec4 color = vec4(coord.x + coord.y);\n"
               << "    imageStore(storageImage, coord, color);\n"
               << "}\n";

    std::ostringstream read_frag;
    read_frag << "#version 450\n"
              << "layout (location=0) out vec4 out_color;\n"
              << "layout (location=0) in vec2 texCoord;\n";
    if (m_parameters.readAccess == VK_ACCESS_2_SHADER_STORAGE_READ_BIT)
        read_frag << "layout (binding = 0, r32f) uniform image2D storageImage;\n";
    else
        read_frag << "layout (binding = 1) uniform sampler2D image;\n";
    read_frag << "void main()\n"
              << "{\n"
              << "    ivec2 coord = ivec2(texCoord * 128.0f);\n";
    if (m_parameters.readAccess == VK_ACCESS_2_SHADER_STORAGE_READ_BIT)
        read_frag << "    vec4 color = imageLoad(storageImage, coord);\n ";
    else
        read_frag << "    vec4 color = texture(image, texCoord);\n ";
    read_frag << "    out_color = color;\n"
              << "}\n";

    std::ostringstream write_comp;
    write_comp << "#version 450\n"
               << "layout (local_size_x = 1, local_size_y = 1) in;\n"
               << "layout (binding = 0, r32f) uniform image2D storageImage;\n"
               << "void main()\n"
               << "{\n"
               << "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n "
               << "    vec4 color = vec4(coord.x + coord.y);\n"
               << "    imageStore(storageImage, coord, color);\n"
               << "}\n";

    std::ostringstream read_comp;
    read_comp << "#version 450\n"
              << "layout (local_size_x = 1, local_size_y = 1) in;\n";
    if (m_parameters.readAccess == VK_ACCESS_2_SHADER_STORAGE_READ_BIT)
        read_comp << "layout (binding = 0, r32f) uniform image2D storageImage;\n";
    else
        read_comp << "layout (binding = 1) uniform sampler2D image;\n";
    read_comp << "layout (binding = 2) buffer OutputBuffer { float data[]; } outputBuffer;\n"
              << "void main()\n"
              << "{\n"
              << "    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n ";
    if (m_parameters.readAccess == VK_ACCESS_2_SHADER_STORAGE_READ_BIT)
        read_comp << "    vec4 color = imageLoad(storageImage, coord);\n ";
    else
        read_comp << "    vec4 color = texture(image, vec2(coord) / vec2(128.0f));\n ";
    read_comp << "    outputBuffer.data[coord.y * 128 + coord.x] = color.r;\n"
              << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("write_frag") << glu::FragmentSource(write_frag.str());
    programCollection.glslSources.add("read_frag") << glu::FragmentSource(read_frag.str());
    programCollection.glslSources.add("write_comp") << glu::ComputeSource(write_comp.str());
    programCollection.glslSources.add("read_comp") << glu::ComputeSource(read_comp.str());
}

enum BarrierTestType
{
    BARRIER_TEST_EXECUTION = 0,
    BARRIER_TEST_MEMORY,
    BARRIER_TEST_IMAGE,
    BARRIER_TEST_LAST,
};

struct InputAttachmentParams
{
    bool inputAttachment;
    BarrierTestType barrierTest;
    bool dynamicRendering;
};

class InputAttachmentTestInstance : public TestInstance
{
public:
    InputAttachmentTestInstance(Context &context, const InputAttachmentParams &parameters)
        : TestInstance(context)
        , m_parameters(parameters)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    const InputAttachmentParams m_parameters;
};

tcu::TestStatus InputAttachmentTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &alloc                = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const VkFormat format        = VK_FORMAT_R8G8B8A8_UNORM;
    const VkExtent3D imageExtent = makeExtent3D(128u, 128u, 1u);
    const auto componentMapping  = makeComponentMappingRGBA();
    const auto subresourceRange  = makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto subresourceLayers = makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

    const uint32_t bufferCount           = imageExtent.width * imageExtent.height;
    const uint32_t bufferSize            = (uint32_t)(bufferCount * 4u);
    de::MovePtr<BufferWithMemory> buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, alloc,
        makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                             vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    std::vector<uint8_t> testData(bufferSize);
    de::Random rnd(0x01234);
    for (uint32_t i = 0; i < bufferCount; ++i)
        testData[i] = rnd.getUint8();

    uint8_t *bufferData = reinterpret_cast<uint8_t *>(buffer->getAllocation().getHostPtr());
    memcpy(bufferData, testData.data(), bufferSize);

    VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        format,
        imageExtent,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    vk::ImageWithMemory image1(vk, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any);
    vk::ImageWithMemory image2(vk, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any);

    vk::VkImageViewCreateInfo imageViewCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        (vk::VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        *image1,                                      // VkImage image;
        vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        imageCreateInfo.format,                       // VkFormat format;
        componentMapping,                             // VkComponentMapping components;
        subresourceRange                              // VkImageSubresourceRange subresourceRange;
    };
    const auto imageView1     = createImageView(vk, device, &imageViewCreateInfo);
    imageViewCreateInfo.image = *image2;
    const auto imageView2     = createImageView(vk, device, &imageViewCreateInfo);

    const auto sampler = makeSampler(vk, device);

    const VkAttachmentDescription attachmentDescs[] = {
        {
            0u,                               // VkAttachmentDescriptionFlags	flags;
            format,                           // VkFormat						format;
            VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits			samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,       // VkAttachmentLoadOp				loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp			storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp				stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp			stencilStoreOp;
            VK_IMAGE_LAYOUT_GENERAL,          // VkImageLayout					initialLayout;
            VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout					finalLayout;
        },
        {
            0u,                               // VkAttachmentDescriptionFlags	flags;
            format,                           // VkFormat						format;
            VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits			samples;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp				loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp			storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp				stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp			stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout					initialLayout;
            VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout					finalLayout;
        }};

    const VkAttachmentReference attachmentRefs[] = {
        {0u, VK_IMAGE_LAYOUT_GENERAL},
        {1u, VK_IMAGE_LAYOUT_GENERAL},
    };

    const VkSubpassDescription subpass1 = {
        0u,                              // VkSubpassDescriptionFlags		flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint				pipelineBindPoint;
        1u,                              // uint32_t						inputAttachmentCount;
        &attachmentRefs[0],              // const VkAttachmentReference*	pInputAttachments;
        1u,                              // uint32_t						colorAttachmentCount;
        &attachmentRefs[1],              // const VkAttachmentReference*	pColorAttachments;
        nullptr,                         // const VkAttachmentReference*	pResolveAttachments;
        nullptr,                         // const VkAttachmentReference*	pDepthStencilAttachment;
        0u,                              // uint32_t						preserveAttachmentCount;
        nullptr                          // const uint32_t*					pPreserveAttachments;
    };

    const VkSubpassDescription subpass2 = {
        0u,                              // VkSubpassDescriptionFlags		flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint				pipelineBindPoint;
        1u,                              // uint32_t						inputAttachmentCount;
        &attachmentRefs[1],              // const VkAttachmentReference*	pInputAttachments;
        1u,                              // uint32_t						colorAttachmentCount;
        &attachmentRefs[0],              // const VkAttachmentReference*	pColorAttachments;
        nullptr,                         // const VkAttachmentReference*	pResolveAttachments;
        nullptr,                         // const VkAttachmentReference*	pDepthStencilAttachment;
        0u,                              // uint32_t						preserveAttachmentCount;
        nullptr                          // const uint32_t*					pPreserveAttachments;
    };

    const VkSubpassDependency dependencies[3] = {
        {
            0u,                                                                                    // srcSubpass
            0u,                                                                                    // dstSubpass
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,                                         // dstStageMask
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,                      // srcAccessMask
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,                                                  // dstAccessMask
            VK_DEPENDENCY_BY_REGION_BIT                                                            // dependencyFlags
        },
        {
            0u,                                            // srcSubpass
            1u,                                            // dstSubpass
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // dstStageMask
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccessMask
            VK_ACCESS_SHADER_READ_BIT,                     // dstAccessMask
            VK_DEPENDENCY_BY_REGION_BIT                    // dependencyFlags
        },
        {
            1u,                                            // srcSubpass
            1u,                                            // dstSubpass
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // dstStageMask
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccessMask
            VK_ACCESS_SHADER_READ_BIT,                     // dstAccessMask
            VK_DEPENDENCY_BY_REGION_BIT                    // dependencyFlags
        }};

    VkSubpassDescription subpasses[] = {subpass1, subpass2};

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType				    sType;
        nullptr,                                   // const void*					    pNext;
        0u,                                        // VkRenderPassCreateFlags		    flags;
        2u,                                        // uint32_t						    attachmentCount;
        attachmentDescs,                           // const VkAttachmentDescription*    pAttachments;
        2u,                                        // uint32_t						    subpassCount;
        subpasses,                                 // const VkSubpassDescription*	    pSubpasses;
        3u,                                        // uint32_t						    dependencyCount;
        dependencies                               // const VkSubpassDependency*	    pDependencies;
    };

    const vk::Move<vk::VkRenderPass> renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

    const VkImageView imageViews[] = {*imageView1, *imageView2};

    const VkFramebufferCreateInfo framebufferCreateInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
        nullptr,                                   // pNext
        0u,                                        // flags
        *renderPass,                               // renderPass
        2u,                                        // attachmentCount
        imageViews,                                // pAttachments
        imageExtent.width,                         // width
        imageExtent.height,                        // height
        1u                                         // layers
    };

    const vk::Move<vk::VkFramebuffer> framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);

    VkDescriptorType descriptorType =
        m_parameters.inputAttachment ? VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    vk::Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(descriptorType, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device);

    vk::Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(descriptorType, 1)
            .addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

    vk::Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

    VkDescriptorImageInfo imageInfo1 = makeDescriptorImageInfo(*sampler, *imageView1, VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorImageInfo imageInfo2 = makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView2, VK_IMAGE_LAYOUT_GENERAL);

    DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                              &imageInfo1);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                              VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfo2);
    updateBuilder.update(vk, device);

    const vk::Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device, 1u, &*descriptorSetLayout);
    vk::Move<VkShaderModule> vertexShader = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    vk::Move<VkShaderModule> writeFragmentShader =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag1"));
    vk::Move<VkShaderModule> readFragmentShader =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag2"));

    std::vector<VkViewport> viewports{makeViewport(imageExtent)};
    std::vector<VkRect2D> scissors{makeRect2D(imageExtent)};

    VkPipelineVertexInputStateCreateInfo vertexInput = vk::initVulkanStructure();

    VkRenderPass renderPassHandle = *renderPass;
    void *pNext                   = nullptr;
#ifndef CTS_USES_VULKANSC
    uint32_t locations[]    = {0, 1};
    uint32_t inputIndices[] = {1, 0};

    VkRenderingInputAttachmentIndexInfo inputAttachmentIndexInfo = {
        VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO, // VkStructureType	sType;
        nullptr,                                                 // const void*		pNext;
        2u,                                                      // uint32_t		colorAttachmentCount;
        inputIndices,                                            // const uint32_t*	pColorAttachmentInputIndices;
        nullptr,                                                 // const uint32_t*	pDepthInputAttachmentIndex;
        nullptr,                                                 // const uint32_t*	pStencilInputAttachmentIndex;
    };
    VkRenderingAttachmentLocationInfo renderingAttachmentLocationInfo = {
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO, // VkStructureType	sType;
        &inputAttachmentIndexInfo,                            // const void*		pNext;
        2u,                                                   // uint32_t		    colorAttachmentCount;
        locations                                             // const uint32_t*	pColorAttachmentLocations;
    };

    VkFormat formats[] = {format, format};

    VkPipelineRenderingCreateInfo pipelineRendering = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, // VkStructureType	sType;
        &renderingAttachmentLocationInfo,                 // const void*		pNext;
        0u,                                               // uint32_t		    viewMask;
        2u,                                               // uint32_t		    colorAttachmentCount;
        formats,                                          // const VkFormat*	pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                              // VkFormat		    depthAttachmentFormat;
        VK_FORMAT_UNDEFINED                               // VkFormat		    stencilAttachmentFormat;
    };

    if (m_parameters.dynamicRendering)
    {
        pNext            = &pipelineRendering;
        renderPassHandle = VK_NULL_HANDLE;
    }
#endif

    VkPipelineColorBlendAttachmentState colorBlendAttachments[] = {
        {
            VK_FALSE,                            // VkBool32				blendEnable;
            VK_BLEND_FACTOR_ONE,                 // VkBlendFactor			srcColorBlendFactor;
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // VkBlendFactor			dstColorBlendFactor;
            VK_BLEND_OP_ADD,                     // VkBlendOp				colorBlendOp;
            VK_BLEND_FACTOR_ONE,                 // VkBlendFactor			srcAlphaBlendFactor;
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // VkBlendFactor			dstAlphaBlendFactor;
            VK_BLEND_OP_ADD,                     // VkBlendOp				alphaBlendOp;
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT // VkColorComponentFlags	colorWriteMask;
        },
        {
            VK_FALSE,                            // VkBool32				blendEnable;
            VK_BLEND_FACTOR_ONE,                 // VkBlendFactor			srcColorBlendFactor;
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // VkBlendFactor			dstColorBlendFactor;
            VK_BLEND_OP_ADD,                     // VkBlendOp				colorBlendOp;
            VK_BLEND_FACTOR_ONE,                 // VkBlendFactor			srcAlphaBlendFactor;
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // VkBlendFactor			dstAlphaBlendFactor;
            VK_BLEND_OP_ADD,                     // VkBlendOp				alphaBlendOp;
            0u                                   // VkColorComponentFlags	colorWriteMask;
        }};

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType							sType;
        nullptr,                                                  // const void*								pNext;
        0u,                                                       // VkPipelineColorBlendStateCreateFlags		flags;
        VK_FALSE,                                                 // VkBool32									logicOpEnable;
        VK_LOGIC_OP_CLEAR,                                        // VkLogicOp									logicOp;
        m_parameters.dynamicRendering ? 2u : 1u,                  // uint32_t									attachmentCount;
        colorBlendAttachments,   // const VkPipelineColorBlendAttachmentState*	pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f} // float										blendConstants[4];
    };

    vk::Move<VkPipeline> pipeline1 = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertexShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        *writeFragmentShader, renderPassHandle, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u,
        &vertexInput, nullptr, nullptr, nullptr, &colorBlendStateCreateInfo, nullptr, pNext);
    vk::Move<VkPipeline> pipeline2 = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertexShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *readFragmentShader,
        renderPassHandle, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 1u, 0u, &vertexInput, nullptr,
        nullptr, nullptr, &colorBlendStateCreateInfo, nullptr, pNext);

    const auto commandPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    VkClearValue clearValue = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 0.0f);
    (void)clearValue;
    beginCommandBuffer(vk, *cmdBuffer);

    VkImageMemoryBarrier imageBarriers[] = {
        makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_GENERAL, *image1, subresourceRange),
        makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *image2, subresourceRange),
    };
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_NONE,
                          VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 2u, imageBarriers);
    const vk::VkBufferImageCopy copyRegion = makeBufferImageCopy(imageExtent, subresourceLayers);
    vk.cmdCopyBufferToImage(*cmdBuffer, **buffer, *image1, VK_IMAGE_LAYOUT_GENERAL, 1u, &copyRegion);
    VkMemoryBarrier preMemoryBarrier = makeMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          0u, 1u, &preMemoryBarrier, 0u, nullptr, 0u, nullptr);

    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             nullptr);

    if (m_parameters.dynamicRendering)
    {
#ifndef CTS_USES_VULKANSC
        VkRenderingAttachmentInfo renderingAttachmentInfos[2] = {
            {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType			sType;
                nullptr,                                     // const void*				pNext;
                *imageView2,                                 // VkImageView				imageView;
                VK_IMAGE_LAYOUT_GENERAL,                     // VkImageLayout			imageLayout;
                VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits	resolveMode;
                VK_NULL_HANDLE,                              // VkImageView				resolveImageView;
                VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout			resolveImageLayout;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,             // VkAttachmentLoadOp		loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,                // VkAttachmentStoreOp		storeOp;
                clearValue                                   // VkClearValue		    clearValue;
            },
            {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType			sType;
                nullptr,                                     // const void*				pNext;
                *imageView1,                                 // VkImageView				imageView;
                VK_IMAGE_LAYOUT_GENERAL,                     // VkImageLayout			imageLayout;
                VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits	resolveMode;
                VK_NULL_HANDLE,                              // VkImageView				resolveImageView;
                VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout			resolveImageLayout;
                VK_ATTACHMENT_LOAD_OP_LOAD,                  // VkAttachmentLoadOp		loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,                // VkAttachmentStoreOp		storeOp;
                clearValue                                   // VkClearValue	        clearValue;
            }};

        VkRenderingInfo renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO, // VkStructureType						sType;
            nullptr,                          // const void*							pNext;
            0u,                               // VkRenderingFlags					flags;
            scissors[0],                      // VkRect2D							renderArea;
            1u,                               // uint32_t							layerCount;
            0u,                               // uint32_t							viewMask;
            2u,                               // uint32_t							colorAttachmentCount;
            renderingAttachmentInfos,         // const VkRenderingAttachmentInfo*	pColorAttachments;
            nullptr,                          // const VkRenderingAttachmentInfo*	pDepthAttachment;
            nullptr,                          // const VkRenderingAttachmentInfo*	pStencilAttachment;
        };
        vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
#endif
    }
    else
    {
        const VkRenderPassBeginInfo renderPassBeginInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType		sType;
            nullptr,                                  // const void*			pNext;
            *renderPass,                              // VkRenderPass		    renderPass;
            *framebuffer,                             // VkFramebuffer		    framebuffer;
            makeRect2D(imageExtent),                  // VkRect2D			    renderArea;
            0u,                                       // uint32_t			    clearValueCount;
            nullptr                                   // const VkClearValue*	pClearValues;
        };

        vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline1);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

    VkMemoryBarrier postDrawMemoryBarrier = makeMemoryBarrier(
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1u,
                          &postDrawMemoryBarrier, 0u, nullptr, 0u, nullptr);
    if (m_parameters.dynamicRendering)
    {
#ifndef CTS_USES_VULKANSC
        vk.cmdEndRendering(*cmdBuffer);

        VkRenderingAttachmentInfo renderingAttachmentInfos[2] = {
            {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType			sType;
                nullptr,                                     // const void*				pNext;
                *imageView1,                                 // VkImageView				imageView;
                VK_IMAGE_LAYOUT_GENERAL,                     // VkImageLayout			imageLayout;
                VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits	resolveMode;
                VK_NULL_HANDLE,                              // VkImageView				resolveImageView;
                VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout			resolveImageLayout;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,             // VkAttachmentLoadOp		loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,                // VkAttachmentStoreOp		storeOp;
                clearValue                                   // VkClearValue			clearValue;
            },
            {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType			sType;
                nullptr,                                     // const void*				pNext;
                *imageView2,                                 // VkImageView				imageView;
                VK_IMAGE_LAYOUT_GENERAL,                     // VkImageLayout			imageLayout;
                VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits	resolveMode;
                VK_NULL_HANDLE,                              // VkImageView				resolveImageView;
                VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout			resolveImageLayout;
                VK_ATTACHMENT_LOAD_OP_LOAD,                  // VkAttachmentLoadOp		loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,                // VkAttachmentStoreOp		storeOp;
                clearValue                                   // VkClearValue			clearValue;
            }};

        VkRenderingInfo renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO, // VkStructureType					sType;
            nullptr,                          // const void*						pNext;
            0u,                               // VkRenderingFlags					flags;
            scissors[0],                      // VkRect2D							renderArea;
            1u,                               // uint32_t							layerCount;
            0u,                               // uint32_t							viewMask;
            2u,                               // uint32_t							colorAttachmentCount;
            renderingAttachmentInfos,         // const VkRenderingAttachmentInfo*	pColorAttachments;
            nullptr,                          // const VkRenderingAttachmentInfo*	pDepthAttachment;
            nullptr,                          // const VkRenderingAttachmentInfo*	pStencilAttachment;
        };

        VkMemoryBarrier memoryBarrier = makeMemoryBarrier(0u, 0u);
        VkImageMemoryBarrier imageMemoryBarrier =
            makeImageMemoryBarrier(0u, 0u, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, *image1, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
                              (m_parameters.barrierTest == BARRIER_TEST_MEMORY) ? 1u : 0u, &memoryBarrier, 0u, nullptr,
                              (m_parameters.barrierTest == BARRIER_TEST_IMAGE) ? 1u : 0u, &imageMemoryBarrier);

        vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
#endif
    }
    else
    {
        vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

        VkMemoryBarrier memoryBarrier = makeMemoryBarrier(0u, 0u);
        VkImageMemoryBarrier imageMemoryBarrier =
            makeImageMemoryBarrier(0u, 0u, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, *image1, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
                              (m_parameters.barrierTest == BARRIER_TEST_MEMORY) ? 1u : 0u, &memoryBarrier, 0u, nullptr,
                              (m_parameters.barrierTest == BARRIER_TEST_IMAGE) ? 1u : 0u, &imageMemoryBarrier);
    }

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline2);
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

    if (m_parameters.dynamicRendering)
    {
#ifndef CTS_USES_VULKANSC
        vk.cmdEndRendering(*cmdBuffer);
#endif
    }
    else
    {
        vk.cmdEndRenderPass(*cmdBuffer);
    }

    VkMemoryBarrier postMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          1u, &postMemoryBarrier, 0u, nullptr, 0u, nullptr);
    vk.cmdCopyImageToBuffer(*cmdBuffer, *image1, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &copyRegion);
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    tcu::ConstPixelBufferAccess resultCopyBuffer =
        tcu::ConstPixelBufferAccess(mapVkFormat(imageCreateInfo.format), imageExtent.width, imageExtent.height, 1u,
                                    (const void *)buffer->getAllocation().getHostPtr());
    for (uint32_t i = 0; i < bufferSize; ++i)
    {
        uint8_t value         = (reinterpret_cast<const uint8_t *>(resultCopyBuffer.getDataPtr()))[i];
        uint8_t expectedValue = (uint8_t)(255u - testData[i] / 2u);
        if (std::abs(value - expectedValue) > 1)
        {
            uint32_t fail_print_count = 20;
            for (uint32_t j = 0; j < bufferSize; ++j)
            {
                log << tcu::TestLog::Message << "At byte " << j << " expected value is " << (uint32_t)expectedValue
                    << ", but actual value is " << (uint32_t)value << " (epsilon is 1) " << tcu::TestLog::EndMessage;
                if (fail_print_count-- == 0)
                {
                    log << tcu::TestLog::Message << "Remaining errors not logged" << tcu::TestLog::EndMessage;
                    break;
                }
            }
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
} // namespace

class InputAttachmentCase : public TestCase
{
public:
    InputAttachmentCase(tcu::TestContext &testCtx, const std::string &name, const InputAttachmentParams &parameters)
        : TestCase(testCtx, name)
        , m_parameters(parameters)
    {
    }
    virtual void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

protected:
    const InputAttachmentParams m_parameters;
};

void InputAttachmentCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_synchronization2");
    if (m_parameters.dynamicRendering)
    {
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");
    }
}

TestInstance *InputAttachmentCase::createInstance(Context &context) const
{
    return new InputAttachmentTestInstance(context, m_parameters);
}

void InputAttachmentCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 450\n"
         << "layout (location = 0) out vec2 texCoord;\n"
         << "void main()\n"
         << "{\n"
         << "    texCoord = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);\n"
         << "    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    std::ostringstream frag1;
    frag1 << "#version 450\n"
          << "layout (location = 0) in vec2 texCoord;\n";
    if (m_parameters.inputAttachment)
        frag1 << "layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputAttachment;\n";
    else
        frag1 << "layout(set = 0, binding = 0) uniform sampler2D image;\n";
    frag1 << "layout(location = 0) out vec4 out_color;\n"
          << "void main()\n"
          << "{\n";
    if (m_parameters.inputAttachment)
        frag1 << "    out_color = subpassLoad(inputAttachment) / 2.0f;\n";
    else
        frag1 << "    out_color = texture(image, texCoord) / 2.0f;\n";
    frag1 << "}\n";

    std::ostringstream frag2;
    frag2 << "#version 450\n"
          << "layout(input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput inputAttachment;\n"
          << "layout(location = 0) out vec4 out_color;\n"
          << "void main()\n"
          << "{\n"
          << "    out_color = vec4(1.0f) - subpassLoad(inputAttachment);\n"
          << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("frag1") << glu::FragmentSource(frag1.str());
    programCollection.glslSources.add("frag2") << glu::FragmentSource(frag2.str());
}

struct MsaaParams
{
    bool sameAttachments;
    uint32_t attachmentCount;
};

class MsaaTestInstance : public TestInstance
{
public:
    MsaaTestInstance(Context &context, const MsaaParams &parameters) : TestInstance(context), m_parameters(parameters)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    const MsaaParams m_parameters;
};

tcu::TestStatus MsaaTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &alloc                = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const VkFormat format        = VK_FORMAT_R8G8B8A8_UNORM;
    const VkExtent3D imageExtent = makeExtent3D(128u, 128u, 1u);
    const auto componentMapping  = makeComponentMappingRGBA();
    const auto subresourceRange  = makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto subresourceLayers = makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

    const uint32_t maxColorAttachments = m_parameters.attachmentCount;
    const uint32_t renderCount         = maxColorAttachments * 2u;

    std::vector<de::MovePtr<ImageWithMemory>> images;
    std::vector<vk::Move<VkImageView>> imageViews;
    std::vector<de::MovePtr<ImageWithMemory>> additionalImages;
    std::vector<de::MovePtr<ImageWithMemory>> resolveImages;
    std::vector<vk::Move<VkImageView>> additionalImageViews;

    for (uint32_t i = 0; i < renderCount; ++i)
    {
        const bool isMSAA                       = (i % 2 == 0);
        const VkSampleCountFlagBits sampleCount = isMSAA ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;

        const VkImageCreateInfo imageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            VK_IMAGE_TYPE_2D,
            format,
            imageExtent,
            1u,
            1u,
            sampleCount,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        images.push_back(de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any)));

        VkImageViewCreateInfo imageViewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            **images[i],
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            componentMapping,
            subresourceRange,
        };

        imageViews.push_back(createImageView(vk, device, &imageViewCreateInfo));

        if (!m_parameters.sameAttachments)
        {
            if (isMSAA)
            {
                additionalImages.push_back(de::MovePtr<ImageWithMemory>(
                    new ImageWithMemory(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any)));
                imageViewCreateInfo.image = **additionalImages.back();
                additionalImageViews.push_back(createImageView(vk, device, &imageViewCreateInfo));
            }
            else
            {
                resolveImages.push_back(de::MovePtr<ImageWithMemory>(
                    new ImageWithMemory(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any)));
            }
        }
    }

    std::vector<vk::Move<vk::VkRenderPass>> renderPasses;
    std::vector<vk::Move<vk::VkFramebuffer>> framebuffers;

    for (uint32_t i = 0; i < renderCount; ++i)
    {
        const bool isMSAA                       = (i % 2 == 0);
        const VkSampleCountFlagBits sampleCount = isMSAA ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;

        const VkAttachmentDescription attachmentDesc = {
            0u,                               // VkAttachmentDescriptionFlags	flags;
            format,                           // VkFormat						format;
            sampleCount,                      // VkSampleCountFlagBits			samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,      // VkAttachmentLoadOp				loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp			storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp				stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp			stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout					initialLayout;
            VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout					finalLayout;
        };

        const VkAttachmentReference attachmentRef = {0, VK_IMAGE_LAYOUT_GENERAL};

        const VkSubpassDescription subpass = {
            0u,                              // VkSubpassDescriptionFlags		flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint				pipelineBindPoint;
            0u,                              // uint32_t						inputAttachmentCount;
            nullptr,                         // const VkAttachmentReference*	pInputAttachments;
            1u,                              // uint32_t						colorAttachmentCount;
            &attachmentRef,                  // const VkAttachmentReference*	pColorAttachments;
            nullptr,                         // const VkAttachmentReference*	pResolveAttachments;
            nullptr,                         // const VkAttachmentReference*	pDepthStencilAttachment;
            0u,                              // uint32_t						preserveAttachmentCount;
            nullptr                          // const uint32_t*					pPreserveAttachments;
        };

        const VkRenderPassCreateInfo renderPassCreateInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType				    sType;
            nullptr,                                   // const void*					    pNext;
            0u,                                        // VkRenderPassCreateFlags		    flags;
            1u,                                        // uint32_t						    attachmentCount;
            &attachmentDesc,                           // const VkAttachmentDescription*    pAttachments;
            1u,                                        // uint32_t						    subpassCount;
            &subpass,                                  // const VkSubpassDescription*	    pSubpasses;
            0u,                                        // uint32_t						    dependencyCount;
            nullptr                                    // const VkSubpassDependency*	    pDependencies;
        };

        renderPasses.push_back(createRenderPass(vk, device, &renderPassCreateInfo));

        const VkFramebufferCreateInfo framebufferCreateInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
            nullptr,                                   // pNext
            0u,                                        // flags
            *renderPasses[i],                          // renderPass
            1u,                                        // attachmentCount
            &*imageViews[i],                           // pAttachments
            imageExtent.width,                         // width
            imageExtent.height,                        // height
            1u                                         // layers
        };

        framebuffers.push_back(createFramebuffer(vk, device, &framebufferCreateInfo));
    }

    vk::Move<VkShaderModule> vertexShader = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    vk::Move<VkShaderModule> fragmentShader1 =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag1"));
    vk::Move<VkShaderModule> fragmentShader2 =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag2"));

    std::vector<VkViewport> viewports{makeViewport(imageExtent)};
    std::vector<VkRect2D> scissors{makeRect2D(imageExtent)};

    VkPipelineVertexInputStateCreateInfo vertexInput      = vk::initVulkanStructure();
    VkPipelineMultisampleStateCreateInfo multisampleState = vk::initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_4_BIT;

    const vk::Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device);
    const vk::Move<VkPipeline> msaaPipeline =
        makeGraphicsPipeline(vk, device, *pipelineLayout, *vertexShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             *fragmentShader1, *renderPasses[0], viewports, scissors,
                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInput, nullptr, &multisampleState);
    const vk::Move<VkPipeline> pipeline = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertexShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragmentShader1,
        *renderPasses[1], viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInput);

    const auto commandPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    for (uint32_t i = 0; i < renderCount; ++i)
    {
        VkClearValue clearValue = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);

        const VkRenderPassBeginInfo renderPassBeginInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType		sType;
            nullptr,                                  // const void*			pNext;
            *renderPasses[i],                         // VkRenderPass		    renderPass;
            *framebuffers[i],                         // VkFramebuffer		    framebuffer;
            scissors[0],                              // VkRect2D			    renderArea;
            1u,                                       // uint32_t			    clearValueCount;
            &clearValue                               // const VkClearValue*	pClearValues;
        };

        vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        if (i % 2 == 0)
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *msaaPipeline);
        else
            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vk.cmdDraw(*cmdBuffer, 3, 1, 0, 0);
        vk.cmdEndRenderPass(*cmdBuffer);
    }

    for (uint32_t i = 0; i < renderCount; ++i)
    {
        const VkImageMemoryBarrier imageBarrier =
            makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, **images[i], subresourceRange);

        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
    }

    if (!m_parameters.sameAttachments)
    {
        for (uint32_t i = 0; i < maxColorAttachments; ++i)
        {
            const VkImageMemoryBarrier imageBarrier = makeImageMemoryBarrier(
                VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, **additionalImages[i], subresourceRange);

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_NONE,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                                  nullptr, 0, nullptr, 1, &imageBarrier);
        }
        for (uint32_t i = 0; i < maxColorAttachments; ++i)
        {
            const VkImageMemoryBarrier imageBarrier =
                makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_GENERAL, **resolveImages[i], subresourceRange);

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                  nullptr, 1, &imageBarrier);
        }
    }

    std::vector<VkAttachmentDescription> attachmentDescriptions(renderCount);
    std::vector<VkAttachmentReference> colorAttachmentReferences(maxColorAttachments);
    std::vector<VkAttachmentReference> resolveAttachmentReferences(maxColorAttachments);

    for (uint32_t i = 0; i < maxColorAttachments; ++i)
    {
        attachmentDescriptions[i * 2] = {0,
                                         VK_FORMAT_R8G8B8A8_UNORM,
                                         VK_SAMPLE_COUNT_4_BIT,
                                         VK_ATTACHMENT_LOAD_OP_CLEAR,
                                         VK_ATTACHMENT_STORE_OP_STORE,
                                         VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                         VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_LAYOUT_GENERAL};

        attachmentDescriptions[i * 2 + 1] = {0,
                                             VK_FORMAT_R8G8B8A8_UNORM,
                                             VK_SAMPLE_COUNT_1_BIT,
                                             VK_ATTACHMENT_LOAD_OP_CLEAR,
                                             VK_ATTACHMENT_STORE_OP_STORE,
                                             VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_LAYOUT_GENERAL};

        colorAttachmentReferences[i]   = {i * 2, VK_IMAGE_LAYOUT_GENERAL};
        resolveAttachmentReferences[i] = {i * 2 + 1, VK_IMAGE_LAYOUT_GENERAL};
    }

    const VkSubpassDescription subpassDescription = {0,
                                                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                     0,
                                                     nullptr,
                                                     maxColorAttachments,
                                                     colorAttachmentReferences.data(),
                                                     resolveAttachmentReferences.data(),
                                                     nullptr,
                                                     0,
                                                     nullptr};

    const VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                                   nullptr,
                                                   0,
                                                   static_cast<uint32_t>(attachmentDescriptions.size()),
                                                   attachmentDescriptions.data(),
                                                   1,
                                                   &subpassDescription,
                                                   0,
                                                   nullptr};

    const vk::Move<vk::VkRenderPass> resolveRenderPass = createRenderPass(vk, device, &renderPassInfo);

    std::vector<VkImageView> imageViewHandles(renderCount);
    for (uint32_t i = 0; i < renderCount; ++i)
    {
        const bool isMSAA = (i % 2 == 0);
        if (isMSAA && !m_parameters.sameAttachments)
            imageViewHandles[i] = *additionalImageViews[i / 2];
        else
            imageViewHandles[i] = *imageViews[i];
    }

    const VkFramebufferCreateInfo resolveFramebufferCreateInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,      // sType
        nullptr,                                        // pNext
        0u,                                             // flags
        *resolveRenderPass,                             // renderPass
        static_cast<uint32_t>(imageViewHandles.size()), // attachmentCount
        imageViewHandles.data(),                        // pAttachments
        imageExtent.width,                              // width
        imageExtent.height,                             // height
        1u                                              // layers
    };

    const vk::Move<VkFramebuffer> resolveFramebuffer = createFramebuffer(vk, device, &resolveFramebufferCreateInfo);

    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(maxColorAttachments, colorBlendAttachment);

    VkPipelineColorBlendStateCreateInfo colorBlendState = vk::initVulkanStructure();
    colorBlendState.attachmentCount                     = maxColorAttachments;
    colorBlendState.pAttachments                        = colorBlendAttachments.data();

    const vk::Move<VkPipeline> resolvePipeline = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertexShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragmentShader2,
        *resolveRenderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInput, nullptr,
        &multisampleState, nullptr, &colorBlendState);

    const VkDeviceSize outputBufferSize = imageExtent.width * imageExtent.height * 4u;
    std::vector<de::MovePtr<BufferWithMemory>> outputBuffers;
    for (uint32_t i = 0; i < renderCount; ++i)
    {
        outputBuffers.push_back(de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, alloc, makeBufferCreateInfo(outputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            MemoryRequirement::HostVisible)));
    }

    const std::vector<VkClearValue> clearValues(renderCount, makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f));

    const VkRenderPassBeginInfo resolveRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                              nullptr,
                                                              *resolveRenderPass,
                                                              *resolveFramebuffer,
                                                              scissors[0],
                                                              renderCount,
                                                              clearValues.data()};

    vk.cmdBeginRenderPass(*cmdBuffer, &resolveRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *resolvePipeline);
    vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

    vk.cmdEndRenderPass(*cmdBuffer);

    const vk::VkBufferImageCopy copyRegion =
        makeBufferImageCopy(makeExtent3D(imageExtent.width, imageExtent.height, 1u), subresourceLayers);

    VkMemoryBarrier postMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          1u, &postMemoryBarrier, 0u, nullptr, 0u, nullptr);
    for (uint32_t i = 0; i < maxColorAttachments; ++i)
        vk.cmdCopyImageToBuffer(*cmdBuffer, **images[i * 2 + 1], VK_IMAGE_LAYOUT_GENERAL, **outputBuffers[i], 1u,
                                &copyRegion);

    if (!m_parameters.sameAttachments)
    {
        for (uint32_t i = 0; i < maxColorAttachments; ++i)
        {
            VkImageResolve resolveRegion;
            resolveRegion.srcSubresource = subresourceLayers;
            resolveRegion.srcOffset      = {0, 0, 0};
            resolveRegion.dstSubresource = subresourceLayers;
            resolveRegion.dstOffset      = {0, 0, 0};
            resolveRegion.extent         = imageExtent;
            vk.cmdResolveImage(*cmdBuffer, **additionalImages[i], VK_IMAGE_LAYOUT_GENERAL, **resolveImages[i],
                               VK_IMAGE_LAYOUT_GENERAL, 1u, &resolveRegion);
        }
        VkMemoryBarrier resolveMemoryBarrier =
            makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u,
                              &resolveMemoryBarrier, 0u, nullptr, 0u, nullptr);
        for (uint32_t i = 0; i < maxColorAttachments; ++i)
            vk.cmdCopyImageToBuffer(*cmdBuffer, **resolveImages[i], VK_IMAGE_LAYOUT_GENERAL,
                                    **outputBuffers[maxColorAttachments + i], 1u, &copyRegion);
    }

    endCommandBuffer(vk, *cmdBuffer);

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer.get(), 0, nullptr};

    vk.queueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vk.queueWaitIdle(queue);

    uint32_t outputCount = maxColorAttachments;
    if (!m_parameters.sameAttachments)
        outputCount *= 2;

    for (uint32_t attachment = 0; attachment < outputCount; ++attachment)
    {
        tcu::ConstPixelBufferAccess resultCopyBuffer =
            tcu::ConstPixelBufferAccess(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), imageExtent.width, imageExtent.height,
                                        1u, (const void *)outputBuffers[attachment]->getAllocation().getHostPtr());
        for (uint32_t i = 0; i < outputBufferSize / 4; ++i)
        {
            const uint8_t *pixel = &(reinterpret_cast<const uint8_t *>(resultCopyBuffer.getDataPtr()))[i * 4];
            int r                = int(pixel[0]);
            int g                = int(pixel[1]);
            // Fragment shader writes uv to color attachment
            // expectedR is u / imageWidth
            // expectedG is v / imageHeight
            int expectedR = (i % imageExtent.width) + 1;
            int expectedG = (i / imageExtent.height) + 1;
            int epsilon   = 2;
            if (std::abs(r - expectedR) > epsilon || std::abs(g - expectedG) > epsilon || pixel[2] != 0 ||
                pixel[3] != 255)
            {
                log << tcu::TestLog::Message << "For attachment " << attachment << " at pixel " << i
                    << " expected value is (" << expectedR << ", " << expectedG << ", 0, 255)"
                    << ", but actual value is (" << r << ", " << g << ", " << std::to_string(pixel[2]) << ", "
                    << std::to_string(pixel[3]) << "), (epsilon is 2) " << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class MsaaCase : public TestCase
{
public:
    MsaaCase(tcu::TestContext &testCtx, const std::string &name, const MsaaParams &parameters)
        : TestCase(testCtx, name)
        , m_parameters(parameters)
    {
    }
    virtual void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

protected:
    const MsaaParams m_parameters;
};

void MsaaCase::checkSupport(Context &context) const
{
    if (context.getDeviceProperties().limits.maxColorAttachments < m_parameters.attachmentCount)
        TCU_THROW(NotSupportedError, "Required maxColorAttachments not supported");
}

TestInstance *MsaaCase::createInstance(Context &context) const
{
    return new MsaaTestInstance(context, m_parameters);
}

void MsaaCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 450\n"
         << "layout (location = 0) out vec2 texCoord;\n"
         << "void main()\n"
         << "{\n"
         << "    texCoord = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);\n"
         << "    gl_Position = vec4(texCoord * 4.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    std::ostringstream frag1;
    frag1 << "#version 450\n"
          << "layout (location = 0) in vec2 texCoord;\n"
          << "layout (location = 0) out vec4 out_color0;\n"
          << "void main()\n"
          << "{\n"
          << "    out_color0 = vec4(texCoord, 0.0f, 1.0f);\n"
          << "}\n";

    std::ostringstream frag2;
    frag2 << "#version 450\n"
          << "layout (location = 0) in vec2 texCoord;\n";
    for (uint32_t i = 0; i < m_parameters.attachmentCount; ++i)
        frag2 << "layout (location = " << i << ") out vec4 out_color" << i << ";\n";
    frag2 << "void main()\n"
          << "{\n";
    for (uint32_t i = 0; i < m_parameters.attachmentCount; ++i)
        frag2 << "    out_color" << i << " = vec4(texCoord, 0.0f, 1.0f);\n";
    frag2 << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("frag1") << glu::FragmentSource(frag1.str());
    programCollection.glslSources.add("frag2") << glu::FragmentSource(frag2.str());
}

} // namespace

tcu::TestCaseGroup *createImageGeneralLayoutTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "general_layout"));

    de::MovePtr<tcu::TestCaseGroup> astcSampleTests(new tcu::TestCaseGroup(testCtx, "astc_sample"));

    struct AtscTests
    {
        AstcTestType type;
        const char *name;
    } astcTests[] = {
        {TEST_TYPE_COPY_INTO_IMAGE, "copy_into_image"},
        {TEST_TYPE_COPY_FROM_IMAGE, "copy_from_image"},
#ifndef CTS_USES_VULKANSC
        {TEST_TYPE_HOST_COPY_INTO_IMAGE, "host_copy_into_image"},
        {TEST_TYPE_HOST_COPY_FROM_IMAGE, "host_copy_from_image"},
#endif
        {TEST_TYPE_SAMPLE_ALIAS, "sample_alias"},
    };

    AstcTestParameters parameters;
    for (const auto &astcTest : astcTests)
    {
        parameters.testType = astcTest.type;
        astcSampleTests->addChild(new AstcSampleCase(testCtx, astcTest.name, parameters));
    }

    miscTests->addChild(astcSampleTests.release());

#ifndef CTS_USES_VULKANSC
    struct StageTest
    {
        VkShaderStageFlagBits stage;
        const char *name;
    } stageTests[]{
        {VK_SHADER_STAGE_COMPUTE_BIT, "compute"},
        {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment"},
    };

    struct AccessTest
    {
        VkAccessFlags2 readAccess;
        VkAccessFlags2 writeAccess;
        const char *name;
    } accessTests[]{
        {VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, "shader_read_write"},
        {VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, "sampled_read_storage_write"},
        {VK_ACCESS_2_SHADER_STORAGE_READ_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, "storage_read_storage_write"},
    };

    de::MovePtr<tcu::TestCaseGroup> memoryBarriersTests(new tcu::TestCaseGroup(testCtx, "memory_barrier"));
    for (const auto &stageTest : stageTests)
    {
        de::MovePtr<tcu::TestCaseGroup> stageTestsGroup(new tcu::TestCaseGroup(testCtx, stageTest.name));
        for (uint32_t readWriteNdx = 0; readWriteNdx < 2; ++readWriteNdx)
        {
            const bool writeFirst = readWriteNdx == 0;
            de::MovePtr<tcu::TestCaseGroup> readWriteTestsGroup(
                new tcu::TestCaseGroup(testCtx, writeFirst ? "write_read" : "read_write"));
            for (const auto accessTest : accessTests)
            {
                MemoryBarrierTestParameters params;
                params.stage       = stageTest.stage;
                params.writeFirst  = writeFirst;
                params.readAccess  = accessTest.readAccess;
                params.writeAccess = accessTest.writeAccess;

                readWriteTestsGroup->addChild(new MemoryBarrierCase(testCtx, accessTest.name, params));
            }
            stageTestsGroup->addChild(readWriteTestsGroup.release());
        }
        memoryBarriersTests->addChild(stageTestsGroup.release());
    }
    miscTests->addChild(memoryBarriersTests.release());
#endif

    struct BarrierTest
    {
        BarrierTestType barrier;
        const char *name;
    } barrierTests[]{
        {BARRIER_TEST_EXECUTION, "execution"},
        {BARRIER_TEST_MEMORY, "memory"},
        {BARRIER_TEST_IMAGE, "image"},
    };

    de::MovePtr<tcu::TestCaseGroup> inputAttachmentTests(new tcu::TestCaseGroup(testCtx, "input_attachment"));
    for (uint32_t attachmentTypeNdx = 0u; attachmentTypeNdx < 2u; ++attachmentTypeNdx)
    {
        bool inputAttachment           = attachmentTypeNdx == 0u;
        const char *attachmentTestName = inputAttachment ? "input_attachment" : "sampled";

        de::MovePtr<tcu::TestCaseGroup> attachmentTests(new tcu::TestCaseGroup(testCtx, attachmentTestName));

        for (const auto barrierTest : barrierTests)
        {
            de::MovePtr<tcu::TestCaseGroup> barrierTestsGroup(new tcu::TestCaseGroup(testCtx, barrierTest.name));
            for (uint32_t renderPassTypeNdx = 0u; renderPassTypeNdx < 2u; ++renderPassTypeNdx)
            {
                bool dynamicRendering     = renderPassTypeNdx == 1u;
                const char *renderingName = dynamicRendering ? "dynamic_rendering" : "render_pass";

                InputAttachmentParams params;
                params.inputAttachment  = inputAttachment;
                params.barrierTest      = barrierTest.barrier;
                params.dynamicRendering = dynamicRendering;
                barrierTestsGroup->addChild(new InputAttachmentCase(testCtx, renderingName, params));
            }
            attachmentTests->addChild(barrierTestsGroup.release());
        }
        inputAttachmentTests->addChild(attachmentTests.release());
    }
    miscTests->addChild(inputAttachmentTests.release());

    de::MovePtr<tcu::TestCaseGroup> msaaTests(new tcu::TestCaseGroup(testCtx, "msaa"));
    for (uint32_t attachments = 0; attachments < 2; ++attachments)
    {
        bool sameAttachments        = attachments == 0;
        const char *attachmentsName = sameAttachments ? "same" : "different";
        de::MovePtr<tcu::TestCaseGroup> attachmentTests(new tcu::TestCaseGroup(testCtx, attachmentsName));
        for (uint32_t attachmentCount = 4; attachmentCount <= 16; attachmentCount *= 2)
        {
            MsaaParams params;
            params.sameAttachments = sameAttachments;
            params.attachmentCount = attachmentCount;
            attachmentTests->addChild(new MsaaCase(testCtx, std::to_string(attachmentCount).c_str(), params));
        }
        msaaTests->addChild(attachmentTests.release());
    }
    miscTests->addChild(msaaTests.release());

    return miscTests.release();
}

} // namespace image
} // namespace vkt
