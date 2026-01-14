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
 * \brief Data Graph Properties Tests
 */
/*--------------------------------------------------------------------*/

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <memory>
#include <vector>

#include "vkBuilderUtil.hpp"
#include "vkDataGraphPipelineConstructionUtil.hpp"
#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"

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

enum class queryNumCallModes
{
    SINGLE_CALL,
    MULTIPLE_CALLS,
};

enum class queryReturnModes
{
    COMPLETE,
    INCOMPLETE,
};

struct availablePropertiesTestParams
{
    TestParams testParams;
    queryReturnModes queryReturnMode;
};

struct getPropertiesTestParams
{
    TestParams testParams;
    queryNumCallModes queryNumCallMode;
    queryReturnModes queryReturnMode;
};

std::ostream &operator<<(std::ostream &os, availablePropertiesTestParams params)
{
    switch (params.queryReturnMode)
    {
    case queryReturnModes::COMPLETE:
        os << "complete";
        break;
    case queryReturnModes::INCOMPLETE:
        os << "incomplete";
        break;
    default:
        break;
    }

    os << "_" << params.testParams;

    return os;
}

std::ostream &operator<<(std::ostream &os, getPropertiesTestParams params)
{
    switch (params.queryNumCallMode)
    {
    case queryNumCallModes::SINGLE_CALL:
        os << "singleCall";
        break;
    case queryNumCallModes::MULTIPLE_CALLS:
        os << "multiCalls";
        break;
    default:
        break;
    }

    os << "_";

    switch (params.queryReturnMode)
    {
    case queryReturnModes::COMPLETE:
        os << "complete";
        break;
    case queryReturnModes::INCOMPLETE:
        os << "incomplete";
        break;
    default:
        break;
    }

    os << "_" << params.testParams;

    return os;
}

void checkSupport(Context &ctx, availablePropertiesTestParams params)
{
    TestParams::checkSupport(ctx, params.testParams);
}

void checkSupport(Context &ctx, getPropertiesTestParams params)
{
    TestParams::checkSupport(ctx, params.testParams);
}

tcu::TestStatus availablePropertiesTest(Context &ctx, availablePropertiesTestParams params)
{
    const DeviceInterface &vk = ctx.getDeviceInterface();
    const VkDevice device     = ctx.getDevice();
    Allocator &allocator      = ctx.getDefaultAllocator();

    // getDataGraphTest cannot return nullptr as will throw an exception in case of errors
    std::unique_ptr<DataGraphTest> graphTest{DataGraphTestProvider::getDataGraphTest(ctx, "TOSA", params.testParams)};
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

    /* get number of properties */

    VkDataGraphPipelineInfoARM pipelineInfo = initVulkanStructure();
    pipelineInfo.dataGraphPipeline          = pipeline.get();

    uint32_t numProperties = 0;
    VK_CHECK(vk.getDataGraphPipelineAvailablePropertiesARM(device, &pipelineInfo, &numProperties, nullptr));

    if (numProperties == 0)
    {
        return tcu::TestStatus::pass("test succeeded");
    }

    std::vector<VkDataGraphPipelinePropertyARM> dataGraphPipelineProperties(numProperties);

    if (params.queryReturnMode == queryReturnModes::COMPLETE)
    {
        VK_CHECK(vk.getDataGraphPipelineAvailablePropertiesARM(device, &pipelineInfo, &numProperties,
                                                               dataGraphPipelineProperties.data()));
    }
    else if (params.queryReturnMode == queryReturnModes::INCOMPLETE)
    {
        numProperties--;
        VK_CHECK_INCOMPLETE(vk.getDataGraphPipelineAvailablePropertiesARM(device, &pipelineInfo, &numProperties,
                                                                          dataGraphPipelineProperties.data()));
    }

    return tcu::TestStatus::pass("test succeeded");
}

tcu::TestStatus getPropertiesTest(Context &ctx, getPropertiesTestParams params)
{
    const DeviceInterface &vk = ctx.getDeviceInterface();
    const VkDevice device     = ctx.getDevice();
    Allocator &allocator      = ctx.getDefaultAllocator();

    // getDataGraphTest cannot return nullptr as will throw an exception in case of errors
    std::unique_ptr<DataGraphTest> graphTest{DataGraphTestProvider::getDataGraphTest(ctx, "TOSA", params.testParams)};
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

    /* get number of properties */

    VkDataGraphPipelineInfoARM pipelineInfo = initVulkanStructure();
    pipelineInfo.dataGraphPipeline          = pipeline.get();

    uint32_t numProperties = 0;
    vk.getDataGraphPipelineAvailablePropertiesARM(device, &pipelineInfo, &numProperties, nullptr);

    if (numProperties == 0)
    {
        return tcu::TestStatus::pass("test succeeded");
    }

    /* get properties */

    std::vector<VkDataGraphPipelinePropertyARM> dataGraphPipelineProperties(numProperties);
    vk.getDataGraphPipelineAvailablePropertiesARM(device, &pipelineInfo, &numProperties,
                                                  dataGraphPipelineProperties.data());

    /* query memory requirements for all properties */

    std::vector<VkDataGraphPipelinePropertyQueryResultARM> dataGraphPropertyQueries(numProperties);

    for (size_t i = 0; i < numProperties; i++)
    {
        dataGraphPropertyQueries[i]          = initVulkanStructure();
        dataGraphPropertyQueries[i].property = dataGraphPipelineProperties[i];
    }

    if (params.queryNumCallMode == queryNumCallModes::SINGLE_CALL)
    {
        vk.getDataGraphPipelinePropertiesARM(device, &pipelineInfo, numProperties, dataGraphPropertyQueries.data());

        /* allocate memory for queries result */

        size_t totalMemoryRequirement = 0;
        for (auto &dataGraphPropertyQuery : dataGraphPropertyQueries)
        {
            if (dataGraphPropertyQuery.dataSize == 0 && dataGraphPropertyQuery.isText)
            {
                return tcu::TestStatus::fail("dataSize = 0 and isText is VK_TRUE. No space available to terminate the "
                                             "string with a NUL character");
            }

            // only reduce the required memory if the remaining value is > 0
            if (params.queryReturnMode == queryReturnModes::INCOMPLETE && dataGraphPropertyQuery.dataSize > 1)
            {
                dataGraphPropertyQuery.dataSize--;
            }

            totalMemoryRequirement += dataGraphPropertyQuery.dataSize;
        }

        if (totalMemoryRequirement == 0 && params.queryReturnMode == queryReturnModes::INCOMPLETE)
        {
            throw tcu::NotSupportedError("Not possible to query propoerties with less than required memory");
        }

        constexpr char initVal = 0x7F;
        std::vector<uint8_t> queriesResultData(totalMemoryRequirement, initVal);

        size_t queriesResultDataOffset = 0;
        for (auto &dataGraphPropertyQuery : dataGraphPropertyQueries)
        {
            dataGraphPropertyQuery.pData = &queriesResultData[queriesResultDataOffset];
            queriesResultDataOffset += dataGraphPropertyQuery.dataSize;
        }

        /* get the properties */

        auto res =
            vk.getDataGraphPipelinePropertiesARM(device, &pipelineInfo, numProperties, dataGraphPropertyQueries.data());

        if (params.queryReturnMode == queryReturnModes::INCOMPLETE)
        {
            VK_CHECK_INCOMPLETE(res);
        }
        else
        {
            VK_CHECK(res);
        }

        /* verify that the properties has been written */

        for (auto &dataGraphPropertyQuery : dataGraphPropertyQueries)
        {
            /* check that the data does not contain initData */
            const auto rawDataBegin = static_cast<char *>(dataGraphPropertyQuery.pData);
            const auto rawDataEnd   = rawDataBegin + dataGraphPropertyQuery.dataSize;
            bool success            = std::find(rawDataBegin, rawDataEnd, initVal) == rawDataEnd;
            if (!success)
            {
                std::ostringstream msg;
                return tcu::TestStatus::fail("Property data not written");
            }
        }
    }
    else
    {
        for (auto &dataGraphPropertyQuery : dataGraphPropertyQueries)
        {
            if (dataGraphPropertyQuery.dataSize == 0 && dataGraphPropertyQuery.isText)
            {
                return tcu::TestStatus::fail("dataSize = 0 and isText is VK_TRUE. No space available to terminate the "
                                             "string with a NUL character");
            }

            // only reduce the required memory if the remaining value is > 0
            if (params.queryReturnMode == queryReturnModes::INCOMPLETE)
            {
                if (dataGraphPropertyQuery.dataSize > 1)
                {
                    dataGraphPropertyQuery.dataSize--;
                }
                else
                {
                    continue;
                }
            }

            auto res = vk.getDataGraphPipelinePropertiesARM(device, &pipelineInfo, 1, &dataGraphPropertyQuery);

            if (params.queryReturnMode == queryReturnModes::INCOMPLETE)
            {
                VK_CHECK_INCOMPLETE(res);
            }
            else
            {
                VK_CHECK(res);
            }
        }
    }

    return tcu::TestStatus::pass("test succeeded");
}

} // namespace

void availablePropertiesTests(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations = getTestParamsVariations();
    for (const auto &params : paramsVariations)
    {

        for (auto queryReturnMode : {
                 queryReturnModes::COMPLETE,
                 queryReturnModes::INCOMPLETE,
             })
        {
            availablePropertiesTestParams availablePropertiesTestParam = {params, queryReturnMode};
            addFunctionCase(group, de::toString(availablePropertiesTestParam), checkSupport, availablePropertiesTest,
                            availablePropertiesTestParam);
        }
    }
}

void getPropertiesTests(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations = getTestParamsVariations();
    for (const auto &params : paramsVariations)
    {
        for (auto queryNumCallMode : {
                 queryNumCallModes::SINGLE_CALL,
                 queryNumCallModes::MULTIPLE_CALLS,
             })
        {
            for (auto queryReturnMode : {
                     queryReturnModes::COMPLETE,
                     queryReturnModes::INCOMPLETE,
                 })
            {
                getPropertiesTestParams getPropertiesTestParam = {params, queryNumCallMode, queryReturnMode};
                addFunctionCase(group, de::toString(getPropertiesTestParam), checkSupport, getPropertiesTest,
                                getPropertiesTestParam);
            }
        }
    }
}

void propertiesTestsGroup(tcu::TestCaseGroup *group)
{
    addTestGroup(group, "available", availablePropertiesTests);
    addTestGroup(group, "get", getPropertiesTests);
}

} // namespace dataGraph
} // namespace vkt
