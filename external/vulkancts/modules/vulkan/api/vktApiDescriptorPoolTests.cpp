/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief Descriptor pool tests
 *//*--------------------------------------------------------------------*/

#include "vktApiDescriptorPoolTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deInt32.h"
#include "deSTLUtil.hpp"

#ifdef CTS_USES_VULKANSC
#include "vkSafetyCriticalUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#endif

#define VK_DESCRIPTOR_TYPE_LAST (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1)

namespace vkt
{
namespace api
{

namespace
{

using namespace std;
using namespace vk;

struct ResetDescriptorPoolTestParams
{
    ResetDescriptorPoolTestParams(uint32_t numIterations, bool freeDescriptorSets = false)
        : m_numIterations(numIterations)
        , m_freeDescriptorSets(freeDescriptorSets)
    {
    }

    uint32_t m_numIterations;
    bool m_freeDescriptorSets;
};

void checkSupportFreeDescriptorSets(Context &context, const ResetDescriptorPoolTestParams params)
{
#ifdef CTS_USES_VULKANSC
    if (params.m_freeDescriptorSets && context.getDeviceVulkanSC10Properties().recycleDescriptorSetMemory == VK_FALSE)
        TCU_THROW(NotSupportedError, "vkFreeDescriptorSets not supported");
#else
    DE_UNREF(context);
    DE_UNREF(params);
#endif // CTS_USES_VULKANSC
}

tcu::TestStatus resetDescriptorPoolTest(Context &context, const ResetDescriptorPoolTestParams params)
{
#ifndef CTS_USES_VULKANSC
    const uint32_t numDescriptorSetsPerIter = 2048;
#else
    const uint32_t numDescriptorSetsPerIter = 100;
#endif // CTS_USES_VULKANSC
    const DeviceInterface &vkd = context.getDeviceInterface();
    const VkDevice device      = context.getDevice();

    const VkDescriptorPoolSize descriptorPoolSize = {
        VK_DESCRIPTOR_TYPE_SAMPLER, // type
        numDescriptorSetsPerIter    // descriptorCount
    };

    const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, // sType
        NULL,                                          // pNext
        (params.m_freeDescriptorSets) ? (VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT :
                                        0u, // flags
        numDescriptorSetsPerIter,           // maxSets
        1,                                  // poolSizeCount
        &descriptorPoolSize                 // pPoolSizes
    };

    {
        const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(vkd, device, &descriptorPoolInfo));

        const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {
            0,                          // binding
            VK_DESCRIPTOR_TYPE_SAMPLER, // descriptorType
            1,                          // descriptorCount
            VK_SHADER_STAGE_ALL,        // stageFlags
            NULL                        // pImmutableSamplers
        };

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
            NULL,                                                // pNext
            0,                                                   // flags
            1,                                                   // bindingCount
            &descriptorSetLayoutBinding                          // pBindings
        };

        {
            typedef de::SharedPtr<Unique<VkDescriptorSetLayout>> DescriptorSetLayoutPtr;

            vector<DescriptorSetLayoutPtr> descriptorSetLayouts;
            descriptorSetLayouts.reserve(numDescriptorSetsPerIter);

            for (uint32_t ndx = 0; ndx < numDescriptorSetsPerIter; ++ndx)
            {
                descriptorSetLayouts.push_back(DescriptorSetLayoutPtr(new Unique<VkDescriptorSetLayout>(
                    createDescriptorSetLayout(vkd, device, &descriptorSetLayoutInfo))));
            }

            vector<VkDescriptorSetLayout> descriptorSetLayoutsRaw(numDescriptorSetsPerIter);

            for (uint32_t ndx = 0; ndx < numDescriptorSetsPerIter; ++ndx)
            {
                descriptorSetLayoutsRaw[ndx] = **descriptorSetLayouts[ndx];
            }

            const VkDescriptorSetAllocateInfo descriptorSetInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // sType
                NULL,                                           // pNext
                *descriptorPool,                                // descriptorPool
                numDescriptorSetsPerIter,                       // descriptorSetCount
                &descriptorSetLayoutsRaw[0]                     // pSetLayouts
            };

            vector<VkDescriptorSet> testSets(numDescriptorSetsPerIter);

            for (uint32_t ndx = 0; ndx < params.m_numIterations; ++ndx)
            {
                if (ndx % 1024 == 0)
                    context.getTestContext().touchWatchdog();
                // The test should crash in this loop at some point if there is a memory leak
                VK_CHECK(vkd.allocateDescriptorSets(device, &descriptorSetInfo, &testSets[0]));
                if (params.m_freeDescriptorSets)
                    VK_CHECK(vkd.freeDescriptorSets(device, *descriptorPool, 1, &testSets[0]));
                VK_CHECK(vkd.resetDescriptorPool(device, *descriptorPool, 0));
            }
        }
    }

    // If it didn't crash, pass
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus outOfPoolMemoryTest(Context &context)
{
    const DeviceInterface &vkd            = context.getDeviceInterface();
    const VkDevice device                 = context.getDevice();
    const bool expectOutOfPoolMemoryError = context.isDeviceFunctionalitySupported("VK_KHR_maintenance1");
    uint32_t numErrorsReturned            = 0;

    const struct FailureCase
    {
        uint32_t poolDescriptorCount;    //!< total number of descriptors (of a given type) in the descriptor pool
        uint32_t poolMaxSets;            //!< max number of descriptor sets that can be allocated from the pool
        uint32_t bindingCount;           //!< number of bindings per descriptor set layout
        uint32_t bindingDescriptorCount; //!< number of descriptors in a binding (array size) (in all bindings)
        uint32_t descriptorSetCount;     //!< number of descriptor sets to allocate
        string description;              //!< the log message for this failure condition
    } failureCases[] = {
        //    pool            pool        binding        binding        alloc set
        //    descr. count    max sets    count        array size    count
        {
            4u,
            2u,
            1u,
            1u,
            3u,
            "Out of descriptor sets",
        },
        {
            3u,
            4u,
            1u,
            1u,
            4u,
            "Out of descriptors (due to the number of sets)",
        },
        {
            2u,
            1u,
            3u,
            1u,
            1u,
            "Out of descriptors (due to the number of bindings)",
        },
        {
            3u,
            2u,
            1u,
            2u,
            2u,
            "Out of descriptors (due to descriptor array size)",
        },
        {
            5u,
            1u,
            2u,
            3u,
            1u,
            "Out of descriptors (due to descriptor array size in all bindings)",
        },
    };

    context.getTestContext().getLog()
        << tcu::TestLog::Message
        << "Creating a descriptor pool with insufficient resources. Descriptor set allocation is likely to fail."
        << tcu::TestLog::EndMessage;

    for (uint32_t failureCaseNdx = 0u; failureCaseNdx < DE_LENGTH_OF_ARRAY(failureCases); ++failureCaseNdx)
    {
        const FailureCase &params = failureCases[failureCaseNdx];
        context.getTestContext().getLog()
            << tcu::TestLog::Message << "Checking: " << params.description << tcu::TestLog::EndMessage;

        for (VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; descriptorType < VK_DESCRIPTOR_TYPE_LAST;
             descriptorType                  = static_cast<VkDescriptorType>(descriptorType + 1))
        {
            context.getTestContext().getLog()
                << tcu::TestLog::Message << "- " << getDescriptorTypeName(descriptorType) << tcu::TestLog::EndMessage;

            const VkDescriptorPoolSize descriptorPoolSize = {
                descriptorType,             // type
                params.poolDescriptorCount, // descriptorCount
            };

            const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, // VkStructureType                sType;
                DE_NULL,                                       // const void*                    pNext;
                (VkDescriptorPoolCreateFlags)0,                // VkDescriptorPoolCreateFlags    flags;
                params.poolMaxSets,                            // uint32_t                       maxSets;
                1u,                                            // uint32_t                       poolSizeCount;
                &descriptorPoolSize,                           // const VkDescriptorPoolSize*    pPoolSizes;
            };

            const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(vkd, device, &descriptorPoolCreateInfo));

            VkShaderStageFlags stageFlags = (descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) ?
                                                VK_SHADER_STAGE_ALL :
                                                VK_SHADER_STAGE_FRAGMENT_BIT;
            const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {
                0u,                            // uint32_t              binding;
                descriptorType,                // VkDescriptorType      descriptorType;
                params.bindingDescriptorCount, // uint32_t              descriptorCount;
                stageFlags,                    // VkShaderStageFlags    stageFlags;
                DE_NULL,                       // const VkSampler*      pImmutableSamplers;
            };

            vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings(params.bindingCount,
                                                                             descriptorSetLayoutBinding);

            for (uint32_t binding = 0; binding < uint32_t(descriptorSetLayoutBindings.size()); ++binding)
            {
                descriptorSetLayoutBindings[binding].binding = binding;
            }

            const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
                DE_NULL,                                             // const void*                            pNext;
                (VkDescriptorSetLayoutCreateFlags)0,                 // VkDescriptorSetLayoutCreateFlags       flags;
                static_cast<uint32_t>(
                    descriptorSetLayoutBindings.size()), // uint32_t                               bindingCount;
                &descriptorSetLayoutBindings[0],         // const VkDescriptorSetLayoutBinding*    pBindings;
            };

            const Unique<VkDescriptorSetLayout> descriptorSetLayout(
                createDescriptorSetLayout(vkd, device, &descriptorSetLayoutInfo));
            const vector<VkDescriptorSetLayout> rawSetLayouts(params.descriptorSetCount, *descriptorSetLayout);
            vector<VkDescriptorSet> rawDescriptorSets(params.descriptorSetCount, VK_NULL_HANDLE);

            const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType;
                DE_NULL,                                        // const void*                     pNext;
                *descriptorPool,                                // VkDescriptorPool                descriptorPool;
                static_cast<uint32_t>(rawSetLayouts.size()),    // uint32_t                        descriptorSetCount;
                &rawSetLayouts[0],                              // const VkDescriptorSetLayout*    pSetLayouts;
            };

            const VkResult result =
                vkd.allocateDescriptorSets(device, &descriptorSetAllocateInfo, &rawDescriptorSets[0]);

            if (result != VK_SUCCESS)
            {
                ++numErrorsReturned;

                if (expectOutOfPoolMemoryError && result != VK_ERROR_OUT_OF_POOL_MEMORY)
                    return tcu::TestStatus::fail("Expected VK_ERROR_OUT_OF_POOL_MEMORY but got " +
                                                 string(getResultName(result)) + " instead");
            }
            else
                context.getTestContext().getLog()
                    << tcu::TestLog::Message << "  Allocation was successful anyway" << tcu::TestLog::EndMessage;
        }
    }

    if (numErrorsReturned == 0u)
        return tcu::TestStatus::pass("Not validated");
    else
        return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus zeroPoolSizeCount(Context &context)
{
    const DeviceInterface &vkd = context.getDeviceInterface();
    const VkDevice device      = context.getDevice();

    const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType                sType;
        DE_NULL,                                           // const void*                    pNext;
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags    flags;
        1u,                                                // uint32_t                       maxSets;
        0u,                                                // uint32_t                       poolSizeCount;
        DE_NULL,                                           // const VkDescriptorPoolSize*    pPoolSizes;
    };

    // Test a pool can be created for empty descriptor sets.
    const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(vkd, device, &descriptorPoolCreateInfo));

    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
        DE_NULL,                                             // const void*                            pNext;
        (VkDescriptorSetLayoutCreateFlags)0,                 // VkDescriptorSetLayoutCreateFlags       flags;
        0u,                                                  // uint32_t                               bindingCount;
        DE_NULL,                                             // const VkDescriptorSetLayoutBinding*    pBindings;
    };

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        createDescriptorSetLayout(vkd, device, &descriptorSetLayoutInfo));

    const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType;
        DE_NULL,                                        // const void*                     pNext;
        *descriptorPool,                                // VkDescriptorPool                descriptorPool;
        1u,                                             // uint32_t                        descriptorSetCount;
        &descriptorSetLayout.get(),                     // const VkDescriptorSetLayout*    pSetLayouts;
    };

    // Create an empty descriptor set from the pool.
    VkDescriptorSet descriptorSet;
    VkResult result = vkd.allocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail("Expected vkAllocateDescriptorSets to return VK_SUCCESS but got " +
                                     string(getResultName(result)) + " instead");

    // Free the empty descriptor set back to the pool.
    result = vkd.freeDescriptorSets(device, *descriptorPool, 1, &descriptorSet);
    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail("Expected vkFreeDescriptorSets to return VK_SUCCESS but got " +
                                     string(getResultName(result)) + " instead");

    return tcu::TestStatus::pass("Pass");
}

#ifdef CTS_USES_VULKANSC
tcu::TestStatus noResetDescriptorPoolTest(Context &context, const ResetDescriptorPoolTestParams params)
{
    const DeviceInterface &vkd = context.getDeviceInterface();

    const uint32_t numDescriptorSetsPerIter = 100;
    const float queuePriority               = 1.0f;

    const VkDeviceQueueCreateInfo deviceQueueCI = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
        DE_NULL,                                    // pNext
        (VkDeviceQueueCreateFlags)0u,               // flags
        0,                                          //queueFamilyIndex;
        1,                                          //queueCount;
        &queuePriority,                             //pQueuePriorities;
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // sType;
        DE_NULL,                              // pNext;
        (VkDeviceCreateFlags)0u,              // flags
        1,                                    // queueCount;
        &deviceQueueCI,                       // pQueues;
        0,                                    // layerCount;
        DE_NULL,                              // ppEnabledLayerNames;
        0,                                    // extensionCount;
        DE_NULL,                              // ppEnabledExtensionNames;
        DE_NULL,                              // pEnabledFeatures;
    };

    VkDeviceObjectReservationCreateInfo objectInfo    = resetDeviceObjectReservationCreateInfo();
    objectInfo.descriptorPoolRequestCount             = 1u;
    objectInfo.descriptorSetRequestCount              = numDescriptorSetsPerIter;
    objectInfo.descriptorSetLayoutRequestCount        = numDescriptorSetsPerIter;
    objectInfo.descriptorSetLayoutBindingRequestCount = numDescriptorSetsPerIter;
    objectInfo.descriptorSetLayoutBindingLimit        = 1u;
    objectInfo.pNext                                  = DE_NULL;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = &objectInfo;

    deviceCreateInfo.pNext = &sc10Features;

    const VkDescriptorPoolSize descriptorPoolSize = {
        VK_DESCRIPTOR_TYPE_SAMPLER, // type
        numDescriptorSetsPerIter    // descriptorCount
    };

    const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,                                  // sType
        NULL,                                                                           // pNext
        (VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // flags
        numDescriptorSetsPerIter,                                                       // maxSets
        1,                                                                              // poolSizeCount
        &descriptorPoolSize                                                             // pPoolSizes
    };

    for (uint32_t i = 0; i < params.m_numIterations; ++i)
    {
        vkt::CustomInstance instance = vkt::createCustomInstanceFromContext(context);
        VkPhysicalDevice physicalDevice =
            chooseDevice(instance.getDriver(), instance, context.getTestContext().getCommandLine());
        Move<VkDevice> device = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                                                   context.getPlatformInterface(), instance, instance.getDriver(),
                                                   physicalDevice, &deviceCreateInfo);

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VK_CHECK(vkd.createDescriptorPool(*device, &descriptorPoolInfo, DE_NULL, &descriptorPool));
        if (!descriptorPool)
            TCU_THROW(TestError, "create descriptor pool failed");

        const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {
            0,                          // binding
            VK_DESCRIPTOR_TYPE_SAMPLER, // descriptorType
            1,                          // descriptorCount
            VK_SHADER_STAGE_ALL,        // stageFlags
            NULL                        // pImmutableSamplers
        };

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
            NULL,                                                // pNext
            0,                                                   // flags
            1,                                                   // bindingCount
            &descriptorSetLayoutBinding                          // pBindings
        };

        typedef de::SharedPtr<Unique<VkDescriptorSetLayout>> DescriptorSetLayoutPtr;

        vector<DescriptorSetLayoutPtr> descriptorSetLayouts;
        descriptorSetLayouts.reserve(numDescriptorSetsPerIter);

        for (uint32_t ndx = 0; ndx < numDescriptorSetsPerIter; ++ndx)
        {
            descriptorSetLayouts.push_back(DescriptorSetLayoutPtr(
                new Unique<VkDescriptorSetLayout>(createDescriptorSetLayout(vkd, *device, &descriptorSetLayoutInfo))));
        }

        vector<VkDescriptorSetLayout> descriptorSetLayoutsRaw(numDescriptorSetsPerIter);

        for (uint32_t ndx = 0; ndx < numDescriptorSetsPerIter; ++ndx)
        {
            descriptorSetLayoutsRaw[ndx] = **descriptorSetLayouts[ndx];
        }

        const VkDescriptorSetAllocateInfo descriptorSetInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // sType
            NULL,                                           // pNext
            descriptorPool,                                 // descriptorPool
            numDescriptorSetsPerIter,                       // descriptorSetCount
            &descriptorSetLayoutsRaw[0]                     // pSetLayouts
        };

        vector<VkDescriptorSet> testSets(numDescriptorSetsPerIter);

        VK_CHECK(vkd.allocateDescriptorSets(*device, &descriptorSetInfo, &testSets[0]));
        VK_CHECK(vkd.freeDescriptorSets(*device, descriptorPool, numDescriptorSetsPerIter, &testSets[0]));
    }

    // If it didn't crash, pass
    return tcu::TestStatus::pass("Pass");
}
#endif

} // namespace

tcu::TestCaseGroup *createDescriptorPoolTests(tcu::TestContext &testCtx)
{
    const uint32_t numIterationsHigh = 4096;

    de::MovePtr<tcu::TestCaseGroup> descriptorPoolTests(new tcu::TestCaseGroup(testCtx, "descriptor_pool"));

    // Test 2 cycles of vkAllocateDescriptorSets and vkResetDescriptorPool (should pass)
    addFunctionCase(descriptorPoolTests.get(), "repeated_reset_short", checkSupportFreeDescriptorSets,
                    resetDescriptorPoolTest, ResetDescriptorPoolTestParams(2U));
    // Test many cycles of vkAllocateDescriptorSets and vkResetDescriptorPool
    addFunctionCase(descriptorPoolTests.get(), "repeated_reset_long", checkSupportFreeDescriptorSets,
                    resetDescriptorPoolTest, ResetDescriptorPoolTestParams(numIterationsHigh));
    // Test 2 cycles of vkAllocateDescriptorSets, vkFreeDescriptorSets and vkResetDescriptorPool (should pass)
    addFunctionCase(descriptorPoolTests.get(), "repeated_free_reset_short", checkSupportFreeDescriptorSets,
                    resetDescriptorPoolTest, ResetDescriptorPoolTestParams(2U, true));
    // Test many cycles of vkAllocateDescriptorSets, vkFreeDescriptorSets and vkResetDescriptorPool
    addFunctionCase(descriptorPoolTests.get(), "repeated_free_reset_long", checkSupportFreeDescriptorSets,
                    resetDescriptorPoolTest, ResetDescriptorPoolTestParams(numIterationsHigh, true));
    // Test that when we run out of descriptors a correct error code is returned
    addFunctionCase(descriptorPoolTests.get(), "out_of_pool_memory", outOfPoolMemoryTest);
    // Test a descriptor pool object can be created with zero pools without error or crash
    addFunctionCase(descriptorPoolTests.get(), "zero_pool_size_count", zeroPoolSizeCount);
#ifdef CTS_USES_VULKANSC
    // Test 2 cycles of vkAllocateDescriptorSets, vkFreeDescriptorSets and vkDestroyDevice (should pass)
    addFunctionCase(descriptorPoolTests.get(), "repeated_free_no_reset_short", noResetDescriptorPoolTest,
                    ResetDescriptorPoolTestParams(2U, true));

    // Test many cycles of vkAllocateDescriptorSets, vkFreeDescriptorSets and vkDestroyDevice (should pass)
    addFunctionCase(descriptorPoolTests.get(), "repeated_free_no_reset_long", noResetDescriptorPoolTest,
                    ResetDescriptorPoolTestParams(200U, true));
#endif

    return descriptorPoolTests.release();
}

} // namespace api
} // namespace vkt
