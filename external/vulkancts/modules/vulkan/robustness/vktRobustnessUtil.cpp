/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Imagination Technologies Ltd.
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
 * \brief Robustness Utilities
 *//*--------------------------------------------------------------------*/

#include "vktRobustnessUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "tcuCommandLine.hpp"
#include "vkDeviceUtil.hpp"
#include "deMath.h"
#include <iomanip>
#include <limits>
#include <sstream>

namespace vkt
{
namespace robustness
{

using namespace vk;
using std::string;
using std::vector;

Move<VkDevice> createRobustBufferAccessDevice(Context &context, const VkPhysicalDeviceFeatures2 *enabledFeatures2)
{
    const float queuePriority = 1.0f;

    // Create a universal queue that supports graphics and compute
    const VkDeviceQueueCreateInfo queueParams = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                    // const void* pNext;
        0u,                                         // VkDeviceQueueCreateFlags flags;
        context.getUniversalQueueFamilyIndex(),     // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    VkPhysicalDeviceFeatures enabledFeatures = context.getDeviceFeatures();
    enabledFeatures.robustBufferAccess       = true;

    // \note Extensions in core are not explicitly enabled even though
    //         they are in the extension list advertised to tests.
    const auto &extensionPtrs = context.getDeviceCreationExtensions();

    void *pNext = (void *)enabledFeatures2;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;

    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (context.getTestContext().getCommandLine().isSubProcess())
    {
        if (context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                context.getResourceInterface()->getCacheDataSize(),       // uintptr_t initialDataSize;
                context.getResourceInterface()->getCacheData()            // const void* pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }
#endif

    const VkDeviceCreateInfo deviceParams = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,         // VkStructureType sType;
        pNext,                                        // const void* pNext;
        0u,                                           // VkDeviceCreateFlags flags;
        1u,                                           // uint32_t queueCreateInfoCount;
        &queueParams,                                 // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                           // uint32_t enabledLayerCount;
        nullptr,                                      // const char* const* ppEnabledLayerNames;
        de::sizeU32(extensionPtrs),                   // uint32_t enabledExtensionCount;
        de::dataOrNull(extensionPtrs),                // const char* const* ppEnabledExtensionNames;
        enabledFeatures2 ? nullptr : &enabledFeatures // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    // We are creating a custom device with a potentially large amount of extensions and features enabled, using the default device
    // as a reference. Some implementations may only enable certain device extensions if some instance extensions are enabled, so in
    // this case it's important to reuse the context instance when creating the device.
    const auto &vki           = context.getInstanceInterface();
    const auto instance       = context.getInstance();
    const auto physicalDevice = chooseDevice(vki, instance, context.getTestContext().getCommandLine());

    return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                              context.getPlatformInterface(), instance, vki, physicalDevice, &deviceParams);
}

bool areEqual(float a, float b)
{
    return deFloatAbs(a - b) <= 0.001f;
}

bool isValueZero(const void *valuePtr, size_t valueSizeInBytes)
{
    const uint8_t *bytePtr = reinterpret_cast<const uint8_t *>(valuePtr);

    for (size_t i = 0; i < valueSizeInBytes; i++)
    {
        if (bytePtr[i] != 0)
            return false;
    }

    return true;
}

bool isValueWithinBuffer(const void *buffer, VkDeviceSize bufferSize, const void *valuePtr, size_t valueSizeInBytes)
{
    const uint8_t *byteBuffer = reinterpret_cast<const uint8_t *>(buffer);

    if (bufferSize < ((VkDeviceSize)valueSizeInBytes))
        return false;

    for (VkDeviceSize i = 0; i <= (bufferSize - valueSizeInBytes); i++)
    {
        if (!deMemCmp(&byteBuffer[i], valuePtr, valueSizeInBytes))
            return true;
    }

    return false;
}

bool isValueWithinBufferOrZero(const void *buffer, VkDeviceSize bufferSize, const void *valuePtr,
                               size_t valueSizeInBytes)
{
    return isValueWithinBuffer(buffer, bufferSize, valuePtr, valueSizeInBytes) ||
           isValueZero(valuePtr, valueSizeInBytes);
}

template <typename T>
bool verifyVec4IntegerValues(const void *vecPtr)
{
    const T Tzero = T{0};
    const T Tone  = T{1};
    const T Tmax  = std::numeric_limits<T>::max();

    T values[4];
    deMemcpy(values, vecPtr, 4 * sizeof(T));
    return (values[0] == Tzero && values[1] == Tzero && values[2] == Tzero &&
            (values[3] == Tzero || values[3] == Tone || values[3] == Tmax));
}

bool verifyOutOfBoundsVec4(const void *vecPtr, VkFormat bufferFormat)
{
    if (isUintFormat(bufferFormat))
    {
        if (bufferFormat == VK_FORMAT_R64_UINT)
            return verifyVec4IntegerValues<uint64_t>(vecPtr);
        return verifyVec4IntegerValues<uint32_t>(vecPtr);
    }
    else if (isIntFormat(bufferFormat))
    {
        if (bufferFormat == VK_FORMAT_R64_SINT)
            return verifyVec4IntegerValues<int64_t>(vecPtr);
        return verifyVec4IntegerValues<int32_t>(vecPtr);
    }
    else if (isFloatFormat(bufferFormat))
    {
        const float *data = (float *)vecPtr;

        return areEqual(data[0], 0.0f) && areEqual(data[1], 0.0f) && areEqual(data[2], 0.0f) &&
               (areEqual(data[3], 0.0f) || areEqual(data[3], 1.0f));
    }
    else if (bufferFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
    {
        return *((uint32_t *)vecPtr) == 0xc0000000u;
    }

    DE_ASSERT(false);
    return false;
}

void populateBufferWithTestValues(void *buffer, VkDeviceSize size, VkFormat format)
{
    // Assign a sequence of 32-bit values
    for (VkDeviceSize scalarNdx = 0; scalarNdx < size / 4; scalarNdx++)
    {
        const uint32_t valueIndex = (uint32_t)(2 + scalarNdx); // Do not use 0 or 1

        if (isUintFormat(format))
        {
            reinterpret_cast<uint32_t *>(buffer)[scalarNdx] = valueIndex;
        }
        else if (isIntFormat(format))
        {
            reinterpret_cast<int32_t *>(buffer)[scalarNdx] = -int32_t(valueIndex);
        }
        else if (isFloatFormat(format))
        {
            reinterpret_cast<float *>(buffer)[scalarNdx] = float(valueIndex);
        }
        else if (format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
        {
            const uint32_t r = ((valueIndex + 0) & ((2u << 10) - 1u));
            const uint32_t g = ((valueIndex + 1) & ((2u << 10) - 1u));
            const uint32_t b = ((valueIndex + 2) & ((2u << 10) - 1u));
            const uint32_t a = ((valueIndex + 0) & ((2u << 2) - 1u));

            reinterpret_cast<uint32_t *>(buffer)[scalarNdx] = (a << 30) | (b << 20) | (g << 10) | r;
        }
        else
        {
            DE_ASSERT(false);
        }
    }
}

void logValue(std::ostringstream &logMsg, const void *valuePtr, VkFormat valueFormat, size_t valueSize)
{
    if (isUintFormat(valueFormat))
    {
        logMsg << *reinterpret_cast<const uint32_t *>(valuePtr);
    }
    else if (isIntFormat(valueFormat))
    {
        logMsg << *reinterpret_cast<const int32_t *>(valuePtr);
    }
    else if (isFloatFormat(valueFormat))
    {
        logMsg << *reinterpret_cast<const float *>(valuePtr);
    }
    else
    {
        const uint8_t *bytePtr               = reinterpret_cast<const uint8_t *>(valuePtr);
        const std::ios::fmtflags streamFlags = logMsg.flags();

        logMsg << std::hex;
        for (size_t i = 0; i < valueSize; i++)
        {
            logMsg << " " << (uint32_t)bytePtr[i];
        }
        logMsg.flags(streamFlags);
    }
}

// TestEnvironment

TestEnvironment::TestEnvironment(Context &context, const DeviceInterface &vk, VkDevice device,
                                 VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet descriptorSet)
    : m_context(context)
    , m_device(device)
    , m_descriptorSetLayout(descriptorSetLayout)
    , m_descriptorSet(descriptorSet)
{
    // Create command pool
    {
        const VkCommandPoolCreateInfo commandPoolParams = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                    // const void* pNext;
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,       // VkCommandPoolCreateFlags flags;
            context.getUniversalQueueFamilyIndex()      // uint32_t queueFamilyIndex;
        };

        m_commandPool = createCommandPool(vk, m_device, &commandPoolParams);
    }

    // Create command buffer
    {
        const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType sType;
            DE_NULL,                                        // const void* pNext;
            *m_commandPool,                                 // VkCommandPool commandPool;
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel level;
            1u,                                             // uint32_t bufferCount;
        };

        m_commandBuffer = allocateCommandBuffer(vk, m_device, &commandBufferAllocateInfo);
    }
}

VkCommandBuffer TestEnvironment::getCommandBuffer(void)
{
    return *m_commandBuffer;
}

// GraphicsEnvironment

GraphicsEnvironment::GraphicsEnvironment(Context &context, const DeviceInterface &vk, VkDevice device,
                                         VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet descriptorSet,
                                         const VertexBindings &vertexBindings, const VertexAttributes &vertexAttributes,
                                         const DrawConfig &drawConfig, bool testPipelineRobustness)

    : TestEnvironment(context, vk, device, descriptorSetLayout, descriptorSet)
    , m_renderSize(16, 16)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
{
    const auto &vki                               = context.getInstanceInterface();
    const auto instance                           = context.getInstance();
    const uint32_t queueFamilyIndex               = context.getUniversalQueueFamilyIndex();
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    const VkPhysicalDevice physicalDevice = chooseDevice(vki, instance, context.getTestContext().getCommandLine());
    SimpleAllocator memAlloc(vk, m_device, getPhysicalDeviceMemoryProperties(vki, physicalDevice));

    // Create color image and view
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            DE_NULL,                                                               // const void* pNext;
            0u,                                                                    // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
            m_colorFormat,                                                         // VkFormat format;
            {(uint32_t)m_renderSize.x(), (uint32_t)m_renderSize.y(), 1u},          // VkExtent3D extent;
            1u,                                                                    // uint32_t mipLevels;
            1u,                                                                    // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            1u,                                                                    // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout initialLayout;
        };

        m_colorImage = createImage(vk, m_device, &colorImageParams);
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, m_device, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(m_device, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));

        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
            DE_NULL,                                    // const void* pNext;
            0u,                                         // VkImageViewCreateFlags flags;
            *m_colorImage,                              // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
            m_colorFormat,                              // VkFormat format;
            componentMappingRGBA,                       // VkComponentMapping components;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, m_device, &colorAttachmentViewParams);
    }

    // Create render pass
    m_renderPass = makeRenderPass(vk, m_device, m_colorFormat);

    // Create framebuffer
    {
        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            1u,                                        // uint32_t attachmentCount;
            &m_colorAttachmentView.get(),              // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_framebuffer = createFramebuffer(vk, m_device, &framebufferParams);
    }

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t setLayoutCount;
            &m_descriptorSetLayout,                        // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            DE_NULL                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = createPipelineLayout(vk, m_device, &pipelineLayoutParams);
    }

    m_vertexShaderModule   = createShaderModule(vk, m_device, m_context.getBinaryCollection().get("vertex"), 0);
    m_fragmentShaderModule = createShaderModule(vk, m_device, m_context.getBinaryCollection().get("fragment"), 0);

    // Create pipeline
    {
        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            (uint32_t)vertexBindings.size(),                           // uint32_t vertexBindingDescriptionCount;
            vertexBindings.data(),             // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            (uint32_t)vertexAttributes.size(), // uint32_t vertexAttributeDescriptionCount;
            vertexAttributes.data()            // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const std::vector<VkViewport> viewports(1, makeViewport(m_renderSize));
        const std::vector<VkRect2D> scissors(1, makeRect2D(m_renderSize));

        const void *pNext = DE_NULL;
#ifndef CTS_USES_VULKANSC
        VkPipelineRobustnessCreateInfoEXT pipelineRobustnessInfo = initVulkanStructure();

        if (testPipelineRobustness)
        {
            pipelineRobustnessInfo.storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
            pipelineRobustnessInfo.uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
            pipelineRobustnessInfo.vertexInputs   = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
            pipelineRobustnessInfo.images         = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DISABLED_EXT;
            pNext                                 = &pipelineRobustnessInfo;
        }
#else
        DE_UNREF(testPipelineRobustness);
#endif

        m_graphicsPipeline = makeGraphicsPipeline(
            vk,                      // const DeviceInterface&                        vk
            m_device,                // const VkDevice                                device
            *m_pipelineLayout,       // const VkPipelineLayout                        pipelineLayout
            *m_vertexShaderModule,   // const VkShaderModule                          vertexShaderModule
            DE_NULL,                 // const VkShaderModule                          tessellationControlShaderModule
            DE_NULL,                 // const VkShaderModule                          tessellationEvalShaderModule
            DE_NULL,                 // const VkShaderModule                          geometryShaderModule
            *m_fragmentShaderModule, // const VkShaderModule                          fragmentShaderModule
            *m_renderPass,           // const VkRenderPass                            renderPass
            viewports,               // const std::vector<VkViewport>&                viewports
            scissors,                // const std::vector<VkRect2D>&                  scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // const VkPrimitiveTopology                     topology
            0u,                                   // const uint32_t                                subpass
            0u,                                   // const uint32_t                                patchControlPoints
            &vertexInputStateParams, // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
            DE_NULL,                 // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
            DE_NULL,                 // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
            DE_NULL,                 // const VkPipelineDepthStencilStateCreateInfo*        depthStencilStateCreateInfo
            DE_NULL,                 // const VkPipelineColorBlendStateCreateInfo*        colorBlendStateCreateInfo
            DE_NULL,                 // const VkPipelineDynamicStateCreateInfo*            dynamicStateCreateInfo
            pNext);                  // void* pNext
    }

    // Record commands
    {
        const VkImageMemoryBarrier imageLayoutBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType sType;
            DE_NULL,                                    // const void* pNext;
            (VkAccessFlags)0,                           // VkAccessFlags srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_colorImage,                              // VkImage image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        beginCommandBuffer(vk, *m_commandBuffer, 0u);
        {
            vk.cmdPipelineBarrier(*m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0u, DE_NULL, 0u,
                                  DE_NULL, 1u, &imageLayoutBarrier);

            beginRenderPass(vk, *m_commandBuffer, *m_renderPass, *m_framebuffer,
                            makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), tcu::Vec4(0.0f));
            {
                const std::vector<VkDeviceSize> vertexBufferOffsets(drawConfig.vertexBuffers.size(), 0ull);

                vk.cmdBindPipeline(*m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);
                vk.cmdBindDescriptorSets(*m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1,
                                         &m_descriptorSet, 0, DE_NULL);
                vk.cmdBindVertexBuffers(*m_commandBuffer, 0, (uint32_t)drawConfig.vertexBuffers.size(),
                                        drawConfig.vertexBuffers.data(), vertexBufferOffsets.data());

                if (drawConfig.indexBuffer == DE_NULL || drawConfig.indexCount == 0)
                {
                    vk.cmdDraw(*m_commandBuffer, drawConfig.vertexCount, drawConfig.instanceCount, 0, 0);
                }
                else
                {
                    vk.cmdBindIndexBuffer(*m_commandBuffer, drawConfig.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                    vk.cmdDrawIndexed(*m_commandBuffer, drawConfig.indexCount, drawConfig.instanceCount, 0, 0, 0);
                }
            }
            endRenderPass(vk, *m_commandBuffer);
        }
        endCommandBuffer(vk, *m_commandBuffer);
    }
}

// ComputeEnvironment

ComputeEnvironment::ComputeEnvironment(Context &context, const DeviceInterface &vk, VkDevice device,
                                       VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet descriptorSet,
                                       bool testPipelineRobustness)

    : TestEnvironment(context, vk, device, descriptorSetLayout, descriptorSet)
{
    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t setLayoutCount;
            &m_descriptorSetLayout,                        // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            DE_NULL                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = createPipelineLayout(vk, m_device, &pipelineLayoutParams);
    }

    // Create compute pipeline
    {
        m_computeShaderModule = createShaderModule(vk, m_device, m_context.getBinaryCollection().get("compute"), 0);

        const VkPipelineShaderStageCreateInfo computeStageParams = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            *m_computeShaderModule,                              // VkShaderModule module;
            "main",                                              // const char* pName;
            DE_NULL,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        const void *pNext = DE_NULL;
#ifndef CTS_USES_VULKANSC
        VkPipelineRobustnessCreateInfoEXT pipelineRobustnessInfo = initVulkanStructure();

        if (testPipelineRobustness)
        {
            pipelineRobustnessInfo.storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
            pipelineRobustnessInfo.uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
            pipelineRobustnessInfo.vertexInputs   = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT;
            pipelineRobustnessInfo.images         = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DISABLED_EXT;
            pNext                                 = &pipelineRobustnessInfo;
        }
#else
        DE_UNREF(testPipelineRobustness);
#endif

        const VkComputePipelineCreateInfo computePipelineParams = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
            pNext,                                          // const void* pNext;
            0u,                                             // VkPipelineCreateFlags flags;
            computeStageParams,                             // VkPipelineShaderStageCreateInfo stage;
            *m_pipelineLayout,                              // VkPipelineLayout layout;
            DE_NULL,                                        // VkPipeline basePipelineHandle;
            0u                                              // int32_t basePipelineIndex;
        };

        m_computePipeline = createComputePipeline(vk, m_device, DE_NULL, &computePipelineParams);
    }

    // Record commands
    {
        beginCommandBuffer(vk, *m_commandBuffer, 0u);
        vk.cmdBindPipeline(*m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);
        vk.cmdBindDescriptorSets(*m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, 1,
                                 &m_descriptorSet, 0, DE_NULL);
        vk.cmdDispatch(*m_commandBuffer, 32, 32, 1);
        endCommandBuffer(vk, *m_commandBuffer);
    }
}

} // namespace robustness
} // namespace vkt
