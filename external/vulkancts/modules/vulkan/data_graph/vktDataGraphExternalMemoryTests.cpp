/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Data Graph Basic Tests
 */
/*--------------------------------------------------------------------*/

#include "vktDataGraphExternalMemoryTests.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <memory>
#include <vector>

#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDataGraphPipelineConstructionUtil.hpp"
#include "vkDataGraphSessionWithMemory.hpp"
#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"

#include "../tensor/vktTensorTestsUtil.hpp"
#include "vktDataGraphTestProvider.hpp"
#include "vktDataGraphTestUtil.hpp"
#include "vkTensorMemoryUtil.hpp"
#include "vkTensorWithMemory.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;
using namespace std::placeholders;

namespace vkt
{

namespace dataGraph
{

namespace
{

tcu::TestStatus submitPipelineDmaBufTest(Context &m_context, TestParams m_params)
{
    const InstanceInterface &vki    = m_context.getInstanceInterface();
    const VkPhysicalDevice physDev  = m_context.getPhysicalDevice();
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vki.getPhysicalDeviceMemoryProperties(physDev, &memoryProperties);
    DmaHeapAllocator dmaHeapAllocator(vk, device, memoryProperties);

    // getDataGraphTest cannot return nullptr as will throw an exception in case of errors
    std::unique_ptr<DataGraphTest> graphTest{DataGraphTestProvider::getDataGraphTest(m_context, "TOSA", m_params)};
    std::vector<DataGraphTestResource> testResources(graphTest->numResources());

    /* Create tensors */

    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);

        tr.dimensions = ri.params.dimensions;
        tr.strides    = ri.params.strides;
        tr.desc       = makeTensorDescription(ri.params.tiling, ri.params.format, tr.dimensions, tr.strides,
                                              VK_TENSOR_USAGE_DATA_GRAPH_BIT_ARM);

        if (ri.isTensor())
        {
            /* create tensor and view */
            VkTensorCreateInfoARM tensorCreateInfo                 = makeTensorCreateInfo(&tr.desc);
            VkExternalMemoryTensorCreateInfoARM externalCreateInfo = initVulkanStructure();
            externalCreateInfo.handleTypes                         = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
            tensorCreateInfo.pNext                                 = &externalCreateInfo;

            tr.tensor = de::MovePtr<TensorWithMemory>(
                new TensorWithMemory(vk, device, dmaHeapAllocator, tensorCreateInfo, vk::MemoryRequirement::Any));
            tr.view = makeTensorView(vk, device, tr.tensor->get(), ri.params.format);

            /* fill host and tensor data */
            graphTest->initData(i, &*tr.tensor);
        }
        else
        {
            /* fill only host data, e.g. for constants */
            graphTest->initData(i, nullptr, {0, ri.sparsityInfo});
        }
    }

    /* Create descriptor set */

    DescriptorSetLayoutBuilder setLayoutBuilder;
    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        if (ri.isTensor())
        {
            /* constants do not need to be in the descriptor set */
            setLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_ALL, ri.binding);
        }
    }
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(setLayoutBuilder.build(vk, device));

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM, static_cast<uint32_t>(graphTest->numTensors()));
    const Unique<VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    DescriptorSetUpdateBuilder updatebuilder;
    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);
        if (ri.isTensor())
        {
            tr.writeDesc = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr, 1, &tr.view.get()};
            updatebuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(ri.binding),
                                      VK_DESCRIPTOR_TYPE_TENSOR_ARM, &tr.writeDesc);
        }
    }
    updatebuilder.update(vk, device);

    /* Create DataGraph pipeline */

    DataGraphPipelineWrapper pipeline(vk, device);
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.addShaderModule(graphTest->shaderModule());

    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);
        if (ri.isTensor())
        {
            pipeline.addTensor(tr.desc, ri.descriptorSet, ri.binding);
        }
        else
        {
            pipeline.addConstant(tr.desc, ri.hostData, ri.id, ri.sparsityInfo);
        }
    }
    pipeline.buildPipeline(VK_NULL_HANDLE);

    /* Create DataGraph pipeline session */

    VkDataGraphPipelineSessionCreateInfoARM sessionCreateInfo = initVulkanStructure();
    sessionCreateInfo.dataGraphPipeline                       = pipeline.get();
    const DataGraphSessionWithMemory dataGraphSession(vk, device, dmaHeapAllocator, sessionCreateInfo,
                                                      vk::MemoryRequirement::Any, m_params.sessionMemory);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, cmdBuffer.get());

    pipeline.bind(cmdBuffer.get());
    vk.cmdBindDescriptorSets(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM, pipeline.getPipelineLayout(), 0u,
                             1u, &descriptorSet.get(), 0u, nullptr);

    vk.cmdDispatchDataGraphARM(cmdBuffer.get(), *dataGraphSession, nullptr);

    endCommandBuffer(vk, cmdBuffer.get());

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    // Validate the results

    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);

        if (ri.isTensor() && ri.requiresVerify())
        {
            auto testStatus = graphTest->verifyData(i, &*tr.tensor);
            if (testStatus.isFail())
            {
                return testStatus;
            }
        }
    }

    return tcu::TestStatus::pass("test succeeded");
}

void checkSupport(Context &ctx, TestParams params)
{
    TestParams::checkSupport(ctx, params);

    ctx.requireDeviceFunctionality("VK_EXT_external_memory_dma_buf");
    if (!vk::DmaHeapAllocator::isSupported())
    {
        TCU_THROW(NotSupportedError, "Target does not support DMA heap allocator");
    }

    std::unique_ptr<DataGraphTest> graphTest{DataGraphTestProvider::getDataGraphTest(ctx, "TOSA", params)};
    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        if (ri.isTensor())
        {
            const VkTensorDescriptionARM tensorDescription =
                makeTensorDescription(ri.params.tiling, ri.params.format, ri.params.dimensions, ri.params.strides,
                                      VK_TENSOR_USAGE_DATA_GRAPH_BIT_ARM);

            if (!tensor::tensorSupportsDmaBufImport(ctx, tensorDescription))
            {
                TCU_THROW(NotSupportedError, "Target tensor description does not support DMA BUF imported memory");
            }
        }
    }
}

} // namespace

void submitPipelineDmaBufTests(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations = getTestParamsVariations();
    for (const auto &params : paramsVariations)
    {
        addFunctionCase(group, de::toString(params), checkSupport, submitPipelineDmaBufTest, params);
    }
}

void externalMemoryTestsGroup(tcu::TestCaseGroup *group)
{
    addTestGroup(group, "dma_buf", submitPipelineDmaBufTests);
}

} // namespace dataGraph
} // namespace vkt
