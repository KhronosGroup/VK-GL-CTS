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
 * \brief Pipeline Binaries Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineBinaryTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPipelineBinaryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"
#include "tcuTestLog.hpp"

#include <sstream>
#include <vector>

namespace vkt::pipeline
{

using namespace vk;

enum class TestType
{
    CREATE_INCOMPLETE = 0,
    NOT_ENOUGH_SPACE,
    DESTROY_NULL_BINARY,
    CREATE_WITH_ZERO_BINARY_COUNT,
    GRAPHICS_PIPELINE_FROM_INTERNAL_CACHE,
    GRAPHICS_PIPELINE_WITH_ZERO_BINARY_COUNT,
    COMPUTE_PIPELINE_FROM_INTERNAL_CACHE,
    RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE,
    RAY_TRACING_PIPELINE_FROM_PIPELINE,
    RAY_TRACING_PIPELINE_FROM_BINARY_DATA,
    RAY_TRACING_PIPELINE_WITH_ZERO_BINARY_COUNT,
    UNIQUE_KEY_PAIRS,
    VALID_KEY,
};

enum class BinariesStatus
{
    VALID = 0,
    INVALID,
    NOT_FOUND,
};

struct TestParams
{
    PipelineConstructionType pipelineConstructionType;
    TestType type;
    bool usePipelineLibrary;
};

const uint32_t kNumPipelineLibs = 4;

namespace
{

class BasicComputePipelineTestInstance : public vkt::TestInstance
{
public:
    BasicComputePipelineTestInstance(Context &context, const TestParams &testParams);
    virtual ~BasicComputePipelineTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void) override;

private:
    const TestParams m_testParams;
};

BasicComputePipelineTestInstance::BasicComputePipelineTestInstance(Context &context, const TestParams &testParams)
    : TestInstance(context)
    , m_testParams(testParams)
{
}

tcu::TestStatus BasicComputePipelineTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const VkDescriptorType descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(descType, 1)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder().addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT).build(vk, device));

    const auto pipelineLayout = makePipelineLayout(vk, device, *descriptorSetLayout);
    const auto shaderModule   = createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"));
    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = initVulkanStructure();
    pipelineFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;
    VkComputePipelineCreateInfo pipelineCreateInfo               = initVulkanStructure();
    pipelineCreateInfo.pNext                                     = &pipelineFlags2CreateInfo;
    pipelineCreateInfo.stage                                     = initVulkanStructure();
    pipelineCreateInfo.stage.stage                               = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.pName                               = "main";
    pipelineCreateInfo.stage.module                              = *shaderModule;
    pipelineCreateInfo.layout                                    = *pipelineLayout;

    const auto pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);

    if (m_testParams.type == TestType::CREATE_INCOMPLETE)
    {
        VkPipelineBinaryCreateInfoKHR pipelineBinaryCreateInfo = initVulkanStructure();
        pipelineBinaryCreateInfo.pipeline                      = *pipeline;

        // check how many binaries will be created
        VkPipelineBinaryHandlesInfoKHR binaryHandlesInfo = initVulkanStructure();
        VK_CHECK(vk.createPipelineBinariesKHR(device, &pipelineBinaryCreateInfo, nullptr, &binaryHandlesInfo));

        std::size_t binaryCount = binaryHandlesInfo.pipelineBinaryCount;
        if (binaryCount < 2)
            return tcu::TestStatus::pass("Binary count too small");

        std::vector<VkPipelineBinaryKHR> binariesRaw(binaryCount, VK_NULL_HANDLE);
        binaryHandlesInfo.pPipelineBinaries   = binariesRaw.data();
        binaryHandlesInfo.pipelineBinaryCount = 1;

        // test that vkCreatePipelineBinariesKHR returns VK_INCOMPLETE when pipelineBinaryCount
        // is less than the total count of binaries that might be created
        VkResult result = vk.createPipelineBinariesKHR(device, &pipelineBinaryCreateInfo, nullptr, &binaryHandlesInfo);
        if (result == VK_INCOMPLETE)
            return tcu::TestStatus::pass("Pass");
    }
    else if (m_testParams.type == TestType::NOT_ENOUGH_SPACE)
    {
        PipelineBinaryWrapper binaries(vk, device);
        binaries.createPipelineBinariesFromPipeline(*pipeline);

        VkPipelineBinaryKeyKHR binaryKey       = initVulkanStructure();
        const VkPipelineBinaryKHR *binariesRaw = binaries.getPipelineBinaries();

        VkPipelineBinaryDataInfoKHR binaryInfo = initVulkanStructure();
        binaryInfo.pipelineBinary              = binariesRaw[0];

        // get first binary key and data size
        size_t binaryDataSize = 0;
        VK_CHECK(vk.getPipelineBinaryDataKHR(device, &binaryInfo, &binaryKey, &binaryDataSize, NULL));
        DE_ASSERT(binaryDataSize > 1);

        // try getting binary data while providing not enough space
        std::vector<uint8_t> pipelineDataBlob(binaryDataSize);
        --binaryDataSize;
        VkResult result =
            vk.getPipelineBinaryDataKHR(device, &binaryInfo, &binaryKey, &binaryDataSize, pipelineDataBlob.data());

        // check if NOT_ENOUGH_SPACE error was returned and if binaryDataSize has been updated to the correct size
        if ((result == VK_ERROR_NOT_ENOUGH_SPACE_KHR) && (binaryDataSize == pipelineDataBlob.size()))
            return tcu::TestStatus::pass("Pass");
    }
    else if (m_testParams.type == TestType::DESTROY_NULL_BINARY)
    {
        PipelineBinaryWrapper binaries(vk, device);
        binaries.createPipelineBinariesFromPipeline(*pipeline);

        vk.destroyPipelineBinaryKHR(device, VK_NULL_HANDLE, nullptr);
        return tcu::TestStatus::pass("Pass");
    }
    else if (m_testParams.type == TestType::CREATE_WITH_ZERO_BINARY_COUNT)
    {
        VkPipelineBinaryInfoKHR binaryInfo = initVulkanStructure();
        pipelineCreateInfo.pNext           = &binaryInfo;
        auto testPipeline                  = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
        return tcu::TestStatus::pass("Pass");
    }

    return tcu::TestStatus::fail("Fail");
}

class ComputePipelineInternalCacheTestInstance : public vkt::TestInstance
{
public:
    ComputePipelineInternalCacheTestInstance(Context &context);
    virtual ~ComputePipelineInternalCacheTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void) override;
};

ComputePipelineInternalCacheTestInstance::ComputePipelineInternalCacheTestInstance(Context &context)
    : TestInstance(context)
{
}

tcu::TestStatus ComputePipelineInternalCacheTestInstance::iterate(void)
{
    using BufferWithMemorySp = de::SharedPtr<BufferWithMemory>;

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    PipelineBinaryWrapper pipelineBinaryWrapper(vk, device);

    const VkDescriptorType descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(descType, 1)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder().addSingleBinding(descType, VK_SHADER_STAGE_COMPUTE_BIT).build(vk, device));

    auto shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp")));
    const auto pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

    // create compute pipeline
    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.pName                 = "main";
    pipelineCreateInfo.stage.module                = *shaderModule;
    pipelineCreateInfo.layout                      = *pipelineLayout;
    auto pipeline(createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo));

    // create pipeline binaries from internal cache
    BinariesStatus binariesStatus = BinariesStatus::VALID;
    if (pipelineBinaryWrapper.createPipelineBinariesFromInternalCache(&pipelineCreateInfo))
        binariesStatus = BinariesStatus::NOT_FOUND;

    // check pipeline binary data
    if (binariesStatus == BinariesStatus::VALID)
    {
        // delete pipeline and shader module
        pipeline     = Move<VkPipeline>();
        shaderModule = Move<VkShaderModule>();

        std::vector<VkPipelineBinaryDataKHR> pipelineDataInfo;
        std::vector<std::vector<uint8_t>> pipelineDataBlob;

        pipelineBinaryWrapper.getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

        // check first blob and make sure that it does not contain only 0
        if (std::all_of(pipelineDataBlob[0].cbegin(), pipelineDataBlob[0].cend(), [](uint8_t d) { return d == 0; }))
            binariesStatus = BinariesStatus::INVALID;
    }

    // test pipeline
    Allocator &memAlloc = m_context.getDefaultAllocator();
    const VkDeviceSize bufferSize(static_cast<VkDeviceSize>(8 * sizeof(uint32_t)));
    VkBufferCreateInfo bufferCreateInfo(
        makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    BufferWithMemorySp bufferWithemory(BufferWithMemorySp(
        new BufferWithMemory(vk, device, memAlloc, bufferCreateInfo, MemoryRequirement::HostVisible)));

    const auto descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
    VkDescriptorBufferInfo bufferDescriptorInfo(makeDescriptorBufferInfo(**bufferWithemory, 0ull, bufferSize));
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &bufferDescriptorInfo)
        .update(vk, device);

    // create pipeline form internal cache or fallback to normal pipeline when binary data is not valid
    VkPipelineBinaryInfoKHR binaryInfo;
    if (binariesStatus == BinariesStatus::VALID)
    {
        binaryInfo                      = pipelineBinaryWrapper.preparePipelineBinaryInfo();
        pipelineCreateInfo.pNext        = &binaryInfo;
        pipelineCreateInfo.stage.module = VK_NULL_HANDLE;
    }
    else if (binariesStatus == BinariesStatus::INVALID)
    {
        shaderModule                    = createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"));
        pipelineCreateInfo.stage.module = *shaderModule;
    }

    pipeline       = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    auto cmdPool   = makeCommandPool(vk, device, m_context.getUniversalQueueFamilyIndex());
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             0);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    auto &allocation = bufferWithemory->getAllocation();
    invalidateAlloc(vk, device, allocation);
    uint32_t *bufferPtr = static_cast<uint32_t *>(allocation.getHostPtr());
    for (uint32_t i = 0; i < 8; ++i)
    {
        if (bufferPtr[i] != i)
            return tcu::TestStatus::fail("Invalid value in buffer");
    }

    if (binariesStatus == BinariesStatus::VALID)
        return tcu::TestStatus::pass("Pass");

    if (binariesStatus == BinariesStatus::INVALID)
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Driver wasn't able to pull out valid binary");

    //if (binariesStatus == BinariesStatus::NOT_FOUND)
    return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Pipeline binary was not found in internal cache");
}

class GraphicsPipelineInternalCacheTestInstance : public vkt::TestInstance
{
public:
    GraphicsPipelineInternalCacheTestInstance(Context &context, const TestParams &testParams);
    virtual ~GraphicsPipelineInternalCacheTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void) override;

private:
    const TestParams m_testParams;
};

GraphicsPipelineInternalCacheTestInstance::GraphicsPipelineInternalCacheTestInstance(Context &context,
                                                                                     const TestParams &testParams)
    : TestInstance(context)
    , m_testParams(testParams)
{
}

tcu::TestStatus GraphicsPipelineInternalCacheTestInstance::iterate(void)
{
    const InstanceInterface &vki           = m_context.getInstanceInterface();
    const DeviceInterface &vk              = m_context.getDeviceInterface();
    const VkDevice device                  = m_context.getDevice();
    VkPhysicalDevice physicalDevice        = m_context.getPhysicalDevice();
    vk::BinaryCollection &binaryCollection = m_context.getBinaryCollection();
    const auto pipelineConstructionType    = m_testParams.pipelineConstructionType;
    const uint32_t renderSize              = 8;
    const std::vector<VkViewport> viewport{makeViewport(renderSize, renderSize)};
    const std::vector<VkRect2D> scissor{makeRect2D(renderSize, renderSize)};
    const Move<VkRenderPass> renderPass                   = makeRenderPass(vk, device, VK_FORMAT_R8G8B8A8_UNORM);
    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo   = initVulkanStructure();
    const PipelineLayoutWrapper pipelineLayout(pipelineConstructionType, vk, device, &pipelineLayoutInfo);
    ShaderWrapper vertShader(vk, device, binaryCollection.get("vert"));
    ShaderWrapper fragShader(vk, device, binaryCollection.get("frag"));
    PipelineBinaryWrapper pipelineBinaryWrapper[]{
        {vk, device},
        {vk, device},
        {vk, device},
        {vk, device},
    };

    uint32_t usedBinaryWrappersCount = 4;
    if (pipelineConstructionType == PipelineConstructionType::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        usedBinaryWrappersCount = 1;

    // use local scope to delete pipeline
    BinariesStatus binariesStatus = BinariesStatus::VALID;
    {
        GraphicsPipelineWrapper pipelineWrapper(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(),
                                                pipelineConstructionType);

        // pipelineBinaryInternalCache is available so create pipeline without VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR
        pipelineWrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultColorBlendState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setMonolithicPipelineLayout(pipelineLayout)
            .setupVertexInputState(&vertexInputState)
            .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, *renderPass, 0u, vertShader)
            .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragShader)
            .setupFragmentOutputState(*renderPass)
            .buildPipeline();

        // reuse code to check 0 binary count
        if (m_testParams.type == TestType::GRAPHICS_PIPELINE_WITH_ZERO_BINARY_COUNT)
        {
            auto pipelineCreateInfo            = pipelineWrapper.getPipelineCreateInfo();
            VkPipelineBinaryInfoKHR binaryInfo = initVulkanStructure();
            pipelineCreateInfo.pNext           = &binaryInfo;
            auto testPipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
            return tcu::TestStatus::pass("Pass");
        }

        if (pipelineConstructionType == PipelineConstructionType::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        {
            const auto &pipelineCreateInfo = pipelineWrapper.getPipelineCreateInfo();
            if (pipelineBinaryWrapper[0].createPipelineBinariesFromInternalCache(&pipelineCreateInfo))
                binariesStatus = BinariesStatus::NOT_FOUND;
        }
        else
        {
            for (uint32_t i = 0; i < 4; ++i)
            {
                const auto &pipelinePartCreateInfo = pipelineWrapper.getPartialPipelineCreateInfo(i);
                if (pipelineBinaryWrapper[i].createPipelineBinariesFromInternalCache(&pipelinePartCreateInfo))
                    binariesStatus = BinariesStatus::NOT_FOUND;
            }
        }

        // destroy pipeline when leaving local scope
    }

    // check pipeline binary data
    if (binariesStatus == BinariesStatus::VALID)
    {
        std::vector<VkPipelineBinaryDataKHR> pipelineDataInfo;
        std::vector<std::vector<uint8_t>> pipelineDataBlob;

        // find pipelineBinaryWrapper that has binaries and make sure first binary is valid
        for (uint32_t i = 0; i < usedBinaryWrappersCount; ++i)
        {
            if (pipelineBinaryWrapper[i].getBinariesCount() == 0)
                continue;

            pipelineBinaryWrapper[i].getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

            // check first blob and make sure that it does not contain only 0
            if (std::all_of(pipelineDataBlob[0].cbegin(), pipelineDataBlob[0].cend(), [](uint8_t d) { return d == 0; }))
                binariesStatus = BinariesStatus::INVALID;
            break;
        }
    }

    // test pipeline
    const VkExtent3D extent = makeExtent3D(renderSize, renderSize, 1u);
    Allocator &memAlloc     = m_context.getDefaultAllocator();
    const auto srr          = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto srl          = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    ImageWithBuffer imageWithBuffer(vk, device, memAlloc, extent, VK_FORMAT_R8G8B8A8_UNORM,
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                    VK_IMAGE_TYPE_2D, srr);
    Move<VkImageView> imageView =
        makeImageView(vk, device, imageWithBuffer.getImage(), VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, srr);
    const auto framebuffer             = makeFramebuffer(vk, device, *renderPass, *imageView, renderSize, renderSize);
    VkClearValue clearValue            = makeClearValueColor(tcu::Vec4(0.0f));
    const VkBufferImageCopy copyRegion = makeBufferImageCopy(extent, srl);
    const auto imageBarrier            = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageWithBuffer.getImage(), srr);
    GraphicsPipelineWrapper pipelineWrapper(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(),
                                            pipelineConstructionType);

    VkPipelineBinaryInfoKHR binaryInfo[4];
    VkPipelineBinaryInfoKHR *binaryInfoPtr[4];
    deMemset(binaryInfoPtr, 0, 4 * sizeof(nullptr));

    // create pipeline form internal cache or fallback to normal pipeline when binary data is not valid
    if (binariesStatus == BinariesStatus::VALID)
    {
        for (uint32_t i = 0; i < usedBinaryWrappersCount; ++i)
        {
            binaryInfo[i]    = pipelineBinaryWrapper[i].preparePipelineBinaryInfo();
            binaryInfoPtr[i] = &binaryInfo[i];
        }
    }

    pipelineWrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setMonolithicPipelineLayout(pipelineLayout)
        .disableShaderModules(binariesStatus == BinariesStatus::VALID)
        .setupVertexInputState(&vertexInputState, nullptr, VK_NULL_HANDLE, {}, binaryInfoPtr[0])
        .setupPreRasterizationShaderState3(viewport, scissor, pipelineLayout, *renderPass, 0u, vertShader, 0, {}, {},
                                           {}, {}, {}, {}, {}, 0, 0, 0, 0, 0, {}, VK_NULL_HANDLE, {}, binaryInfoPtr[1])
        .setupFragmentShaderState2(pipelineLayout, *renderPass, 0u, fragShader, 0, 0, 0, 0, VK_NULL_HANDLE, {}, {},
                                   binaryInfoPtr[2])
        .setupFragmentOutputState(*renderPass, 0, 0, 0, VK_NULL_HANDLE, {}, {}, binaryInfoPtr[3])
        .buildPipeline(VK_NULL_HANDLE, VK_NULL_HANDLE, 0, {}, binaryInfoPtr[0]);

    const auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const auto cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    const auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissor[0], clearValue);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineWrapper.getPipeline());
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0, 0, 0, 0, 1u, &imageBarrier);
    vk.cmdCopyImageToBuffer(*cmdBuffer, imageWithBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            imageWithBuffer.getBuffer(), 1u, &copyRegion);
    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    const auto &bufferAllocation = imageWithBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    // check just few fragments around diagonal
    uint8_t expected[] = {255, 0, 255, 0};
    uint8_t *bufferPtr = static_cast<uint8_t *>(bufferAllocation.getHostPtr());
    for (uint32_t i = 0; i < 7; ++i)
    {
        if (deMemCmp(bufferPtr + 4 * (i * i + i), expected, 4))
            return tcu::TestStatus::fail("Invalid fragment color");
    }

    if (binariesStatus == BinariesStatus::VALID)
        return tcu::TestStatus::pass("Pass");

    if (binariesStatus == BinariesStatus::INVALID)
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Driver wasn't able to pull out valid binary");

    //if (binariesStatus == BinariesStatus::NOT_FOUND)
    return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Pipeline binary was not found in internal cache");
}

class RayTracingPipelineTestInstance : public TestInstance
{
public:
    RayTracingPipelineTestInstance(Context &context, const TestParams &testParams);
    virtual ~RayTracingPipelineTestInstance(void) = default;

    virtual tcu::TestStatus iterate(void) override;

protected:
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) const;
    de::MovePtr<BufferWithMemory> createShaderBindingTable(const VkPipeline pipeline, const uint32_t &firstGroup) const;
    void deletePipelineAndModules(void);

private:
    const TestParams m_testParams;
    Move<VkPipeline> m_pipeline;
    std::vector<Move<VkShaderModule>> m_shaderModules;
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderCreateInfoVect;
};

RayTracingPipelineTestInstance::RayTracingPipelineTestInstance(Context &context, const TestParams &testParams)
    : TestInstance(context)
    , m_testParams(testParams)
{
}

tcu::TestStatus RayTracingPipelineTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();
    Allocator &memAlloc       = m_context.getDefaultAllocator();
    PipelineBinaryWrapper pipelineBinaryWrapper(vk, device);

    const uint32_t imageSize(8u);
    const bool usePipelineLibrary(m_testParams.usePipelineLibrary);

    const uint32_t sgHandleSize    = m_context.getRayTracingPipelineProperties().shaderGroupHandleSize;
    const VkFlags rayTracingStages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                     VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

    const VkDeviceSize resultBufferSize = imageSize * imageSize * sizeof(int);
    const VkBufferCreateInfo resultBufferCreateInfo =
        makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    de::SharedPtr<BufferWithMemory> resultBuffer = de::SharedPtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, memAlloc, resultBufferCreateInfo, MemoryRequirement::HostVisible));
    auto &bufferAlloc = resultBuffer->getAllocation();
    void *bufferPtr   = bufferAlloc.getHostPtr();
    deMemset(bufferPtr, 1, static_cast<size_t>(resultBufferSize));
    vk::flushAlloc(vk, device, bufferAlloc);

    const Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, rayTracingStages)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, rayTracingStages)
            .build(vk, device);
    const Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

    const std::pair<VkShaderStageFlagBits, std::string> stageNames[]{
        {VK_SHADER_STAGE_RAYGEN_BIT_KHR, "rgen"},  {VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "chit"},
        {VK_SHADER_STAGE_MISS_BIT_KHR, "miss"},    {VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "isec"},
        {VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "ahit"}, {VK_SHADER_STAGE_CALLABLE_BIT_KHR, "call"},
    };

    const auto shaderCount                                  = de::arrayLength(stageNames);
    VkPipelineShaderStageCreateInfo defaultShaderCreateInfo = initVulkanStructure();
    defaultShaderCreateInfo.pName                           = "main";
    m_shaderCreateInfoVect.resize(shaderCount, defaultShaderCreateInfo);
    m_shaderModules.resize(shaderCount);

    // define shader stages
    BinaryCollection &bc = m_context.getBinaryCollection();
    for (uint32_t index = 0; index < shaderCount; ++index)
    {
        const auto &[shaderStage, shaderName] = stageNames[index];
        m_shaderModules[index]                = createShaderModule(vk, device, bc.get(shaderName));
        auto &shaderCreateInfo                = m_shaderCreateInfoVect[index];
        shaderCreateInfo.stage                = shaderStage;
        shaderCreateInfo.module               = *m_shaderModules[index];
    }

    // Define four shader groups: rgen, hit, miss, call in that order.
    const size_t shaderGroupCount = 4u;

    const VkRayTracingShaderGroupCreateInfoKHR defaultShaderGroupCreateInfo{
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,               // VkRayTracingShaderGroupTypeKHR type;
        VK_SHADER_UNUSED_KHR,                                       // uint32_t generalShader;
        VK_SHADER_UNUSED_KHR,                                       // uint32_t closestHitShader;
        VK_SHADER_UNUSED_KHR,                                       // uint32_t anyHitShader;
        VK_SHADER_UNUSED_KHR,                                       // uint32_t intersectionShader;
        nullptr,                                                    // const void* pShaderGroupCaptureReplayHandle;
    };

    // Fill indices to each shader in the shaders array.
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroupCreateInfoVect(shaderGroupCount,
                                                                                defaultShaderGroupCreateInfo);
    shaderGroupCreateInfoVect[0].generalShader      = 0u;
    shaderGroupCreateInfoVect[1].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    shaderGroupCreateInfoVect[1].anyHitShader       = 4u;
    shaderGroupCreateInfoVect[1].intersectionShader = 3u;
    shaderGroupCreateInfoVect[1].closestHitShader   = 1u;
    shaderGroupCreateInfoVect[2].generalShader      = 2u;
    shaderGroupCreateInfoVect[3].generalShader      = 5u;

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = initVulkanStructure();
    pipelineFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;

    // define structures required for pipeline library
    VkRayTracingPipelineInterfaceCreateInfoKHR libInterfaceInfo   = initVulkanStructure();
    libInterfaceInfo.maxPipelineRayPayloadSize                    = static_cast<uint32_t>(sizeof(int));
    VkRayTracingPipelineInterfaceCreateInfoKHR *pLibraryInterface = nullptr;

    // create ray tracing pipeline that will capture its data (except for RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE mode);
    // when we use internal cache then pipeline should be created without VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR
    void *pNext = &pipelineFlags2CreateInfo;
    if (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE)
    {
        pNext                          = nullptr;
        pipelineFlags2CreateInfo.flags = 0;
    }

    // create ray tracing pipeline library instead of regular ray tracing pipeline
    if (usePipelineLibrary)
    {
        pNext = &pipelineFlags2CreateInfo;
        pipelineFlags2CreateInfo.flags |= VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR;
        pLibraryInterface = &libInterfaceInfo;
    }

    Move<VkPipeline> pipelineLibrary;
    VkPipelineLibraryCreateInfoKHR libraryInfo = initVulkanStructure();
    Move<VkPipelineLayout> pipelineLayout      = makePipelineLayout(vk, device, *descriptorSetLayout);
    VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo{
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        pNext,
        0,                                         // VkPipelineCreateFlags                       flags;
        de::sizeU32(m_shaderCreateInfoVect),       // uint32_t                                    stageCount;
        de::dataOrNull(m_shaderCreateInfoVect),    // const VkPipelineShaderStageCreateInfo*      pStages;
        de::sizeU32(shaderGroupCreateInfoVect),    // uint32_t                                    groupCount;
        de::dataOrNull(shaderGroupCreateInfoVect), // const VkRayTracingShaderGroupCreateInfoKHR* pGroups;
        1u,                // uint32_t                                    maxPipelineRayRecursionDepth;
        nullptr,           // VkPipelineLibraryCreateInfoKHR*             pLibraryInfo;
        pLibraryInterface, // VkRayTracingPipelineInterfaceCreateInfoKHR* pLibraryInterface;
        nullptr,           // const VkPipelineDynamicStateCreateInfo*     pDynamicState;
        *pipelineLayout,   // VkPipelineLayout                            layout;
        VK_NULL_HANDLE,    // VkPipeline                                  basePipelineHandle;
        0,                 // int32_t                                     basePipelineIndex;
    };
    m_pipeline = createRayTracingPipelineKHR(vk, device, VK_NULL_HANDLE, VK_NULL_HANDLE, &pipelineCreateInfo);
    BinariesStatus binariesStatus = BinariesStatus::VALID;

    if (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_PIPELINE)
    {
        // reuse this test to also check if pipeline key is valid
        auto pipelineKey = pipelineBinaryWrapper.getPipelineKey(&pipelineCreateInfo);
        if (pipelineKey.keySize == 0)
            return tcu::TestStatus::fail("vkGetPipelineKeyKHR returned keySize == 0");

        // create pipeline binary objects from pipeline
        pipelineBinaryWrapper.createPipelineBinariesFromPipeline(*m_pipeline);

        // delete pipeline and shader modules after creating binaries
        deletePipelineAndModules();
    }
    else if (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_BINARY_DATA)
    {
        // create pipeline binary objects from pipeline
        pipelineBinaryWrapper.createPipelineBinariesFromPipeline(*m_pipeline);

        // read binaries data out of the device
        std::vector<VkPipelineBinaryDataKHR> pipelineDataInfo;
        std::vector<std::vector<uint8_t>> pipelineDataBlob;
        pipelineBinaryWrapper.getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

        // clear pipeline binaries objects
        pipelineBinaryWrapper.deletePipelineBinariesKeepKeys();

        // recreate binaries from data blobs
        pipelineBinaryWrapper.createPipelineBinariesFromBinaryData(pipelineDataInfo);

        // delete pipeline and shader modules after creating binaries
        deletePipelineAndModules();
    }
    else if (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE)
    {
        if (pipelineBinaryWrapper.createPipelineBinariesFromInternalCache(&pipelineCreateInfo))
            binariesStatus = BinariesStatus::NOT_FOUND;
        else
        {
            // delete pipeline and shader modules after creating binaries
            deletePipelineAndModules();

            std::vector<VkPipelineBinaryDataKHR> pipelineDataInfo;
            std::vector<std::vector<uint8_t>> pipelineDataBlob;

            // attempt to call vkGetPipelineBinaryDataKHR
            pipelineBinaryWrapper.getPipelineBinaryData(pipelineDataInfo, pipelineDataBlob);

            // check first blob and make sure that it does not contain only 0
            if (std::all_of(pipelineDataBlob[0].cbegin(), pipelineDataBlob[0].cend(), [](uint8_t d) { return d == 0; }))
                binariesStatus = BinariesStatus::INVALID;
        }
    }
    else if (m_testParams.type == TestType::RAY_TRACING_PIPELINE_WITH_ZERO_BINARY_COUNT)
    {
        VkPipelineBinaryInfoKHR binaryInfo = initVulkanStructure();
        pipelineCreateInfo.pNext           = &binaryInfo;
        auto testPipeline =
            createRayTracingPipelineKHR(vk, device, VK_NULL_HANDLE, VK_NULL_HANDLE, &pipelineCreateInfo);
        return tcu::TestStatus::pass("Pass");
    }

    // recreate pipeline using binaries or fallback to normal pipelines when binaries aren't found
    VkPipelineBinaryInfoKHR binaryInfo;
    if (binariesStatus == BinariesStatus::VALID)
    {
        binaryInfo               = pipelineBinaryWrapper.preparePipelineBinaryInfo();
        pipelineCreateInfo.pNext = &binaryInfo;

        if (usePipelineLibrary)
        {
            pipelineFlags2CreateInfo.flags = VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR;
            binaryInfo.pNext               = &pipelineFlags2CreateInfo;
        }
    }
    else
    {
        for (uint32_t index = 0; index < shaderCount; ++index)
        {
            const auto &[shaderStage, shaderName] = stageNames[index];
            m_shaderModules[index]                = createShaderModule(vk, device, bc.get(shaderName));
            auto &shaderCreateInfo                = m_shaderCreateInfoVect[index];
            shaderCreateInfo.stage                = shaderStage;
            shaderCreateInfo.module               = *m_shaderModules[index];
        }
        pipelineCreateInfo.pStages = m_shaderCreateInfoVect.data();
    }

    if (usePipelineLibrary)
    {
        // create raytracing pipeline library from pipeline library
        pipelineLibrary = createRayTracingPipelineKHR(vk, device, VK_NULL_HANDLE, VK_NULL_HANDLE, &pipelineCreateInfo);

        // create raytracing pipeline from pipeline library
        libraryInfo.libraryCount = 1u;
        libraryInfo.pLibraries   = &*pipelineLibrary;

        pipelineCreateInfo                              = initVulkanStructure();
        pipelineCreateInfo.maxPipelineRayRecursionDepth = 1u;
        pipelineCreateInfo.pLibraryInterface            = pLibraryInterface;
        pipelineCreateInfo.layout                       = *pipelineLayout;
        pipelineCreateInfo.pLibraryInfo                 = &libraryInfo;
    }

    m_pipeline = createRayTracingPipelineKHR(vk, device, VK_NULL_HANDLE, VK_NULL_HANDLE, &pipelineCreateInfo);

    auto rgenShaderBT = createShaderBindingTable(*m_pipeline, 0);
    auto chitShaderBT = createShaderBindingTable(*m_pipeline, 1);
    auto missShaderBT = createShaderBindingTable(*m_pipeline, 2);
    auto callShaderBT = createShaderBindingTable(*m_pipeline, 3);

    auto &makeSDARegion     = makeStridedDeviceAddressRegionKHR;
    const auto rgenSBTR     = makeSDARegion(getBufferDeviceAddress(**rgenShaderBT), sgHandleSize, sgHandleSize);
    const auto chitSBTR     = makeSDARegion(getBufferDeviceAddress(**chitShaderBT), sgHandleSize, sgHandleSize);
    const auto missSBTR     = makeSDARegion(getBufferDeviceAddress(**missShaderBT), sgHandleSize, sgHandleSize);
    const auto callableSBTR = makeSDARegion(getBufferDeviceAddress(**callShaderBT), sgHandleSize, sgHandleSize);

    auto tlas      = makeTopLevelAccelerationStructure();
    auto cmdPool   = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer, 0u);

    // build acceleration structure - single, big aabb
    auto blas = makeBottomLevelAccelerationStructure();
    AccelerationStructBufferProperties bufferProps;
    bufferProps.props.residency = ResourceResidency::TRADITIONAL;
    blas->setGeometryData(
        {
            {0.0, 0.0, -8.0},
            {8.0, 8.0, -1.0},
        },
        false, 0);
    blas->createAndBuild(vk, device, *cmdBuffer, memAlloc, bufferProps);
    tlas->setInstanceCount(1);
    tlas->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(blas.release()));
    tlas->createAndBuild(vk, device, *cmdBuffer, memAlloc, bufferProps);

    // update descriptor sets
    {
        typedef DescriptorSetUpdateBuilder::Location DSL;
        VkWriteDescriptorSetAccelerationStructureKHR as = initVulkanStructure();
        as.accelerationStructureCount                   = 1;
        as.pAccelerationStructures                      = tlas->getPtr();
        const VkDescriptorBufferInfo ssbo               = makeDescriptorBufferInfo(**resultBuffer, 0u, VK_WHOLE_SIZE);
        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DSL::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &as)
            .writeSingle(*descriptorSet, DSL::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssbo)
            .update(vk, device);
    }

    // wait for data transfers
    const VkMemoryBarrier bufferUploadBarrier =
        makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    cmdPipelineMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &bufferUploadBarrier, 1u);

    // wait for as build
    const VkMemoryBarrier asBuildBarrier = makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                                             VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
    cmdPipelineMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &asBuildBarrier, 1u);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipeline);

    // generate result
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &*descriptorSet,
                             0, nullptr);
    cmdTraceRays(vk, *cmdBuffer, &rgenSBTR, &missSBTR, &chitSBTR, &callableSBTR, imageSize, imageSize, 1);

    const VkMemoryBarrier postTraceMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    cmdPipelineMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    // verify result buffer
    const uint32_t fragmentCount = imageSize * imageSize;
    uint32_t brightRedCount      = 0;
    uint32_t darkRedCount        = 0;
    auto resultAllocation        = resultBuffer->getAllocation();
    auto data                    = static_cast<uint32_t *>(resultAllocation.getHostPtr());
    invalidateMappedMemoryRange(vk, device, resultAllocation.getMemory(), resultAllocation.getOffset(),
                                resultBufferSize);
    for (uint32_t i = 0; i < fragmentCount; ++i)
    {
        uint32_t value = data[i];
        brightRedCount += (value == 0xFF0000FF);
        darkRedCount += (value == 0xFF000080);
    }

    // expect half of fragments to have dark red color and other half bright red color
    // check also if colors in top corrners are ok
    if (((brightRedCount + darkRedCount) == fragmentCount) && (data[0] == 0xFF0000FF) &&
        (data[imageSize - 1] == 0xFF000080))
    {
        if (binariesStatus == BinariesStatus::VALID)
            return tcu::TestStatus::pass("Pass");

        if (binariesStatus == BinariesStatus::INVALID)
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Driver wasn't able to pull out valid binary");

        //if (binariesStatus == BinariesStatus::NOT_FOUND)
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Pipeline binary was not found in internal cache");
    }

    tcu::TextureFormat imageFormat(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
    tcu::PixelBufferAccess resultAccess(imageFormat, imageSize, imageSize, 1, data);
    m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result", "")
                                        << tcu::TestLog::Image("Output", "", resultAccess) << tcu::TestLog::EndImageSet;

    return tcu::TestStatus::fail("Fail");
}

VkDeviceAddress RayTracingPipelineTestInstance::getBufferDeviceAddress(VkBuffer buffer) const
{
    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice device                       = m_context.getDevice();
    VkBufferDeviceAddressInfo deviceAddressInfo = initVulkanStructure();

    deviceAddressInfo.buffer = buffer;
    return vk.getBufferDeviceAddress(device, &deviceAddressInfo);
};

de::MovePtr<BufferWithMemory> RayTracingPipelineTestInstance::createShaderBindingTable(const VkPipeline pipeline,
                                                                                       const uint32_t &firstGroup) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &memAlloc       = m_context.getDefaultAllocator();
    const uint32_t sgHandleSize(m_context.getRayTracingPipelineProperties().shaderGroupHandleSize);
    std::vector<uint8_t> shaderHandles(sgHandleSize);

    vk.getRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, 1u, static_cast<uint32_t>(shaderHandles.size()),
                                          de::dataOrNull(shaderHandles));

    const auto totalEntrySize         = deAlign32(sgHandleSize, sgHandleSize);
    const VkBufferUsageFlags sbtFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkBufferCreateInfo sbtCreateInfo = makeBufferCreateInfo(totalEntrySize, sbtFlags);

    const MemoryRequirement sbtMemRequirements =
        MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress | MemoryRequirement::Any;
    de::MovePtr<BufferWithMemory> sbtBuffer =
        de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, memAlloc, sbtCreateInfo, sbtMemRequirements));
    vk::Allocation &sbtAlloc = sbtBuffer->getAllocation();

    deMemcpy(sbtAlloc.getHostPtr(), shaderHandles.data(), sgHandleSize);
    flushMappedMemoryRange(vk, device, sbtAlloc.getMemory(), sbtAlloc.getOffset(), VK_WHOLE_SIZE);

    return sbtBuffer;
}

void RayTracingPipelineTestInstance::deletePipelineAndModules()
{
    m_pipeline = Move<VkPipeline>();
    for (uint32_t index = 0; index < (uint32_t)m_shaderModules.size(); ++index)
    {
        m_shaderModules[index]               = Move<VkShaderModule>();
        m_shaderCreateInfoVect[index].module = VK_NULL_HANDLE;
    }
}

class UniqueKayPairsTestInstance : public vkt::TestInstance
{
public:
    UniqueKayPairsTestInstance(Context &context, const TestParams &testParams);
    virtual ~UniqueKayPairsTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void) override;

private:
    const TestParams m_testParams;
};

UniqueKayPairsTestInstance::UniqueKayPairsTestInstance(Context &context, const TestParams &testParams)
    : TestInstance(context)
    , m_testParams(testParams)
{
}

tcu::TestStatus UniqueKayPairsTestInstance::iterate(void)
{
    const InstanceInterface &vki           = m_context.getInstanceInterface();
    const DeviceInterface &vkd             = m_context.getDeviceInterface();
    const VkDevice vkDevice                = m_context.getDevice();
    VkPhysicalDevice vkPhysicalDevice      = m_context.getPhysicalDevice();
    vk::BinaryCollection &binaryCollection = m_context.getBinaryCollection();
    const auto pipelineConstructionType    = m_testParams.pipelineConstructionType;
    const std::vector<VkViewport> viewport{makeViewport(16, 16)};
    const std::vector<VkRect2D> scissor{makeRect2D(16, 16)};
    const Move<VkRenderPass> renderPass                 = makeRenderPass(vkd, vkDevice, VK_FORMAT_R8G8B8A8_UNORM);
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = initVulkanStructure();
    const PipelineLayoutWrapper pipelineLayout(pipelineConstructionType, vkd, vkDevice, &pipelineLayoutInfo);

    ShaderWrapper vertShaderModule = ShaderWrapper(vkd, vkDevice, binaryCollection.get("vert"), 0);
    ShaderWrapper fragShaderModule = ShaderWrapper(vkd, vkDevice, binaryCollection.get("frag"), 0);
    GraphicsPipelineWrapper gpwCombinations[]{
        {vki, vkd, vkPhysicalDevice, vkDevice, m_context.getDeviceExtensions(), pipelineConstructionType},
        {vki, vkd, vkPhysicalDevice, vkDevice, m_context.getDeviceExtensions(), pipelineConstructionType},
        {vki, vkd, vkPhysicalDevice, vkDevice, m_context.getDeviceExtensions(), pipelineConstructionType},
        {vki, vkd, vkPhysicalDevice, vkDevice, m_context.getDeviceExtensions(), pipelineConstructionType},
    };
    PipelineBinaryWrapper binaries[]{
        {vkd, vkDevice},
        {vkd, vkDevice},
        {vkd, vkDevice},
        {vkd, vkDevice},
    };
    std::vector<VkPipelineBinaryDataKHR> pipelineDataInfo[4];
    std::vector<std::vector<uint8_t>> pipelineDataBlob[4];

    const float specializationData[][2]{
        {0.2f, 0.3f},
        {0.2f, 0.4f},
        {0.1f, 0.3f},
        {0.1f, 0.4f},
    };

    // specialization constants
    const auto entrySize = static_cast<uintptr_t>(sizeof(float));
    const VkSpecializationMapEntry specializationMap[]{
        //    constantID        offset                                size
        {0u, 0u, entrySize},
        {1u, static_cast<uint32_t>(entrySize), entrySize},
    };
    VkSpecializationInfo specializationInfo{
        2u,                                                    // uint32_t mapEntryCount;
        specializationMap,                                     // const VkSpecializationMapEntry* pMapEntries;
        static_cast<uintptr_t>(sizeof(specializationData[0])), // uintptr_t dataSize;
        nullptr,                                               // const void* pData;
    };

    for (int32_t i = 0; i < 4; ++i)
    {
        specializationInfo.pData = specializationData[i];
        gpwCombinations[i]
            .setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR)
            .setDefaultRasterizationState()
            .setDefaultColorBlendState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setMonolithicPipelineLayout(pipelineLayout)
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, *renderPass, 0u, vertShaderModule, 0,
                                              ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), &specializationInfo)
            .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragShaderModule, 0, 0, &specializationInfo)
            .setupFragmentOutputState(*renderPass)
            .buildPipeline();

        binaries[i].createPipelineBinariesFromPipeline(gpwCombinations[i].getPipeline());

        // read binaries data out of the device
        binaries[i].getPipelineBinaryData(pipelineDataInfo[i], pipelineDataBlob[i]);

        for (uint32_t currDataBlobIndex = 0; currDataBlobIndex < (uint32_t)pipelineDataBlob[i].size();
             ++currDataBlobIndex)
        {
            const auto &currDataBlob = pipelineDataBlob[i][currDataBlobIndex];

            // compare with binaries from previous pipelines
            for (int32_t p = i - 1; p >= 0; --p)
            {
                for (uint32_t prevDataBlobIndex = 0; prevDataBlobIndex < (uint32_t)pipelineDataBlob[p].size();
                     ++prevDataBlobIndex)
                {
                    const auto &prevDataBlob = pipelineDataBlob[p][prevDataBlobIndex];

                    // skip if blob has different size
                    if (currDataBlob.size() != prevDataBlob.size())
                        continue;

                    // if pipeline binary data is the same but the keys are different flag a QualityWarning
                    if (deMemCmp(currDataBlob.data(), prevDataBlob.data(), currDataBlob.size()) == 0)
                    {
                        const auto &currKey = binaries[i].getBinaryKeys()[currDataBlobIndex];
                        const auto &prevKey = binaries[p].getBinaryKeys()[prevDataBlobIndex];

                        if (currKey.keySize != prevKey.keySize)
                            continue;

                        if (deMemCmp(currKey.key, prevKey.key, currKey.keySize) != 0)
                            TCU_THROW(QualityWarning, "Multiple keys generated for identical binaries");
                    }
                }
            }
        }
    }

    // there is no duplicated pipeline binary data
    return tcu::TestStatus::pass("Pass");
}

class PipelineBinaryTestWrapper : public PipelineBinaryWrapper
{
public:
    PipelineBinaryTestWrapper(const DeviceInterface &vk, const VkDevice vkDevice) : PipelineBinaryWrapper(vk, vkDevice)
    {
    }

    void getPipelineBinaryKeyOnly();
};

void PipelineBinaryTestWrapper::getPipelineBinaryKeyOnly()
{
    // for graphics pipeline libraries not all pipeline stages have to have binaries
    const std::size_t binaryCount = m_binariesRaw.size();
    if (binaryCount == 0)
        return;

    m_binaryKeys.resize(binaryCount);

    for (std::size_t i = 0; i < binaryCount; ++i)
    {
        VkPipelineBinaryDataInfoKHR binaryInfo = initVulkanStructure();
        binaryInfo.pipelineBinary              = m_binariesRaw[i];

        // get binary key and data size
        size_t binaryDataSize = 0;
        m_binaryKeys[i]       = initVulkanStructure();
        VK_CHECK(m_vk.getPipelineBinaryDataKHR(m_device, &binaryInfo, &m_binaryKeys[i], &binaryDataSize, NULL));
        DE_ASSERT(binaryDataSize > 0);
    }
}

class PipelineBinaryKeyTestInstance : public vkt::TestInstance
{
public:
    PipelineBinaryKeyTestInstance(Context &context, const TestParams &testParams);
    virtual ~PipelineBinaryKeyTestInstance(void) = default;
    virtual tcu::TestStatus iterate(void) override;

private:
    const TestParams m_testParams;

protected:
    PipelineBinaryTestWrapper m_testBinaries[kNumPipelineLibs];
};

PipelineBinaryKeyTestInstance::PipelineBinaryKeyTestInstance(Context &context, const TestParams &testParams)
    : TestInstance(context)
    , m_testParams(testParams)
    , m_testBinaries{
          {context.getDeviceInterface(), context.getDevice()},
          {context.getDeviceInterface(), context.getDevice()},
          {context.getDeviceInterface(), context.getDevice()},
          {context.getDeviceInterface(), context.getDevice()},
      }
{
}

tcu::TestStatus PipelineBinaryKeyTestInstance::iterate(void)
{
    const InstanceInterface &vki           = m_context.getInstanceInterface();
    const DeviceInterface &vkd             = m_context.getDeviceInterface();
    const VkDevice vkDevice                = m_context.getDevice();
    VkPhysicalDevice vkPhysicalDevice      = m_context.getPhysicalDevice();
    vk::BinaryCollection &binaryCollection = m_context.getBinaryCollection();
    tcu::TestLog &log                      = m_context.getTestContext().getLog();
    const auto Message                     = tcu::TestLog::Message;
    const auto EndMessage                  = tcu::TestLog::EndMessage;
    const auto pipelineConstructionType    = m_testParams.pipelineConstructionType;
    const uint32_t renderSize              = 16;
    bool testOk                            = true;

    const std::vector<VkViewport> viewport{makeViewport(renderSize, renderSize)};
    const std::vector<VkRect2D> scissor{makeRect2D(renderSize, renderSize)};
    const Move<VkRenderPass> renderPass                 = makeRenderPass(vkd, vkDevice, VK_FORMAT_R8G8B8A8_UNORM);
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = initVulkanStructure();
    const PipelineLayoutWrapper pipelineLayout(pipelineConstructionType, vkd, vkDevice, &pipelineLayoutInfo);

    ShaderWrapper vertShaderModule = ShaderWrapper(vkd, vkDevice, binaryCollection.get("vert"), 0);
    ShaderWrapper fragShaderModule = ShaderWrapper(vkd, vkDevice, binaryCollection.get("frag"), 0);
    GraphicsPipelineWrapper pipelineWrapper(vki, vkd, vkPhysicalDevice, vkDevice, m_context.getDeviceExtensions(),
                                            pipelineConstructionType);

    pipelineWrapper.setPipelineCreateFlags2(VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR)
        .setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setMonolithicPipelineLayout(pipelineLayout)
        .setupVertexInputState()
        .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, *renderPass, 0u, vertShaderModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragShaderModule, 0, 0)
        .setupFragmentOutputState(*renderPass)
        .buildPipeline();

    if (m_testParams.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        VkPipeline pipeline = pipelineWrapper.getPipeline();
        m_testBinaries[0].createPipelineBinariesFromPipeline(pipeline);
        // read key
        m_testBinaries[0].getPipelineBinaryKeyOnly();

        const uint32_t keyCount = m_testBinaries[0].getKeyCount();
        if (!keyCount)
            log << Message << "Pipeline binary: 0 has no keys" << EndMessage;

        const VkPipelineBinaryKeyKHR *keys = m_testBinaries[0].getBinaryKeys();
        for (uint32_t keyIdx = 0; keyIdx < keyCount; keyIdx++)
        {
            const uint32_t keySize = keys[keyIdx].keySize;
            log << Message << "Pipeline binary: 0, key: " << keyIdx << " has key size: " << keySize << EndMessage;
            if ((keySize <= 0u) || (keySize > VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR))
            {
                testOk = false;
                break;
            }
        }
    }
    else
    {
        for (uint32_t libIdx = 0; libIdx < kNumPipelineLibs; ++libIdx)
        {
            VkPipeline partialPipeline = pipelineWrapper.getPartialPipeline(libIdx);
            m_testBinaries[libIdx].createPipelineBinariesFromPipeline(partialPipeline);
            // read key
            m_testBinaries[libIdx].getPipelineBinaryKeyOnly();

            const uint32_t keyCount = m_testBinaries[libIdx].getKeyCount();
            if (!keyCount)
                log << Message << "Pipeline binary: " << libIdx << " has no keys" << EndMessage;

            const VkPipelineBinaryKeyKHR *keys = m_testBinaries[libIdx].getBinaryKeys();
            for (uint32_t keyIdx = 0; keyIdx < keyCount; keyIdx++)
            {
                const uint32_t keySize = keys[keyIdx].keySize;
                log << Message << "Pipeline binary: " << libIdx << ", key: " << keyIdx << " has key size: " << keySize
                    << EndMessage;
                if ((keys[keyIdx].keySize <= 0u) || (keys[keyIdx].keySize > VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR))
                {
                    testOk = false;
                    break;
                }
            }

            if (!testOk)
                break;
        }
    }

    return testOk ? tcu::TestStatus::pass("Passed") : tcu::TestStatus::pass("Failed");
}

class BaseTestCase : public vkt::TestCase
{
public:
    BaseTestCase(tcu::TestContext &testContext, const std::string &name, TestParams &&testParams)
        : vkt::TestCase(testContext, name)
        , m_testParams(std::move(testParams))
    {
    }
    virtual ~BaseTestCase(void) = default;

    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual void checkSupport(Context &context) const override;
    virtual TestInstance *createInstance(Context &context) const override;

protected:
    TestParams m_testParams;
};

void BaseTestCase::initPrograms(SourceCollections &programCollection) const
{
    if ((m_testParams.type == TestType::GRAPHICS_PIPELINE_FROM_INTERNAL_CACHE) ||
        (m_testParams.type == TestType::GRAPHICS_PIPELINE_WITH_ZERO_BINARY_COUNT))
    {
        programCollection.glslSources.add("vert")
            << glu::VertexSource("#version 450\n"
                                 "out gl_PerVertex { vec4 gl_Position; };\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "  const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
                                 "  const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
                                 "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
                                 "}\n");

        programCollection.glslSources.add("frag")
            << glu::FragmentSource("#version 450\n"
                                   "layout(location = 0) out highp vec4 fragColor;\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "  fragColor = vec4(1.0, 0.0, 1.0, 0.0);\n"
                                   "}\n");
    }
    else if ((m_testParams.type == TestType::CREATE_INCOMPLETE) || (m_testParams.type == TestType::NOT_ENOUGH_SPACE) ||
             (m_testParams.type == TestType::DESTROY_NULL_BINARY) ||
             (m_testParams.type == TestType::CREATE_WITH_ZERO_BINARY_COUNT) ||
             (m_testParams.type == TestType::COMPUTE_PIPELINE_FROM_INTERNAL_CACHE))
    {
        programCollection.glslSources.add("comp")
            << glu::ComputeSource("#version 310 es\n"
                                  "layout(local_size_x = 8) in;\n"
                                  "layout(binding = 0) writeonly buffer Output\n"
                                  "{\n"
                                  "  uint v[];\n"
                                  "} output_data;\n"
                                  "void main()\n"
                                  "{\n"
                                  "  output_data.v[gl_GlobalInvocationID.x] = gl_GlobalInvocationID.x;\n"
                                  "}");
    }
    else if ((m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE) ||
             (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_PIPELINE) ||
             (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_BINARY_DATA) ||
             (m_testParams.type == TestType::RAY_TRACING_PIPELINE_WITH_ZERO_BINARY_COUNT))
    {
        const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(
                   "#version 460 core\n"
                   "#extension GL_EXT_ray_tracing : require\n"
                   "layout(location = 0) rayPayloadEXT int payload;\n"
                   "layout(location = 0) callableDataEXT int callableIO;\n"

                   "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
                   "layout(set = 0, binding = 1, std430) writeonly buffer Result {\n"
                   "    int value[];\n"
                   "} result;\n"

                   "void main()\n"
                   "{\n"
                   "  float tmin        =  0.0;\n"
                   "  float tmax        = 10.0;\n"
                   "  vec3  origin      = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 2.0);\n"
                   "  vec3  direction   = vec3(0.0,0.0,-1.0);\n"
                   "  uint  resultIndex = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x;\n"

                   "  traceRayEXT(tlas, gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, 0, 0, 0, origin, tmin, direction, "
                   "tmax, 0);\n"
                   // to be able to display result in cherry this is interpreated as r8g8b8a8 during verification
                   // we are using only red but we need to add alpha (note: r and a may be swapped depending on endianness)
                   "  callableIO = 0;\n"
                   "  executeCallableEXT(0, 0);\n"
                   "  result.value[resultIndex] = payload + callableIO;\n" // 0xFF000000
                   "};\n")
            << buildOptions;

        programCollection.glslSources.add("isec") << glu::IntersectionSource("#version 460 core\n"
                                                                             "#extension GL_EXT_ray_tracing : require\n"

                                                                             "void main()\n"
                                                                             "{\n"
                                                                             "  if (gl_WorldRayOriginEXT.x < 4.0)\n"
                                                                             "    reportIntersectionEXT(2.0, 0);\n"
                                                                             "}\n")
                                                  << buildOptions;

        programCollection.glslSources.add("ahit")
            << glu::AnyHitSource("#version 460 core\n"
                                 "#extension GL_EXT_ray_tracing : require\n"
                                 "layout(location = 0) rayPayloadInEXT int payload;\n"
                                 "void main()\n"
                                 "{\n"
                                 "  payload = 128;\n"
                                 "}\n")
            << buildOptions;

        programCollection.glslSources.add("chit")
            << glu::ClosestHitSource("#version 460 core\n"
                                     "#extension GL_EXT_ray_tracing : require\n"
                                     "layout(location = 0) rayPayloadInEXT int payload;\n"
                                     "\n"
                                     "void main()\n"
                                     "{\n"
                                     "  payload = payload + 127;\n"
                                     "}\n")
            << buildOptions;

        programCollection.glslSources.add("miss")
            << glu::MissSource("#version 460 core\n"
                               "#extension GL_EXT_ray_tracing : require\n"
                               "layout(location = 0) rayPayloadInEXT int payload;\n"
                               "void main()\n"
                               "{\n"
                               "  payload = 128;\n"
                               "}\n")
            << buildOptions;

        programCollection.glslSources.add("call")
            << glu::CallableSource("#version 460 core\n"
                                   "#extension GL_EXT_ray_tracing : require\n"
                                   "layout(location = 0) callableDataInEXT int callableIO;\n"
                                   "void main()\n"
                                   "{\n"
                                   "  callableIO = callableIO + 0xFF000000;\n"
                                   "}\n")
            << buildOptions;
    }
    else if ((m_testParams.type == TestType::UNIQUE_KEY_PAIRS) || (m_testParams.type == TestType::VALID_KEY))
    {
        programCollection.glslSources.add("vert")
            << glu::VertexSource("#version 450\n"
                                 "layout(location = 0) in vec4 position;\n"
                                 "layout(location = 0) out highp vec4 vertColor;\n"
                                 "layout(constant_id = 0) const float vColor = 0.1;\n"
                                 "out gl_PerVertex { vec4 gl_Position; };\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "  vertColor = vec4(vColor * gl_VertexIndex);\n"
                                 "  gl_Position = position;\n"
                                 "}\n");
        programCollection.glslSources.add("frag")
            << glu::FragmentSource("#version 450\n"
                                   "layout(location = 0) in highp vec4 vertColor;\n"
                                   "layout(location = 0) out highp vec4 fragColor;\n"
                                   "layout(constant_id = 1) const float fColor = 0.1;\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "  fragColor = vertColor + vec4(fColor);\n"
                                   "}\n");
    }
}

void BaseTestCase::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    context.requireDeviceFunctionality("VK_KHR_pipeline_binary");
    checkPipelineConstructionRequirements(vki, physicalDevice, m_testParams.pipelineConstructionType);

    if ((m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE) ||
        (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_PIPELINE) ||
        (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_BINARY_DATA) ||
        (m_testParams.type == TestType::RAY_TRACING_PIPELINE_WITH_ZERO_BINARY_COUNT))
    {
        context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
        context.requireDeviceFunctionality("VK_KHR_buffer_device_address");
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
    }

    if (m_testParams.usePipelineLibrary)
        context.requireDeviceFunctionality("VK_KHR_pipeline_library");

    const auto &binaryProperties = context.getPipelineBinaryProperties();
    if ((m_testParams.type == TestType::GRAPHICS_PIPELINE_FROM_INTERNAL_CACHE) ||
        (m_testParams.type == TestType::COMPUTE_PIPELINE_FROM_INTERNAL_CACHE) ||
        (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE))
    {
        if (!binaryProperties.pipelineBinaryInternalCache)
            TCU_THROW(NotSupportedError, "pipelineBinaryInternalCache property not supported");
    }
}

TestInstance *BaseTestCase::createInstance(Context &context) const
{
    if ((m_testParams.type == TestType::CREATE_INCOMPLETE) || (m_testParams.type == TestType::NOT_ENOUGH_SPACE) ||
        (m_testParams.type == TestType::DESTROY_NULL_BINARY) ||
        (m_testParams.type == TestType::CREATE_WITH_ZERO_BINARY_COUNT))
        return new BasicComputePipelineTestInstance(context, m_testParams);
    if ((m_testParams.type == TestType::GRAPHICS_PIPELINE_FROM_INTERNAL_CACHE) ||
        (m_testParams.type == TestType::GRAPHICS_PIPELINE_WITH_ZERO_BINARY_COUNT))
        return new GraphicsPipelineInternalCacheTestInstance(context, m_testParams);
    if (m_testParams.type == TestType::COMPUTE_PIPELINE_FROM_INTERNAL_CACHE)
        return new ComputePipelineInternalCacheTestInstance(context);
    if ((m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE) ||
        (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_PIPELINE) ||
        (m_testParams.type == TestType::RAY_TRACING_PIPELINE_FROM_BINARY_DATA) ||
        (m_testParams.type == TestType::RAY_TRACING_PIPELINE_WITH_ZERO_BINARY_COUNT))
        return new RayTracingPipelineTestInstance(context, m_testParams);
    if (m_testParams.type == TestType::VALID_KEY)
        return new PipelineBinaryKeyTestInstance(context, m_testParams);

    return new UniqueKayPairsTestInstance(context, m_testParams);
}

} // namespace

de::MovePtr<tcu::TestCaseGroup> addPipelineBinaryDedicatedTests(tcu::TestContext &testCtx,
                                                                PipelineConstructionType pipelineConstructionType,
                                                                de::MovePtr<tcu::TestCaseGroup> binaryGroup)
{
    de::MovePtr<tcu::TestCaseGroup> dedicatedTests(new tcu::TestCaseGroup(testCtx, "dedicated"));
    dedicatedTests->addChild(
        new BaseTestCase(testCtx, "unique_key_pairs", {pipelineConstructionType, TestType::UNIQUE_KEY_PAIRS, false}));
    dedicatedTests->addChild(
        new BaseTestCase(testCtx, "graphics_pipeline_from_internal_cache",
                         {pipelineConstructionType, TestType::GRAPHICS_PIPELINE_FROM_INTERNAL_CACHE, false}));

    dedicatedTests->addChild(
        new BaseTestCase(testCtx, "valid_key", {pipelineConstructionType, TestType::VALID_KEY, false}));

    if (pipelineConstructionType == PipelineConstructionType::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        dedicatedTests->addChild(new BaseTestCase(testCtx, "create_incomplete",
                                                  {pipelineConstructionType, TestType::CREATE_INCOMPLETE, false}));
        dedicatedTests->addChild(new BaseTestCase(testCtx, "not_enough_space",
                                                  {pipelineConstructionType, TestType::NOT_ENOUGH_SPACE, false}));
        dedicatedTests->addChild(new BaseTestCase(testCtx, "destroy_null_binary",
                                                  {pipelineConstructionType, TestType::DESTROY_NULL_BINARY, false}));
        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "compute_pipeline_with_zero_binary_count",
                             {pipelineConstructionType, TestType::CREATE_WITH_ZERO_BINARY_COUNT, false}));
        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "compute_pipeline_from_internal_cache",
                             {pipelineConstructionType, TestType::COMPUTE_PIPELINE_FROM_INTERNAL_CACHE, false}));

        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "graphics_pipeline_with_zero_binary_count",
                             {pipelineConstructionType, TestType::GRAPHICS_PIPELINE_WITH_ZERO_BINARY_COUNT, false}));

        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "ray_tracing_pipeline_from_internal_cache",
                             {pipelineConstructionType, TestType::RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE, false}));
        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "ray_tracing_pipeline_from_pipeline",
                             {pipelineConstructionType, TestType::RAY_TRACING_PIPELINE_FROM_PIPELINE, false}));
        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "ray_tracing_pipeline_from_binary_data",
                             {pipelineConstructionType, TestType::RAY_TRACING_PIPELINE_FROM_BINARY_DATA, false}));

        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "ray_tracing_pipeline_library_from_internal_cache",
                             {pipelineConstructionType, TestType::RAY_TRACING_PIPELINE_FROM_INTERNAL_CACHE, true}));
        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "ray_tracing_pipeline_library_from_pipeline",
                             {pipelineConstructionType, TestType::RAY_TRACING_PIPELINE_FROM_PIPELINE, true}));
        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "ray_tracing_pipeline_library_from_binary_data",
                             {pipelineConstructionType, TestType::RAY_TRACING_PIPELINE_FROM_BINARY_DATA, true}));
        dedicatedTests->addChild(
            new BaseTestCase(testCtx, "ray_tracing_pipeline_with_zero_binary_count",
                             {pipelineConstructionType, TestType::RAY_TRACING_PIPELINE_WITH_ZERO_BINARY_COUNT, true}));
    }

    binaryGroup->addChild(dedicatedTests.release());
    return binaryGroup;
}

} // namespace vkt::pipeline
