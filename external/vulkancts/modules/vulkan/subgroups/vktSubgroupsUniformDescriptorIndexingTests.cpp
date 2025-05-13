/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
* \brief Subgroups uniform descriptor indexing tests
*//*--------------------------------------------------------------------*/

#include "vktSubgroupsUniformDescriptorIndexingTests.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "deSTLUtil.hpp"
#include "tcuStringTemplate.hpp"

#include <string_view>
#include <array>
#include <map>

namespace
{
using namespace vk;
using namespace vkt;
using namespace tcu;

class UniformDescriptorIndexingTestCaseTestInstance : public TestInstance
{
public:
    UniformDescriptorIndexingTestCaseTestInstance(Context &context, VkDescriptorType descriptorType);
    ~UniformDescriptorIndexingTestCaseTestInstance() = default;

    tcu::TestStatus iterate(void) override;

protected:
    Move<VkRenderPass> setupRenderPass(uint32_t inputAttachmentCount) const;

    void setupImages(uint32_t imagesCount, uint32_t imageSize, VkImageUsageFlags usage,
                     uint32_t descriptorImageInfosOffset = 0);
    void setupStorageBuffers(uint32_t buffersCount, const std::vector<float> &clearColors, VkBufferUsageFlags usage);
    void setupUniformBuffers(uint32_t descriptorCount, const std::vector<float> &clearColors);
    void setupTexelBuffer(uint32_t descriptorCount, const std::vector<float> &clearColors, VkBufferUsageFlags usage);

    VkImageCreateInfo getImageCreateInfo(VkFormat format, VkExtent3D extent, VkImageUsageFlags usage) const;

    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
    using BufferWithMemoryVec = std::vector<BufferWithMemoryPtr>;
    using BufferViewPtr       = Move<VkBufferView>;
    using BufferViewVec       = std::vector<BufferViewPtr>;
    using ImageWithMemoryPtr  = std::unique_ptr<ImageWithMemory>;
    using ImageViewPtr        = Move<VkImageView>;
    using ImageWithMemoryVec  = std::vector<ImageWithMemoryPtr>;
    using ImageViewVec        = std::vector<ImageViewPtr>;
    using SamplerPtr          = Move<VkSampler>;
    using SamplerVec          = std::vector<SamplerPtr>;

protected:
    const uint32_t m_imageSize   = 32;
    const VkFormat m_imageFormat = VK_FORMAT_R8_UNORM;
    const VkImageSubresourceRange m_imageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    VkDescriptorType m_descriptorType;

    ImageWithMemoryVec m_imagesWithMemoryVec;
    ImageViewVec m_imagesViewVec;
    BufferWithMemoryVec m_buffersWithMemoryVec;
    BufferViewVec m_buffersViewVec;
    SamplerVec m_samplersVec;

    std::vector<VkImageView> m_framebufferImageViews;
    std::vector<VkDescriptorImageInfo> m_imageInfos;
    std::vector<VkDescriptorBufferInfo> m_bufferInfos;
    std::vector<VkBufferView> m_bufferViewsRaw;
};

UniformDescriptorIndexingTestCaseTestInstance::UniformDescriptorIndexingTestCaseTestInstance(
    Context &context, VkDescriptorType descriptorType)
    : TestInstance(context)
    , m_descriptorType(descriptorType)
{
}

tcu::TestStatus UniformDescriptorIndexingTestCaseTestInstance::iterate(void)
{
    struct TestConfig
    {
        uint32_t descriptorCount; // number of descriptor of tested type
        uint32_t imagesCount;     // number of images required by the test (not counting color attachment)
        uint32_t buffersCount;    // number of buffers required by the test
        uint32_t samplersCount;   // number of samplers required by the test
        uint32_t minGroupsCount;  // minimal allowed number of unique colors found in output image

        TestConfig(uint32_t descriptors, uint32_t images, uint32_t buffers, uint32_t samplers, uint32_t minExpected)
            : descriptorCount(descriptors)
            , imagesCount(images)
            , buffersCount(buffers)
            , samplersCount(samplers)
            , minGroupsCount(minExpected)
        {
        }
    };

    // note: minGroupsCount was arbitrarily selected basing on results returned by implementations;
    //       there is no obvious verification method for those tests and number of returned groups
    //       of fragments depends from used image size and from noize used in shader
    static const std::map<VkDescriptorType, TestConfig> configurationMap{
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, TestConfig(4, 0, 4, 0, 4)},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, TestConfig(12, 0, 1, 0, 9)},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, TestConfig(16, 0, 1, 0, 5)},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, TestConfig(16, 0, 1, 0, 5)},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, TestConfig(4, 4, 0, 0, 4)},
        {VK_DESCRIPTOR_TYPE_SAMPLER, TestConfig(4, 1, 0, 4, 2)},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, TestConfig(16, 16, 0, 1, 10)},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TestConfig(4, 4, 0, 4, 4)},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, TestConfig(4, 4, 0, 0, 4)},
    };

    auto [descriptorCount, imagesCount, buffersCount, samplersCount, minGroupsCount] =
        configurationMap.at(m_descriptorType);

    // create vector of required number of clear values
    std::vector<float> clearColors(descriptorCount + 1, 0.0f);
    std::vector<VkClearValue> clearValues(descriptorCount + 1);
    for (uint32_t i = 1; i < (uint32_t)clearColors.size(); ++i)
    {
        clearColors[i] = float(i) / float(descriptorCount + 1);
        clearValues[i] = makeClearValueColor(tcu::Vec4(clearColors[i]));
    }

    const VkExtent3D extent   = makeExtent3D(m_imageSize, m_imageSize, 1u);
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    const std::vector<VkViewport> viewports{makeViewport(extent)};
    const std::vector<VkRect2D> scissors{makeRect2D(extent)};

    // create image that will be used as color attachment to which we will write test result
    const auto srl(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
    const auto copyRegion(makeBufferImageCopy(extent, srl));
    const VkImageUsageFlags imageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto imageLayout((m_descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ?
                               VK_IMAGE_LAYOUT_GENERAL :
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ImageWithBuffer outImageWithBuffer(vk, device, allocator, extent, m_imageFormat, imageUsage, VK_IMAGE_TYPE_2D,
                                       m_imageSubresourceRange);
    auto outImageView(makeImageView(vk, device, outImageWithBuffer.getImage(), VK_IMAGE_VIEW_TYPE_2D, m_imageFormat,
                                    m_imageSubresourceRange));

    m_framebufferImageViews.push_back(*outImageView);

    if (imagesCount)
    {
        m_imagesWithMemoryVec.resize(imagesCount);
        m_imagesViewVec.resize(imagesCount);

        // include number of required samplers when allocating DescriptorImageInfo (when there are samplers there are always also images);
        // but dont include samplersCount when combined image sampler case is executed as images and samplers share DescriptorImageInfos
        uint32_t imageInfoCount =
            imagesCount + samplersCount * (m_descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        m_imageInfos = std::vector<VkDescriptorImageInfo>(
            imageInfoCount, makeDescriptorImageInfo(VK_NULL_HANDLE, VK_NULL_HANDLE, imageLayout));

        if (m_descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
            setupImages(imagesCount, 3, VK_IMAGE_USAGE_SAMPLED_BIT, samplersCount);
        else if (m_descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
            setupImages(imagesCount, 3, VK_IMAGE_USAGE_SAMPLED_BIT);
        else if (m_descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            setupImages(imagesCount, 3, VK_IMAGE_USAGE_SAMPLED_BIT);
        else if (m_descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            setupImages(imagesCount, 3, VK_IMAGE_USAGE_STORAGE_BIT);
        else if (m_descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
            setupImages(imagesCount, m_imageSize, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    }

    if (buffersCount)
    {
        m_buffersWithMemoryVec.resize(buffersCount);
        m_bufferInfos = std::vector<VkDescriptorBufferInfo>(descriptorCount,
                                                            makeDescriptorBufferInfo(VK_NULL_HANDLE, 0, VK_WHOLE_SIZE));

        if (m_descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            setupUniformBuffers(descriptorCount, clearColors);
        else if (m_descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            setupStorageBuffers(buffersCount, clearColors, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        else if (m_descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
            setupTexelBuffer(descriptorCount, clearColors, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
        else if (m_descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
            setupTexelBuffer(descriptorCount, clearColors, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
    }

    if (samplersCount)
    {
        m_samplersVec.resize(samplersCount);

        // ofset imageInfos only for sampled image case
        uint32_t descriptorImageInfosOffset = (m_descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) * descriptorCount;

        VkSamplerCreateInfo samplerCreateInfo = initVulkanStructure();
        samplerCreateInfo.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VkSamplerAddressMode addressModes[4] = {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER};
        for (uint32_t i = 0; i < samplersCount; ++i)
        {
            samplerCreateInfo.addressModeU                       = addressModes[i % 4];
            m_samplersVec[i]                                     = createSampler(vk, device, &samplerCreateInfo);
            m_imageInfos[descriptorImageInfosOffset + i].sampler = *m_samplersVec[i];
        }
    }

    DescriptorPoolBuilder descriptorPoolBuilder;
    descriptorPoolBuilder.addType(m_descriptorType, descriptorCount);
    DescriptorSetLayoutBuilder descriptorSetLayoutBuilder;
    descriptorSetLayoutBuilder.addBinding(m_descriptorType, descriptorCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

    // some cases require additional descriptor types that are neede to be able to check currently tested ones
    if (m_descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
    {
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1);
        descriptorSetLayoutBuilder.addIndexedBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                                     4, nullptr);
    }
    else if (m_descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
    {
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER, 1);
        descriptorSetLayoutBuilder.addIndexedBinding(VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                                     descriptorCount, nullptr);
    }

    // create descriptors
    const auto descriptorPool(
        descriptorPoolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const auto descriptorSetLayout(descriptorSetLayoutBuilder.build(vk, device));
    const auto descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
    descriptorSetUpdateBuilder.write(*descriptorSet, 0, 0, descriptorCount, m_descriptorType,
                                     de::dataOrNull(m_imageInfos), de::dataOrNull(m_bufferInfos),
                                     de::dataOrNull(m_bufferViewsRaw));

    if (m_descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
        descriptorSetUpdateBuilder.write(*descriptorSet, descriptorCount, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                         &m_imageInfos[descriptorCount], 0, 0);
    else if (m_descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
        descriptorSetUpdateBuilder.write(*descriptorSet, descriptorCount, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER,
                                         &m_imageInfos[descriptorCount], 0, 0);

    descriptorSetUpdateBuilder.update(vk, device);

    const auto pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    auto &bc(m_context.getBinaryCollection());
    const auto vertModule(createShaderModule(vk, device, bc.get("vert")));
    const auto fragModule(createShaderModule(vk, device, bc.get("frag")));
    const uint32_t inputsCount((m_descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) * descriptorCount);
    const auto renderPass(setupRenderPass(inputsCount));
    const auto framebuffer(makeFramebuffer(vk, device, *renderPass, (uint32_t)m_framebufferImageViews.size(),
                                           m_framebufferImageViews.data(), m_imageSize, m_imageSize));
    const auto pipeline(makeGraphicsPipeline(vk, device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                             VK_NULL_HANDLE, *fragModule, *renderPass, viewports, scissors,
                                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0, &vertexInputState));

    // prepare barriers needed by all test variants
    const auto beforeClearBarrier(makeImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_NULL_HANDLE,
                                                         m_imageSubresourceRange));
    const auto afterClearBarrier(makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, VK_NULL_HANDLE, m_imageSubresourceRange));
    const auto beforeCopyBarrier(makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outImageWithBuffer.getImage(), m_imageSubresourceRange));
    const auto bufferAccessMask((m_descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? VK_ACCESS_UNIFORM_READ_BIT :
                                                                                          VK_ACCESS_SHADER_READ_BIT);
    const auto beforeDrawBarrier(
        makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, bufferAccessMask, VK_NULL_HANDLE, 0, VK_WHOLE_SIZE));

    const auto queueFamilyIndex(m_context.getUniversalQueueFamilyIndex());
    const auto cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    if (imagesCount)
    {
        std::vector<VkImageMemoryBarrier> beforeClearBarriers(imagesCount, beforeClearBarrier);
        std::vector<VkImageMemoryBarrier> afterClearBarriers(imagesCount, afterClearBarrier);

        for (uint32_t i = 0; i < imagesCount; ++i)
        {
            beforeClearBarriers[i].image = **m_imagesWithMemoryVec[i];
            afterClearBarriers[i].image  = **m_imagesWithMemoryVec[i];
        }

        vk.cmdPipelineBarrier(*cmdBuffer, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0, 0, 0, 0, imagesCount,
                              beforeClearBarriers.data());
        for (uint32_t i = 0; i < imagesCount; ++i)
            vk.cmdClearColorImage(*cmdBuffer, **m_imagesWithMemoryVec[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  &clearValues[i + 1].color, 1, &m_imageSubresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0u, 0, 0,
                              0, 0, imagesCount, afterClearBarriers.data());
    }

    if (buffersCount)
    {
        std::vector<VkBufferMemoryBarrier> beforeDrawBarriers(buffersCount, beforeDrawBarrier);
        for (uint32_t i = 0; i < buffersCount; ++i)
            beforeDrawBarriers[i].buffer = **m_buffersWithMemoryVec[i];

        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                              0, buffersCount, beforeDrawBarriers.data(), 0, 0);
    }

    // draw single triangle big enough to cover whole framebuffer
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissors[0], 1, clearValues.data());
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             0);
    vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0, 0, 0, 0, 1u, &beforeCopyBarrier);
    vk.cmdCopyImageToBuffer(*cmdBuffer, outImageWithBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            outImageWithBuffer.getBuffer(), 1u, &copyRegion);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    const auto &outBufferAllocation = outImageWithBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, outBufferAllocation);

    // count number of fragments that have same values
    uint8_t *outBufferPtr = static_cast<uint8_t *>(outBufferAllocation.getHostPtr());
    std::map<uint32_t, uint32_t> resultMap;
    for (uint32_t i = 0; i < m_imageSize * m_imageSize; ++i)
    {
        uint32_t value   = outBufferPtr[i];
        resultMap[value] = resultMap[value] + 1;
    }

    // make sure that none of fragments has background color and there is expected number of color groups
    if ((resultMap.count(0) == 0) && (resultMap.size() >= minGroupsCount) && (resultMap.size() <= descriptorCount))
        return tcu::TestStatus::pass("Pass");

    tcu::PixelBufferAccess resultAccess(mapVkFormat(m_imageFormat), m_imageSize, m_imageSize, 1, outBufferPtr);
    m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result", "")
                                        << tcu::TestLog::Image("Output", "", resultAccess) << tcu::TestLog::EndImageSet;

    if (resultMap.count(0))
        return tcu::TestStatus::fail(std::to_string(resultMap[0]) + " fragments have background color");

    return tcu::TestStatus::fail(std::to_string(resultMap.size()) + " groups, expected <" +
                                 std::to_string(minGroupsCount) + ", " + std::to_string(descriptorCount) + ">");
}

Move<VkRenderPass> UniformDescriptorIndexingTestCaseTestInstance::setupRenderPass(uint32_t inputAttachmentCount) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const VkAttachmentDescription defaultAttachmentDescription{
        0,                                       // VkAttachmentDescriptionFlags flags
        m_imageFormat,                           // VkFormat                     format
        VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits        samples
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp           loadOp
        VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp          storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp           stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp          stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                initialLayout
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                finalLayout
    };

    std::vector<VkAttachmentDescription> attachmentDescriptions(1 + inputAttachmentCount, defaultAttachmentDescription);
    const VkAttachmentReference colorAttachmentRef{0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    std::vector<VkAttachmentReference> inputAttachmentsRefs(4, {0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

    for (uint32_t i = 0; i < inputAttachmentCount; i++)
    {
        auto &inputAttachment              = attachmentDescriptions[i + 1];
        inputAttachment.loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD;
        inputAttachment.initialLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        inputAttachment.finalLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        inputAttachmentsRefs[i].attachment = i + 1;
    };

    const VkSubpassDescription subpassDescription{
        (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags      flags
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint            pipelineBindPoint
        inputAttachmentCount,            // uint32_t                       inputAttachmentCount
        inputAttachmentsRefs.data(),     // const VkAttachmentReference*   pInputAttachments
        1u,                              // uint32_t                       colorAttachmentCount
        &colorAttachmentRef,             // const VkAttachmentReference*   pColorAttachments
        nullptr,                         // const VkAttachmentReference*   pResolveAttachments
        nullptr,                         // const VkAttachmentReference*   pDepthStencilAttachment
        0u,                              // uint32_t                       preserveAttachmentCount
        nullptr                          // const uint32_t*                pPreserveAttachments
    };

    const VkRenderPassCreateInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                sType
        nullptr,                                   // const void*                    pNext
        (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags        flags
        (uint32_t)attachmentDescriptions.size(),   // uint32_t                       attachmentCount
        attachmentDescriptions.data(),             // const VkAttachmentDescription* pAttachments
        1u,                                        // uint32_t                       subpassCount
        &subpassDescription,                       // const VkSubpassDescription*    pSubpasses
        0u,                                        // uint32_t                       dependencyCount
        nullptr                                    // const VkSubpassDependency*     pDependencies
    };

    return createRenderPass(vk, device, &renderPassInfo, nullptr);
}

void UniformDescriptorIndexingTestCaseTestInstance::setupImages(uint32_t imagesCount, uint32_t imageSize,
                                                                VkImageUsageFlags usage,
                                                                uint32_t descriptorImageInfosOffset)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    VkImageUsageFlags finalUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage;
    const bool isAttachment      = usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    const VkExtent3D extent      = makeExtent3D(imageSize, imageSize, 1u);
    const VkImageCreateInfo imageCreateInfo(getImageCreateInfo(m_imageFormat, extent, finalUsage));

    // create additional images that will be used as input attachments
    if (isAttachment)
        m_framebufferImageViews.resize(imagesCount + 1);

    for (uint32_t i = 0; i < imagesCount; ++i)
    {
        m_imagesWithMemoryVec[i] =
            ImageWithMemoryPtr(new ImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any));
        m_imagesViewVec[i] = makeImageView(vk, device, **m_imagesWithMemoryVec[i], VK_IMAGE_VIEW_TYPE_2D, m_imageFormat,
                                           m_imageSubresourceRange);
        m_imageInfos[descriptorImageInfosOffset + i].imageView = *m_imagesViewVec[i];

        if (isAttachment)
            m_framebufferImageViews[i + 1] = *m_imagesViewVec[i]; // first view is output color attachment
    }
}

void UniformDescriptorIndexingTestCaseTestInstance::setupStorageBuffers(uint32_t buffersCount,
                                                                        const std::vector<float> &clearColors,
                                                                        VkBufferUsageFlags usage)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    VkDeviceSize bufferValuesCount(m_imageSize * m_imageSize);
    auto bufferCreateInfo(
        makeBufferCreateInfo(bufferValuesCount * sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage));

    for (uint32_t i = 0; i < buffersCount; ++i)
    {
        m_buffersWithMemoryVec[i] = BufferWithMemoryPtr(
            new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
        m_bufferInfos[i]    = {**m_buffersWithMemoryVec[i], 0, VK_WHOLE_SIZE};
        auto *bufferHostPtr = static_cast<float *>(m_buffersWithMemoryVec[i]->getAllocation().getHostPtr());

        std::fill(bufferHostPtr, bufferHostPtr + bufferValuesCount,
                  clearColors[1 + i % uint32_t(clearColors.size() - 1)]);
        flushAlloc(vk, device, m_buffersWithMemoryVec[i]->getAllocation());
    }
}

void UniformDescriptorIndexingTestCaseTestInstance::setupUniformBuffers(uint32_t descriptorCount,
                                                                        const std::vector<float> &clearColors)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    const VkDeviceSize alignment(m_context.getDeviceProperties().limits.minUniformBufferOffsetAlignment);
    const VkDeviceSize roundedSize(deAlign64(sizeof(float), static_cast<uint32_t>(alignment)));

    VkBufferCreateInfo bufferCreateInfo(makeBufferCreateInfo(
        descriptorCount * roundedSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
    m_buffersWithMemoryVec[0] = BufferWithMemoryPtr(
        new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

    auto *bufferHostPtr = static_cast<uint8_t *>(m_buffersWithMemoryVec[0]->getAllocation().getHostPtr());
    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        auto &info  = m_bufferInfos[i];
        info.buffer = **m_buffersWithMemoryVec[0];
        info.offset = i * roundedSize;
        info.range  = roundedSize;

        float data = clearColors[i + 1];
        deMemcpy(bufferHostPtr + info.offset, &data, sizeof(float));
    }

    flushAlloc(vk, device, m_buffersWithMemoryVec[0]->getAllocation());
}

void UniformDescriptorIndexingTestCaseTestInstance::setupTexelBuffer(uint32_t descriptorCount,
                                                                     const std::vector<float> &clearColors,
                                                                     VkBufferUsageFlags usage)
{
    const InstanceInterface &vki = m_context.getInstanceInterface();
    const DeviceInterface &vk    = m_context.getDeviceInterface();
    const VkDevice device        = m_context.getDevice();
    Allocator &allocator         = m_context.getDefaultAllocator();

    auto alignment = m_context.getDeviceProperties().limits.minTexelBufferOffsetAlignment;
    if (m_context.getTexelBufferAlignmentFeaturesEXT().texelBufferAlignment)
    {
        VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT alignmentProperties = initVulkanStructure();
        VkPhysicalDeviceProperties2 properties2 = initVulkanStructure(&alignmentProperties);
        vki.getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties2);

        bool isUniformTexelBuffer     = (usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
        VkBool32 singleTexelAlignment = isUniformTexelBuffer ?
                                            alignmentProperties.uniformTexelBufferOffsetSingleTexelAlignment :
                                            alignmentProperties.storageTexelBufferOffsetSingleTexelAlignment;
        VkDeviceSize align = isUniformTexelBuffer ? alignmentProperties.uniformTexelBufferOffsetAlignmentBytes :
                                                    alignmentProperties.storageTexelBufferOffsetAlignmentBytes;
        alignment          = align;
        if (singleTexelAlignment)
            alignment = de::min(4u, (uint32_t)align);
    }

    const uint32_t viewItems(2);
    const VkDeviceSize usedViewSize(viewItems * sizeof(float));
    const VkDeviceSize requiredAlignment(alignment - usedViewSize % alignment);
    const VkDeviceSize alignedViewSize(usedViewSize + requiredAlignment);
    const VkDeviceSize bufferSize(descriptorCount * alignedViewSize);
    auto bufferCreateInfo(makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage));

    m_buffersWithMemoryVec[0] = BufferWithMemoryPtr(
        new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
    m_bufferInfos[0]    = {**m_buffersWithMemoryVec[0], 0, VK_WHOLE_SIZE};
    auto &allocation    = m_buffersWithMemoryVec[0]->getAllocation();
    auto *bufferHostPtr = static_cast<char *>(allocation.getHostPtr());

    VkBufferViewCreateInfo bufferViewCreateInfo = initVulkanStructure();
    bufferViewCreateInfo.buffer                 = **m_buffersWithMemoryVec[0];
    bufferViewCreateInfo.format                 = m_imageFormat;
    bufferViewCreateInfo.range                  = usedViewSize;

    m_bufferViewsRaw.resize(descriptorCount);
    m_buffersViewVec.resize(descriptorCount);
    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        bufferViewCreateInfo.offset = i * alignedViewSize;
        m_buffersViewVec[i]         = createBufferView(vk, device, &bufferViewCreateInfo);
        m_bufferViewsRaw[i]         = *m_buffersViewVec[i];

        float *viewStart = reinterpret_cast<float *>(bufferHostPtr + bufferViewCreateInfo.offset);
        std::fill(viewStart, viewStart + viewItems, clearColors[1 + i % uint32_t(clearColors.size() - 1)]);
    }

    flushAlloc(vk, device, allocation);
}

VkImageCreateInfo UniformDescriptorIndexingTestCaseTestInstance::getImageCreateInfo(VkFormat format, VkExtent3D extent,
                                                                                    VkImageUsageFlags usage) const
{
    return {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType       sType
        nullptr,                             // const void*           pNext
        0u,                                  // VkImageCreateFlags    flags
        VK_IMAGE_TYPE_2D,                    // VkImageType           imageType
        format,                              // VkFormat              format
        extent,                              // VkExtent3D            extent
        1u,                                  // uint32_t              mipLevels
        1u,                                  // uint32_t              arrayLayers
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling         tiling
        usage,                               // VkImageUsageFlags     usage
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode         sharingMode
        0u,                                  // uint32_t              queueFamilyIndexCount
        nullptr,                             // const uint32_t*       pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout         initialLayout
    };
};

class UniformDescriptorIndexingTestCase : public vkt::TestCase
{
public:
    UniformDescriptorIndexingTestCase(TestContext &testCtx, const std::string &name, VkDescriptorType descriptorType);
    virtual ~UniformDescriptorIndexingTestCase(void) = default;
    virtual void checkSupport(Context &context) const override;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override;

private:
    VkDescriptorType m_descriptorType;
};

UniformDescriptorIndexingTestCase::UniformDescriptorIndexingTestCase(TestContext &testCtx, const std::string &name,
                                                                     VkDescriptorType descriptorType)
    : vkt::TestCase(testCtx, name)
    , m_descriptorType(descriptorType)
{
}

void UniformDescriptorIndexingTestCase::checkSupport(Context &context) const
{
    const auto &sProperties = context.getSubgroupProperties();
    if (sProperties.subgroupSize == 1)
        TCU_THROW(NotSupportedError, "subgroupSize is 1");
    if ((sProperties.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
        TCU_THROW(NotSupportedError, "fragment stage doesn't support subgroup operations");

    const auto &diFeatures = context.getDescriptorIndexingFeatures();
    if (!diFeatures.runtimeDescriptorArray)
        TCU_THROW(NotSupportedError, "runtimeDescriptorArray not supported");

    switch (m_descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        if (!diFeatures.shaderStorageBufferArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError,
                      "Non-uniform indexing over storage buffer descriptor arrays is not supported.");
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        if (!diFeatures.shaderUniformBufferArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError, "Non-uniform indexing for uniform buffer descriptor arrays is not supported.");
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        if (!diFeatures.shaderStorageTexelBufferArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError,
                      "Non-uniform indexing for storage texel buffer descriptor arrays is not supported.");
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        if (!diFeatures.shaderUniformTexelBufferArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError,
                      "Non-uniform indexing for uniform texel buffer descriptor arrays is not supported.");
        break;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        if (!diFeatures.shaderInputAttachmentArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError,
                      "Non-uniform indexing over input attachment descriptor arrays is not supported.");
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        if (!diFeatures.shaderSampledImageArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError, "Non-uniform indexing over sampler descriptor arrays is not supported.");
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        if (!diFeatures.shaderSampledImageArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError, "Non-uniform indexing over sampled image descriptor arrays is not supported.");
        break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        if (!diFeatures.shaderSampledImageArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError,
                      "Non-uniform indexing over combined image sampler descriptor arrays is not supported.");
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        if (!diFeatures.shaderStorageImageArrayNonUniformIndexing)
            TCU_THROW(NotSupportedError, "Non-uniform indexing over storage image descriptor arrays is not supported.");
        break;
    default:
        DE_FATAL("Unknown Descriptor Type");
        break;
    }
}

void UniformDescriptorIndexingTestCase::initPrograms(SourceCollections &programCollection) const
{
    const SpirvVersion spirvVersion(vk::SPIRV_VERSION_1_3);
    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, spirvVersion, 0u);

    struct ShaderConfiguration
    {
        std::map<std::string, std::string> specializationMap;

        ShaderConfiguration(const std::string &declaration, const std::string &count, const std::string &accessMethod,
                            const std::string &extraDeclarations, const std::string &extraLayout)
            : specializationMap{{"declaration", declaration},
                                {"count", count},
                                {"accessMethod", accessMethod},
                                {"extraDeclarations", extraDeclarations},
                                {"extraLayout", extraLayout}}
        {
        }
    };

    const std::map<VkDescriptorType, ShaderConfiguration> shaderPartsMap{
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ShaderConfiguration("buffer Data { float c; }", "4", "data[i].c", "", "")},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         ShaderConfiguration("uniform Data { float c; }", "12", "data[i].c", "", "")},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
         ShaderConfiguration("uniform imageBuffer", "16", "imageLoad(data[i], 0).r", "", "r8,")},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
         ShaderConfiguration("uniform samplerBuffer", "16", "texelFetch(data[i], 0).r", "", "")},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
         ShaderConfiguration("uniform subpassInput", "4", "subpassLoad(data[i]).r", "", "input_attachment_index=0,")},
        {VK_DESCRIPTOR_TYPE_SAMPLER,
         ShaderConfiguration("uniform sampler", "4", "texture(sampler2D(tex, data[i]), vec2(1.5)).r",
                             "layout(binding = 4) uniform texture2D tex;\n", "")},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         ShaderConfiguration("uniform texture2D", "16", "texture(sampler2D(data[i], samp), vec2(0.5)).r",
                             "layout(binding = 16) uniform sampler samp;\n", "")},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         ShaderConfiguration("uniform sampler2D", "4", "texture(data[i], uvec2(0.5)).r", "", "")},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         ShaderConfiguration("uniform image2D", "4", "imageLoad(data[i], ivec2(0)).r", "", "r8,")},
    };

    std::string fragTemplate =
        "#version 450\n"
        "#extension GL_KHR_shader_subgroup_ballot: enable\n"
        "#extension GL_EXT_nonuniform_qualifier: enable\n"
        "layout(location = 0) out highp float fragColor;\n"
        "layout(${extraLayout}binding = 0) ${declaration} data[];\n"
        "${extraDeclarations}"
        "void main (void)\n"
        "{\n"
        // use cosine to generate pseudo-random value for each fragment; coordinates of each fragment are used
        // to calculate angle for cosine; both coordinates are multiplied by big numbers in order to make small
        // change in coordinates produce completely different cosine value; amplitude is also multiplied by big
        // number before calculating fraction in order to reduce any visible pattern for selected image size;
        // there was no reason why those numbers were hosen and they could be replaced with any other big
        // numbers to get different noize
        "  const float noize = fract(9876.54 * cos(654.3267 * gl_FragCoord.x + 1234.5678 * gl_FragCoord.y));\n"
        // pseudo-randomly select material for fragment
        "  const uint materialIndex = uint(noize * ${count});\n"
        "  fragColor.r = 0.0;\n"
        // do a "peeling loop" - iterate over each unique index used such that the accessed resource
        // is always uniform within the subgroup; and in a way that it's not uniform across the draw
        "  for(;;)\n"
        "  {\n"
        "    uint i = subgroupBroadcastFirst(materialIndex);\n"
        "    if(i == materialIndex)\n"
        "    {\n"
        //     we don't use nonuniformEXT(i) - that is the purpose of tests in this file
        "      fragColor.r = ${accessMethod};\n"
        "      break;\n"
        "    }\n"
        "  }\n"
        "}\n";

    // draw single triangle big enough to cover whole framebuffer
    programCollection.glslSources.add("vert")
        << glu::VertexSource("#version 450\n"
                             "void main (void)\n"
                             "{\n"
                             "  const float x = -1.0 + 4.0 * ((gl_VertexIndex & 2)>>1);\n"
                             "  const float y = -1.0 + 4.0 * (gl_VertexIndex % 2);\n"
                             "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
                             "}\n");

    auto &specializationMap = shaderPartsMap.at(m_descriptorType).specializationMap;
    programCollection.glslSources.add("frag")
        << glu::FragmentSource(StringTemplate(fragTemplate).specialize(specializationMap)) << buildOptions;
}

TestInstance *UniformDescriptorIndexingTestCase::createInstance(Context &context) const
{
    return new UniformDescriptorIndexingTestCaseTestInstance(context, m_descriptorType);
}

} // unnamed namespace

namespace vkt::subgroups
{

tcu::TestCaseGroup *createSubgroupsUniformDescriptorIndexingTests(tcu::TestContext &testCtx)
{
    std::pair<const char *, VkDescriptorType> caseList[]{
        {
            "storage_buffer",
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {
            "storage_texel_buffer",
            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        },
        {
            "uniform_texel_buffer",
            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        },
        {
            "storage_image",
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        },
        {
            "sampler",
            VK_DESCRIPTOR_TYPE_SAMPLER,
        },
        {
            "sampled_image",
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {
            "combined_image_sampler",
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        },
        {
            "uniform_buffer",
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        {
            "input_attachment",
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        },
    };

    de::MovePtr<TestCaseGroup> group(new TestCaseGroup(testCtx, "uniform_descriptor_indexing"));
    for (auto &[name, type] : caseList)
        group->addChild(new UniformDescriptorIndexingTestCase(testCtx, name, type));

    return group.release();
}

} // namespace vkt::subgroups
