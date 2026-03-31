/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Google LLC.
 * Copyright (c) 2023 LunarG, Inc.
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
 * \brief Test robustness with pipeline cache
 *//*--------------------------------------------------------------------*/

#include "vktPipelineRobustnessCacheTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkComputePipelineConstructionUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"

namespace vkt
{
namespace pipeline
{

enum RobustnessBehaviour
{
    ROBUSTNESS   = 0,
    ROBUSTNESS_2 = 1,
};

enum RobustnessType
{
    STORAGE = 0,
    UNIFORM,
    VERTEX_INPUT,
    IMAGE,
};

namespace
{

std::unique_ptr<vk::BufferWithMemory> makeBufferForImage(const vk::DeviceInterface &vk, const vk::VkDevice device,
                                                         vk::Allocator &allocator, vk::VkFormat imageFormat,
                                                         vk::VkExtent2D imageExtent)
{
    const auto tcuFormat      = mapVkFormat(imageFormat);
    const auto outBufferSize  = static_cast<vk::VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) *
                                                             imageExtent.width * imageExtent.height);
    const auto outBufferUsage = vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto outBufferInfo  = vk::makeBufferCreateInfo(outBufferSize, outBufferUsage);

    auto outBuffer = std::unique_ptr<vk::BufferWithMemory>(
        new vk::BufferWithMemory(vk, device, allocator, outBufferInfo, vk::MemoryRequirement::HostVisible));

    return outBuffer;
}

vk::VkImageCreateInfo makeImageCreateInfo(vk::VkFormat format, vk::VkExtent3D extent, vk::VkImageUsageFlags usage)
{
    const vk::VkImageCreateInfo imageCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        (vk::VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                                  // VkFormat format;
        extent,                                  // VkExtent3D extent;
        1u,                                      // uint32_t mipLevels;
        1u,                                      // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                                   // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                      // uint32_t queueFamilyIndexCount;
        nullptr,                                 // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    return imageCreateInfo;
}

vk::Move<vk::VkSampler> makeSampler(const vk::DeviceInterface &vk, const vk::VkDevice device)
{
    const vk::VkSamplerCreateInfo samplerInfo = {
        vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // sType
        nullptr,                                     // pNext
        0u,                                          // flags
        vk::VK_FILTER_NEAREST,                       // magFilter
        vk::VK_FILTER_NEAREST,                       // minFilter
        vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,          // mipmapMode
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // addressModeU
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // addressModeV
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // addressModeW
        0.0f,                                        // mipLodBias
        VK_FALSE,                                    // anisotropyEnable
        1.0f,                                        // maxAnisotropy
        false,                                       // compareEnable
        vk::VK_COMPARE_OP_ALWAYS,                    // compareOp
        0.0f,                                        // minLod
        1.0f,                                        // maxLod
        vk::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // borderColor
        VK_FALSE,                                    // unnormalizedCoords
    };

    return createSampler(vk, device, &samplerInfo);
}

vk::VkQueue getQueue(vkt::Context &context, bool useCompute)
{
    return useCompute ? context.getComputeQueue() : context.getUniversalQueue();
}

uint32_t getQueueFamilyIndex(vkt::Context &context, bool useCompute)
{
    return useCompute ? context.getComputeQueueFamilyIndex() : context.getUniversalQueueFamilyIndex();
}

class PipelineCacheTestInstance : public vkt::TestInstance
{
public:
    PipelineCacheTestInstance(vkt::Context &context, vk::PipelineConstructionType pipelineConstructionType,
                              RobustnessBehaviour robustnessBufferBehaviour, RobustnessType type, bool useCompute)
        : vkt::TestInstance(context)
        , m_pipelineConstructionType(pipelineConstructionType)
        , m_robustnessBufferBehaviour(robustnessBufferBehaviour)
        , m_type(type)
        , m_useCompute(useCompute)
        , m_extent()
    {
    }

private:
    void draw(const vk::GraphicsPipelineWrapper &pipeline);
    void dispatch(const vk::ComputePipelineWrapper &pipeline);
    bool verifyImage(tcu::Vec4 value, bool oob);
    tcu::TestStatus iterate(void);

    const vk::PipelineConstructionType m_pipelineConstructionType;
    const RobustnessBehaviour m_robustnessBufferBehaviour;
    const RobustnessType m_type;
    const bool m_useCompute;

    vk::VkExtent2D m_extent;
    vk::Move<vk::VkCommandPool> m_cmdPool;
    vk::Move<vk::VkCommandBuffer> m_cmdBuffer;
    de::MovePtr<vk::BufferWithMemory> m_buffer;
    vk::PipelineLayoutWrapper m_pipelineLayout;
    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    de::MovePtr<vk::ImageWithMemory> m_renderTarget;
    std::unique_ptr<vk::BufferWithMemory> m_outBuffer;

    // Graphics pipeline related
    vk::RenderPassWrapper m_renderPass;
    de::MovePtr<vk::GraphicsPipelineWrapper> m_graphicsPipeline;
    de::MovePtr<vk::GraphicsPipelineWrapper> m_robustGraphicsPipeline;

    // Compute pipeline related
    de::MovePtr<vk::ComputePipelineWrapper> m_computePipeline;
    de::MovePtr<vk::ComputePipelineWrapper> m_robustComputePipeline;
};

void PipelineCacheTestInstance::draw(const vk::GraphicsPipelineWrapper &pipeline)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();
    const vk::VkQueue queue       = m_context.getUniversalQueue();

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    vk::beginCommandBuffer(vk, *m_cmdBuffer);
    if (m_type == VERTEX_INPUT)
    {
        vk::VkDeviceSize offset = 0u;
        vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &**m_buffer, &offset);
    }
    m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(m_extent), clearColor);
    vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1,
                             &*m_descriptorSet, 0, nullptr);
    pipeline.bind(*m_cmdBuffer);
    vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
    m_renderPass.end(vk, *m_cmdBuffer);
    vk::endCommandBuffer(vk, *m_cmdBuffer);

    vk::submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

    vk::beginCommandBuffer(vk, *m_cmdBuffer);
    vk::copyImageToBuffer(vk, *m_cmdBuffer, m_renderTarget->get(), (*m_outBuffer).get(),
                          tcu::IVec2(m_extent.width, m_extent.height));
    vk::endCommandBuffer(vk, *m_cmdBuffer);
    vk::submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);
}

void PipelineCacheTestInstance::dispatch(const vk::ComputePipelineWrapper &pipeline)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();
    const vk::VkQueue queue       = m_context.getComputeQueue();

    vk::beginCommandBuffer(vk, *m_cmdBuffer);

    // Predispathch image transition
    const vk::VkImageMemoryBarrier preDispatchBarrier = vk::makeImageMemoryBarrier(
        0, vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL,
        m_renderTarget->get(), vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));

    vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 0, nullptr, 0, nullptr, 1, &preDispatchBarrier);

    pipeline.bind(*m_cmdBuffer);
    vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, 1,
                             &*m_descriptorSet, 0, nullptr);

    vk.cmdDispatch(*m_cmdBuffer, m_extent.width / 8, m_extent.height / 8, 1);
    vk::endCommandBuffer(vk, *m_cmdBuffer);
    vk::submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

    vk::beginCommandBuffer(vk, *m_cmdBuffer);
    vk::copyImageToBuffer(vk, *m_cmdBuffer, m_renderTarget->get(), (*m_outBuffer).get(),
                          tcu::IVec2(m_extent.width, m_extent.height), vk::VK_ACCESS_SHADER_WRITE_BIT,
                          vk::VK_IMAGE_LAYOUT_GENERAL);
    vk::endCommandBuffer(vk, *m_cmdBuffer);
    vk::submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);
}

bool PipelineCacheTestInstance::verifyImage(tcu::Vec4 value, bool oob)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();

    auto &outBufferAlloc = m_outBuffer->getAllocation();

    invalidateAlloc(vk, device, outBufferAlloc);
    const tcu::ConstPixelBufferAccess result(vk::mapVkFormat(vk::VK_FORMAT_R32G32B32A32_SFLOAT),
                                             tcu::IVec3(m_extent.width, m_extent.height, 1),
                                             (const char *)outBufferAlloc.getHostPtr());

    const uint32_t h = result.getHeight();
    const uint32_t w = result.getWidth();
    for (uint32_t y = 0; y < h; y++)
    {
        for (uint32_t x = 0; x < w; x++)
        {
            tcu::Vec4 pix = result.getPixel(x, y);

            if (oob && m_type == IMAGE)
            {
                for (uint32_t i = 0; i < 4; ++i)
                    if (pix[i] != 0.0f && pix[i] != 1.0f)
                        return false;
            }
            else if (pix != value)
                return false;
        }
    }
    return true;
}

tcu::TestStatus PipelineCacheTestInstance::iterate(void)
{
    const vk::InstanceInterface &vki          = m_context.getInstanceInterface();
    const vk::DeviceInterface &vk             = m_context.getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const vk::VkDevice device                 = m_context.getDevice();
    const uint32_t queueFamilyIndex           = getQueueFamilyIndex(m_context, m_useCompute);
    const vk::VkQueue queue                   = getQueue(m_context, m_useCompute);
    auto &alloc                               = m_context.getDefaultAllocator();
    const auto &deviceExtensions              = m_context.getDeviceExtensions();

    m_extent                       = {32, 32};
    const uint32_t bufferSize      = sizeof(float) * 4u;
    const uint32_t indexBufferSize = sizeof(uint32_t);

    const auto subresourceRange = vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_cmdPool   = createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    m_cmdBuffer = (allocateCommandBuffer(vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    vk::VkBufferUsageFlags bufferUsage =
        (vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
         vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    bufferUsage |= (m_type == VERTEX_INPUT) ? vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0;
    m_buffer = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
        vk, device, alloc, vk::makeBufferCreateInfo(bufferSize, bufferUsage), vk::MemoryRequirement::HostVisible));

    de::MovePtr<vk::BufferWithMemory> indexBuffer = de::MovePtr<vk::BufferWithMemory>(
        new vk::BufferWithMemory(vk, device, alloc,
                                 vk::makeBufferCreateInfo(indexBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                               vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                 vk::MemoryRequirement::HostVisible));
    de::MovePtr<vk::ImageWithMemory> image = de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(
        vk, device, alloc,
        makeImageCreateInfo(vk::VK_FORMAT_R32G32B32A32_SFLOAT, {1, 1, 1},
                            vk::VK_IMAGE_USAGE_STORAGE_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        vk::MemoryRequirement::Any));
    const auto imageView                   = makeImageView(vk, device, image->get(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                           vk::VK_FORMAT_R32G32B32A32_SFLOAT, subresourceRange);
    const auto sampler                     = makeSampler(vk, device);

    const vk::VkImageUsageFlags rtUsage =
        vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_STORAGE_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    m_renderTarget = de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(
        vk, device, alloc,
        makeImageCreateInfo(vk::VK_FORMAT_R32G32B32A32_SFLOAT, {m_extent.width, m_extent.height, 1}, rtUsage),
        vk::MemoryRequirement::Any));

    const auto renderTargetView = makeImageView(vk, device, m_renderTarget->get(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                vk::VK_FORMAT_R32G32B32A32_SFLOAT, subresourceRange);

    auto &bufferAlloc      = m_buffer->getAllocation();
    auto &indexBufferAlloc = indexBuffer->getAllocation();
    const float values[4]  = {0.5f, 0.5f, 0.5f, 0.5f};
    deMemcpy(bufferAlloc.getHostPtr(), values, sizeof(float) * 4);
    flushAlloc(vk, device, bufferAlloc);
    const uint32_t index = 0u;
    deMemcpy(indexBufferAlloc.getHostPtr(), &index, sizeof(uint32_t));
    flushAlloc(vk, device, indexBufferAlloc);

    const vk::VkDescriptorBufferInfo descriptorBufferInfo(makeDescriptorBufferInfo(m_buffer->get(), 0, bufferSize));
    const vk::VkDescriptorImageInfo descriptorImageInfo(
        makeDescriptorImageInfo(sampler.get(), imageView.get(), vk::VK_IMAGE_LAYOUT_GENERAL));
    const vk::VkDescriptorBufferInfo indexBufferInfo(makeDescriptorBufferInfo(indexBuffer->get(), 0, indexBufferSize));

    const std::vector<vk::VkViewport> viewports{makeViewport(m_extent)};
    const std::vector<vk::VkRect2D> scissors{makeRect2D(m_extent)};

    // Building descriptor set layout
    vk::DescriptorSetLayoutBuilder layoutBuilder;
    vk::VkDescriptorType descriptorType = vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    if (m_type == STORAGE)
    {
        descriptorType = vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    else if (m_type == UNIFORM)
    {
        descriptorType = vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    else if (m_type == IMAGE)
    {
        descriptorType = vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    vk::VkShaderStageFlags stage = m_useCompute ? vk::VK_SHADER_STAGE_COMPUTE_BIT : vk::VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBuilder.addSingleBinding(descriptorType, stage); // binding 0 (input buffer or image)

    vk::VkShaderStageFlags indexStage = m_useCompute ?
                                            vk::VK_SHADER_STAGE_COMPUTE_BIT :
                                            (vk::VK_SHADER_STAGE_VERTEX_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT);
    layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, indexStage); // binding 1 (index buffer)
    if (m_useCompute)
        layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       vk::VK_SHADER_STAGE_COMPUTE_BIT); // binding 2 (output image)
    const auto descriptorSetLayout = layoutBuilder.build(vk, device);

    m_pipelineLayout = vk::PipelineLayoutWrapper(m_pipelineConstructionType, vk, device, *descriptorSetLayout);

    m_descriptorPool = (vk::DescriptorPoolBuilder()
                            .addType(descriptorType)
                            .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                            .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_useCompute ? 1 : 0)
                            .build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1));
    m_descriptorSet  = makeDescriptorSet(vk, device, *m_descriptorPool, *descriptorSetLayout);
    vk::DescriptorSetUpdateBuilder builder;
    if (m_type == STORAGE || m_type == UNIFORM)
        builder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                            &descriptorBufferInfo);
    if (m_type == IMAGE)
        builder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                            &descriptorImageInfo);
    builder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u),
                        vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indexBufferInfo);
    if (m_useCompute)
    {
        const vk::VkDescriptorImageInfo outputImageInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, renderTargetView.get(), vk::VK_IMAGE_LAYOUT_GENERAL);

        builder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u),
                            vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outputImageInfo);
    }
    builder.update(vk, device);

    //buffer to read the output image
    m_outBuffer = makeBufferForImage(vk, device, alloc, vk::VK_FORMAT_R32G32B32A32_SFLOAT, m_extent);

    // Setting up pipeline primitives
    vk::VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo     = vk::initVulkanStructure();
    vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = vk::initVulkanStructure();
    vk::VkVertexInputBindingDescription bindingDescription;
    std::vector<vk::VkVertexInputAttributeDescription> attributeDescriptions(16);

    if (!m_useCompute)
    {
        // Setup RenderPass
        m_renderPass = vk::RenderPassWrapper(m_pipelineConstructionType, vk, device, vk::VK_FORMAT_R32G32B32A32_SFLOAT);
        m_renderPass.createFramebuffer(vk, device, **m_renderTarget, *renderTargetView, m_extent.width,
                                       m_extent.height);

        // Vertex Input State
        vertexInputStateCreateInfo.sType = vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        bindingDescription.binding       = 0;
        bindingDescription.stride        = sizeof(float);
        bindingDescription.inputRate     = vk::VK_VERTEX_INPUT_RATE_INSTANCE;

        for (uint32_t i = 0; i < (uint32_t)attributeDescriptions.size(); ++i)
        {
            attributeDescriptions[i].location = i;
            attributeDescriptions[i].binding  = 0;
            attributeDescriptions[i].format   = vk::VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[i].offset   = (uint32_t)(sizeof(float) * i);
        }

        if (m_type == VERTEX_INPUT)
        {
            vertexInputStateCreateInfo.vertexBindingDescriptionCount   = 1u;
            vertexInputStateCreateInfo.pVertexBindingDescriptions      = &bindingDescription;
            vertexInputStateCreateInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
            vertexInputStateCreateInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();
        }

        inputAssemblyStateCreateInfo.sType    = vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCreateInfo.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }

    const vk::VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                          // const void* pNext;
        0u,                                               // VkPipelineCacheCreateFlags flags;
        0u,                                               // uintptr_t initialDataSize;
        nullptr,                                          // const void* pInitialData;
    };

    vk::Move<vk::VkPipelineCache> pipelineCache = createPipelineCache(vk, device, &pipelineCacheCreateInfo);

    vk::VkPipelineRobustnessCreateInfoEXT pipelineRobustnessInfo = vk::initVulkanStructure();
    vk::PipelineRobustnessCreateInfoWrapper pipelineRobustnessWrapper(&pipelineRobustnessInfo);
    if (m_robustnessBufferBehaviour == ROBUSTNESS)
    {
        if (m_type == STORAGE)
            pipelineRobustnessInfo.storageBuffers = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
        else if (m_type == UNIFORM)
            pipelineRobustnessInfo.uniformBuffers = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
        else if (m_type == VERTEX_INPUT)
            pipelineRobustnessInfo.vertexInputs = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
        else if (m_type == IMAGE)
            pipelineRobustnessInfo.images = vk::VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_EXT;
    }
    else
    {
        if (m_type == STORAGE)
            pipelineRobustnessInfo.storageBuffers =
                vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT;
        else if (m_type == UNIFORM)
            pipelineRobustnessInfo.uniformBuffers =
                vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT;
        else if (m_type == VERTEX_INPUT)
            pipelineRobustnessInfo.vertexInputs = vk::VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT;
        else if (m_type == IMAGE)
            pipelineRobustnessInfo.images = vk::VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT;
    }

    // Create pipelines
    if (m_useCompute)
    {
        const auto compBinary = &m_context.getBinaryCollection().get("comp");
        const vk::ComputePipelineConstructionType computeConstructionType =
            vk::graphicsToComputeConstructionType(m_pipelineConstructionType);

        m_computePipeline = de::MovePtr<vk::ComputePipelineWrapper>(
            new vk::ComputePipelineWrapper(vk, device, computeConstructionType, *compBinary));
        m_computePipeline->setDescriptorSetLayout(*descriptorSetLayout);
        m_computePipeline->buildPipeline(*pipelineCache);

        m_robustComputePipeline = de::MovePtr<vk::ComputePipelineWrapper>(
            new vk::ComputePipelineWrapper(vk, device, computeConstructionType, *compBinary));
        m_robustComputePipeline->setDescriptorSetLayout(*descriptorSetLayout);
        m_robustComputePipeline->setPipelineCreatePNext(&pipelineRobustnessInfo);
        m_robustComputePipeline->buildPipeline(*pipelineCache);
    }
    else
    {
        vk::ShaderWrapper vert = vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
        vk::ShaderWrapper frag = vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

        m_graphicsPipeline = de::MovePtr<vk::GraphicsPipelineWrapper>(new vk::GraphicsPipelineWrapper(
            vki, vk, physicalDevice, device, deviceExtensions, m_pipelineConstructionType));
        m_graphicsPipeline->setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setDefaultDepthStencilState()
            .setDefaultColorBlendState()
            .setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vert)
            .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, frag)
            .setupFragmentOutputState(*m_renderPass)
            .setMonolithicPipelineLayout(m_pipelineLayout)
            .buildPipeline(*pipelineCache);

        m_robustGraphicsPipeline = de::MovePtr<vk::GraphicsPipelineWrapper>(new vk::GraphicsPipelineWrapper(
            vki, vk, physicalDevice, device, deviceExtensions, m_pipelineConstructionType));
        m_robustGraphicsPipeline->setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setDefaultDepthStencilState()
            .setDefaultColorBlendState()
            .setPipelineRobustnessState(pipelineRobustnessWrapper)
            .setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vert)
            .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, frag)
            .setupFragmentOutputState(*m_renderPass)
            .setMonolithicPipelineLayout(m_pipelineLayout)
            .buildPipeline(*pipelineCache, VK_NULL_HANDLE, 0, vk::PipelineCreationFeedbackCreateInfoWrapper());
    }

    if (m_type == IMAGE)
    {
        // Initialize image
        const vk::VkImageMemoryBarrier preImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            **image,                                    // VkImage image;
            {
                // VkImageSubresourceRange subresourceRange;
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
                0u,                            // uint32_t baseMipLevel;
                1u,                            // uint32_t mipLevels;
                0u,                            // uint32_t baseArraySlice;
                1u,                            // uint32_t arraySize;
            }};

        const vk::VkImageMemoryBarrier postImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_SHADER_READ_BIT,              // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            **image,                                    // VkImage image;
            {
                // VkImageSubresourceRange subresourceRange;
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
                0u,                            // uint32_t baseMipLevel;
                1u,                            // uint32_t mipLevels;
                0u,                            // uint32_t baseArraySlice;
                1u,                            // uint32_t arraySize;
            }};

        const vk::VkBufferImageCopy copyRegion = {
            0u, // VkDeviceSize bufferOffset;
            0u, // uint32_t bufferRowLength;
            0u, // uint32_t bufferImageHeight;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
                0u,                            // uint32_t mipLevel;
                0u,                            // uint32_t baseArrayLayer;
                1u,                            // uint32_t layerCount;
            },                                 // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                         // VkOffset3D imageOffset;
            {1, 1, 1},                         // VkExtent3D imageExtent;
        };

        vk::beginCommandBuffer(vk, *m_cmdBuffer);
        vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (vk::VkDependencyFlags)0, 0, nullptr, 0u, nullptr, 1u, &preImageBarrier);
        vk.cmdCopyBufferToImage(*m_cmdBuffer, **m_buffer, **image, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                                &copyRegion);
        vk::VkPipelineStageFlags dstStage =
            m_useCompute ? vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, (vk::VkDependencyFlags)0, 0,
                              nullptr, 0, nullptr, 1, &postImageBarrier);
        vk::endCommandBuffer(vk, *m_cmdBuffer);
        vk::submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);
    }

    if (m_useCompute)
        dispatch(*m_computePipeline);
    else
        draw(*m_graphicsPipeline);

    if (!verifyImage(tcu::Vec4(values[0]), false))
        return tcu::TestStatus::fail("Fail");

    uint32_t invalidIndex = m_type == VERTEX_INPUT ? 15u : 999u;
    deMemcpy(indexBufferAlloc.getHostPtr(), &invalidIndex, sizeof(uint32_t));
    flushAlloc(vk, device, indexBufferAlloc);

    if (m_useCompute)
        dispatch(*m_robustComputePipeline);
    else
        draw(*m_robustGraphicsPipeline);

    if (m_robustnessBufferBehaviour == ROBUSTNESS_2)
    {
        if (!verifyImage(tcu::Vec4(0.0f), true))
            return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class PipelineCacheTestCase : public vkt::TestCase
{
public:
    PipelineCacheTestCase(tcu::TestContext &context, const char *name,
                          vk::PipelineConstructionType pipelineConstructionType,
                          RobustnessBehaviour robustnessBufferBehaviour, RobustnessType type, bool useCompute)
        : TestCase(context, name)
        , m_pipelineConstructionType(pipelineConstructionType)
        , m_robustnessBufferBehaviour(robustnessBufferBehaviour)
        , m_type(type)
        , m_useCompute(useCompute)
    {
    }

private:
    void checkSupport(vkt::Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new PipelineCacheTestInstance(context, m_pipelineConstructionType, m_robustnessBufferBehaviour, m_type,
                                             m_useCompute);
    }

    const vk::PipelineConstructionType m_pipelineConstructionType;
    const RobustnessBehaviour m_robustnessBufferBehaviour;
    const RobustnessType m_type;
    const bool m_useCompute;
};

void PipelineCacheTestCase::checkSupport(vkt::Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_pipeline_robustness");
    if (m_robustnessBufferBehaviour == ROBUSTNESS_2)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_robustness2") &&
            !context.isDeviceFunctionalitySupported("VK_EXT_robustness2"))

            TCU_THROW(NotSupportedError, "VK_KHR_robustness2 and VK_EXT_robustness2 are not supported");
    }

    vk::VkPhysicalDevicePipelineRobustnessFeaturesEXT pipelineRobustnessFeatures = vk::initVulkanStructure();
    vk::VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features =
        vk::initVulkanStructure(&pipelineRobustnessFeatures);
    vk::VkPhysicalDeviceFeatures2 features2;

    features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &robustness2Features;

    context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

    if (pipelineRobustnessFeatures.pipelineRobustness == false)
        TCU_THROW(NotSupportedError,
                  "VkPhysicalDevicePipelineRobustnessFeaturesEXT::pipelineRobustness feature not supported");

    if (m_robustnessBufferBehaviour == ROBUSTNESS_2)
    {
        if (m_type == IMAGE)
        {
            if (robustness2Features.robustImageAccess2 == false)
                TCU_THROW(NotSupportedError,
                          "VkPhysicalDeviceRobustness2FeaturesEXT::robustImageAccess2 feature not supported");
        }
        else
        {
            if (robustness2Features.robustBufferAccess2 == false)
                TCU_THROW(NotSupportedError,
                          "VkPhysicalDeviceRobustness2FeaturesEXT::robustBufferAccess2 feature not supported");
        }
    }

    if (m_useCompute)
    {
        if (context.getComputeQueueFamilyIndex() == -1)
            TCU_THROW(NotSupportedError, "Compute queue required");
    }
    else
    {
        vk::checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                                  m_pipelineConstructionType);
    }
}

void PipelineCacheTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    if (m_useCompute)
    {
        std::string descriptor = {};
        std::string readVal    = {};

        if (m_type == STORAGE)
        {
            descriptor = "layout (set=0, binding=0) restrict readonly buffer StorageBuffer { float values[]; };\n";
            readVal    = "vec4(values[index])";
        }
        else if (m_type == UNIFORM)
        {
            descriptor = "layout (std140, set=0, binding=0) restrict uniform UniformBuffer { float values[1000]; };\n";
            readVal    = "vec4(values[index])";
        }
        else if (m_type == IMAGE)
        {
            descriptor = "layout (set=0, binding=0, rgba32f) uniform image2D tex;\n";
            readVal    = "imageLoad(tex, ivec2(index, 0))";
        }

        std::ostringstream comp;
        comp << "#version 450\n"
             << "layout(local_size_x = 8, local_size_y = 8) in;\n"
             << descriptor << "layout (set=0, binding=1) restrict readonly buffer IndexBuffer { uint index; };\n"
             << "layout (set=0, binding=2, rgba32f) uniform writeonly image2D outImage;\n"
             << "void main()\n"
             << "{\n"
             << "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
             << "    vec4 color = " << readVal << ";\n"
             << "    imageStore(outImage, pos, color);\n"
             << "}\n";
        programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
    }
    else if (m_type == VERTEX_INPUT)
    {
        std::ostringstream vert;
        vert << "#version 450\n"
             << "layout(location = 0) in float in_values[16];\n"
             << "layout(location = 0) out float out_value;\n"
             << "layout (set=0, binding=1) restrict readonly buffer IndexBuffer { uint index; };\n"
             << "void main()\n"
             << "{\n"
             << "    vec2 vertex = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);\n"
             << "    gl_Position = vec4(vertex * 2.0f - 1.0f, 0.0f, 1.0f);\n"
             << "    out_value = in_values[index];\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        std::ostringstream frag;
        frag << "#version 450\n"
             << "layout (location=0) in float in_value;\n"
             << "layout (location=0) out vec4 out_color;\n"
             << "void main()\n"
             << "{\n"
             << "    out_color = vec4(in_value);\n"
             << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
    else
    {
        std::ostringstream vert;
        vert << "#version 450\n"
             << "void main()\n"
             << "{\n"
             << "    vec2 vertex = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);\n"
             << "    gl_Position = vec4(vertex * 2.0f - 1.0f, 0.0f, 1.0f);\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        std::string descriptor = {};
        std::string write      = {};
        if (m_type == STORAGE)
        {
            descriptor = "layout (set=0, binding=0) restrict readonly buffer StorageBuffer { float values[]; };\n";
            write      = "    out_color = vec4(values[index]);\n";
        }
        else if (m_type == UNIFORM)
        {
            descriptor = "layout (std140, set=0, binding=0) restrict uniform UniformBuffer { float values[1000]; };\n";
            write      = "    out_color = vec4(values[index]);\n";
        }
        else if (m_type == IMAGE)
        {
            descriptor = "layout (set=0, binding=0, rgba32f) uniform image2D tex;\n";
            write      = "    out_color = imageLoad(tex, ivec2(index, 0));\n";
        }

        std::ostringstream frag;
        frag << "#version 450\n"
             << "layout (location=0) out vec4 out_color;\n"
             << descriptor << "layout (set=0, binding=1) restrict readonly buffer IndexBuffer { uint index; };\n"
             << "void main()\n"
             << "{\n"
             << write << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
}

} // namespace

tcu::TestCaseGroup *createPipelineRobustnessCacheTests(tcu::TestContext &testCtx,
                                                       vk::PipelineConstructionType pipelineConstructionType)
{
    // Test pipeline cache with different robustness enabled
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "pipeline_cache"));

    const struct
    {
        RobustnessBehaviour robustnessBehaviour;
        const char *name;
    } robustnessTests[] = {
        {ROBUSTNESS, "robustness"},
        {ROBUSTNESS_2, "robustness2"},
    };

    const struct
    {
        RobustnessType type;
        const char *name;
    } typeTests[] = {
        {STORAGE, "storage"},
        {UNIFORM, "uniform"},
        {VERTEX_INPUT, "vertex_input"},
        {IMAGE, "image"},
    };

    for (const auto &robustnessTest : robustnessTests)
    {
        de::MovePtr<tcu::TestCaseGroup> robustnessGroup(new tcu::TestCaseGroup(testCtx, robustnessTest.name));
        for (const auto &typeTest : typeTests)
        {
            robustnessGroup->addChild(new PipelineCacheTestCase(testCtx, typeTest.name, pipelineConstructionType,
                                                                robustnessTest.robustnessBehaviour, typeTest.type,
                                                                false));

            // Compute variant
            if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC && typeTest.type != VERTEX_INPUT)
            {
                std::string computeName = std::string(typeTest.name) + "_compute";
                robustnessGroup->addChild(
                    new PipelineCacheTestCase(testCtx, computeName.c_str(), pipelineConstructionType,
                                              robustnessTest.robustnessBehaviour, typeTest.type, true));
            }
        }
        testGroup->addChild(robustnessGroup.release());
    }

    return testGroup.release();
}

} // namespace pipeline
} // namespace vkt
