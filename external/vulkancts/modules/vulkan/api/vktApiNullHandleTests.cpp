/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Null handle tests
 *//*--------------------------------------------------------------------*/

#include "vktApiNullHandleTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"

namespace vkt
{
namespace api
{
namespace
{

using namespace vk;

inline void release(Context &context, VkBuffer buffer, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyBuffer(context.getDevice(), buffer, pAllocator);
}

inline void release(Context &context, VkBufferView bufferView, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyBufferView(context.getDevice(), bufferView, pAllocator);
}

#ifndef CTS_USES_VULKANSC
inline void release(Context &context, VkCommandPool commandPool, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyCommandPool(context.getDevice(), commandPool, pAllocator);
}

inline void release(Context &context, VkDescriptorPool descriptorPool, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyDescriptorPool(context.getDevice(), descriptorPool, pAllocator);
}
#endif // CTS_USES_VULKANSC

inline void release(Context &context, VkDescriptorSetLayout descriptorSetLayout,
                    const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyDescriptorSetLayout(context.getDevice(), descriptorSetLayout, pAllocator);
}

inline void release(Context &context, VkDevice device, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyDevice(device, pAllocator);
}

inline void release(Context &context, VkEvent event, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyEvent(context.getDevice(), event, pAllocator);
}

inline void release(Context &context, VkFence fence, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyFence(context.getDevice(), fence, pAllocator);
}

inline void release(Context &context, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyFramebuffer(context.getDevice(), framebuffer, pAllocator);
}

inline void release(Context &context, VkImage image, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyImage(context.getDevice(), image, pAllocator);
}

inline void release(Context &context, VkImageView imageView, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyImageView(context.getDevice(), imageView, pAllocator);
}

inline void release(Context &context, VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
    context.getInstanceInterface().destroyInstance(instance, pAllocator);
}

inline void release(Context &context, VkPipeline pipeline, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyPipeline(context.getDevice(), pipeline, pAllocator);
}

inline void release(Context &context, VkPipelineCache pipelineCache, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyPipelineCache(context.getDevice(), pipelineCache, pAllocator);
}

inline void release(Context &context, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyPipelineLayout(context.getDevice(), pipelineLayout, pAllocator);
}

#ifndef CTS_USES_VULKANSC
inline void release(Context &context, VkQueryPool queryPool, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyQueryPool(context.getDevice(), queryPool, pAllocator);
}
#endif // CTS_USES_VULKANSC

inline void release(Context &context, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyRenderPass(context.getDevice(), renderPass, pAllocator);
}

inline void release(Context &context, VkSampler sampler, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroySampler(context.getDevice(), sampler, pAllocator);
}

inline void release(Context &context, VkSemaphore semaphore, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroySemaphore(context.getDevice(), semaphore, pAllocator);
}

inline void release(Context &context, VkShaderModule shaderModule, const VkAllocationCallbacks *pAllocator)
{
#ifndef CTS_USES_VULKANSC
    context.getDeviceInterface().destroyShaderModule(context.getDevice(), shaderModule, pAllocator);
#else
    DE_UNREF(context);
    DE_UNREF(shaderModule);
    DE_UNREF(pAllocator);
#endif // CTS_USES_VULKANSC
}

#ifndef CTS_USES_VULKANSC
inline void release(Context &context, VkShaderEXT shaderObject, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().destroyShaderEXT(context.getDevice(), shaderObject, pAllocator);
}
#endif // CTS_USES_VULKANSC

inline void release(Context &context, VkDevice device, VkCommandPool cmdPool, uint32_t numCmdBuffers,
                    const VkCommandBuffer *pCmdBuffers)
{
    DE_ASSERT(device != DE_NULL);
    DE_ASSERT(cmdPool != VK_NULL_HANDLE);
    DE_ASSERT(numCmdBuffers > 0u);
    context.getDeviceInterface().freeCommandBuffers(device, cmdPool, numCmdBuffers, pCmdBuffers);
}

inline void release(Context &context, VkDevice device, VkDescriptorPool descriptorPool, uint32_t numDescriptorSets,
                    const VkDescriptorSet *pDescriptorSets)
{
    DE_ASSERT(device != DE_NULL);
    DE_ASSERT(descriptorPool != VK_NULL_HANDLE);
    DE_ASSERT(numDescriptorSets > 0u);
    context.getDeviceInterface().freeDescriptorSets(device, descriptorPool, numDescriptorSets, pDescriptorSets);
}

#ifndef CTS_USES_VULKANSC
inline void release(Context &context, VkDeviceMemory memory, const VkAllocationCallbacks *pAllocator)
{
    context.getDeviceInterface().freeMemory(context.getDevice(), memory, pAllocator);
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus reportStatus(const bool success)
{
    if (success)
        return tcu::TestStatus::pass("OK: no observable change");
    else
        return tcu::TestStatus::fail("Implementation allocated/freed the memory");
}

template <typename Object>
tcu::TestStatus test(Context &context)
{
    const Object nullHandle                     = VK_NULL_HANDLE;
    const VkAllocationCallbacks *pNullAllocator = DE_NULL;

#ifndef CTS_USES_VULKANSC
    AllocationCallbackRecorder recordingAllocator(getSystemAllocator(), 1u);
#endif // CTS_USES_VULKANSC

    // Implementation should silently ignore a delete/free of a NULL handle.

    release(context, nullHandle, pNullAllocator);
#ifndef CTS_USES_VULKANSC
    // In Vulkan SC VkAllocationCallbacks must be NULL
    release(context, nullHandle, recordingAllocator.getCallbacks());
    return reportStatus(recordingAllocator.getNumRecords() == 0);
#else
    return reportStatus(true);
#endif // CTS_USES_VULKANSC
}

template <>
tcu::TestStatus test<VkCommandBuffer>(Context &context)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const VkCommandPoolCreateInfo cmdPoolCreateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // VkStructureType             sType;
        DE_NULL,                                    // const void*                 pNext;
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,       // VkCommandPoolCreateFlags    flags;
        queueFamilyIndex,                           // uint32_t                    queueFamilyIndex;
    };

    const VkCommandBuffer pNullHandles[] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    const uint32_t numHandles            = static_cast<uint32_t>(DE_LENGTH_OF_ARRAY(pNullHandles));

    // Default allocator
    {
        const Unique<VkCommandPool> cmdPool(createCommandPool(vk, device, &cmdPoolCreateInfo));

        release(context, device, *cmdPool, numHandles, pNullHandles);
    }

    // Custom allocator
#ifndef CTS_USES_VULKANSC
    {
        AllocationCallbackRecorder recordingAllocator(getSystemAllocator(), 1u);
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, &cmdPoolCreateInfo, recordingAllocator.getCallbacks()));
        const std::size_t numInitialRecords = recordingAllocator.getNumRecords();

        release(context, device, *cmdPool, numHandles, pNullHandles);

        return reportStatus(numInitialRecords == recordingAllocator.getNumRecords());
    }
#else
    return reportStatus(true);
#endif // CTS_USES_VULKANSC
}

#ifndef CTS_USES_VULKANSC
void checkSupportShaderObject(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_shader_object");
}
#endif // CTS_USES_VULKANSC

void checkSupportFreeDescriptorSets(Context &context)
{
#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().recycleDescriptorSetMemory == VK_FALSE)
        TCU_THROW(NotSupportedError, "vkFreeDescriptorSets not supported");
#else
    DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

template <>
tcu::TestStatus test<VkDescriptorSet>(Context &context)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();

    const VkDescriptorPoolSize pPoolSizes[] = {
        // type, descriptorCount
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2u}, // arbitrary values
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1u},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3u},
    };
    const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,         // VkStructureType                sType;
        DE_NULL,                                               // const void*                    pNext;
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,     // VkDescriptorPoolCreateFlags    flags;
        2u,                                                    // uint32_t                       maxSets;
        static_cast<uint32_t>(DE_LENGTH_OF_ARRAY(pPoolSizes)), // uint32_t                       poolSizeCount;
        pPoolSizes,                                            // const VkDescriptorPoolSize*    pPoolSizes;
    };

    const VkDescriptorSet pNullHandles[] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    const uint32_t numHandles            = static_cast<uint32_t>(DE_LENGTH_OF_ARRAY(pNullHandles));

    // Default allocator
    {
        const Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(vk, device, &descriptorPoolCreateInfo));

        release(context, device, *descriptorPool, numHandles, pNullHandles);
    }

    // Custom allocator
#ifndef CTS_USES_VULKANSC
    {
        AllocationCallbackRecorder recordingAllocator(getSystemAllocator(), 1u);
        const Unique<VkDescriptorPool> descriptorPool(
            createDescriptorPool(vk, device, &descriptorPoolCreateInfo, recordingAllocator.getCallbacks()));
        const std::size_t numInitialRecords = recordingAllocator.getNumRecords();

        release(context, device, *descriptorPool, numHandles, pNullHandles);

        return reportStatus(numInitialRecords == recordingAllocator.getNumRecords());
    }
#else
    return reportStatus(true);
#endif // CTS_USES_VULKANSC
}

void checkEventSupport(Context &context)
{
#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().events)
        TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
#else
    DE_UNREF(context);
#endif // CTS_USES_VULKANSC
}

void addTestsToGroup(tcu::TestCaseGroup *group)
{
    addFunctionCase(group, "destroy_buffer", test<VkBuffer>);
    addFunctionCase(group, "destroy_buffer_view", test<VkBufferView>);
#ifndef CTS_USES_VULKANSC
    // Removed from Vulkan SC test set: vkDestroyCommandPool and vkDestroyDescriptorPool command do not exist in Vulkan SC
    addFunctionCase(group, "destroy_command_pool", test<VkCommandPool>);
    addFunctionCase(group, "destroy_descriptor_pool", test<VkDescriptorPool>);
#endif // CTS_USES_VULKANSC
    addFunctionCase(group, "destroy_descriptor_set_layout", test<VkDescriptorSetLayout>);
    addFunctionCase(group, "destroy_device", test<VkDevice>);
    addFunctionCase(group, "destroy_event", checkEventSupport, test<VkEvent>);
    addFunctionCase(group, "destroy_fence", test<VkFence>);
    addFunctionCase(group, "destroy_framebuffer", test<VkFramebuffer>);
    addFunctionCase(group, "destroy_image", test<VkImage>);
    addFunctionCase(group, "destroy_image_view", test<VkImageView>);
    addFunctionCase(group, "destroy_instance", test<VkInstance>);
    addFunctionCase(group, "destroy_pipeline", test<VkPipeline>);
    addFunctionCase(group, "destroy_pipeline_cache", test<VkPipelineCache>);
    addFunctionCase(group, "destroy_pipeline_layout", test<VkPipelineLayout>);
#ifndef CTS_USES_VULKANSC
    // Removed from Vulkan SC test set: vkDestroyQueryPool command does not exist in Vulkan SC
    addFunctionCase(group, "destroy_query_pool", test<VkQueryPool>);
#endif // CTS_USES_VULKANSC
    addFunctionCase(group, "destroy_render_pass", test<VkRenderPass>);
    addFunctionCase(group, "destroy_sampler", test<VkSampler>);
    addFunctionCase(group, "destroy_semaphore", test<VkSemaphore>);
    addFunctionCase(group, "destroy_shader_module", test<VkShaderModule>);
#ifndef CTS_USES_VULKANSC
    addFunctionCase(group, "destroy_shader_object", checkSupportShaderObject, test<VkShaderEXT>);
#endif // CTS_USES_VULKANSC
    addFunctionCase(group, "free_command_buffers", test<VkCommandBuffer>);
    addFunctionCase(group, "free_descriptor_sets", checkSupportFreeDescriptorSets, test<VkDescriptorSet>);
#ifndef CTS_USES_VULKANSC
    // Removed from Vulkan SC test set: vkFreeMemory command does not exist in Vulkan SC
    addFunctionCase(group, "free_memory", test<VkDeviceMemory>);
#endif // CTS_USES_VULKANSC
}

} // namespace

tcu::TestCaseGroup *createNullHandleTests(tcu::TestContext &testCtx)
{
    // Destroying/freeing a VK_NULL_HANDLE should be silently ignored
    return createTestGroup(testCtx, "null_handle", addTestsToGroup);
}

} // namespace api
} // namespace vkt
