/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 Arm Ltd.
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
 * \brief Data Graph Pipeline Cache Tests
 */
/*--------------------------------------------------------------------*/

#include "vktDataGraphPipelineCacheTests.hpp"

#include "../tensor/vktTensorTestsUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vktDataGraphTestUtil.hpp"
#include "vktDataGraphTestProvider.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTensorWithMemory.hpp"
#include "vkDataGraphSessionWithMemory.hpp"
#include "vkDataGraphPipelineConstructionUtil.hpp"
#include "vkTensorMemoryUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <vector>

using namespace vk;
using namespace std::placeholders;

namespace vkt
{

namespace dataGraph
{

namespace
{

enum CacheTestPipelineMode
{
    FILL_CACHE,
    HIT_CACHE,
    MISS_CACHE,
};

enum CacheFailureMode
{
    IGNORE_CACHE_MISS,
    FAIL_ON_CACHE_MISS_NO_EARLY_RETURN,
    FAIL_ON_CACHE_MISS_EARLY_RETURN,
};

struct CacheTestParams
{
    TestParams testParams;
    CacheFailureMode failureMode;
    std::vector<CacheTestPipelineMode> cacheModes;
};

std::ostream &operator<<(std::ostream &os, CacheTestParams params)
{
    switch (params.failureMode)
    {
    case IGNORE_CACHE_MISS:
        os << "ignoreMiss";
        break;
    case FAIL_ON_CACHE_MISS_NO_EARLY_RETURN:
        os << "failOnMissNoEarlyReturn";
        break;
    case FAIL_ON_CACHE_MISS_EARLY_RETURN:
        os << "failOnMissEarlyReturn";
        break;
    default:
        break;
    }

    os << "_";

    for (const auto &cacheMode : params.cacheModes)
    {
        switch (cacheMode)
        {
        case FILL_CACHE:
            os << "Fill";
            break;
        case HIT_CACHE:
            os << "Hit";
            break;
        case MISS_CACHE:
            os << "Miss";
            break;
        default:
            break;
        }
    }

    os << "_" << params.testParams;

    return os;
}

void checkSupport(Context &ctx, CacheTestParams params)
{

    const auto &vki           = ctx.getInstanceInterface();
    const auto physicalDevice = ctx.getPhysicalDevice();

    VkPhysicalDevicePipelineCreationCacheControlFeatures cacheControlFeatures = initVulkanStructure();
    VkPhysicalDeviceFeatures2 featuresProp = initVulkanStructure(&cacheControlFeatures);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &featuresProp);

    if (!cacheControlFeatures.pipelineCreationCacheControl)
    {
        TCU_THROW(NotSupportedError, "pipeline creation cache control feature not present");
    }

    TestParams::checkSupport(ctx, params.testParams);
}

tcu::TestStatus createPipelineMultiCallsTest(Context &ctx, CacheTestParams params)
{
    const DeviceInterface &vk = ctx.getDeviceInterface();
    const VkDevice device     = ctx.getDevice();
    Allocator &allocator      = ctx.getDefaultAllocator();
    const size_t numPipelines = params.cacheModes.size();

    std::vector<de::MovePtr<DataGraphTest>> tests;
    std::vector<std::vector<DataGraphTestResource>> testsResources;
    std::vector<Move<VkPipelineCache>> pipelineCaches;

    pipelineCaches.reserve(numPipelines);

    size_t referenceCacheIndex = 0;

    for (size_t i = 0; i < numPipelines; i++)
    {
        // getDataGraphTest cannot return nullptr as will throw an exception in case of errors
        const auto &test = DataGraphTestProvider::getDataGraphTest(ctx, "TOSA", params.testParams);
        std::vector<DataGraphTestResource> testResources(test->numResources());

        /* create tensors */

        for (size_t r = 0; r < test->numResources(); r++)
        {
            const auto &ri = test->resourceInfo(r);
            auto &tr       = testResources.at(r);

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
                test->initData(r, &*tr.tensor);
            }
            else
            {
                /* fill only host data, e.g. for constants */
                test->initData(r, nullptr, {0, ri.sparsityInfo});
            }
        }

        /* create descriptor set */

        DescriptorSetLayoutBuilder setLayoutBuilder;
        for (size_t r = 0; r < test->numResources(); r++)
        {
            const auto &ri = test->resourceInfo(r);
            if (ri.isTensor())
            {
                /* constants do not need to be in the descriptor set */
                setLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_ALL,
                                                         ri.binding);
            }
        }
        const Unique<VkDescriptorSetLayout> descriptorSetLayout(setLayoutBuilder.build(vk, device));

        /* create pipeline chache */

        const VkPipelineCacheCreateInfo cacheCreateInfo = initVulkanStructure();
        pipelineCaches.emplace_back(createPipelineCache(vk, device, &cacheCreateInfo));

        if (params.cacheModes.at(i) == FILL_CACHE)
        {
            referenceCacheIndex = i;
        }

        // in order for a pipeline to miss the cache we use a newly created one (empty)
        auto &pipelineCache =
            (params.cacheModes.at(i) == MISS_CACHE) ? pipelineCaches.at(i) : pipelineCaches.at(referenceCacheIndex);

        /* Create DataGraph pipeline */

        std::vector<VkDataGraphPipelineResourceInfoARM> graph_resources{};
        std::vector<VkDataGraphPipelineConstantARM> graph_constants{};
        for (size_t resId = 0; resId < test->numResources(); resId++)
        {
            const auto &ri = test->resourceInfo(resId);
            auto &tr       = testResources.at(resId);
            if (ri.isTensor())
            {
                VkDataGraphPipelineResourceInfoARM pplRes = initVulkanStructure();
                pplRes.pNext                              = &tr.desc;
                pplRes.descriptorSet                      = ri.descriptorSet;
                pplRes.binding                            = ri.binding;
                graph_resources.push_back(pplRes);
            }
            else
            {
                VkDataGraphPipelineConstantARM pplConst = initVulkanStructure();
                pplConst.pNext                          = &tr.desc;
                pplConst.id                             = ri.id;
                pplConst.pConstantData                  = ri.hostData;
                graph_constants.push_back(pplConst);
            }
        }

        Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device, descriptorSetLayout.get());

        Move<VkShaderModule> shaderModule = test->shaderModule();
        std::vector<uint32_t> binary      = test->spirvBinary();

        void *pNextPipelineCreateInfo = nullptr;

        VkPipelineCreationFeedback pipelineCreationFeedback       = {};
        VkPipelineCreationFeedbackCreateInfo creationFeedbackInfo = initVulkanStructure();
        creationFeedbackInfo.pNext                                = pNextPipelineCreateInfo;
        creationFeedbackInfo.sType                     = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO;
        creationFeedbackInfo.pPipelineCreationFeedback = &pipelineCreationFeedback;
        creationFeedbackInfo.pipelineStageCreationFeedbackCount = 0;
        creationFeedbackInfo.pPipelineStageCreationFeedbacks    = nullptr;
        pNextPipelineCreateInfo                                 = &creationFeedbackInfo;

        VkDataGraphPipelineShaderModuleCreateInfoARM dataGraphShaderModuleInfo = initVulkanStructure();
        dataGraphShaderModuleInfo.pNext                                        = pNextPipelineCreateInfo;
        dataGraphShaderModuleInfo.constantCount = static_cast<uint32_t>(graph_constants.size());
        dataGraphShaderModuleInfo.pConstants    = graph_constants.data();
        dataGraphShaderModuleInfo.pName         = "main";
        dataGraphShaderModuleInfo.module        = shaderModule.get();
        pNextPipelineCreateInfo                 = &dataGraphShaderModuleInfo;

        VkDataGraphPipelineCreateInfoARM pipelineCreateInfo = initVulkanStructure();
        pipelineCreateInfo.pNext                            = pNextPipelineCreateInfo;
        pipelineCreateInfo.layout                           = pipelineLayout.get();
        pipelineCreateInfo.resourceInfoCount                = static_cast<uint32_t>(graph_resources.size());
        pipelineCreateInfo.pResourceInfos                   = graph_resources.data();

        if (params.failureMode != IGNORE_CACHE_MISS &&
            (params.cacheModes.at(i) == HIT_CACHE || params.cacheModes.at(i) == MISS_CACHE))
        {
            pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT;
        }

        if (params.cacheModes.at(i) == FILL_CACHE)
        {
            auto pipeline = createDataGraphPipelineARM(vk, device, VK_NULL_HANDLE, pipelineCache.get(),
                                                       &pipelineCreateInfo, nullptr);

            DE_ASSERT(pipelineCreationFeedback.flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT);

            if ((pipelineCreationFeedback.flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT) !=
                0)
            {
                return tcu::TestStatus::fail("Pipeline expected to not hit cache but the relative bit in the pipeline "
                                             "creation feedback flags is set.");
            }
        }
        else if (params.cacheModes.at(i) == HIT_CACHE)
        {
            auto pipeline =
                createDataGraphPipelineARM(vk, device, VK_NULL_HANDLE, pipelineCache.get(), &pipelineCreateInfo);

            DE_ASSERT(pipelineCreationFeedback.flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT);

            if ((pipelineCreationFeedback.flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT) ==
                0)
            {
                return tcu::TestStatus::fail("Pipeline expected to hit cache but the relative bit in the pipeline "
                                             "creation feedback flags is not set.");
            }
        }
        else if (params.cacheModes.at(i) == MISS_CACHE)
        {
            if (params.failureMode == IGNORE_CACHE_MISS)
            {
                auto pipeline =
                    createDataGraphPipelineARM(vk, device, VK_NULL_HANDLE, pipelineCache.get(), &pipelineCreateInfo);

                DE_ASSERT(pipelineCreationFeedback.flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT);

                if ((pipelineCreationFeedback.flags &
                     VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT) != 0)
                {
                    return tcu::TestStatus::fail("Pipeline expected to not hit cache but the relative bit in the "
                                                 "pipeline creation feedback flags is set.");
                }
            }
            else
            {
                // cache missed and failure expected
                VkPipeline object = VK_NULL_HANDLE;
                VK_CHECK_COMPILE_REQUIRED(vk.createDataGraphPipelinesARM(device, VK_NULL_HANDLE, pipelineCache.get(),
                                                                         1u, &pipelineCreateInfo, nullptr, &object));
                checkIsNull(object);
            }
        }
    }

    return tcu::TestStatus::pass("test succeeded");
}

tcu::TestStatus createPipelineSingleCallTest(Context &ctx, CacheTestParams params)
{
    const DeviceInterface &vk = ctx.getDeviceInterface();
    const VkDevice device     = ctx.getDevice();
    Allocator &allocator      = ctx.getDefaultAllocator();
    const size_t numPipelines = params.cacheModes.size();

    std::vector<de::MovePtr<DataGraphSessionWithMemory>> sessions;
    std::vector<de::MovePtr<DataGraphTest>> tests;
    std::vector<Move<VkDescriptorPool>> descriptorPools;
    std::vector<Move<VkDescriptorSet>> descriptorSets;
    std::vector<Move<VkDescriptorSetLayout>> descriptorSetLayouts;
    std::vector<Move<VkPipelineCache>> pipelineCaches;
    std::vector<Move<VkPipelineLayout>> pipelineLayouts;
    std::vector<Move<VkShaderModule>> shaderModules;
    std::vector<std::vector<DataGraphTestResource>> testsResources;

    std::vector<std::vector<VkDataGraphPipelineConstantARM>> graphsConstants{};
    std::vector<std::vector<VkDataGraphPipelineResourceInfoARM>> graphsResources{};
    std::vector<VkDataGraphPipelineCreateInfoARM> pipelineCreateInfos;
    std::vector<VkDataGraphPipelineShaderModuleCreateInfoARM> pipelineShaderModuleInfos;
    std::vector<VkPipelineCreationFeedbackCreateInfo> pipelineCreationFeedbackInfos;
    std::vector<VkPipelineCreationFeedback> pipelineCreationFeedbacks;
    std::vector<VkPipeline> pipelines;

    sessions.reserve(numPipelines);
    tests.reserve(numPipelines);
    descriptorPools.reserve(numPipelines);
    descriptorSets.reserve(numPipelines);
    descriptorSetLayouts.reserve(numPipelines);
    pipelineCaches.reserve(numPipelines);
    pipelineLayouts.reserve(numPipelines);
    shaderModules.reserve(numPipelines);
    testsResources.reserve(numPipelines);

    graphsConstants.resize(numPipelines);
    graphsResources.resize(numPipelines);
    pipelineCreateInfos.resize(numPipelines);
    pipelineShaderModuleInfos.resize(numPipelines);
    pipelineCreationFeedbackInfos.resize(numPipelines);
    pipelineCreationFeedbacks.resize(numPipelines);
    pipelines.resize(numPipelines, VK_NULL_HANDLE);

    // keep track is one of the pipeline will miss the cache
    bool testContainsCacheMiss = false;

    // to miss the cache, change the first resource's strides for this specific pipeline
    auto cacheMissTestParams = params.testParams;

    // change strides to trigger cache miss
    cacheMissTestParams.tiling = VK_TENSOR_TILING_LINEAR_ARM;
    if (cacheMissTestParams.strides.inputs == TENSOR_STRIDES_NOT_PACKED)
    {
        cacheMissTestParams.strides.inputs = TENSOR_STRIDES_PACKED;
    }
    else
    {
        cacheMissTestParams.strides.inputs = TENSOR_STRIDES_NOT_PACKED;
    }

    for (size_t i = 0; i < numPipelines; i++)
    {
        testContainsCacheMiss |= (params.cacheModes.at(i) == MISS_CACHE);

        auto pipelineSpecificTestParams =
            (params.cacheModes.at(i) == MISS_CACHE) ? cacheMissTestParams : params.testParams;

        auto &test          = tests.emplace_back(de::MovePtr<DataGraphTest>(
            DataGraphTestProvider::getDataGraphTest(ctx, "TOSA", pipelineSpecificTestParams)));
        auto &testResources = testsResources.emplace_back(test->numResources());

        /* create tensors */

        for (size_t r = 0; r < test->numResources(); r++)
        {
            const auto &ri = test->resourceInfo(r);
            auto &tr       = testResources.at(r);

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
                test->initData(r, &*tr.tensor);
            }
            else
            {
                /* fill only host data, e.g. for constants */
                test->initData(r, nullptr, {0, ri.sparsityInfo});
            }
        }

        /* create descriptor set */

        DescriptorSetLayoutBuilder setLayoutBuilder;
        for (size_t r = 0; r < test->numResources(); r++)
        {
            const auto &ri = test->resourceInfo(r);
            if (ri.isTensor())
            {
                /* constants do not need to be in the descriptor set */
                setLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_ALL,
                                                         ri.binding);
            }
        }

        auto &descriptorSetLayout = descriptorSetLayouts.emplace_back(setLayoutBuilder.build(vk, device));

        /* Create DataGraph pipeline */

        auto &graphResources = graphsResources.at(i);
        auto &graphConstants = graphsConstants.at(i);
        for (size_t resId = 0; resId < test->numResources(); resId++)
        {
            const auto &ri = test->resourceInfo(resId);
            auto &tr       = testResources.at(resId);
            if (ri.isTensor())
            {
                VkDataGraphPipelineResourceInfoARM pplRes = initVulkanStructure();
                pplRes.pNext                              = &tr.desc;
                pplRes.descriptorSet                      = ri.descriptorSet;
                pplRes.binding                            = ri.binding;
                graphResources.push_back(pplRes);
            }
            else
            {
                VkDataGraphPipelineConstantARM pplConst = initVulkanStructure();
                pplConst.pNext                          = &tr.desc;
                pplConst.id                             = ri.id;
                pplConst.pConstantData                  = ri.hostData;
                graphConstants.push_back(pplConst);
            }
        }

        auto &pipelineLayout = pipelineLayouts.emplace_back(makePipelineLayout(vk, device, descriptorSetLayout.get()));
        auto &shaderModule   = shaderModules.emplace_back(test->shaderModule());

        void *pNextPipelineCreateInfo = nullptr;

        auto &pipelineCreationFeedback  = pipelineCreationFeedbacks.at(i);
        auto &creationFeedbackInfo      = pipelineCreationFeedbackInfos.at(i);
        auto &dataGraphShaderModuleInfo = pipelineShaderModuleInfos.at(i);
        auto &pipelineCreateInfo        = pipelineCreateInfos.at(i);

        creationFeedbackInfo                           = initVulkanStructure();
        creationFeedbackInfo.pNext                     = pNextPipelineCreateInfo;
        creationFeedbackInfo.sType                     = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO;
        creationFeedbackInfo.pPipelineCreationFeedback = &pipelineCreationFeedback;
        creationFeedbackInfo.pipelineStageCreationFeedbackCount = 0;
        creationFeedbackInfo.pPipelineStageCreationFeedbacks    = nullptr;
        pNextPipelineCreateInfo                                 = &creationFeedbackInfo;

        dataGraphShaderModuleInfo               = initVulkanStructure();
        dataGraphShaderModuleInfo.pNext         = pNextPipelineCreateInfo;
        dataGraphShaderModuleInfo.constantCount = static_cast<uint32_t>(graphConstants.size());
        dataGraphShaderModuleInfo.pConstants    = graphConstants.data();
        dataGraphShaderModuleInfo.pName         = "main";
        dataGraphShaderModuleInfo.module        = shaderModule.get();
        pNextPipelineCreateInfo                 = &dataGraphShaderModuleInfo;

        pipelineCreateInfo                   = initVulkanStructure();
        pipelineCreateInfo.pNext             = pNextPipelineCreateInfo;
        pipelineCreateInfo.layout            = pipelineLayout.get();
        pipelineCreateInfo.resourceInfoCount = static_cast<uint32_t>(graphResources.size());
        pipelineCreateInfo.pResourceInfos    = graphResources.data();

        if (params.failureMode != IGNORE_CACHE_MISS &&
            (params.cacheModes.at(i) == HIT_CACHE || params.cacheModes.at(i) == MISS_CACHE))
        {
            pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT;
        }

        if (params.failureMode == FAIL_ON_CACHE_MISS_EARLY_RETURN && params.cacheModes.at(i) == MISS_CACHE)
        {
            pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT;
        }
    }

    /* create pipeline cache */

    const VkPipelineCacheCreateInfo cacheCreateInfo = initVulkanStructure();
    auto pipelineCache                              = createPipelineCache(vk, device, &cacheCreateInfo);

    /* create pipelines */

    if (testContainsCacheMiss && params.failureMode != IGNORE_CACHE_MISS)
    {
        VK_CHECK_COMPILE_REQUIRED(vk.createDataGraphPipelinesARM(
            device, VK_NULL_HANDLE, pipelineCache.get(), static_cast<uint32_t>(pipelineCreateInfos.size()),
            pipelineCreateInfos.data(), nullptr, pipelines.data()));
    }
    else
    {
        VK_CHECK(vk.createDataGraphPipelinesARM(device, VK_NULL_HANDLE, pipelineCache.get(),
                                                static_cast<uint32_t>(pipelineCreateInfos.size()),
                                                pipelineCreateInfos.data(), nullptr, pipelines.data()));
    }

    /* check that pipelines are valid/invalid as expected */

    bool expectFailure = false;
    for (size_t i = 0; i < numPipelines; i++)
    {
        const auto &pipeline = pipelines.at(i);

        if (params.cacheModes.at(i) != MISS_CACHE && !expectFailure)
        {
            check<VkPipeline>(pipeline);
            vk.destroyPipeline(device, pipeline, nullptr);
        }
        else
        {
            checkIsNull<VkPipeline>(pipeline);

            // in case of early return, after the first failure all other pipelines must fail
            if (params.failureMode == FAIL_ON_CACHE_MISS_EARLY_RETURN)
            {
                expectFailure = true;
            }
        }
    }

    return tcu::TestStatus::pass("test succeeded");
}

tcu::TestStatus submitPipelineTest(Context &ctx, CacheTestParams params)
{
    const DeviceInterface &vk       = ctx.getDeviceInterface();
    const VkDevice device           = ctx.getDevice();
    const VkQueue queue             = ctx.getUniversalQueue();
    const uint32_t queueFamilyIndex = ctx.getUniversalQueueFamilyIndex();
    Allocator &allocator            = ctx.getDefaultAllocator();
    const size_t numPipelines       = params.cacheModes.size();

    std::vector<de::MovePtr<DataGraphTest>> tests;
    std::vector<std::vector<DataGraphTestResource>> testsResources;
    std::vector<DataGraphPipelineWrapper> pipelines;
    std::vector<Move<VkDescriptorPool>> descriptorPools;
    std::vector<Move<VkDescriptorSet>> descriptorSets;
    std::vector<de::MovePtr<DataGraphSessionWithMemory>> sessions;

    tests.reserve(numPipelines);
    testsResources.reserve(numPipelines);
    pipelines.reserve(numPipelines);
    descriptorPools.reserve(numPipelines);
    descriptorSets.reserve(numPipelines);
    sessions.reserve(numPipelines);

    for (size_t i = 0; i < numPipelines; i++)
    {
        // getDataGraphTest cannot return nullptr as will throw an exception in case of errors
        de::MovePtr<DataGraphTest> &test = tests.emplace_back(
            de::MovePtr<DataGraphTest>(DataGraphTestProvider::getDataGraphTest(ctx, "TOSA", params.testParams)));
        std::vector<DataGraphTestResource> &testResources = testsResources.emplace_back(test->numResources());

        /* Create tensors */

        for (size_t r = 0; r < test->numResources(); r++)
        {
            const auto &ri = test->resourceInfo(r);
            auto &tr       = testResources.at(r);

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
                test->initData(r, &*tr.tensor);
            }
            else
            {
                /* fill only host data, e.g. for constants */
                test->initData(r, nullptr, {0, ri.sparsityInfo});
            }
        }
    }

    /* We use the descriptors from one of the tests as a reference for the parts of the code that are in common. Hence, the use of const. */

    const auto &refTest = tests.at(0);

    /* Create descriptor set */

    DescriptorSetLayoutBuilder setLayoutBuilder;
    for (size_t r = 0; r < refTest->numResources(); r++)
    {
        const auto &ri = refTest->resourceInfo(r);
        if (ri.isTensor())
        {
            /* constants do not need to be in the descriptor set */
            setLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_ALL, ri.binding);
        }
    }
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(setLayoutBuilder.build(vk, device));

    /* Pipeline cache */

    const VkPipelineCacheCreateInfo cacheCreateInfo = initVulkanStructure();
    auto pipelineCache                              = createPipelineCache(vk, device, &cacheCreateInfo);

    /* Create DataGraph pipeline */

    for (size_t i = 0; i < numPipelines; i++)
    {
        de::MovePtr<DataGraphTest> &test   = tests.at(i);
        DataGraphPipelineWrapper &pipeline = pipelines.emplace_back(vk, device);

        VkPipelineCreationFeedback pipelineCreateFeedback = {};
        pipeline.setPipelineFeedback(&pipelineCreateFeedback);

        pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
        pipeline.addShaderModule(test->shaderModule());

        for (size_t r = 0; r < test->numResources(); r++)
        {
            const auto &ri = test->resourceInfo(r);
            auto &tr       = testsResources.at(i).at(r);
            if (ri.isTensor())
            {
                pipeline.addTensor(tr.desc, ri.descriptorSet, ri.binding);
            }
            else
            {
                pipeline.addConstant(tr.desc, ri.hostData, ri.id, ri.sparsityInfo);
            }
        }

        if (params.cacheModes.at(i) != MISS_CACHE)
        {
            pipeline.buildPipeline(pipelineCache.get());
        }
        else
        {
            try
            {
                pipeline.buildPipeline(pipelineCache.get());
                // we expect cache to miss, vulkan to return an error, and the test to throw an exception
                return tcu::TestStatus::fail(
                    "Pipeline creation expected to fail due to cache miss, but succeeded instead.");
            }
            catch (const vk::Error &e)
            {
                // we skip creating a session for a failed pipeline
                continue;
            }
        }

        /* Create DataGraph pipeline session */

        VkDataGraphPipelineSessionCreateInfoARM sessionCreateInfo = initVulkanStructure();
        sessionCreateInfo.dataGraphPipeline                       = pipelines.at(i).get();
        sessions.push_back(de::MovePtr<DataGraphSessionWithMemory>(new DataGraphSessionWithMemory(
            vk, device, allocator, sessionCreateInfo, vk::MemoryRequirement::Any, params.testParams.sessionMemory)));
    }

    /* create descriptor sets */
    for (size_t i = 0; i < numPipelines; i++)
    {
        de::MovePtr<DataGraphTest> &test                  = tests.at(i);
        std::vector<DataGraphTestResource> &testResources = testsResources.at(i);

        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM, static_cast<uint32_t>(test->numTensors()));
        auto &descriptorPool = descriptorPools.emplace_back(
            poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

        auto &descriptorSet =
            descriptorSets.emplace_back(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

        DescriptorSetUpdateBuilder updatebuilder;
        for (size_t r = 0; r < test->numResources(); r++)
        {
            const auto &ri = test->resourceInfo(r);
            auto &tr       = testResources.at(r);
            if (ri.isTensor())
            {
                tr.writeDesc = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr, 1, &tr.view.get()};
                updatebuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(ri.binding),
                                          VK_DESCRIPTOR_TYPE_TENSOR_ARM, &tr.writeDesc);
            }
        }
        updatebuilder.update(vk, device);
    }

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, cmdBuffer.get());

    for (size_t i = 0; i < numPipelines; i++)
    {
        pipelines.at(i).bind(cmdBuffer.get());
        vk.cmdBindDescriptorSets(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM,
                                 pipelines.at(i).getPipelineLayout(), 0u, 1u, &descriptorSets.at(i).get(), 0u, nullptr);
        vk.cmdDispatchDataGraphARM(cmdBuffer.get(), **sessions.at(i), nullptr);
    }

    endCommandBuffer(vk, cmdBuffer.get());

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    // Validate the results

    for (size_t i = 0; i < numPipelines; i++)
    {
        de::MovePtr<DataGraphTest> &test                  = tests.at(i);
        std::vector<DataGraphTestResource> &testResources = testsResources.at(i);

        for (size_t r = 0; r < test->numResources(); r++)
        {
            const auto &ri = test->resourceInfo(r);
            auto &tr       = testResources.at(r);

            if (ri.isTensor())
            {
                auto testStatus = test->verifyData(r, &*tr.tensor);
                if (testStatus.isFail())
                {
                    return testStatus;
                }
            }
        }
    }

    return tcu::TestStatus::pass("test succeeded");
}

} // namespace
void createPipelineSingleCallTests(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations = getTestParamsVariations();
    for (const auto &params : paramsVariations)
    {
        {
            CacheTestParams cacheTestParams = {
                params, FAIL_ON_CACHE_MISS_NO_EARLY_RETURN, {FILL_CACHE, HIT_CACHE, HIT_CACHE, HIT_CACHE}};
            addFunctionCase(group, de::toString(cacheTestParams), checkSupport, createPipelineSingleCallTest,
                            cacheTestParams);
        }
        {
            CacheTestParams cacheTestParams = {
                params, FAIL_ON_CACHE_MISS_NO_EARLY_RETURN, {FILL_CACHE, HIT_CACHE, MISS_CACHE, HIT_CACHE}};
            addFunctionCase(group, de::toString(cacheTestParams), checkSupport, createPipelineSingleCallTest,
                            cacheTestParams);
        }
        {
            CacheTestParams cacheTestParams = {
                params, FAIL_ON_CACHE_MISS_EARLY_RETURN, {FILL_CACHE, HIT_CACHE, MISS_CACHE, HIT_CACHE}};
            addFunctionCase(group, de::toString(cacheTestParams), checkSupport, createPipelineSingleCallTest,
                            cacheTestParams);
        }
    }
}

void createPipelineMultiCallsTests(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations = getTestParamsVariations();
    for (const auto &params : paramsVariations)
    {
        CacheTestParams cacheTestParams = {
            params, FAIL_ON_CACHE_MISS_NO_EARLY_RETURN, {FILL_CACHE, HIT_CACHE, MISS_CACHE, HIT_CACHE}};
        addFunctionCase(group, de::toString(cacheTestParams), checkSupport, createPipelineMultiCallsTest,
                        cacheTestParams);
    }
}

void createPipelineTests(tcu::TestCaseGroup *group)
{
    addTestGroup(group, "single_call", createPipelineSingleCallTests);
    addTestGroup(group, "multi_calls", createPipelineMultiCallsTests);
}

void submitPipelineTests(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations = getTestParamsVariations();
    for (const auto &params : paramsVariations)
    {
        CacheTestParams cacheTestParams = {
            params, FAIL_ON_CACHE_MISS_NO_EARLY_RETURN, {FILL_CACHE, HIT_CACHE, HIT_CACHE}};
        addFunctionCase(group, de::toString(cacheTestParams), checkSupport, submitPipelineTest, cacheTestParams);
    }
}

void cacheTestsGroup(tcu::TestCaseGroup *group)
{
    addTestGroup(group, "create_pipeline", createPipelineTests);
    addTestGroup(group, "submit_pipeline", submitPipelineTests);
}

} // namespace dataGraph
} // namespace vkt
