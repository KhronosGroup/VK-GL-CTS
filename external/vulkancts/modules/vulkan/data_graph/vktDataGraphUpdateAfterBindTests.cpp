/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 ARM Ltd.
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
 * \brief Data Graph Update After Bind Tests
 */
/*--------------------------------------------------------------------*/

#include "vktDataGraphUpdateAfterBindTests.hpp"

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

void checkUpdateAfterBindSupport(Context &ctx, TestParams params)
{
    const auto &vki           = ctx.getInstanceInterface();
    const auto physicalDevice = ctx.getPhysicalDevice();

    VkPhysicalDeviceDataGraphFeaturesARM dataGraphFeaturesProp = initVulkanStructure();
    VkPhysicalDeviceFeatures2 featuresProp                     = initVulkanStructure(&dataGraphFeaturesProp);
    vki.getPhysicalDeviceFeatures2(physicalDevice, &featuresProp);

    if (!dataGraphFeaturesProp.dataGraphUpdateAfterBind)
    {
        TCU_THROW(NotSupportedError, "update after bind feature for data graph not present");
    }

    TestParams::checkSupport(ctx, params);
}

tcu::TestStatus updateAfterBindTest(Context &m_context, TestParams m_params)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

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
            tr.tensor = de::MovePtr<TensorWithMemory>(new TensorWithMemory(
                vk, device, allocator, makeTensorCreateInfo(&tr.desc), vk::MemoryRequirement::Any));
            tr.view   = makeTensorView(vk, device, tr.tensor->get(), ri.params.format);

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
            setLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_ALL, ri.binding,
                                                     VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
        }
    }
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        setLayoutBuilder.build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT));

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM, static_cast<uint32_t>(graphTest->numTensors()));
    const Unique<VkDescriptorPool> descriptorPool(poolBuilder.build(
        vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

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
    const DataGraphSessionWithMemory dataGraphSession(vk, device, allocator, sessionCreateInfo,
                                                      vk::MemoryRequirement::Any);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, cmdBuffer.get());

    pipeline.bind(cmdBuffer.get());
    vk.cmdBindDescriptorSets(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM, pipeline.getPipelineLayout(), 0u,
                             1u, &descriptorSet.get(), 0u, nullptr);

    vk.cmdDispatchDataGraphARM(cmdBuffer.get(), *dataGraphSession, nullptr);

    // Update after bind

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

} // namespace

void updateAfterBindTestsGroup(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations = getTestParamsVariations();
    for (const auto &params : paramsVariations)
    {
        addFunctionCase(group, de::toString(params), checkUpdateAfterBindSupport, updateAfterBindTest, params);
    }
}

} // namespace dataGraph
} // namespace vkt
