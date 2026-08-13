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
 * \brief Data Graph Descriptor Buffer Tests
 */
/*--------------------------------------------------------------------*/

#include "vktDataGraphDescriptorBufferTests.hpp"

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

void checkDescriptorBufferSupport(Context &ctx, TestParams params)
{
    const auto &vki           = ctx.getInstanceInterface();
    const auto physicalDevice = ctx.getPhysicalDevice();

    VkPhysicalDeviceDataGraphFeaturesARM dataGraphFeaturesProp = initVulkanStructure();
    VkPhysicalDeviceFeatures2 featuresProp                     = initVulkanStructure(&dataGraphFeaturesProp);
    vki.getPhysicalDeviceFeatures2(physicalDevice, &featuresProp);

    if (!dataGraphFeaturesProp.dataGraphDescriptorBuffer)
    {
        TCU_THROW(NotSupportedError, "descriptor buffer feature for data graph not present");
    }

    TestParams::checkSupport(ctx, params);
}

tcu::TestStatus descriptorBufferTest(Context &m_context, TestParams m_params)
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
            // The test assumes the descriptor buffer has descriptorSet equal to 0
            DE_ASSERT(ri.descriptorSet == 0);
            setLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_ALL, ri.binding);
        }
    }
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        setLayoutBuilder.build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    /* Create DataGraph pipeline */

    DataGraphPipelineWrapper pipeline(vk, device);
    pipeline.setPipelineCreateFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
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

    /* Create Descriptor Buffer */

    VkDeviceSize descriptorBufferSize;
    vk.getDescriptorSetLayoutSizeEXT(device, *descriptorSetLayout, &descriptorBufferSize);

    const auto descriptorBufferUsage =
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    const VkBufferCreateInfo descriptorBufferCreateInfo =
        makeBufferCreateInfo(descriptorBufferSize, descriptorBufferUsage);
    const MemoryRequirement bufferMemoryRequirement = MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress;

    BufferWithMemory descriptorBuffer(vk, device, allocator, descriptorBufferCreateInfo, bufferMemoryRequirement);
    auto descriptorBufferHostPtr = static_cast<char *>(descriptorBuffer.getAllocation().getHostPtr());

    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);

        if (ri.isTensor())
        {
            VkDeviceSize offset = 0;
            vk.getDescriptorSetLayoutBindingOffsetEXT(device, *descriptorSetLayout, ri.binding, &offset);

            VkDescriptorGetTensorInfoARM tensorDescriptorInfo{};
            tensorDescriptorInfo.sType      = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_TENSOR_INFO_ARM;
            tensorDescriptorInfo.pNext      = nullptr;
            tensorDescriptorInfo.tensorView = tr.view.get();

            VkDescriptorGetInfoEXT descriptorInfo{};
            descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
            descriptorInfo.type  = VK_DESCRIPTOR_TYPE_TENSOR_ARM;
            descriptorInfo.pNext = &tensorDescriptorInfo;

            constexpr uint32_t EXPECTED_TENSOR_DESCRIPTOR_SIZE = 64;
            vk.getDescriptorEXT(device, &descriptorInfo, EXPECTED_TENSOR_DESCRIPTOR_SIZE,
                                descriptorBufferHostPtr + offset);
        }
    }
    flushAlloc(vk, device, descriptorBuffer.getAllocation());

    /* Start recording commands */

    beginCommandBuffer(vk, cmdBuffer.get());
    pipeline.bind(cmdBuffer.get());

    /* Bind descriptor buffer */

    {
        VkBufferDeviceAddressInfo deviceAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                    *descriptorBuffer};
        auto bufferDeviceAddress = vk.getBufferDeviceAddress(device, &deviceAddressInfo);

        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo = initVulkanStructure();
        descriptorBufferBindingInfo.address                          = bufferDeviceAddress;
        descriptorBufferBindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

        vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1, &descriptorBufferBindingInfo);

        uint32_t bufferIndex = 0;
        VkDeviceSize offset  = 0;
        vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM,
                                            pipeline.getPipelineLayout(), 0, 1, &bufferIndex, &offset);
    }

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

} // namespace

void descriptorBufferTestsGroup(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations = getTestParamsVariations();
    for (const auto &params : paramsVariations)
    {
        addFunctionCase(group, de::toString(params), checkDescriptorBufferSupport, descriptorBufferTest, params);
    }
}

} // namespace dataGraph
} // namespace vkt
