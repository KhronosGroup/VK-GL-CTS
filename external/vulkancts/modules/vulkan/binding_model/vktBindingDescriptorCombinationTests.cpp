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
 * \file vktBindingDescriptorCombinationTests.cpp
 * \brief Test using descriptor buffers in combination with other extensions.
 *//*--------------------------------------------------------------------*/

#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"
#include "vktBindingDescriptorCombinationTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkRefUtil.hpp"
#include "tcuVectorUtil.hpp"
#include <utility>
#include <array>

namespace vkt::BindingModel
{
namespace
{

using namespace vk;

enum class TestType
{
    DESCRIPTOR_BUFFER_AND_LEGACY_DESCRIPTOR_IN_COMMAND_BUFFER = 0,
    DESCRIPTOR_BUFFER_CAPTURE_REPLAY_WITH_CUSTOM_BORDER_COLOR = 1,
};

struct TestParams
{
    TestType testType;
};

class DescriptorCombinationTestInstance : public TestInstance
{
public:
    DescriptorCombinationTestInstance(Context &context, const TestParams &params);

    tcu::TestStatus iterate() override;

protected:
    Move<VkPipeline> createBasicPipeline(VkPipelineLayout layout, VkShaderModule shaderModule,
                                         VkPipelineCreateFlags flags = 0) const;

protected:
    TestParams m_params;
};

DescriptorCombinationTestInstance::DescriptorCombinationTestInstance(Context &context, const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus DescriptorCombinationTestInstance::iterate()
{
    // test using both descriptor buffers & legacy descriptors in the same command buffer

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    // Create three storage buffers, one for each way we setup descriptors
    using BufferWithMemorySp      = de::SharedPtr<BufferWithMemory>;
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(16 * sizeof(uint32_t));
    const auto bufferUsage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferCreateInfo bufferCreateInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemorySp bufferForLegacyDS = BufferWithMemorySp(
        new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
    BufferWithMemorySp bufferForPushDesc = BufferWithMemorySp(
        new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
    bufferCreateInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    BufferWithMemorySp bufferForDescBuffer = BufferWithMemorySp(new BufferWithMemory(
        vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));

    // Create descriptor pool - we need just one descriptor set
    const VkDescriptorType descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(descType, 1)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    // Create three descriptor layouts
    const Unique<VkDescriptorSetLayout> descriptorSetLayoutForPushDesc(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
    const Unique<VkDescriptorSetLayout> descriptorSetLayoutForLegacyDS(
        DescriptorSetLayoutBuilder().addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT).build(vk, device));
    const Unique<VkDescriptorSetLayout> descriptorSetLayoutForDescBuffer(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    // Create legacy descriptor set
    const Unique<VkDescriptorSet> descriptorSet(
        makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayoutForLegacyDS));
    VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(**bufferForLegacyDS, 0ull, bufferSize);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &bufferDescriptorInfo)
        .update(vk, device);

    // Define WriteDescriptorSet for push descriptor
    bufferDescriptorInfo.buffer           = **bufferForPushDesc;
    VkWriteDescriptorSet descriptorWrites = initVulkanStructure();
    descriptorWrites.descriptorCount      = 1u;
    descriptorWrites.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites.pBufferInfo          = &bufferDescriptorInfo;

    // Check how big descriptor buffer we need
    VkDeviceSize descriptorBufferSize;
    vk.getDescriptorSetLayoutSizeEXT(device, *descriptorSetLayoutForDescBuffer, &descriptorBufferSize);

    // Create descriptor buffer with needed size
    const auto descriptorBufferUsage =
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    const VkBufferCreateInfo descriptorBufferCreateInfo =
        makeBufferCreateInfo(descriptorBufferSize, descriptorBufferUsage);
    const MemoryRequirement bufferMemoryRequirement = MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress;
    BufferWithMemorySp descriptorBuffer             = BufferWithMemorySp(
        new BufferWithMemory(vk, device, allocator, descriptorBufferCreateInfo, bufferMemoryRequirement));
    char *descriptorBufferHostPtr = static_cast<char *>(descriptorBuffer->getAllocation().getHostPtr());

    // Get adress of ssbo to which we will write in shader
    VkBufferDeviceAddressInfo bdaInfo                = initVulkanStructure();
    bdaInfo.buffer                                   = **bufferForDescBuffer;
    VkDescriptorAddressInfoEXT descriptorAddressInfo = initVulkanStructure();
    descriptorAddressInfo.address                    = vk.getBufferDeviceAddress(device, &bdaInfo);
    descriptorAddressInfo.range                      = bufferSize;

    VkDescriptorGetInfoEXT descriptorGetInfo = initVulkanStructure();
    descriptorGetInfo.type                   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorGetInfo.data.pStorageBuffer    = &descriptorAddressInfo;

    VkDeviceSize offset = 0;
    std::size_t size    = m_context.getDescriptorBufferPropertiesEXT().storageBufferDescriptorSize;
    vk.getDescriptorSetLayoutBindingOffsetEXT(device, *descriptorSetLayoutForDescBuffer, 0, &offset);
    vk.getDescriptorEXT(device, &descriptorGetInfo, size, descriptorBufferHostPtr + offset);
    flushAlloc(vk, device, descriptorBuffer->getAllocation());

    // Get adress of descriptor buffer
    bdaInfo.buffer                                               = **descriptorBuffer;
    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo = initVulkanStructure();
    descriptorBufferBindingInfo.address                          = vk.getBufferDeviceAddress(device, &bdaInfo);
    descriptorBufferBindingInfo.usage                            = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkMemoryBarrier memoryBarrier = initVulkanStructure();
    memoryBarrier.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;

    const VkPushConstantRange pushConstantRange{
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,          // offset
        sizeof(int) // size
    };

    const uint32_t descriptorBufferIndices     = 0;
    const VkDeviceSize descriptorBufferoffsets = 0;
    uint32_t pushConstValue                    = 3;
    BinaryCollection &binaryCollection         = m_context.getBinaryCollection();

    const auto shaderModuleInit = createShaderModule(vk, device, binaryCollection.get("comp_init"));
    const auto shaderModuleAdd  = createShaderModule(vk, device, binaryCollection.get("comp_add"));
    const auto pipelineLayoutForLegacyDS =
        makePipelineLayout(vk, device, *descriptorSetLayoutForLegacyDS, &pushConstantRange);
    const auto pipelineLayoutForPushDesc =
        makePipelineLayout(vk, device, *descriptorSetLayoutForPushDesc, &pushConstantRange);
    const auto pipelineLayoutForDescBuffer =
        makePipelineLayout(vk, device, *descriptorSetLayoutForDescBuffer, &pushConstantRange);
    const auto pipelineInitForPushDesc   = createBasicPipeline(*pipelineLayoutForPushDesc, *shaderModuleInit);
    const auto pipelineInitForLegacyDS   = createBasicPipeline(*pipelineLayoutForLegacyDS, *shaderModuleInit);
    const auto pipelineInitForDescBuffer = createBasicPipeline(*pipelineLayoutForDescBuffer, *shaderModuleInit,
                                                               VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    const auto pipelineAddForPushDesc    = createBasicPipeline(*pipelineLayoutForPushDesc, *shaderModuleAdd);
    const auto pipelineAddForLegacyDS    = createBasicPipeline(*pipelineLayoutForLegacyDS, *shaderModuleAdd);
    const auto pipelineAddForDescBuffer  = createBasicPipeline(*pipelineLayoutForDescBuffer, *shaderModuleAdd,
                                                               VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    auto cmdPool                         = makeCommandPool(vk, device, m_context.getUniversalQueueFamilyIndex());
    auto cmdBuffer                       = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    // use push descriptor
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineInitForPushDesc);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForPushDesc, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int),
                        &pushConstValue);
    vk.cmdPushDescriptorSet(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForPushDesc, 0, 1,
                            &descriptorWrites);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    // use legacy descriptor
    pushConstValue = 5;
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineInitForLegacyDS);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForLegacyDS, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int),
                        &pushConstValue);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForLegacyDS, 0u, 1u,
                             &*descriptorSet, 0u, 0);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    // use descriptor buffer
    pushConstValue = 6;
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineInitForDescBuffer);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForDescBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int),
                        &pushConstValue);
    vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1, &descriptorBufferBindingInfo);
    vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForDescBuffer, 0, 1,
                                        &descriptorBufferIndices, &descriptorBufferoffsets);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                          &memoryBarrier, 0, 0, 0, 0);

    // use push descriptor
    pushConstValue = 2;
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForPushDesc);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForPushDesc, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int),
                        &pushConstValue);
    vk.cmdPushDescriptorSet(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForPushDesc, 0, 1,
                            &descriptorWrites);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    // use legacy descriptor
    pushConstValue = 1;
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForLegacyDS);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForLegacyDS, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int),
                        &pushConstValue);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForLegacyDS, 0u, 1u,
                             &*descriptorSet, 0u, 0);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                          &memoryBarrier, 0, 0, 0, 0);

    // use push descriptor
    pushConstValue = 2;
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForPushDesc);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForPushDesc, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int),
                        &pushConstValue);
    vk.cmdPushDescriptorSet(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForPushDesc, 0, 1,
                            &descriptorWrites);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    // use descriptor buffer
    pushConstValue = 3;
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForDescBuffer);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForDescBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int),
                        &pushConstValue);
    vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1, &descriptorBufferBindingInfo);
    vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForDescBuffer, 0, 1,
                                        &descriptorBufferIndices, &descriptorBufferoffsets);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                          &memoryBarrier, 0, 0, 0, 0);

    // use push descriptor
    pushConstValue = 2;
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineAddForPushDesc);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutForPushDesc, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int),
                        &pushConstValue);
    vk.cmdPushDescriptorSet(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutForPushDesc, 0, 1,
                            &descriptorWrites);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    std::pair<BufferWithMemorySp, std::array<int, 16>> expectedResults[]{
        {bufferForLegacyDS, {1, 6, 11, 16, 21, 26, 31, 36, 41, 46, 51, 56, 61, 66, 71, 76}},
        {bufferForPushDesc, {6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51}},
        {bufferForDescBuffer, {3, 9, 15, 21, 27, 33, 39, 45, 51, 57, 63, 69, 75, 81, 87, 93}},
    };

    // Verify all three result buffers
    for (const auto &[bufferWithemory, expectedValues] : expectedResults)
    {
        auto &allocation = bufferWithemory->getAllocation();
        invalidateAlloc(vk, device, allocation);

        int *bufferPtr = static_cast<int *>(allocation.getHostPtr());
        if (deMemCmp(bufferPtr, expectedValues.data(), bufferSize) != 0)
            return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

Move<VkPipeline> DescriptorCombinationTestInstance::createBasicPipeline(VkPipelineLayout layout,
                                                                        VkShaderModule shaderModule,
                                                                        VkPipelineCreateFlags flags) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const VkComputePipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                        sType
        nullptr,                                        // const void*                            pNext
        flags,                                          // VkPipelineCreateFlags                flags
        {
            // VkPipelineShaderStageCreateInfo        stage
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //        VkStructureType                        sType
            nullptr,                                             //        const void*                            pNext
            0u,                                                  //        VkPipelineShaderStageCreateFlags    flags
            VK_SHADER_STAGE_COMPUTE_BIT,                         //        VkShaderStageFlagBits                stage
            shaderModule,                                        //        VkShaderModule                        module
            "main",                                              //        const char*                            pName
            nullptr, //        const VkSpecializationInfo*            pSpecializationInfo
        },
        layout,         // VkPipelineLayout                        layout
        VK_NULL_HANDLE, // VkPipeline                            basePipelineHandle
        0,              // int32_t                                basePipelineIndex
    };

    return createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

class DescriptorCustomBorderColorTestInstance : public TestInstance
{
public:
    DescriptorCustomBorderColorTestInstance(Context &context, const TestParams &params);

    tcu::TestStatus iterate() override;

protected:
    TestParams m_params;
};

DescriptorCustomBorderColorTestInstance::DescriptorCustomBorderColorTestInstance(Context &context,
                                                                                 const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus DescriptorCustomBorderColorTestInstance::iterate()
{
    // Coverage for capture/replay with custom border color;
    // Create samplers A, B, C  ->  replay samplers C, B, A;
    // Draw uding samplers to verify that border colors are fine.

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    const uint32_t size        = 8;
    const VkFormat format      = VK_FORMAT_R8G8B8A8_UNORM;
    const uint32_t numSamplers = 3;
    const VkExtent3D imageExtent{size, size, 1};
    const std::vector<VkViewport> viewports{makeViewport(size, size)};
    const std::vector<VkRect2D> scissors{makeRect2D(size, size)};
    const uint32_t descriptorBufferIndices     = 0;
    const VkDeviceSize descriptorBufferOffsets = 0;
    auto srr                                   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    VkImageCreateInfo textureInfo = initVulkanStructure();
    textureInfo.imageType         = VK_IMAGE_TYPE_2D;
    textureInfo.format            = format;
    textureInfo.extent            = imageExtent;
    textureInfo.mipLevels         = 1u;
    textureInfo.arrayLayers       = 1u;
    textureInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    textureInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    textureInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    auto usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ImageWithMemory textureImage(vk, device, allocator, textureInfo, MemoryRequirement::Any);
    auto textureImageView = makeImageView(vk, device, *textureImage, VK_IMAGE_VIEW_TYPE_2D, format, srr);
    ImageWithBuffer colorBuffer(vk, device, allocator, imageExtent, format, usage, VK_IMAGE_TYPE_2D);

    VkSamplerCustomBorderColorCreateInfoEXT customBorderColorInfo = initVulkanStructure();
    customBorderColorInfo.format                                  = format;

    VkSamplerCreateInfo createInfo = initVulkanStructure(&customBorderColorInfo);
    createInfo.flags               = VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;
    createInfo.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    createInfo.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    createInfo.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    createInfo.maxAnisotropy       = 1.0f;
    createInfo.borderColor         = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;

    // create samplers A, B, C
    tcu::Vec4 colors[]{
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 1.0f},
    };
    Move<VkSampler> samplers[numSamplers];
    for (uint32_t i = 0; i < numSamplers; ++i)
    {
        std::memcpy(&customBorderColorInfo.customBorderColor.float32, &colors[i], sizeof(VkClearColorValue));
        samplers[i] = createSampler(vk, device, &createInfo);
    }

    auto renderPass  = makeRenderPass(vk, device, format);
    auto framebuffer = makeFramebuffer(vk, device, *renderPass, colorBuffer.getImageView(), size, size);

    DescriptorSetLayoutBuilder dslBuilder;
    dslBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
    for (uint32_t i = 0; i < numSamplers; ++i)
        dslBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto descriptorSetLayout(
        dslBuilder.build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    // check how big descriptor buffer we need
    VkDeviceSize bufferSize = 0ull;
    vk.getDescriptorSetLayoutSizeEXT(device, *descriptorSetLayout, &bufferSize);

    // create buffer for descriptor buffer
    const auto bufferUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                             VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                             VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemory bufferWithMemory(vk, device, allocator, bufferCreateInfo,
                                      MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    // get adress of descriptor buffer
    VkBufferDeviceAddressInfo bufferAddressInfo                  = initVulkanStructure();
    bufferAddressInfo.buffer                                     = *bufferWithMemory;
    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo = initVulkanStructure();
    descriptorBufferBindingInfo.address = vk.getBufferDeviceAddress(device, &bufferAddressInfo);
    descriptorBufferBindingInfo.usage =
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;

    auto &bufferAllocation        = bufferWithMemory.getAllocation();
    char *descriptorBufferHostPtr = static_cast<char *>(bufferAllocation.getHostPtr());

    // place texture image into descriptor buffer
    const auto &dbProperties    = m_context.getDescriptorBufferPropertiesEXT();
    const auto sampledImageSize = dbProperties.sampledImageDescriptorSize;
    std::vector<uint8_t> firstDescriptorData(sampledImageSize);
    VkDescriptorImageInfo imageInfo{VK_NULL_HANDLE, *textureImageView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};
    VkDescriptorGetInfoEXT descGetInfo = initVulkanStructure();
    descGetInfo.type                   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descGetInfo.data.pSampledImage     = &imageInfo;
    vk.getDescriptorEXT(device, &descGetInfo, sampledImageSize, descriptorBufferHostPtr);

    // place samplers into descriptor buffer in order A, B, C (order of creation)
    descriptorBufferHostPtr += sampledImageSize;
    const auto samplerDescriptorSize = dbProperties.samplerDescriptorSize;
    for (uint32_t i = 0; i < numSamplers; ++i)
    {
        descGetInfo.type          = VK_DESCRIPTOR_TYPE_SAMPLER;
        descGetInfo.data.pSampler = &*samplers[i];
        vk.getDescriptorEXT(device, &descGetInfo, samplerDescriptorSize, descriptorBufferHostPtr);
        descriptorBufferHostPtr += samplerDescriptorSize;
    }
    flushAlloc(vk, device, bufferAllocation);

    // capture replay data for all samplers
    const auto samplerReplaySize = dbProperties.samplerCaptureReplayDescriptorDataSize;
    std::vector<uint8_t> captureReplayData(numSamplers * samplerReplaySize);
    auto *captureReplayDataPtr                 = captureReplayData.data();
    VkSamplerCaptureDescriptorDataInfoEXT info = initVulkanStructure();
    for (uint32_t i = 0; i < numSamplers; ++i)
    {
        info.sampler = *samplers[i];
        vk.getSamplerOpaqueCaptureDescriptorDataEXT(device, &info, captureReplayDataPtr);
        captureReplayDataPtr += samplerReplaySize;
    }

    // destroy all samplers
    for (uint32_t i = 0; i < numSamplers; ++i)
        samplers[i] = Move<VkSampler>();

    // recreate samplers in order C, B, A (reverse order)
    VkOpaqueCaptureDescriptorDataCreateInfoEXT opaqueCreateInfo = initVulkanStructure();
    captureReplayDataPtr                                        = captureReplayData.data() + captureReplayData.size();
    customBorderColorInfo.pNext                                 = &opaqueCreateInfo;
    for (uint32_t i = 0; i < numSamplers; ++i)
    {
        captureReplayDataPtr -= samplerReplaySize;
        std::memcpy(&customBorderColorInfo.customBorderColor.float32, &colors[numSamplers - 1 - i],
                    sizeof(VkClearColorValue));
        opaqueCreateInfo.opaqueCaptureDescriptorData = captureReplayDataPtr;
        samplers[i]                                  = createSampler(vk, device, &createInfo);
    }

    // define empty VertexInputState, full screen triangle will be generated in vertex shader
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    BinaryCollection &bc = m_context.getBinaryCollection();
    auto vertModule      = createShaderModule(vk, device, bc.get("vert"));
    auto fragModule      = createShaderModule(vk, device, bc.get("frag"));

    auto pipelineLayout = makePipelineLayout(vk, device, *descriptorSetLayout);
    auto graphicsPipeline =
        makeGraphicsPipeline(vk, device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                             *fragModule, *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0,
                             &vertexInputState, 0, 0, 0, 0, 0, 0, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    const auto bpg = VK_PIPELINE_BIND_POINT_GRAPHICS;
    auto cmdPool   = makeCommandPool(vk, device, m_context.getUniversalQueueFamilyIndex());
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    // transition texture image to SHADER_READ_ONLY_OPTIMAL
    const auto textureBarrier =
        makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *textureImage, srr);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                          nullptr, 0, nullptr, 1, &textureBarrier);

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissors[0], tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));

    vk.cmdBindPipeline(*cmdBuffer, bpg, *graphicsPipeline);
    vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1, &descriptorBufferBindingInfo);
    vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, bpg, *pipelineLayout, 0, 1, &descriptorBufferIndices,
                                        &descriptorBufferOffsets);
    vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);

    endRenderPass(vk, *cmdBuffer);

    copyImageToBuffer(vk, *cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), tcu::IVec2(size),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    auto &allocation = colorBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, allocation);
    uint8_t *bufferPtr = static_cast<uint8_t *>(allocation.getHostPtr());

    // verify result
    tcu::Vec4 expectedColor = tcu::mix(tcu::mix(colors[0], colors[1], 0.25f), colors[2], 0.7f);
    for (uint32_t i = 0; i < 4u; ++i)
    {
        // it should be sufficient to verify just few fragments
        uint8_t *fragment = bufferPtr + (i * 4 * (size + i * 2));
        tcu::Vec4 renderedColor(fragment[0], fragment[1], fragment[2], fragment[3]);
        renderedColor = renderedColor / 255.0f;

        if (tcu::boolAny(tcu::greaterThan(tcu::absDiff(expectedColor, renderedColor), tcu::Vec4(0.05f))))
        {
            tcu::PixelBufferAccess resultAccess(mapVkFormat(format), size, size, 1, bufferPtr);
            m_context.getTestContext().getLog() << tcu::LogImage("image", "", resultAccess);

            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class DescriptorCombinationTestCase : public TestCase
{
public:
    DescriptorCombinationTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params);

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

private:
    const TestParams m_params;
};

DescriptorCombinationTestCase::DescriptorCombinationTestCase(tcu::TestContext &testCtx, const std::string &name,
                                                             const TestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void DescriptorCombinationTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_descriptor_buffer");
    if (m_params.testType == TestType::DESCRIPTOR_BUFFER_AND_LEGACY_DESCRIPTOR_IN_COMMAND_BUFFER)
        context.requireDeviceFunctionality("VK_KHR_push_descriptor");
    if (m_params.testType == TestType::DESCRIPTOR_BUFFER_CAPTURE_REPLAY_WITH_CUSTOM_BORDER_COLOR)
    {
        context.requireDeviceFunctionality("VK_EXT_custom_border_color");
        if (!context.getDescriptorBufferFeaturesEXT().descriptorBufferCaptureReplay)
            TCU_THROW(NotSupportedError, "descriptorBufferCaptureReplay is not supported");
    }
}

void DescriptorCombinationTestCase::initPrograms(vk::SourceCollections &programs) const
{
    if (m_params.testType == TestType::DESCRIPTOR_BUFFER_AND_LEGACY_DESCRIPTOR_IN_COMMAND_BUFFER)
    {
        const char *compInitSrc{"#version 460\n"
                                "layout(local_size_x = 4, local_size_y = 4) in;\n"
                                "layout(push_constant) uniform Params { int mulVal; } params;\n"
                                "layout(binding = 0, std430) buffer OutBuf { uint v[]; } outBuf;\n"
                                "void main()\n"
                                "{\n"
                                "  outBuf.v[gl_LocalInvocationIndex] = gl_LocalInvocationIndex * params.mulVal;\n"
                                "}\n"};

        const char *compAddSrc{"#version 460\n"
                               "layout(local_size_x = 4, local_size_y = 4) in;\n"
                               "layout(push_constant) uniform Params { int addVal; } params;\n"
                               "layout(binding = 0, std430) buffer InOutBuf { uint v[]; } inOutBuf;\n"
                               "void main()\n"
                               "{\n"
                               "  uint value = inOutBuf.v[gl_LocalInvocationIndex];"
                               "  inOutBuf.v[gl_LocalInvocationIndex] = value + params.addVal;\n"
                               "}\n"};

        programs.glslSources.add("comp_init") << glu::ComputeSource(compInitSrc);
        programs.glslSources.add("comp_add") << glu::ComputeSource(compAddSrc);
    }
    else if (m_params.testType == TestType::DESCRIPTOR_BUFFER_CAPTURE_REPLAY_WITH_CUSTOM_BORDER_COLOR)
    {
        programs.glslSources.add("vert") << glu::VertexSource(R"(
            #version 460
            void main()
            {
                const float x = (-1.0+4.0*((gl_VertexIndex & 2)>>1));
                const float y = ( 1.0-4.0* (gl_VertexIndex % 2));
                gl_Position = vec4(x, y, 1.0, 1.0);
            })");

        programs.glslSources.add("frag") << glu::FragmentSource(R"(
            #version 460
            layout(binding = 0) uniform texture2D tex;
            layout(binding = 1) uniform sampler samplerA;
            layout(binding = 2) uniform sampler samplerB;
            layout(binding = 3) uniform sampler samplerC;
            layout(location = 0) out vec4 outColor;
            void main()
            {
                vec2 uv = vec2(2.0, 2.0);
                vec4 colorA = texture(sampler2D(tex, samplerA), uv);
                vec4 colorB = texture(sampler2D(tex, samplerB), uv);
                vec4 colorC = texture(sampler2D(tex, samplerC), uv);
                outColor = mix(mix(colorA, colorB, 0.25), colorC, 0.7);
            })");
    }
}

TestInstance *DescriptorCombinationTestCase::createInstance(Context &context) const
{
    if (m_params.testType == TestType::DESCRIPTOR_BUFFER_CAPTURE_REPLAY_WITH_CUSTOM_BORDER_COLOR)
        return new DescriptorCustomBorderColorTestInstance(context, m_params);

    return new DescriptorCombinationTestInstance(context, m_params);
}

void populateDescriptorCombinationTests(tcu::TestCaseGroup *topGroup)
{
    std::pair<std::string, TestType> caseList[]{{"descriptor_buffer_and_legacy_descriptor_in_command_buffer",
                                                 TestType::DESCRIPTOR_BUFFER_AND_LEGACY_DESCRIPTOR_IN_COMMAND_BUFFER},
                                                {"descriptor_buffer_capture_replay_with_custom_border_color",
                                                 TestType::DESCRIPTOR_BUFFER_CAPTURE_REPLAY_WITH_CUSTOM_BORDER_COLOR}};

    tcu::TestContext &testCtx = topGroup->getTestContext();
    de::MovePtr<tcu::TestCaseGroup> basicGroup(new tcu::TestCaseGroup(testCtx, "basic"));

    for (const auto &caseInfo : caseList)
    {
        TestParams params{caseInfo.second};
        basicGroup->addChild(new DescriptorCombinationTestCase(testCtx, caseInfo.first, params));
    }

    topGroup->addChild(basicGroup.release());
}

} // namespace

tcu::TestCaseGroup *createDescriptorCombinationTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "descriptor_combination", populateDescriptorCombinationTests);
}

} // namespace vkt::BindingModel
