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
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deInt32.h"
#include "deSTLUtil.hpp"
#include "deMemory.h"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

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
                nullptr,                                       // const void*                    pNext;
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
                nullptr,                       // const VkSampler*      pImmutableSamplers;
            };

            vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings(params.bindingCount,
                                                                             descriptorSetLayoutBinding);

            for (uint32_t binding = 0; binding < uint32_t(descriptorSetLayoutBindings.size()); ++binding)
            {
                descriptorSetLayoutBindings[binding].binding = binding;
            }

            const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
                nullptr,                                             // const void*                            pNext;
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
                nullptr,                                        // const void*                     pNext;
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
        nullptr,                                           // const void*                    pNext;
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags    flags;
        1u,                                                // uint32_t                       maxSets;
        0u,                                                // uint32_t                       poolSizeCount;
        nullptr,                                           // const VkDescriptorPoolSize*    pPoolSizes;
    };

    // Test a pool can be created for empty descriptor sets.
    const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(vkd, device, &descriptorPoolCreateInfo));

    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
        nullptr,                                             // const void*                            pNext;
        (VkDescriptorSetLayoutCreateFlags)0,                 // VkDescriptorSetLayoutCreateFlags       flags;
        0u,                                                  // uint32_t                               bindingCount;
        nullptr,                                             // const VkDescriptorSetLayoutBinding*    pBindings;
    };

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        createDescriptorSetLayout(vkd, device, &descriptorSetLayoutInfo));

    const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType;
        nullptr,                                        // const void*                     pNext;
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
    const uint32_t numDescriptorSetsPerIter = 100;
    const float queuePriority               = 1.0f;

    const VkDeviceQueueCreateInfo deviceQueueCI = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
        nullptr,                                    // pNext
        (VkDeviceQueueCreateFlags)0u,               // flags
        0,                                          //queueFamilyIndex;
        1,                                          //queueCount;
        &queuePriority,                             //pQueuePriorities;
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // sType;
        nullptr,                              // pNext;
        (VkDeviceCreateFlags)0u,              // flags
        1,                                    // queueCount;
        &deviceQueueCI,                       // pQueues;
        0,                                    // layerCount;
        nullptr,                              // ppEnabledLayerNames;
        0,                                    // extensionCount;
        nullptr,                              // ppEnabledExtensionNames;
        nullptr,                              // pEnabledFeatures;
    };

    VkDeviceObjectReservationCreateInfo objectInfo =
        context.getResourceInterface()->getDefaultDeviceObjectReservationCreateInfo();
    objectInfo.descriptorPoolRequestCount             = 1u;
    objectInfo.descriptorSetRequestCount              = numDescriptorSetsPerIter;
    objectInfo.descriptorSetLayoutRequestCount        = numDescriptorSetsPerIter;
    objectInfo.descriptorSetLayoutBindingRequestCount = numDescriptorSetsPerIter;
    objectInfo.descriptorSetLayoutBindingLimit        = 1u;
    objectInfo.pNext                                  = nullptr;

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
        const auto instance  = InstanceWrapper(vkt::createCustomInstanceFromContext(context));
        DeviceWrapper device = instance.createCustomDevice(&deviceCreateInfo);
        const auto &vkd      = device.getDriver();

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VK_CHECK(vkd.createDescriptorPool(*device, &descriptorPoolInfo, nullptr, &descriptorPool));
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

struct OutOfBoundsSetTestParams
{
    VkDescriptorType descriptorType;
    uint32_t numDescriptorSets;
};

struct TestPushConstants
{
    uint32_t setIdx;
};

void outOfBoundsSetWithBuffersCheckSupport(Context &context, const OutOfBoundsSetTestParams testParams)
{
    DE_UNREF(context);
    DE_UNREF(testParams);
}

std::string getBufferGlslTypeStr(VkDescriptorType descType)
{
    std::string str;
    switch (descType)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        str = "uniform";
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        str = "buffer";
        break;
    default:
        break;
    };

    return str;
}

void commonPrograms(SourceCollections &programCollection)
{
    std::ostringstream vert;
    {
        vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "void main()\n"
             << "{\n"
             << "    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
             << "                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
             << "}\n";
    }
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
}

void outOfBoundsSetWithBuffersPrograms(SourceCollections &programCollection, OutOfBoundsSetTestParams testParams)
{
    commonPrograms(programCollection);

    std::ostringstream frag;
    {
        frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";

        frag << "layout(set = 0, binding = 0) " << getBufferGlslTypeStr(testParams.descriptorType) << " aBuffer {\n"
             << "    int data;\n"
             << "} buff;\n"
             << "layout(push_constant, std430) uniform PushConstants\n"
             << "{\n"
             << "    int setId;\n"
             << "} pc;\n"
             << "layout (location = 0) out vec4 outColor;\n"
             << "void main()\n"
             << "{\n"
             << "    if(buff.data != pc.setId)\n"
             << "        outColor = vec4(1, 0, 1, 0);\n"
             << "    else\n"
             << "        outColor = vec4(0, 1, 0, 1);\n"
             << "}\n";
    }
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

VkImageCreateInfo makeImageCreateInfo(const VkFormat format, const uint32_t width, const uint32_t height,
                                      const VkImageUsageFlags usage)
{
    const VkImageCreateInfo imageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                             //  const void* pNext;
        (VkImageCreateFlags)0,               //  VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    //  VkImageType imageType;
        format,                              //  VkFormat format;
        makeExtent3D(width, height, 1),      //  VkExtent3D extent;
        1u,                                  //  uint32_t mipLevels;
        1u,                                  //  uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               //  VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             //  VkImageTiling tiling;
        usage,                               //  VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           //  VkSharingMode sharingMode;
        0u,                                  //  uint32_t queueFamilyIndexCount;
        nullptr,                             //  const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           //  VkImageLayout initialLayout;
    };

    return imageInfo;
}

VkImageViewCreateInfo makeImageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspectMask)
{
    const VkComponentMapping components = {
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A,
    };
    const VkImageSubresourceRange subresourceRange = {
        aspectMask, //  VkImageAspectFlags aspectMask;
        0,          //  uint32_t baseMipLevel;
        1,          //  uint32_t levelCount;
        0,          //  uint32_t baseArrayLayer;
        1,          //  uint32_t layerCount;
    };
    const VkImageViewCreateInfo result = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                  //  const void* pNext;
        0u,                                       //  VkImageViewCreateFlags flags;
        image,                                    //  VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    //  VkImageViewType viewType;
        format,                                   //  VkFormat format;
        components,                               //  VkComponentMapping components;
        subresourceRange,                         //  VkImageSubresourceRange subresourceRange;
    };

    return result;
}

VkBufferUsageFlags getBufferUsage(VkDescriptorType descType)
{
    VkBufferUsageFlags usage = VkBufferUsageFlags(0u);
    switch (descType)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    default:
        break;
    };

    return usage;
}

tcu::TestStatus outOfBoundsSetWithBuffersRun(Context &context, OutOfBoundsSetTestParams testParams)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    const VkQueue queue             = context.getUniversalQueue();
    tcu::TestLog &log               = context.getTestContext().getLog();
    const uint32_t numSets          = testParams.numDescriptorSets + 1u;
    const uint32_t numDescs         = testParams.numDescriptorSets; // each descriptor has its own set
    const auto descType             = testParams.descriptorType;
    const uint32_t numElemsPerSet   = 1u;

    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const tcu::IVec2 renderSize(1, 1);

    // Create descriptor pool
    Move<VkDescriptorPool> descPool;
    {
        descPool = DescriptorPoolBuilder()
                       .addType(descType, numDescs)
                       .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numSets);
    }

    // Create descriptor set layout
    Move<VkDescriptorSetLayout> descSetLayout;
    {
        DescriptorSetLayoutBuilder descSetLayoutBuilder;
        descSetLayoutBuilder.addBinding(descType, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
        descSetLayout = descSetLayoutBuilder.build(vk, device);
    }

    // Allocate descriptor sets using same layout for all sets
    std::vector<Move<VkDescriptorSet>> descSetPtrs(numSets);
    std::vector<VkDescriptorSet> descSets(numSets, VK_NULL_HANDLE);

    uint32_t numSetsAllocated = 0u;

    VkResult allocResult = VK_SUCCESS;
    int32_t failedSetIdx = -1;

    for (uint32_t setIdx = 0; setIdx < numSets; setIdx++)
    {
        VkDescriptorSet object = VK_NULL_HANDLE;

        const VkDescriptorSetAllocateInfo allocateParams = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            *descPool,                                      // VkDescriptorPool descriptorPool;
            1u,                                             // uint32_t setLayoutCount;
            &(*descSetLayout),                              // const VkDescriptorSetLayout* pSetLayouts;
        };

        allocResult = vk.allocateDescriptorSets(device, &allocateParams, &object);

        if (allocResult != VK_SUCCESS)
        {
            std::ostringstream msg;
            msg << "Allocation of set = " << setIdx << " failed due to reason: " << allocResult << "\n";
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;

            failedSetIdx = setIdx;
            break;
        }

        descSets[setIdx] = object;
        numSetsAllocated++;
    }

    if (numSetsAllocated < numDescs)
    {
        return tcu::TestStatus::fail("Required number of descriptor sets could not be allocated: " +
                                     string(getResultName(allocResult)));
    }

    // Last set allocation should fail with VK_ERROR_OUT_OF_POOL_MEMORY
    if ((failedSetIdx == static_cast<int32_t>(numSets - 1u)) && (allocResult != VK_ERROR_OUT_OF_POOL_MEMORY))
    {
        return tcu::TestStatus::fail("Allocation of descriptor set " + de::toString(failedSetIdx) +
                                     " failed: " + string(getResultName(allocResult)));
    }

    // Allocate and initialize buffer
    const VkDeviceSize intBufferSizeBytes = sizeof(uint32_t);
    const auto intBufferInfo = makeBufferCreateInfo(intBufferSizeBytes, getBufferUsage(testParams.descriptorType));
    std::vector<de::MovePtr<BufferWithMemory>> intBuffers(numSetsAllocated);

    for (uint32_t setIdx = 0; setIdx < numSetsAllocated; setIdx++)
    {
        intBuffers[setIdx] = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, intBufferInfo, MemoryRequirement::HostVisible));
        auto &intBufferAlloc = intBuffers[setIdx]->getAllocation();
        auto intBufferPtr    = reinterpret_cast<uint32_t *>(intBufferAlloc.getHostPtr()) + intBufferAlloc.getOffset();

        *intBufferPtr = setIdx;

        flushAlloc(vk, device, intBufferAlloc);
    }

    // Update allocated descriptors
    for (uint32_t setIdx = 0; setIdx < numSetsAllocated; setIdx++)
    {
        const VkDescriptorSet &destSet = descSets[setIdx];

        DescriptorSetUpdateBuilder descSetUpdateBuilder;

        const VkDescriptorBufferInfo descInfo = makeDescriptorBufferInfo(**intBuffers[setIdx], 0u, sizeof(uint32_t));

        descSetUpdateBuilder.writeSingle(destSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                         &descInfo);

        descSetUpdateBuilder.update(vk, device);
    }

    // Create push constants
    const VkPushConstantRange pcRange = {
        VK_SHADER_STAGE_FRAGMENT_BIT,                     // VkShaderStageFlags stageFlags;
        0u,                                               // uint32_t offset;
        static_cast<uint32_t>(sizeof(TestPushConstants)), // uint32_t size;
    };

    // Create pipeline
    const auto pipelineLayout = makePipelineLayout(vk, device, numElemsPerSet, &(*descSetLayout), 1u, &pcRange);

    // Create image and image view that will hold rendered frame
    const VkImageCreateInfo colorImageCreateInfo =
        makeImageCreateInfo(colorFormat, renderSize.x(), renderSize.y(),
                            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
    de::MovePtr<ImageWithMemory> colorImage = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, allocator, colorImageCreateInfo, MemoryRequirement::Any));
    const VkImageViewCreateInfo colorImageViewCreateInfo =
        makeImageViewCreateInfo(**colorImage, colorFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
    const Move<VkImageView> colorImageView = createImageView(vk, device, &colorImageViewCreateInfo);

    // Create renderpass and framebuffer
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, colorFormat);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());

    // Create output buffer for verification
    const VkDeviceSize outputBufferDataSize =
        static_cast<VkDeviceSize>(renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat)));
    const VkBufferCreateInfo outputBufferCreateInfo =
        makeBufferCreateInfo(outputBufferDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory outputBuffer(vk, device, allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);

    // Create graphics pipeline
    Move<VkPipeline> pipeline;
    {
        const Unique<VkShaderModule> vertexShaderModule(
            createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
        const Unique<VkShaderModule> fragmentShaderModule(
            createShaderModule(vk, device, context.getBinaryCollection().get("frag"), 0u));

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = initVulkanStructure();

        const std::vector<VkViewport> viewports(1, makeViewport(renderSize));
        const std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

        pipeline = makeGraphicsPipeline(
            vk,                    // const DeviceInterface&                        vk
            device,                // const VkDevice                                device
            *pipelineLayout,       // const VkPipelineLayout                        pipelineLayout
            *vertexShaderModule,   // const VkShaderModule                          vertexShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          tessellationControlShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          tessellationEvalShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          geometryShaderModule
            *fragmentShaderModule, // const VkShaderModule                          fragmentShaderModule
            *renderPass,           // const VkRenderPass                            renderPass
            viewports,             // const std::vector<VkViewport>&                viewports
            scissors,              // const std::vector<VkRect2D>&                  scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
            0u,                                  // const uint32_t                                subpass
            0u,                                  // const uint32_t                                patchControlPoints
            &vertexInputStateParams); // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
    }

    // Generate reference
    const tcu::TextureFormat tcuFormat = vk::mapVkFormat(colorFormat);
    const tcu::Vec4 referenceColor(0.0f, 1.0f, 0.0f, 1.0f);
    tcu::TextureLevel referenceLevel(tcuFormat, renderSize.x(), renderSize.y());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, referenceColor);

    // Run verification shader for each set
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> commandBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    for (uint32_t setIdx = 0u; setIdx < numSetsAllocated; setIdx++)
    {
        beginCommandBuffer(vk, *commandBuffer);

        const VkRect2D renderArea = makeRect2D(renderSize);
        const tcu::Vec4 clearColor(1.0f, 0.0f, 0.0f, 1.0f);
        beginRenderPass(vk, *commandBuffer, *renderPass, *framebuffer, renderArea, clearColor);

        {
            // Push constant data
            const TestPushConstants pushConsts = {setIdx};
            vk.cmdPushConstants(*commandBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                                static_cast<uint32_t>(sizeof(pushConsts)), &pushConsts);
        }

        vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

        vk.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                 &descSets[setIdx], 0u, nullptr);

        vk.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);

        endRenderPass(vk, *commandBuffer);
        copyImageToBuffer(vk, *commandBuffer, **colorImage, *outputBuffer, renderSize);

        endCommandBuffer(vk, *commandBuffer);
        submitCommandsAndWait(vk, device, queue, *commandBuffer);

        // Validation
        {
            invalidateAlloc(vk, device, outputBuffer.getAllocation());

            tcu::ConstPixelBufferAccess resultBufferAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1,
                                                           outputBuffer.getAllocation().getHostPtr());

            const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
            if (!tcu::floatThresholdCompare(log, "Image comparison", "Image comparison", referenceAccess,
                                            resultBufferAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
            {
                std::ostringstream msg;
                msg << "Image comparison failed on set = " << setIdx << "\n";
                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;

                return tcu::TestStatus::fail("Image comparison failed; check log");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

void outOfBoundsSetWithImagesCheckSupport(Context &context, const OutOfBoundsSetTestParams testParams)
{
    DE_UNREF(context);
    DE_UNREF(testParams);
}

void outOfBoundsSetWithImagesPrograms(SourceCollections &programCollection, OutOfBoundsSetTestParams testParams)
{
    DE_UNREF(testParams);

    commonPrograms(programCollection);

    std::ostringstream frag;
    {
        frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
        frag << "layout(set = 0, binding = 0) uniform sampler2D samplerImage;\n"
             << "layout(location = 0) out vec4 outColor;\n"
             << "void main()\n"
             << "{\n"
             << "    outColor = texture(samplerImage, vec2(0.5, 0.5));\n"
             << "}\n";
    }
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

Move<VkSampler> makeSampler(const DeviceInterface &vk, VkDevice device)
{
    const VkSamplerCreateInfo samplerParams = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        (VkSamplerCreateFlags)0u,                // VkSamplerCreateFlags flags;
        VK_FILTER_NEAREST,                       // VkFilter magFilter;
        VK_FILTER_NEAREST,                       // VkFilter minFilter;
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV;
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
        0.0f,                                    // float mipLodBias;
        VK_FALSE,                                // VkBool32 anisotropyEnable;
        1.0f,                                    // float maxAnisotropy;
        VK_FALSE,                                // VkBool32 compareEnable;
        VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
        0.0f,                                    // float minLod;
        1.0f,                                    // float maxLod;
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
        VK_FALSE,                                // VkBool32 unnormalizedCoordinates;
    };

    return createSampler(vk, device, &samplerParams);
}

tcu::TestStatus outOfBoundsSetWithImagesRun(Context &context, OutOfBoundsSetTestParams testParams)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    const VkQueue queue             = context.getUniversalQueue();
    tcu::TestLog &log               = context.getTestContext().getLog();
    const uint32_t numSets          = testParams.numDescriptorSets + 1u;
    const uint32_t numDescs         = testParams.numDescriptorSets; // each descriptor has its own set
    const auto descType             = testParams.descriptorType;
    const uint32_t numElemsPerSet   = 1u;

    const auto imageFormat             = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorFormat             = imageFormat;
    const tcu::TextureFormat tcuFormat = mapVkFormat(imageFormat);
    const tcu::IVec2 renderSize(1, 1);
    const auto extent                       = makeExtent3D(renderSize.x(), renderSize.y(), 1);
    const VkDeviceSize imageBufferSizeBytes = static_cast<VkDeviceSize>(static_cast<uint32_t>(getPixelSize(tcuFormat)) *
                                                                        extent.width * extent.height * extent.depth);

    // Create sampler
    Move<VkSampler> imageSampler = makeSampler(vk, device);

    // Create descriptor pool
    Move<VkDescriptorPool> descPool;
    {
        descPool = DescriptorPoolBuilder()
                       .addType(descType, numDescs)
                       .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numSets);
    }

    // Create descriptor set layout
    Move<VkDescriptorSetLayout> descSetLayout;
    {
        DescriptorSetLayoutBuilder descSetLayoutBuilder;
        descSetLayoutBuilder.addBinding(descType, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
        descSetLayout = descSetLayoutBuilder.build(vk, device);
    }

    // Allocate descriptor sets using same layout for all sets
    std::vector<Move<VkDescriptorSet>> descSetPtrs(numSets);
    std::vector<VkDescriptorSet> descSets(numSets, VK_NULL_HANDLE);

    uint32_t numSetsAllocated = 0u;

    VkResult allocResult = VK_SUCCESS;
    int32_t failedSetIdx = -1;

    for (uint32_t setIdx = 0; setIdx < numSets; setIdx++)
    {
        VkDescriptorSet object = VK_NULL_HANDLE;

        const VkDescriptorSetAllocateInfo allocateParams = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            *descPool,                                      // VkDescriptorPool descriptorPool;
            1u,                                             // uint32_t setLayoutCount;
            &(*descSetLayout),                              // const VkDescriptorSetLayout* pSetLayouts;
        };

        allocResult = vk.allocateDescriptorSets(device, &allocateParams, &object);

        if (allocResult != VK_SUCCESS)
        {
            std::ostringstream msg;
            msg << "Allocation of set = " << setIdx << " failed due to reason: " << allocResult << "\n";
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;

            failedSetIdx = setIdx;
            break;
        }

        descSets[setIdx] = object;
        numSetsAllocated++;
    }

    if (numSetsAllocated < numDescs)
    {
        return tcu::TestStatus::fail("Required number of descriptor sets could not be allocated: " +
                                     string(getResultName(allocResult)));
    }

    // Last set allocation should fail with VK_ERROR_OUT_OF_POOL_MEMORY
    if ((failedSetIdx == static_cast<int32_t>(numSets - 1u)) && (allocResult != VK_ERROR_OUT_OF_POOL_MEMORY))
    {
        return tcu::TestStatus::fail("Allocation of descriptor set " + de::toString(failedSetIdx) +
                                     " failed: " + string(getResultName(allocResult)));
    }

    // Create images and image views
    std::vector<de::MovePtr<ImageWithBuffer>> images(numSetsAllocated);
    std::vector<Move<VkImageView>> imageViews(numSetsAllocated);
    std::vector<tcu::Vec4> referenceColors(numSetsAllocated);

    de::Random rnd(0x123456);

    for (uint32_t setIdx = 0; setIdx < numSetsAllocated; setIdx++)
    {
        images[setIdx] = de::MovePtr<ImageWithBuffer>(
            new ImageWithBuffer(vk, device, allocator, extent, imageFormat,
                                (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), VK_IMAGE_TYPE_2D));

        const VkImageViewCreateInfo imageViewCreateInfo = makeImageViewCreateInfo(
            images[setIdx]->getImage(), imageFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));

        imageViews[setIdx] = createImageView(vk, device, &imageViewCreateInfo);

        // Initialize reference colors
        {
            const auto channel1Value = rnd.getFloat(0.9f, 1.0f);
            const auto channel2Value = rnd.getFloat(0.9f, 1.0f);
            referenceColors[setIdx]  = tcu::Vec4(channel1Value, 0.0f, channel2Value, 1.0f);
        }

        // Initialize images
        {
            auto &imageBufferAlloc = images[setIdx]->getBufferAllocation();
            auto imageBufferPtr =
                reinterpret_cast<char *>(imageBufferAlloc.getHostPtr()) + imageBufferAlloc.getOffset();

            tcu::PixelBufferAccess imageBufferPixels(tcuFormat, extent.width, extent.height, extent.depth,
                                                     imageBufferPtr);

            tcu::clear(imageBufferPixels, referenceColors[setIdx]);

            flushAlloc(vk, device, imageBufferAlloc);
        }
    }

    // Update allocated descriptors
    for (uint32_t setIdx = 0; setIdx < numSetsAllocated; setIdx++)
    {
        const VkDescriptorSet &destSet = descSets[setIdx];

        DescriptorSetUpdateBuilder descSetUpdateBuilder;

        const auto descInfo =
            makeDescriptorImageInfo(*imageSampler, *imageViews[setIdx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        descSetUpdateBuilder.writeSingle(destSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                         &descInfo);

        descSetUpdateBuilder.update(vk, device);
    }

    // Create pipeline
    const auto pipelineLayout = makePipelineLayout(vk, device, numElemsPerSet, &(*descSetLayout));

    // Create image and image view that will hold rendered frame
    const VkImageCreateInfo colorImageCreateInfo =
        makeImageCreateInfo(colorFormat, renderSize.x(), renderSize.y(),
                            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
    de::MovePtr<ImageWithMemory> colorImage = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, allocator, colorImageCreateInfo, MemoryRequirement::Any));
    const VkImageViewCreateInfo colorImageViewCreateInfo =
        makeImageViewCreateInfo(**colorImage, colorFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
    const Move<VkImageView> colorImageView = createImageView(vk, device, &colorImageViewCreateInfo);

    // Create renderpass and framebuffer
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, colorFormat);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());

    // Create output buffer for verification
    const VkDeviceSize outputBufferDataSize =
        static_cast<VkDeviceSize>(renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat)));
    const VkBufferCreateInfo outputBufferCreateInfo =
        makeBufferCreateInfo(outputBufferDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory outputBuffer(vk, device, allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);

    // Create graphics pipeline
    Move<VkPipeline> pipeline;
    {
        const Unique<VkShaderModule> vertexShaderModule(
            createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
        const Unique<VkShaderModule> fragmentShaderModule(
            createShaderModule(vk, device, context.getBinaryCollection().get("frag"), 0u));

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = initVulkanStructure();

        const std::vector<VkViewport> viewports(1, makeViewport(renderSize));
        const std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

        pipeline = makeGraphicsPipeline(
            vk,                    // const DeviceInterface&                        vk
            device,                // const VkDevice                                device
            *pipelineLayout,       // const VkPipelineLayout                        pipelineLayout
            *vertexShaderModule,   // const VkShaderModule                          vertexShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          tessellationControlShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          tessellationEvalShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          geometryShaderModule
            *fragmentShaderModule, // const VkShaderModule                          fragmentShaderModule
            *renderPass,           // const VkRenderPass                            renderPass
            viewports,             // const std::vector<VkViewport>&                viewports
            scissors,              // const std::vector<VkRect2D>&                  scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
            0u,                                  // const uint32_t                                subpass
            0u,                                  // const uint32_t                                patchControlPoints
            &vertexInputStateParams); // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
    }

    // Run verification shader for each set
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> commandBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    for (uint32_t setIdx = 0u; setIdx < numSetsAllocated; setIdx++)
    {
        beginCommandBuffer(vk, *commandBuffer);

        {
            const VkImageSubresourceLayers layerSubresource =
                makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
            const std::vector<VkBufferImageCopy> bufferImageCopy(1, makeBufferImageCopy(extent, layerSubresource));
            copyBufferToImage(vk, *commandBuffer, images[setIdx]->getBuffer(), imageBufferSizeBytes, bufferImageCopy,
                              VK_IMAGE_ASPECT_COLOR_BIT, 1u, 1u, images[setIdx]->getImage());
        }

        const VkRect2D renderArea = makeRect2D(renderSize);
        const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        beginRenderPass(vk, *commandBuffer, *renderPass, *framebuffer, renderArea, clearColor);

        vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

        vk.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                 &descSets[setIdx], 0u, nullptr);

        vk.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);

        endRenderPass(vk, *commandBuffer);
        copyImageToBuffer(vk, *commandBuffer, **colorImage, *outputBuffer, renderSize);

        endCommandBuffer(vk, *commandBuffer);
        submitCommandsAndWait(vk, device, queue, *commandBuffer);

        // Validation
        {
            invalidateAlloc(vk, device, outputBuffer.getAllocation());

            tcu::ConstPixelBufferAccess resultBufferAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1,
                                                           outputBuffer.getAllocation().getHostPtr());

            auto &referenceBufferAlloc = images[setIdx]->getBufferAllocation();
            auto referenceBufferPtr =
                reinterpret_cast<char *>(referenceBufferAlloc.getHostPtr()) + referenceBufferAlloc.getOffset();

            tcu::PixelBufferAccess referenceAccess(tcuFormat, extent.width, extent.height, extent.depth,
                                                   referenceBufferPtr);

            const tcu::Vec4 threshold(0.001f, 0.0f, 0.001f, 0.0f);
            if (!tcu::floatThresholdCompare(log, "Image comparison", "Image comparison", referenceAccess,
                                            resultBufferAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
            {
                std::ostringstream msg;
                msg << "Image comparison failed on set = " << setIdx << "\n";
                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;

                return tcu::TestStatus::fail("Image comparison failed; check log");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

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

    // Test allocation and use of large number of descriptor sets
    {
        struct DescriptorType
        {
            const VkDescriptorType descType;
            const char *descTypeName;
        } descTypes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "uniform_buffer"},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "storage_buffer"},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "combined_sampler"},
        };

        for (const auto descriptorCount : {64u, 128u, 256u, 1024u})
        {
            for (const auto &descriptorType : descTypes)
            {
                OutOfBoundsSetTestParams params = {descriptorType.descType, descriptorCount};

                const std::string testName =
                    "out_of_bounds_set_count_" + de::toString(descriptorCount) + "_" + descriptorType.descTypeName;

                if (descriptorType.descType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    addFunctionCaseWithPrograms(descriptorPoolTests.get(), testName,
                                                outOfBoundsSetWithImagesCheckSupport, outOfBoundsSetWithImagesPrograms,
                                                outOfBoundsSetWithImagesRun, params);
                else
                    addFunctionCaseWithPrograms(
                        descriptorPoolTests.get(), testName, outOfBoundsSetWithBuffersCheckSupport,
                        outOfBoundsSetWithBuffersPrograms, outOfBoundsSetWithBuffersRun, params);
            }
        }
    }

    return descriptorPoolTests.release();
}

} // namespace api
} // namespace vkt
