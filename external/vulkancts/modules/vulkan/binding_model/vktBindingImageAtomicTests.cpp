/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 LunarG, Inc.
 * Copyright (c) 2026 Google LLC
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
 * \file vktBindingImageAtomicTests.hpp
 * \brief Image atomic binding model tests
 *//*--------------------------------------------------------------------*/

#include "vktBindingImageAtomicTests.hpp"

#include "deUniquePtr.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace vkt
{
namespace BindingModel
{
namespace
{

using namespace vk;

enum class DescriptorType
{
    DESCRIPTOR_SET,
    DESCRIPTOR_BUFFER,
    DESCRIPTOR_HEAP,
};

struct TestParams
{
    DescriptorType mode;
    bool image64bit;
};

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    DE_ASSERT(deIsPowerOfTwo64(alignment));
    return (value + alignment - 1) & ~(alignment - 1);
}

class ImageAtomicInstance : public TestInstance
{
public:
    ImageAtomicInstance(Context &context, const TestParams &params) : TestInstance(context), m_params(params)
    {
    }

    tcu::TestStatus iterate() override;

private:
    de::MovePtr<BufferWithMemory> createBuffer(VkDeviceSize size, VkBufferUsageFlags2KHR usage) const;
    de::MovePtr<BufferWithMemory> createDescriptorBuffer(VkDescriptorSetLayout descriptorSetLayout,
                                                         const uint32_t descriptorCount,
                                                         const VkDescriptorImageInfo *imageInfos) const;
    de::MovePtr<BufferWithMemory> createDescriptorHeap(const VkImage image, const VkFormat format,
                                                       const uint32_t descriptorCount,
                                                       VkDeviceSize &heapUserSize) const;

    const TestParams m_params;
    VkPhysicalDeviceDescriptorHeapPropertiesEXT m_descriptorHeapProperties{};
    VkPhysicalDeviceDescriptorBufferPropertiesEXT m_descriptorBufferProperties{};
};

de::MovePtr<BufferWithMemory> ImageAtomicInstance::createBuffer(VkDeviceSize size, VkBufferUsageFlags2KHR usage) const
{
    const auto &vk        = m_context.getDeviceInterface();
    const VkDevice device = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();

    VkBufferUsageFlags2CreateInfoKHR usage2CreateInfo = initVulkanStructure();
    usage2CreateInfo.usage                            = usage | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;

    VkBufferCreateInfo createInfo = initVulkanStructure(&usage2CreateInfo);
    createInfo.size               = size;

    return de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, alloc, createInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));
}

de::MovePtr<BufferWithMemory> ImageAtomicInstance::createDescriptorBuffer(VkDescriptorSetLayout descriptorSetLayout,
                                                                          const uint32_t descriptorCount,
                                                                          const VkDescriptorImageInfo *imageInfos) const
{
    const auto &vk              = m_context.getDeviceInterface();
    const VkDevice device       = m_context.getDevice();
    VkDeviceSize descriptorSize = m_descriptorBufferProperties.storageImageDescriptorSize;

    VkDeviceSize layoutSize    = 0u;
    VkDeviceSize bindingOffset = 0u;

    vk.getDescriptorSetLayoutSizeEXT(device, descriptorSetLayout, &layoutSize);
    vk.getDescriptorSetLayoutBindingOffsetEXT(device, descriptorSetLayout, 0u, &bindingOffset);

    auto descriptorBuffer =
        createBuffer(layoutSize, static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT));
    auto hostPtr = static_cast<char *>(descriptorBuffer->getAllocation().getHostPtr());

    for (uint32_t i = 0u; i < descriptorCount; ++i)
    {
        VkDescriptorGetInfoEXT descInfo = initVulkanStructure();
        descInfo.type                   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descInfo.data.pStorageImage     = &imageInfos[i];

        vk.getDescriptorEXT(device, &descInfo, static_cast<size_t>(descriptorSize),
                            hostPtr + bindingOffset + i * descriptorSize);
    }
    flushAlloc(vk, device, descriptorBuffer->getAllocation());

    return descriptorBuffer;
}

de::MovePtr<BufferWithMemory> ImageAtomicInstance::createDescriptorHeap(const VkImage image, const VkFormat format,
                                                                        const uint32_t descriptorCount,
                                                                        VkDeviceSize &heapUserSize) const
{
    const auto &vk        = m_context.getDeviceInterface();
    const VkDevice device = m_context.getDevice();

    const VkDeviceSize descriptorStride =
        alignUp(m_descriptorHeapProperties.imageDescriptorSize, m_descriptorHeapProperties.imageDescriptorAlignment);

    heapUserSize = alignUp(descriptorCount * descriptorStride, m_descriptorHeapProperties.resourceHeapAlignment);
    auto descriptorHeap =
        createBuffer(heapUserSize + m_descriptorHeapProperties.minResourceHeapReservedRange,
                     static_cast<VkBufferUsageFlags2KHR>(VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT |
                                                         VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR));

    auto heapHostPtr = static_cast<char *>(descriptorHeap->getAllocation().getHostPtr());
    for (uint32_t i = 0u; i < descriptorCount; ++i)
    {
        VkImageViewCreateInfo viewInfo           = initVulkanStructure();
        viewInfo.image                           = image;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = format;
        viewInfo.components                      = makeComponentMappingIdentity();
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0u;
        viewInfo.subresourceRange.levelCount     = 1u;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount     = 1u;

        VkImageDescriptorInfoEXT imageInfo = initVulkanStructure();
        imageInfo.layout                   = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.pView                    = &viewInfo;

        VkResourceDescriptorInfoEXT resource = initVulkanStructure();
        resource.type                        = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        resource.data.pImage                 = &imageInfo;

        VkHostAddressRangeEXT hostRange;
        hostRange.address = heapHostPtr + i * descriptorStride;
        hostRange.size    = static_cast<size_t>(descriptorStride);

        VK_CHECK(vk.writeResourceDescriptorsEXT(device, 1u, &resource, &hostRange));
    }
    flushAlloc(vk, device, descriptorHeap->getAllocation());

    return descriptorHeap;
}

tcu::TestStatus ImageAtomicInstance::iterate()
{
    const auto &vk              = m_context.getDeviceInterface();
    const VkDevice device       = m_context.getDevice();
    const auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue            = m_context.getUniversalQueue();
    auto &alloc                 = m_context.getDefaultAllocator();
    tcu::TestLog &log           = m_context.getTestContext().getLog();

    if (m_params.mode == DescriptorType::DESCRIPTOR_BUFFER)
        m_descriptorBufferProperties = m_context.getDescriptorBufferPropertiesEXT();
    else if (m_params.mode == DescriptorType::DESCRIPTOR_HEAP)
        m_descriptorHeapProperties = m_context.getDescriptorHeapPropertiesEXT();

    const VkFormat imageFormat          = m_params.image64bit ? VK_FORMAT_R64_UINT : VK_FORMAT_R32_UINT;
    constexpr uint32_t descriptorCount  = 128u;
    const uint32_t outputCount          = descriptorCount * (m_params.image64bit ? 2u : 1u);
    const VkDeviceSize outputBufferSize = static_cast<VkDeviceSize>(outputCount * sizeof(uint32_t));

    auto outputBuffer =
        createBuffer(outputBufferSize, static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_DST_BIT));

    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = imageFormat;
    imageCreateInfo.extent            = {1u, 1u, 1u};
    imageCreateInfo.mipLevels         = 1u;
    imageCreateInfo.arrayLayers       = descriptorCount;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    ImageWithMemory image(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any);

    Move<VkImageView> imageViews[descriptorCount];
    VkDescriptorImageInfo imageInfos[descriptorCount];
    if (m_params.mode != DescriptorType::DESCRIPTOR_HEAP)
    {
        for (uint32_t i = 0u; i < descriptorCount; ++i)
        {
            VkImageViewCreateInfo viewInfo           = initVulkanStructure();
            viewInfo.image                           = *image;
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = imageFormat;
            viewInfo.components                      = makeComponentMappingIdentity();
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel   = 0u;
            viewInfo.subresourceRange.levelCount     = 1u;
            viewInfo.subresourceRange.baseArrayLayer = i;
            viewInfo.subresourceRange.layerCount     = 1u;

            imageViews[i] = createImageView(vk, device, &viewInfo);

            imageInfos[i].sampler     = VK_NULL_HANDLE;
            imageInfos[i].imageView   = *imageViews[i];
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
    }

    Move<VkDescriptorSetLayout> descriptorSetLayout;
    Move<VkPipelineLayout> pipelineLayout;
    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;
    de::MovePtr<BufferWithMemory> descriptorBuffer;
    de::MovePtr<BufferWithMemory> descriptorHeap;
    VkDeviceSize heapUserSize                   = 0u;
    VkDescriptorSetAndBindingMappingEXT mapping = initVulkanStructure();

    if (m_params.mode == DescriptorType::DESCRIPTOR_SET)
    {
        descriptorSetLayout =
            DescriptorSetLayoutBuilder()
                .addArrayBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorCount, VK_SHADER_STAGE_COMPUTE_BIT)
                .build(vk, device);
        pipelineLayout = makePipelineLayout(vk, device, *descriptorSetLayout);
        descriptorPool = DescriptorPoolBuilder()
                             .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorCount)
                             .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

        VkWriteDescriptorSet write = initVulkanStructure();
        write.dstSet               = *descriptorSet;
        write.dstBinding           = 0u;
        write.dstArrayElement      = 0u;
        write.descriptorCount      = descriptorCount;
        write.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo           = imageInfos;

        vk.updateDescriptorSets(m_context.getDevice(), 1u, &write, 0u, nullptr);
    }
    else if (m_params.mode == DescriptorType::DESCRIPTOR_BUFFER)
    {
        descriptorSetLayout =
            DescriptorSetLayoutBuilder()
                .addArrayBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorCount, VK_SHADER_STAGE_COMPUTE_BIT)
                .build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        pipelineLayout   = makePipelineLayout(vk, device, *descriptorSetLayout);
        descriptorBuffer = createDescriptorBuffer(*descriptorSetLayout, descriptorCount, imageInfos);
    }
    else
    {
        descriptorHeap = createDescriptorHeap(*image, imageFormat, descriptorCount, heapUserSize);

        mapping.descriptorSet                             = 0u;
        mapping.firstBinding                              = 0u;
        mapping.bindingCount                              = 1u;
        mapping.resourceMask                              = VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
        mapping.source                                    = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        mapping.sourceData.constantOffset.heapOffset      = 0u;
        mapping.sourceData.constantOffset.heapArrayStride = static_cast<uint32_t>(alignUp(
            m_descriptorHeapProperties.imageDescriptorSize, m_descriptorHeapProperties.imageDescriptorAlignment));
    }

    auto shaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("compute"));

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1u;
    mappingInfo.pMappings                                     = &mapping;

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags = initVulkanStructure();

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure(&pipelineFlags);
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *shaderModule;
    pipelineCreateInfo.stage.pName                 = "main";
    pipelineCreateInfo.layout                      = *pipelineLayout;

    if (m_params.mode == DescriptorType::DESCRIPTOR_BUFFER)
    {
        pipelineFlags.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;
    }
    if (m_params.mode == DescriptorType::DESCRIPTOR_HEAP)
    {
        pipelineFlags.flags            = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;
        pipelineCreateInfo.stage.pNext = &mappingInfo;
    }

    auto pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);

    const auto cmdPool   = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkImageSubresourceRange imageSubresourceRange;
    imageSubresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageSubresourceRange.baseMipLevel   = 0u;
    imageSubresourceRange.levelCount     = 1u;
    imageSubresourceRange.baseArrayLayer = 0u;
    imageSubresourceRange.layerCount     = descriptorCount;

    beginCommandBuffer(vk, *cmdBuffer);

    auto preBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *image, imageSubresourceRange);

    VkClearColorValue clearColor{};
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &preBarrier);
    vk.cmdClearColorImage(*cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1u,
                          &imageSubresourceRange);

    auto clearBarrier = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, *image, imageSubresourceRange);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &clearBarrier);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

    if (m_params.mode == DescriptorType::DESCRIPTOR_SET)
    {
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet,
                                 0u, nullptr);
    }
    else if (m_params.mode == DescriptorType::DESCRIPTOR_BUFFER)
    {
        VkBufferDeviceAddressInfo addressInfo = initVulkanStructure();
        addressInfo.buffer                    = descriptorBuffer->get();

        VkDescriptorBufferBindingInfoEXT bindingInfo = initVulkanStructure();
        bindingInfo.address                          = vk.getBufferDeviceAddress(device, &addressInfo);
        bindingInfo.usage                            = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

        const uint32_t bufferIndex      = 0u;
        const VkDeviceSize bufferOffset = 0u;

        vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1u, &bindingInfo);
        vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                            &bufferIndex, &bufferOffset);
    }
    else
    {
        VkBufferDeviceAddressInfo addressInfo = initVulkanStructure();
        addressInfo.buffer                    = descriptorHeap->get();

        VkBindHeapInfoEXT heapInfo   = initVulkanStructure();
        heapInfo.heapRange.address   = vk.getBufferDeviceAddress(device, &addressInfo);
        heapInfo.heapRange.size      = heapUserSize + m_descriptorHeapProperties.minResourceHeapReservedRange;
        heapInfo.reservedRangeOffset = heapUserSize;
        heapInfo.reservedRangeSize   = m_descriptorHeapProperties.minResourceHeapReservedRange;
        vk.cmdBindResourceHeapEXT(*cmdBuffer, &heapInfo);
    }

    vk.cmdDispatch(*cmdBuffer, descriptorCount, 1u, 1u);

    auto imageBarrier =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *image, imageSubresourceRange);
    auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageExtent                 = {1u, 1u, 1u};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = descriptorCount;

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &imageBarrier);
    vk.cmdCopyImageToBuffer(*cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outputBuffer->get(), 1u,
                            &copyRegion);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                          &bufferBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, outputBuffer->getAllocation());

    std::vector<uint32_t> expected(outputCount);
    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        if (m_params.image64bit)
        {
            expected[2 * i]     = 0x89abcedfu;
            expected[2 * i + 1] = 0x01234567u;
        }
        else
        {
            expected[i] = 0x89abcedfu;
        }
    }

    for (uint32_t i = 0u; i < outputCount; ++i)
    {
        const uint32_t *result = reinterpret_cast<uint32_t *>(outputBuffer->getAllocation().getHostPtr());
        if (result[i] != expected[i])
        {
            log << tcu::TestLog::Message << "At index " << i << " expected " << expected[i] << " but was " << result[i]
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class ImageAtomicCase final : public TestCase
{
public:
    ImageAtomicCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new ImageAtomicInstance(context, m_params);
    }

private:
    const TestParams m_params;
};

void ImageAtomicCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_buffer_device_address");

    if (m_params.mode == DescriptorType::DESCRIPTOR_BUFFER)
        context.requireDeviceFunctionality("VK_EXT_descriptor_buffer");

    if (m_params.mode == DescriptorType::DESCRIPTOR_HEAP)
        context.requireDeviceFunctionality("VK_EXT_descriptor_heap");

    if (m_params.image64bit)
        context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");

    if (!context.getDeviceVulkan12Features().runtimeDescriptorArray)
        TCU_THROW(NotSupportedError, "runtimeDescriptorArray not supported");
    if (!context.getDeviceVulkan12Features().shaderStorageImageArrayNonUniformIndexing)
        TCU_THROW(NotSupportedError, "shaderStorageImageArrayNonUniformIndexing not supported");

    const VkFormat format = m_params.image64bit ? VK_FORMAT_R64_UINT : VK_FORMAT_R32_UINT;
    const auto formatFeatures =
        getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);
    if ((formatFeatures.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT) == 0u)
        TCU_THROW(NotSupportedError, "Format does not support storage image atomics");
}

void ImageAtomicCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string computeShader;

    if (!m_params.image64bit)
    {
        computeShader = R"(#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(r32ui, set = 0, binding = 0) uniform uimage2D images[];

void main()
{
    const uint index = gl_GlobalInvocationID.x;
    const uint value = 0x89abcedfu;
    imageAtomicOr(images[nonuniformEXT(index)], ivec2(0), value);
}
)";
    }
    else
    {
        computeShader = R"(#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_image_int64 : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(r64ui, set = 0, binding = 0) uniform u64image2D images[];

void main()
{
    const uint index = gl_GlobalInvocationID.x;
    const uint64_t value = (uint64_t(0x01234567u) << 32) | uint64_t(0x89abcedfu);
    imageAtomicOr(images[nonuniformEXT(index)], ivec2(0), value);
}
)";
    }

    const vk::ShaderBuildOptions options(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0u, true);
    programCollection.glslSources.add("compute") << glu::ComputeSource(computeShader) << options;
}

} // namespace

tcu::TestCaseGroup *createImageAtomicTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> imageAtomicGroup(new tcu::TestCaseGroup(testCtx, "image_atomic"));

    const struct
    {
        const char *name;
        DescriptorType type;
    } types[] = {
        {"descriptor_set", DescriptorType::DESCRIPTOR_SET},
        {"descriptor_buffer", DescriptorType::DESCRIPTOR_BUFFER},
        {"descriptor_heap", DescriptorType::DESCRIPTOR_HEAP},
    };

    for (const auto &type : types)
    {
        de::MovePtr<tcu::TestCaseGroup> descriptorTypeGropu(new tcu::TestCaseGroup(testCtx, type.name));

        for (const bool image64bit : {false, true})
        {
            TestParams params;
            params.mode       = type.type;
            params.image64bit = image64bit;

            const char *name = image64bit ? "64bit" : "32bit";
            descriptorTypeGropu->addChild(new ImageAtomicCase(testCtx, name, params));
        }
        imageAtomicGroup->addChild(descriptorTypeGropu.release());
    }

    return imageAtomicGroup.release();
}

} // namespace BindingModel
} // namespace vkt
