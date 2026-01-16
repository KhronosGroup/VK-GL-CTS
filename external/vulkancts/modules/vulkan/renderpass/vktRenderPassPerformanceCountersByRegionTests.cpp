/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (C) 2025 Arm Limited or its affiliates.
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
 * \brief VK_ARM_performance_counters_by_region tests.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassPerformanceCountersByRegionTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deMath.h"

#include <limits>

#ifndef CTS_USES_VULKANSC

using namespace vk;

using tcu::UVec2;
using tcu::Vec2;
using tcu::Vec4;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;

using tcu::TestLog;

using std::string;
using std::vector;

namespace vkt
{
namespace
{
using namespace renderpass;

struct CounterConfig
{
    CounterConfig(std::string name_, uint32_t regionMin_, uint32_t regionMax_, uint32_t fragment_)
        : name(name_)
        , regionMin(regionMin_)
        , regionMax(regionMax_)
        , fragment(fragment_)
    {
    }

    std::string name;
    uint32_t regionMin;
    uint32_t regionMax;
    uint32_t fragment;
};

struct TestConfig
{
    TestConfig(VkFormat format_, const SharedGroupParams groupParams_, std::vector<CounterConfig> counters_,
               uint32_t layerCount_)
        : format(format_)
        , groupParams(groupParams_)
        , counters(counters_)
        , layerCount(layerCount_)
    {
    }

    VkFormat format;
    SharedGroupParams groupParams;
    std::vector<CounterConfig> counters;
    uint32_t layerCount;
};

struct pushConstData
{
    float width;
    float height;
    int layer;
};

de::MovePtr<Allocation> createImageMemory(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                          VkImage image)
{
    de::MovePtr<Allocation> allocation(
        allocator.allocate(getImageMemoryRequirements(vkd, device, image), MemoryRequirement::Any));
    VK_CHECK(vkd.bindImageMemory(device, image, allocation->getMemory(), allocation->getOffset()));
    return allocation;
}

Move<VkImage> createImage(const InstanceInterface &vki, VkPhysicalDevice physicalDevice, const DeviceInterface &vk,
                          VkDevice device, VkFormat format, uint32_t width, uint32_t height, uint32_t layerCount)
{
    const VkExtent3D imageExtent = {width, height, 1u};

    const VkImageCreateInfo pCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType;
        nullptr,                             // const void*              pNext;
        0,                                   // VkImageCreateFlags       flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType              imageType;
        format,                              // VkFormat                 format;
        imageExtent,                         // VkExtent3D               extent;
        1,                                   // uint32_t                 mipLevels;
        layerCount,                          // uint32_t                 arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags        usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode;
        0,                                   // uint32_t                 queueFamilyIndexCount;
        nullptr,                             // const uint32_t*          pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout            initialLayout;
    };

    checkImageSupport(vki, physicalDevice, pCreateInfo);

    return createImage(vk, device, &pCreateInfo);
}

Move<VkImageView> createImageView(const DeviceInterface &vk, VkDevice device, VkImage image, VkFormat format,
                                  uint32_t layerCount)
{
    const VkImageSubresourceRange range = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask;
        0u,                        // uint32_t              baseMipLevel;
        1u,                        // uint32_t              levelCount;
        0u,                        // uint32_t              baseArrayLayer;
        layerCount                 // uint32_t              layerCount;
    };

    const VkImageViewCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,                                // VkStructureType            sType;
        nullptr,                                                                 // const void*                pNext;
        0u,                                                                      // VkImageViewCreateFlags     flags;
        image,                                                                   // VkImage                    image;
        (layerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY, // VkImageViewType            viewType;
        format,                                                                  // VkFormat                   format;
        makeComponentMappingRGBA(), // VkComponentMapping         components;
        range,                      // VkImageSubresourceRange    subresourceRange;
    };
    return createImageView(vk, device, &createInfo);
}

VkDeviceSize getImageBufferSize(const InstanceInterface &vki, VkPhysicalDevice physicalDevice, uint32_t width,
                                uint32_t height, uint32_t layerCount, VkFormat format)
{
    const VkDeviceSize nonCoherentAtomSize =
        vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.nonCoherentAtomSize;
    const VkDeviceSize alignmentSize = std::max<VkDeviceSize>(nonCoherentAtomSize, 4u);

    const uint32_t pixelSize           = static_cast<uint32_t>(tcu::getPixelSize(mapVkFormat(format)));
    const VkDeviceSize colorBufferSize = static_cast<VkDeviceSize>(
        deAlignSize(width * height * pixelSize, static_cast<std::size_t>(alignmentSize)) * layerCount);
    return colorBufferSize;
}

Move<VkBuffer> createBuffer(const DeviceInterface &vkd, VkDevice device, VkDeviceSize size,
                            VkBufferUsageFlags bufferUsage)
{
    const VkBufferCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType        sType;
        nullptr,                              // const void*            pNext;
        0u,                                   // VkBufferCreateFlags    flags;

        size,        // VkDeviceSize           size;
        bufferUsage, // VkBufferUsageFlags     usage;

        VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode          sharingMode;
        0u,                        // uint32_t               queueFamilyIndexCount;
        nullptr                    // const uint32_t*        pQueueFamilyIndices;
    };
    return createBuffer(vkd, device, &createInfo);
}

de::MovePtr<Allocation> createBufferMemory(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                           VkBuffer buffer, MemoryRequirement requirements)
{
    de::MovePtr<Allocation> allocation(
        allocator.allocate(getBufferMemoryRequirements(vkd, device, buffer), requirements));
    VK_CHECK(vkd.bindBufferMemory(device, buffer, allocation->getMemory(), allocation->getOffset()));
    return allocation;
}

VkDeviceAddress getBufferDeviceAddress(const DeviceInterface &vkd, VkDevice device, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {};
    bufferDeviceAddressInfo.sType                     = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
    bufferDeviceAddressInfo.pNext                     = nullptr;
    bufferDeviceAddressInfo.buffer                    = buffer;
    return vkd.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
}

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass(const DeviceInterface &vkd, VkDevice device, VkFormat format,
                                    VkSampleCountFlagBits sampleCount)
{
    // Color attachment
    const AttachmentDesc
        attachment //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,     //   ||  const void* pNext;
            0u,          //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
            format,      //  VkFormat format; ||  VkFormat format;
            sampleCount, //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,    //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
        );
    const AttachmentRef
        attachmentRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                                  //   ||  const void* pNext;
            0,                                        //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout layout; ||  VkImageLayout layout;
            0u                                        // ||  VkImageAspectFlags aspectMask;
        );

    const SubpassDesc
        subpass //  VkSubpassDescription                                        ||  VkSubpassDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                      //   ||  const void* pNext;
            (VkSubpassDescriptionFlags)0, //  VkSubpassDescriptionFlags flags; ||  VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, //  VkPipelineBindPoint pipelineBindPoint; ||  VkPipelineBindPoint pipelineBindPoint;
            0u,                              //   ||  uint32_t viewMask;
            0u,                              //  uint32_t inputAttachmentCount; ||  uint32_t inputAttachmentCount;
            nullptr, //  const VkAttachmentReference* pInputAttachments; ||  const VkAttachmentReference2KHR* pInputAttachments;
            1u, //  uint32_t colorAttachmentCount; ||  uint32_t colorAttachmentCount;
            &attachmentRef, //  const VkAttachmentReference* pColorAttachments; ||  const VkAttachmentReference2KHR* pColorAttachments;
            nullptr, //  const VkAttachmentReference* pResolveAttachments; ||  const VkAttachmentReference2KHR* pResolveAttachments;
            nullptr, //  const VkAttachmentReference* pDepthStencilAttachment; ||  const VkAttachmentReference2KHR* pDepthStencilAttachment;
            0u,     //  uint32_t preserveAttachmentCount; ||  uint32_t preserveAttachmentCount;
            nullptr //  const uint32_t* pPreserveAttachments; ||  const uint32_t* pPreserveAttachments;
        );

    const RenderPassCreateInfo
        renderPassCreator //  VkRenderPassCreateInfo                                        ||  VkRenderPassCreateInfo2KHR
        (
            //  VkStructureType sType; ||  VkStructureType sType;
            nullptr,                     //  const void* pNext; ||  const void* pNext;
            (VkRenderPassCreateFlags)0u, //  VkRenderPassCreateFlags flags; ||  VkRenderPassCreateFlags flags;
            1,                           //  uint32_t attachmentCount; ||  uint32_t attachmentCount;
            &attachment, //  const VkAttachmentDescription* pAttachments; ||  const VkAttachmentDescription2KHR* pAttachments;
            1,           //  uint32_t subpassCount; ||  uint32_t subpassCount;
            &subpass, //  const VkSubpassDescription* pSubpasses; ||  const VkSubpassDescription2KHR* pSubpasses;
            0,        //  uint32_t dependencyCount; ||  uint32_t dependencyCount;
            nullptr,  //  const VkSubpassDependency* pDependencies; ||  const VkSubpassDependency2KHR* pDependencies;
            0u,       //   ||  uint32_t correlatedViewMaskCount;
            nullptr   //  ||  const uint32_t* pCorrelatedViewMasks;
        );

    return renderPassCreator.createRenderPass(vkd, device);
}

Move<VkRenderPass> createRenderPass(const DeviceInterface &vkd, VkDevice device, VkFormat format,
                                    VkSampleCountFlagBits sampleCount, const RenderingType renderingType)
{
    switch (renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        return createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1,
                                RenderPassCreateInfo1>(vkd, device, format, sampleCount);
    case RENDERING_TYPE_RENDERPASS2:
        return createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2,
                                RenderPassCreateInfo2>(vkd, device, format, sampleCount);
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        return Move<VkRenderPass>();
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

Move<VkFramebuffer> createFramebuffer(const DeviceInterface &vkd, VkDevice device, VkRenderPass renderPass,
                                      VkImageView imageView, uint32_t width, uint32_t height, uint32_t layerCount)
{
    // When RenderPass was not created then we are testing dynamic rendering
    // and do not require a framebuffer
    if (renderPass == VK_NULL_HANDLE)
    {
        return Move<VkFramebuffer>();
    }

    const VkFramebufferCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType             sType;
        nullptr,                                   // const void*                 pNext;
        0u,                                        // VkFramebufferCreateFlags    flags;
        renderPass,                                // VkRenderPass                renderPass;
        1u,                                        // uint32_t                    attachmentCount;
        &imageView,                                // const VkImageView*          pAttachments;
        width,                                     // uint32_t                    width;
        height,                                    // uint32_t                    height;
        layerCount                                 // uint32_t                    layers;
    };

    return createFramebuffer(vkd, device, &createInfo);
}

Move<VkDescriptorSetLayout> createDescriptorSetLayout(const DeviceInterface &vkd, VkDevice device)
{

    const VkDescriptorSetLayoutBinding binding = {
        0u,                                // uint32_t              binding;
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // VkDescriptorType      descriptorType;
        1u,                                // uint32_t              descriptorCount;
        VK_SHADER_STAGE_FRAGMENT_BIT,      // VkShaderStageFlags    stageFlags;
        nullptr                            // const VkSampler*      pImmutableSamplers;
    };

    const VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType;
        nullptr,                                             // const void*                            pNext;
        0u,                                                  // VkDescriptorSetLayoutCreateFlags       flags;
        1u,                                                  // uint32_t                               bindingCount;
        &binding                                             // const VkDescriptorSetLayoutBinding*    pBindings;
    };

    return createDescriptorSetLayout(vkd, device, &createInfo);
}

Move<VkDescriptorPool> createDescriptorPool(const DeviceInterface &vkd, VkDevice device)
{
    const VkDescriptorPoolSize size = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // VkDescriptorType    type;
        1                                  // uint32_t            descriptorCount;
    };

    const VkDescriptorPoolCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType                sType;
        nullptr,                                           // const void*                    pNext;
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags    flags;
        1u,                                                // uint32_t                       maxSets;
        1u,                                                // uint32_t                       poolSizeCount;
        &size                                              // const VkDescriptorPoolSize*    pPoolSizes;
    };

    return createDescriptorPool(vkd, device, &createInfo);
}

Move<VkDescriptorSet> createDescriptorSet(const DeviceInterface &vkd, VkDevice device, VkDescriptorPool pool,
                                          VkDescriptorSetLayout layout, VkBuffer buffer)
{
    const VkDescriptorSetAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
        pool,   // descriptorPool
        1u,     // descriptorSetCount
        &layout // pSetLayouts
    };
    Move<VkDescriptorSet> set(allocateDescriptorSet(vkd, device, &allocateInfo));

    {
        const VkDescriptorBufferInfo bufferInfo = {buffer, 0, VK_WHOLE_SIZE};

        const VkWriteDescriptorSet writes[] = {{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            *set,                              // dstSet
            0u,                                // dstBinding
            0u,                                // dstArrayElement
            1u,                                // descriptorCount
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
            nullptr,                           // pImageInfo
            &bufferInfo,                       // pBufferInfo
            nullptr                            // pTexelBufferView
        }};

        vkd.updateDescriptorSets(device, 1, writes, 0u, nullptr);
    }
    return set;
}

VkPhysicalDevicePerformanceCountersByRegionPropertiesARM getPtpcProperties(const InstanceInterface &vki,
                                                                           VkPhysicalDevice physicalDevice)
{
    VkPhysicalDevicePerformanceCountersByRegionPropertiesARM perRegionPerfCtrProperties;
    perRegionPerfCtrProperties       = {};
    perRegionPerfCtrProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_COUNTERS_BY_REGION_PROPERTIES_ARM;

    VkPhysicalDeviceProperties2 properties2;
    properties2       = {};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &perRegionPerfCtrProperties;

    vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);

    return perRegionPerfCtrProperties;
}

VkDeviceSize getPtpcBufferSize(const VkPhysicalDevicePerformanceCountersByRegionPropertiesARM &ptpcProperties,
                               uint32_t imageWidth, uint32_t imageHeight)
{
    uint32_t maxCounters = ptpcProperties.maxPerRegionPerformanceCounters;
    uint32_t regionSize =
        deRoundUp32u(static_cast<uint32_t>(sizeof(uint32_t)) * maxCounters, ptpcProperties.regionAlignment);
    uint32_t regionsX  = deRoundUp32u(imageWidth, ptpcProperties.performanceCounterRegionSize.width);
    uint32_t regionsY  = deRoundUp32u(imageHeight, ptpcProperties.performanceCounterRegionSize.height);
    uint32_t rowStride = deRoundUp32u(regionSize * regionsX, ptpcProperties.rowStrideAlignment);

    VkDeviceSize size = rowStride * regionsY;

    return size;
}

Move<VkDevice> createCustomDevice(Context &context, const SharedGroupParams groupParams, float queuePriority)
{
    const auto &vkp                = context.getPlatformInterface();
    const auto &vki                = context.getInstanceInterface();
    const auto instance            = context.getInstance();
    const auto physicalDevice      = context.getPhysicalDevice();
    const auto supportedExtensions = enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);
    const auto queueFamilyIndex    = context.getUniversalQueueFamilyIndex();

    // Add anything that's supported and may be needed, including nullDescriptor.
    VkPhysicalDeviceFeatures2 features2                                  = initVulkanStructure();
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR deviceAddressFeatures = initVulkanStructure();
    std::vector<const char *> deviceExtensions;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures                     = initVulkanStructure();
    VkPhysicalDeviceShaderClockFeaturesKHR deviceShaderClockFeaturesKHR                   = initVulkanStructure();
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeaturesEXT = initVulkanStructure();
    VkPhysicalDevicePerformanceCountersByRegionFeaturesARM performanceCountersByRegion    = initVulkanStructure();

    const auto addFeatures = makeStructChainAdder(&features2);

    if (context.isDeviceFunctionalitySupported("VK_KHR_dynamic_rendering"))
        addFeatures(&dynamicRenderingFeatures);

    if (context.isDeviceFunctionalitySupported("VK_KHR_shader_clock"))
        addFeatures(&deviceShaderClockFeaturesKHR);

    if (context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
        addFeatures(&deviceAddressFeatures);

    if (context.isDeviceFunctionalitySupported("VK_ARM_performance_counters_by_region"))
        addFeatures(&performanceCountersByRegion);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

    // Not promoted yet in Vulkan 1.1.
    deviceExtensions.push_back("VK_KHR_shader_clock");
    deviceExtensions.push_back("VK_KHR_buffer_device_address");
    deviceExtensions.push_back("VK_ARM_performance_counters_by_region");

    if (isConstructionTypeLibrary(groupParams->pipelineConstructionType))
    {
        deviceExtensions.push_back("VK_KHR_pipeline_library");
        deviceExtensions.push_back("VK_EXT_graphics_pipeline_library");
        graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary = VK_TRUE;
        addFeatures(&graphicsPipelineLibraryFeaturesEXT);
    }

    const VkDeviceQueueCreateInfo queueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        0u,                                         // VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                           // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority,                             // const float* pQueuePriorities;
    };

    VkDeviceCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,           // VkStructureType sType;
        features2.pNext,                                // const void* pNext;
        0u,                                             // VkDeviceCreateFlags flags;
        1u,                                             // uint32_t queueCreateInfoCount;
        &queueInfo,                                     // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                             // uint32_t enabledLayerCount;
        nullptr,                                        // const char* const* ppEnabledLayerNames;
        static_cast<uint32_t>(deviceExtensions.size()), // uint32_t enabledExtensionCount;
        deviceExtensions.data(),                        // const char* const* ppEnabledExtensionNames;
        &features2.features,                            // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    const bool validationEnabled = context.getTestContext().getCommandLine().isValidationEnabled();

    vector<const char *> enabledLayers;

    if (createInfo.enabledLayerCount == 0u && validationEnabled)
    {
        enabledLayers                  = getValidationLayers(vki, physicalDevice);
        createInfo.enabledLayerCount   = static_cast<uint32_t>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = (enabledLayers.empty() ? nullptr : enabledLayers.data());
    }

    Move<VkDevice> device = createDevice(vkp, instance, vki, physicalDevice, &createInfo, nullptr);

    return device;
}

struct regionTimeStamps
{
    uint64_t start;
    uint64_t end;
};

struct BufferContainer
{
    Move<VkBuffer> m_ptpcBuffer;
    de::MovePtr<Allocation> m_ptpcBufferMemory;
    VkDeviceAddress m_ptpcBufferMemorDeviceAddress;
};

class PerformanceCountersByRegionContainer
{
public:
    PerformanceCountersByRegionContainer(Context &context, TestConfig config, tcu::ResultCollector &resultCollector,
                                         uint32_t width, uint32_t height, float queuePriority);
    ~PerformanceCountersByRegionContainer(void);

    void build(Context &context, const SharedGroupParams &groupParams);
    void submitRendering()
    {
        submit(m_commandBuffer.get());
    }
    void wait() const;
    void copyImageResults();
    void validateCounters();
    void validateAttachment() const;
    void gatherPerRegionTimestamps(std::vector<regionTimeStamps> &ts) const;
    uint32_t getNumRegions() const;

private:
    void createRenderPipeline(const BinaryCollection &binaryCollection);
    template <typename RenderpassSubpass>
    void buildInternal(Context &context);
    void buildInternalDynamicRendering(Context &context, const SharedGroupParams &groupParams);
    void buildDrawCommands(VkCommandBuffer commandBuffer);

    void initPtpcBeginRenderingStruct(Context &context,
                                      VkRenderPassPerformanceCountersByRegionBeginInfoARM &perRegionPerfCtr,
                                      std::vector<uint32_t> &counterIndices,
                                      std::vector<VkDeviceAddress> &deviceAddresses);
    bool getPerRegionPerformanceCounterIndices(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                               std::vector<std::string> &counterNames,
                                               std::vector<uint32_t> &counterIndices);
    void buildHostBarriers(VkCommandBuffer cmdBuf);
    void submit(VkCommandBuffer commandBuffer);
    std::vector<BufferContainer> createPtpcBuffers();

    tcu::ResultCollector &m_resultCollector;

    VkPhysicalDevicePerformanceCountersByRegionPropertiesARM m_perRegionPerfCtrProperties;

    std::vector<CounterConfig> m_counters;

    const VkFormat m_format;
    const uint32_t m_width;
    const uint32_t m_height;
    const uint32_t m_layerCount;
    const uint32_t m_regionsX;
    const uint32_t m_regionsY;

    const Unique<VkDevice> m_device;
    const DeviceDriver m_deviceDriver;

    SimpleAllocator m_allocator;

    const Move<VkImage> m_image;
    const de::MovePtr<Allocation> m_imageMemory;
    const Move<VkImageView> m_imageView;
    const Move<VkBuffer> m_imageBuffer;
    const de::MovePtr<Allocation> m_imageBufferMemory;

    const Move<VkBuffer> m_ssboBuffer;
    const de::MovePtr<Allocation> m_ssboBufferMemory;

    const std::vector<BufferContainer> m_ptpcBuffers;

    const Unique<VkRenderPass> m_renderPass;
    const Unique<VkFramebuffer> m_framebuffer;

    const Unique<VkDescriptorSetLayout> m_descriptorSetLayout;
    const VkPushConstantRange m_pushConstantRange;
    PipelineLayoutWrapper m_pipelineLayout;
    GraphicsPipelineWrapper m_pipeline;
    const Unique<VkDescriptorPool> m_descriptorPool;
    const Unique<VkDescriptorSet> m_descriptorSet;

    const Unique<VkCommandPool> m_commandPool;
    const Unique<VkCommandBuffer> m_commandBuffer;
    const Unique<VkCommandBuffer> m_secCommandBuffer;
    const Unique<VkCommandBuffer> m_copyCommandBuffer;

    const uint32_t m_queueFamilyIndex;
    const VkQueue m_queue;
    const Unique<VkFence> m_fence;
};

PerformanceCountersByRegionContainer::PerformanceCountersByRegionContainer(Context &context, TestConfig config,
                                                                           tcu::ResultCollector &resultCollector,
                                                                           uint32_t width, uint32_t height,
                                                                           float queuePriority)
    : m_resultCollector(resultCollector)
    , m_perRegionPerfCtrProperties(getPtpcProperties(context.getInstanceInterface(), context.getPhysicalDevice()))
    , m_counters(config.counters)
    , m_format(config.format)
    , m_width(width)
    , m_height(height)
    , m_layerCount(config.layerCount)
    , m_regionsX(deDivRoundUp32(m_width, m_perRegionPerfCtrProperties.performanceCounterRegionSize.width))
    , m_regionsY(deDivRoundUp32(m_height, m_perRegionPerfCtrProperties.performanceCounterRegionSize.height))

    , m_device(createCustomDevice(context, config.groupParams, queuePriority))
    , m_deviceDriver(context.getPlatformInterface(), context.getInstance(), *m_device, context.getUsedApiVersion(),
                     context.getTestContext().getCommandLine())

    , m_allocator(m_deviceDriver, *m_device,
                  getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()))

    , m_image(createImage(context.getInstanceInterface(), context.getPhysicalDevice(), m_deviceDriver, *m_device,
                          m_format, m_width, m_height, m_layerCount))
    , m_imageMemory(createImageMemory(m_deviceDriver, *m_device, m_allocator, *m_image))
    , m_imageView(createImageView(m_deviceDriver, *m_device, *m_image, m_format, m_layerCount))
    , m_imageBuffer(createBuffer(m_deviceDriver, *m_device,
                                 getImageBufferSize(context.getInstanceInterface(), context.getPhysicalDevice(),
                                                    m_width, m_height, m_layerCount, m_format),
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT))
    , m_imageBufferMemory(
          createBufferMemory(m_deviceDriver, *m_device, m_allocator, *m_imageBuffer, MemoryRequirement::HostVisible))

    , m_ssboBuffer(createBuffer(m_deviceDriver, *m_device, m_width * m_height * sizeof(uint64_t),
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
    , m_ssboBufferMemory(
          createBufferMemory(m_deviceDriver, *m_device, m_allocator, *m_ssboBuffer, MemoryRequirement::HostVisible))

    , m_ptpcBuffers(createPtpcBuffers())

    , m_renderPass(createRenderPass(m_deviceDriver, *m_device, m_format, VK_SAMPLE_COUNT_1_BIT,
                                    config.groupParams->renderingType))
    , m_framebuffer(
          createFramebuffer(m_deviceDriver, *m_device, *m_renderPass, *m_imageView, m_width, m_height, m_layerCount))

    , m_descriptorSetLayout(createDescriptorSetLayout(m_deviceDriver, *m_device))

    , m_pushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0u, sizeof(pushConstData)}
    , m_pipelineLayout(config.groupParams->pipelineConstructionType, m_deviceDriver, *m_device, *m_descriptorSetLayout,
                       &m_pushConstantRange)
    , m_pipeline(context.getInstanceInterface(), m_deviceDriver, context.getPhysicalDevice(), *m_device,
                 context.getDeviceExtensions(), config.groupParams->pipelineConstructionType)

    , m_descriptorPool(createDescriptorPool(m_deviceDriver, *m_device))
    , m_descriptorSet(
          createDescriptorSet(m_deviceDriver, *m_device, *m_descriptorPool, *m_descriptorSetLayout, *m_ssboBuffer))
    , m_commandPool(createCommandPool(m_deviceDriver, *m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                      context.getUniversalQueueFamilyIndex()))
    , m_commandBuffer(allocateCommandBuffer(m_deviceDriver, *m_device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
    , m_secCommandBuffer(
          allocateCommandBuffer(m_deviceDriver, *m_device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY))
    , m_copyCommandBuffer(
          allocateCommandBuffer(m_deviceDriver, *m_device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY))

    , m_queueFamilyIndex(context.getUniversalQueueFamilyIndex())
    , m_queue(getDeviceQueue(m_deviceDriver, *m_device, m_queueFamilyIndex, 0u))
    , m_fence(createFence(m_deviceDriver, *m_device))
{
    createRenderPipeline(context.getBinaryCollection());

    const auto clearColor = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 0.0f).color;

    clearColorImage(m_deviceDriver, *m_device, m_queue, m_queueFamilyIndex, *m_image, clearColor,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

PerformanceCountersByRegionContainer::~PerformanceCountersByRegionContainer(void)
{
}

std::vector<BufferContainer> PerformanceCountersByRegionContainer::createPtpcBuffers()
{
    std::vector<BufferContainer> buffers(m_layerCount);
    for (uint32_t i = 0; i < m_layerCount; ++i)
    {
        buffers[i].m_ptpcBuffer =
            createBuffer(m_deviceDriver, *m_device, getPtpcBufferSize(m_perRegionPerfCtrProperties, m_width, m_height),
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR);
        buffers[i].m_ptpcBufferMemory =
            createBufferMemory(m_deviceDriver, *m_device, m_allocator, *buffers[i].m_ptpcBuffer,
                               MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
        buffers[i].m_ptpcBufferMemorDeviceAddress =
            getBufferDeviceAddress(m_deviceDriver, *m_device, *buffers[i].m_ptpcBuffer);
    }
    return buffers;
}

void PerformanceCountersByRegionContainer::createRenderPipeline(const BinaryCollection &binaryCollection)
{
    ShaderWrapper vertexShaderModule(m_deviceDriver, *m_device, binaryCollection.get("vert"), 0u);
    ShaderWrapper fragmentShaderModule(m_deviceDriver, *m_device, binaryCollection.get("frag"), 0u);
    ShaderWrapper geometryShaderModule(m_deviceDriver, *m_device, binaryCollection.get("geom"), 0u);

    // Disable blending
    const VkPipelineColorBlendAttachmentState attachmentBlendState = {
        VK_FALSE,                            // VkBool32                 blendEnable;
        VK_BLEND_FACTOR_SRC_ALPHA,           // VkBlendFactor            srcColorBlendFactor;
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // VkBlendFactor            dstColorBlendFactor;
        VK_BLEND_OP_ADD,                     // VkBlendOp                colorBlendOp;
        VK_BLEND_FACTOR_ONE,                 // VkBlendFactor            srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ONE,                 // VkBlendFactor            dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,                     // VkBlendOp                alphaBlendOp;
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT // VkColorComponentFlags    colorWriteMask;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineVertexInputStateCreateFlags)0u, // VkPipelineVertexInputStateCreateFlags       flags;

        0u,      // uint32_t                                    vertexBindingDescriptionCount;
        nullptr, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions;

        0u,     // uint32_t                                    vertexAttributeDescriptionCount;
        nullptr // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };
    const std::vector<VkViewport> viewports(1, makeViewport(tcu::UVec2(m_width, m_height)));
    const std::vector<VkRect2D> scissors(1, makeRect2D(tcu::UVec2(m_width, m_height)));

    const VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineMultisampleStateCreateFlags)0u, // VkPipelineMultisampleStateCreateFlags    flags;

        VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples;
        VK_FALSE,              // VkBool32                                 sampleShadingEnable;
        0.0f,                  // float                                    minSampleShading;
        nullptr,               // const VkSampleMask*                      pSampleMask;
        VK_FALSE,              // VkBool32                                 alphaToCoverageEnable;
        VK_FALSE,              // VkBool32                                 alphaToOneEnable;
    };

    const VkPipelineColorBlendStateCreateInfo blendState = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineColorBlendStateCreateFlags)0u, // VkPipelineColorBlendStateCreateFlags          flags;
        VK_FALSE,                                 // VkBool32                                      logicOpEnable;
        VK_LOGIC_OP_COPY,                         // VkLogicOp                                     logicOp;
        1u,                                       // uint32_t                                      attachmentCount;
        &attachmentBlendState,                    // const VkPipelineColorBlendAttachmentState*    pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f}                  // float                                         blendConstants[4];
    };

    PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;

    std::vector<VkFormat> colorAttachmentFormats(1, m_format);

    VkPipelineRenderingCreateInfo renderingCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        nullptr,
        0u,                                      // uint32_t           viewMask;
        (uint32_t)colorAttachmentFormats.size(), // uint32_t           colorAttachmentCount;
        colorAttachmentFormats.data(),           // const VkFormat*    pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                     // VkFormat           depthAttachmentFormat;
        VK_FORMAT_UNDEFINED                      // VkFormat           stencilAttachmentFormat;
    };

    if (*m_renderPass == VK_NULL_HANDLE)
        renderingCreateInfoWrapper.ptr = &renderingCreateInfo;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineDepthStencilStateCreateFlags)0u, // VkPipelineDepthStencilStateCreateFlags    flags;

        VK_FALSE,             // VkBool32                                  depthTestEnable;
        VK_FALSE,             // VkBool32                                  depthWriteEnable;
        VK_COMPARE_OP_ALWAYS, // VkCompareOp                               depthCompareOp;
        VK_FALSE,             // VkBool32                                  depthBoundsTestEnable;
        VK_FALSE,             // VkBool32                                  stencilTestEnable;
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0u, 0u,
         0u}, // VkStencilOpState                          front;
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0u, 0u,
         0u}, // VkStencilOpState                          back;

        0.0f, // float                                     minDepthBounds;
        1.0f  // float                                     maxDepthBounds;
    };

    m_pipeline.setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vertexShaderModule,
                                          0u, ShaderWrapper(), ShaderWrapper(),
                                          (m_layerCount == 1) ? ShaderWrapper() : geometryShaderModule, nullptr,
                                          nullptr, renderingCreateInfoWrapper)
        .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fragmentShaderModule, &depthStencilState,
                                  &multisampleState)
        .setupFragmentOutputState(*m_renderPass, 0u, &blendState, &multisampleState)
        .setMonolithicPipelineLayout(m_pipelineLayout)
        .buildPipeline();
}

void PerformanceCountersByRegionContainer::build(Context &context, const SharedGroupParams &groupParams)
{
    switch (groupParams->renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        return buildInternal<RenderpassSubpass1>(context);
    case RENDERING_TYPE_RENDERPASS2:
        return buildInternal<RenderpassSubpass2>(context);
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        return buildInternalDynamicRendering(context, groupParams);
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

bool PerformanceCountersByRegionContainer::getPerRegionPerformanceCounterIndices(const InstanceInterface &vki,
                                                                                 VkPhysicalDevice physicalDevice,
                                                                                 std::vector<std::string> &counterNames,
                                                                                 std::vector<uint32_t> &counterIndices)
{
    uint32_t perfCounterCount = 0;

    // Get the count of counters supported
    vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(physicalDevice, m_queueFamilyIndex,
                                                                         &perfCounterCount, nullptr, nullptr);

    if (perfCounterCount == 0)
    {
        m_resultCollector.fail("No counters found");
        return false;
    }

    std::vector<VkPerformanceCounterARM> perfCounters(perfCounterCount);
    std::vector<VkPerformanceCounterDescriptionARM> perfCounterDescs(perfCounterCount);

    for (uint32_t idx = 0; idx < perfCounterCount; idx++)
    {
        perfCounters[idx].sType     = VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_KHR;
        perfCounterDescs[idx].sType = VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_KHR;
    }

    // Get the counters supported
    uint32_t originalPerfCounterCount = perfCounterCount;
    vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
        physicalDevice, m_queueFamilyIndex, &perfCounterCount, perfCounters.data(), perfCounterDescs.data());

    if (originalPerfCounterCount != perfCounterCount)
    {
        m_resultCollector.fail("Counter count not as expected");
        return false;
    }

    // Search list for matches to requested counters
    for (const auto &ctrName : counterNames)
    {
        bool found = false;
        for (uint32_t i = 0; i < perfCounterCount; i++)
        {
            if (strcmp(ctrName.c_str(), perfCounterDescs[i].name) == 0)
            {
                found = true;
                counterIndices.push_back(perfCounters[i].counterID);
                break;
            }
        }

        if (!found)
        {
            counterIndices.push_back(perfCounters[0].counterID);
            m_resultCollector.fail("Counter " + ctrName + " not found");
            return false;
        }
    }

    return true;
}

void PerformanceCountersByRegionContainer::initPtpcBeginRenderingStruct(
    Context &context, VkRenderPassPerformanceCountersByRegionBeginInfoARM &perRegionPerfCtr,
    std::vector<uint32_t> &counterIndices, std::vector<VkDeviceAddress> &deviceAddresses)
{
    std::vector<std::string> counterNames;

    for (uint32_t i = 0; i < m_counters.size(); ++i)
    {
        counterNames.push_back(m_counters[i].name);
    }

    for (uint32_t i = 0; i < m_layerCount; ++i)
    {
        deviceAddresses.push_back(m_ptpcBuffers[i].m_ptpcBufferMemorDeviceAddress);
    }

    if (!getPerRegionPerformanceCounterIndices(context.getInstanceInterface(), context.getPhysicalDevice(),
                                               counterNames, counterIndices))
    {
        m_resultCollector.fail("Failed to set up counters");
    }

    perRegionPerfCtr.sType               = VK_STRUCTURE_TYPE_RENDER_PASS_PERFORMANCE_COUNTERS_BY_REGION_BEGIN_INFO_ARM;
    perRegionPerfCtr.pNext               = nullptr;
    perRegionPerfCtr.counterAddressCount = static_cast<int>(deviceAddresses.size());
    perRegionPerfCtr.pCounterAddresses   = deviceAddresses.data();
    perRegionPerfCtr.serializeRegions    = true;
    perRegionPerfCtr.counterIndexCount   = static_cast<int>(counterIndices.size());
    perRegionPerfCtr.pCounterIndices     = counterIndices.data();
}

void PerformanceCountersByRegionContainer::buildHostBarriers(VkCommandBuffer cmdBuf)
{
    const VkBufferMemoryBarrier bufferBarrier[2] = {
        {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType;
            nullptr,                                 // const void*        pNext;
            VK_ACCESS_SHADER_WRITE_BIT,              // VkAccessFlags      srcAccessMask;
            VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags      dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           dstQueueFamilyIndex;
            *m_ptpcBuffers[0].m_ptpcBuffer,          // VkBuffer           buffer;
            0,                                       // VkDeviceSize       offset;
            VK_WHOLE_SIZE,                           // VkDeviceSize       size;
        },
        {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType;
            nullptr,                                 // const void*        pNext;
            VK_ACCESS_SHADER_WRITE_BIT,              // VkAccessFlags      srcAccessMask;
            VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags      dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           dstQueueFamilyIndex;
            *m_ssboBuffer,                           // VkBuffer           buffer;
            0,                                       // VkDeviceSize       offset;
            VK_WHOLE_SIZE,                           // VkDeviceSize       size;
        }};

    m_deviceDriver.cmdPipelineBarrier(cmdBuf, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                                      (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)nullptr, 2, bufferBarrier, 0,
                                      (const VkImageMemoryBarrier *)nullptr);
}

void PerformanceCountersByRegionContainer::buildDrawCommands(VkCommandBuffer commandBuffer)
{
    m_deviceDriver.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipeline());

    for (uint32_t layerIdx = 0; layerIdx < m_layerCount; ++layerIdx)
    {
        pushConstData pushConsts{(float)m_width, (float)m_height, (int)layerIdx};
        m_deviceDriver.cmdPushConstants(commandBuffer, *m_pipelineLayout,
                                        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0u,
                                        sizeof(pushConstData), &pushConsts);

        m_deviceDriver.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u,
                                             &*m_descriptorSet, 0u, nullptr);
        m_deviceDriver.cmdDraw(commandBuffer, 3u * (layerIdx + 1), 1u, 0u, 0u);
    }
}

template <typename RenderpassSubpass>
void PerformanceCountersByRegionContainer::buildInternal(Context &context)
{
    const DeviceInterface &vkd(m_deviceDriver);
    const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(nullptr, VK_SUBPASS_CONTENTS_INLINE);
    const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(nullptr);

    beginCommandBuffer(vkd, *m_commandBuffer);

    {
        VkRenderPassPerformanceCountersByRegionBeginInfoARM perRegionPerfCtr = {};
        std::vector<uint32_t> counterIndices;
        std::vector<VkDeviceAddress> deviceAddresses;
        initPtpcBeginRenderingStruct(context, perRegionPerfCtr, counterIndices, deviceAddresses);

        const VkRenderPassBeginInfo beginInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType        sType;
            &perRegionPerfCtr,                        // const void*            pNext;

            *m_renderPass,  // VkRenderPass           renderPass;
            *m_framebuffer, // VkFramebuffer          framebuffer;

            {// VkRect2D               renderArea;
             {0u, 0u},
             {m_width, m_height}},

            0u,     // uint32_t               clearValueCount;
            nullptr // const VkClearValue*    pClearValues;
        };
        RenderpassSubpass::cmdBeginRenderPass(vkd, *m_commandBuffer, &beginInfo, &subpassBeginInfo);
    }

    buildDrawCommands(*m_commandBuffer);

    RenderpassSubpass::cmdEndRenderPass(vkd, *m_commandBuffer, &subpassEndInfo);

    // Insert a barrier so data written by the shader is available to the host
    buildHostBarriers(*m_commandBuffer);

    endCommandBuffer(vkd, *m_commandBuffer);
}

void PerformanceCountersByRegionContainer::buildInternalDynamicRendering(Context &context,
                                                                         const SharedGroupParams &groupParams)
{
    const DeviceInterface &vkd(m_deviceDriver);

    beginCommandBuffer(vkd, *m_commandBuffer);

    VkRenderPassPerformanceCountersByRegionBeginInfoARM perRegionPerfCtr = {};
    std::vector<uint32_t> counterIndices;
    std::vector<VkDeviceAddress> deviceAddresses;
    initPtpcBeginRenderingStruct(context, perRegionPerfCtr, counterIndices, deviceAddresses);

    const VkClearValue clearValue(makeClearValueColor(tcu::Vec4(0.0f)));

    VkRenderingAttachmentInfo attachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        *m_imageView,                             // VkImageView imageView;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                     // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                           // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout resolveImageLayout;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        clearValue                                // VkClearValue clearValue;
    };

    const uint32_t colorAttachmentCount = 1;
    const std::vector<VkRenderingAttachmentInfo> colorAttachments(colorAttachmentCount, attachment);

    VkRenderingFlagsKHR renderingFlags = {};
    if (groupParams->useSecondaryCmdBuffer && !groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
    {
        renderingFlags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
    }

    const VkRenderingInfo renderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        &perRegionPerfCtr,
        renderingFlags,                    // VkRenderingFlagsKHR flags;
        makeRect2D(m_width, m_height),     // VkRect2D renderArea;
        m_layerCount,                      // uint32_t layerCount;
        0u,                                // uint32_t viewMask;
        (uint32_t)colorAttachments.size(), // uint32_t colorAttachmentCount;
        colorAttachments.data(),           // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                           // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                           // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    if (groupParams->useSecondaryCmdBuffer)
    {
        VkCommandBufferUsageFlags usageFlags(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        if (!groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        {
            usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        }

        std::vector<VkFormat> colorAttachmentFormats(colorAttachmentCount, m_format);

        const VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
            &perRegionPerfCtr,                                               // const void* pNext;
            0u,                                                              // VkRenderingFlagsKHR flags;
            0u,                                                              // uint32_t viewMask;
            colorAttachmentCount,                                            // uint32_t colorAttachmentCount;
            colorAttachmentFormats.data(),                                   // const VkFormat* pColorAttachmentFormats;
            VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
            VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
            VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits rasterizationSamples;
        };
        const VkCommandBufferInheritanceInfo bufferInheritanceInfo{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
            groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass ?
                nullptr :
                &inheritanceRenderingInfo,    // const void* pNext;
            VK_NULL_HANDLE,                   // VkRenderPass renderPass;
            0u,                               // uint32_t subpass;
            VK_NULL_HANDLE,                   // VkFramebuffer framebuffer;
            VK_FALSE,                         // VkBool32 occlusionQueryEnable;
            (VkQueryControlFlags)0u,          // VkQueryControlFlags queryFlags;
            (VkQueryPipelineStatisticFlags)0u // VkQueryPipelineStatisticFlags pipelineStatistics;
        };
        const VkCommandBufferBeginInfo commandBufBeginParams{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            usageFlags,                                  // VkCommandBufferUsageFlags flags;
            &bufferInheritanceInfo                       // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
        };
        VK_CHECK(vkd.beginCommandBuffer(*m_secCommandBuffer, &commandBufBeginParams));

        if (groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        {
            vkd.cmdBeginRendering(*m_secCommandBuffer, &renderingInfo);

            buildDrawCommands(*m_secCommandBuffer);

            vkd.cmdEndRendering(*m_secCommandBuffer);
            VK_CHECK(vkd.endCommandBuffer(*m_secCommandBuffer));

            vkd.cmdExecuteCommands(*m_commandBuffer, 1, &*m_secCommandBuffer);
        }
        else
        {
            vkd.cmdBeginRendering(*m_commandBuffer, &renderingInfo);

            buildDrawCommands(*m_secCommandBuffer);

            VK_CHECK(vkd.endCommandBuffer(*m_secCommandBuffer));

            vkd.cmdExecuteCommands(*m_commandBuffer, 1, &*m_secCommandBuffer);

            vkd.cmdEndRendering(*m_commandBuffer);
        }
    }
    else
    {
        vkd.cmdBeginRendering(*m_commandBuffer, &renderingInfo);

        buildDrawCommands(*m_commandBuffer);

        vkd.cmdEndRendering(*m_commandBuffer);
    }

    // Insert barriers to make data written by the shader visible on the host
    buildHostBarriers(*m_commandBuffer);

    endCommandBuffer(vkd, *m_commandBuffer);
}

void PerformanceCountersByRegionContainer::copyImageResults()
{
    // Reset the fence
    VK_CHECK(m_deviceDriver.resetFences(*m_device, 1, &*m_fence));

    beginCommandBuffer(m_deviceDriver, *m_copyCommandBuffer);

    // Build copy output image to results buffer
    {
        const VkImageSubresourceRange colorSubresRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_layerCount};
        const VkImageMemoryBarrier preCopyBarrier      = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                          nullptr,
                                                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                          VK_ACCESS_TRANSFER_READ_BIT,
                                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                          0,
                                                          0,
                                                          *m_image,
                                                          colorSubresRange};
        const VkBufferImageCopy region                 = {
            0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, m_layerCount}, {0, 0, 0}, {m_width, m_height, 1}};
        m_deviceDriver.cmdPipelineBarrier(*m_copyCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u,
                                          &preCopyBarrier);
        m_deviceDriver.cmdCopyImageToBuffer(*m_copyCommandBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                            *m_imageBuffer, 1u, &region);
    }

    // Build transfer to host barrier for the results buffer.

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType;
        nullptr,                                 // const void*        pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags      srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags      dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           dstQueueFamilyIndex;
        *m_imageBuffer,                          // VkBuffer           buffer;
        0,                                       // VkDeviceSize       offset;
        VK_WHOLE_SIZE,                           // VkDeviceSize       size;
    };

    m_deviceDriver.cmdPipelineBarrier(
        *m_copyCommandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
        0, (const VkMemoryBarrier *)nullptr, 1, &bufferBarrier, 0, (const VkImageMemoryBarrier *)nullptr);

    endCommandBuffer(m_deviceDriver, *m_copyCommandBuffer);

    submit(*m_copyCommandBuffer);
    wait();
}

void PerformanceCountersByRegionContainer::submit(VkCommandBuffer commandBuffer)
{
    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        0,                             // uint32_t waitSemaphoreCount;
        nullptr,                       // const VkSemaphore* pWaitSemaphores;
        0,                             // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &commandBuffer,                // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr,                       // const VkSemaphore* pSignalSemaphores;
    };

    VK_CHECK(m_deviceDriver.queueSubmit(m_queue, 1u, &submitInfo, *m_fence));
}

void PerformanceCountersByRegionContainer::wait() const
{
    VK_CHECK(m_deviceDriver.waitForFences(*m_device, 1u, &m_fence.get(), VK_TRUE, ~0ull));
}

void PerformanceCountersByRegionContainer::validateCounters()
{
    for (uint32_t layerIdx = 0; layerIdx < m_layerCount; ++layerIdx)
    {
        invalidateMappedMemoryRange(m_deviceDriver, *m_device, m_ptpcBuffers[layerIdx].m_ptpcBufferMemory->getMemory(),
                                    m_ptpcBuffers[layerIdx].m_ptpcBufferMemory->getOffset(), VK_WHOLE_SIZE);
        const unsigned char *u8Data =
            reinterpret_cast<const unsigned char *>(m_ptpcBuffers[layerIdx].m_ptpcBufferMemory->getHostPtr());
        const uint32_t maxCounters = m_perRegionPerfCtrProperties.maxPerRegionPerformanceCounters;
        const uint32_t regionSize  = deRoundUp32u(static_cast<uint32_t>(sizeof(uint32_t)) * maxCounters,
                                                  m_perRegionPerfCtrProperties.regionAlignment);
        const uint32_t rowStride =
            deRoundUp32u(regionSize * m_regionsX, m_perRegionPerfCtrProperties.rowStrideAlignment);
        bool pass = true;

        for (uint32_t y = 0; y < m_regionsY; ++y)
        {
            const bool completeRegionY =
                (y < m_regionsY - 1) ||
                (m_height % m_perRegionPerfCtrProperties.performanceCounterRegionSize.height == 0);
            const unsigned char *u8RowData = &u8Data[rowStride * y];
            for (uint32_t x = 0; x < m_regionsX; ++x)
            {
                const unsigned char *u8RegionData = &u8RowData[regionSize * x];
                const uint32_t *u32RegionData     = reinterpret_cast<const uint32_t *>(u8RegionData);
                for (uint32_t counterIdx = 0; counterIdx < m_counters.size(); ++counterIdx)
                {
                    bool completeRegionX =
                        (y < m_regionsY - 1) ||
                        (m_height % m_perRegionPerfCtrProperties.performanceCounterRegionSize.width == 0);
                    uint32_t minExpected = m_counters[counterIdx].regionMin;
                    uint32_t maxExpected = m_counters[counterIdx].regionMax;
                    if (completeRegionX && completeRegionY)
                    {
                        minExpected += m_counters[counterIdx].fragment * (layerIdx + 1);
                        maxExpected += m_counters[counterIdx].fragment * (layerIdx + 1);
                    }
                    else
                    {
                        minExpected = std::max(minExpected, 1u);
                        maxExpected += m_counters[counterIdx].fragment * (layerIdx + 1);
                    }
                    pass &= (u32RegionData[counterIdx] >= minExpected && u32RegionData[counterIdx] <= maxExpected);
                }
            }
        }
        if (!pass)
        {
            m_resultCollector.fail("Region results in layer " + std::to_string(layerIdx) + " not as expected for " +
                                   m_counters[0].name);
        }
    }
}

void PerformanceCountersByRegionContainer::gatherPerRegionTimestamps(std::vector<regionTimeStamps> &ts) const
{
    invalidateMappedMemoryRange(m_deviceDriver, *m_device, m_ssboBufferMemory->getMemory(),
                                m_ssboBufferMemory->getOffset(), VK_WHOLE_SIZE);

    uint64_t *u64Data = reinterpret_cast<uint64_t *>(m_ssboBufferMemory->getHostPtr());

    for (uint32_t y = 0; y < m_regionsY; ++y)
    {
        for (uint32_t x = 0; x < m_regionsX; ++x)
        {
            ts[x + y * m_regionsX].start = UINT64_MAX;
            ts[x + y * m_regionsX].end   = 0;
        }
    }

    for (uint32_t y = 0; y < m_height; ++y)
    {
        for (uint32_t x = 0; x < m_width; ++x)
        {
            const uint32_t regionX = x / m_perRegionPerfCtrProperties.performanceCounterRegionSize.width;
            const uint32_t regionY = y / m_perRegionPerfCtrProperties.performanceCounterRegionSize.height;

            ts[regionX + regionY * m_regionsX].start =
                std::min(ts[regionX + regionY * m_regionsX].start, u64Data[x + y * m_width]);
            ts[regionX + regionY * m_regionsX].end =
                std::max(ts[regionX + regionY * m_regionsX].end, u64Data[x + y * m_width]);
        }
    }
}

uint32_t PerformanceCountersByRegionContainer::getNumRegions() const
{
    return m_regionsX * m_regionsY;
}

void PerformanceCountersByRegionContainer::validateAttachment() const
{
    const void *pData = m_imageBufferMemory->getHostPtr();
    invalidateAlloc(m_deviceDriver, *m_device, *m_imageBufferMemory);

    const tcu::ConstPixelBufferAccess image(
        tcu::ConstPixelBufferAccess(mapVkFormat(m_format), m_width, m_height, m_layerCount, pData));

    for (uint32_t layerIdx = 0; layerIdx < m_layerCount; ++layerIdx)
    {
        const tcu::ConstPixelBufferAccess access = tcu::getSubregion(image, 0, 0, layerIdx, m_width, m_height, 1);

        bool pass = true;
        for (int y = 0; y < access.getHeight(); y += 1)
        {
            for (int x = 0; x < access.getWidth(); x += 1)
            {
                const tcu::Vec4 p = access.getPixel(x, y);
                const tcu::Vec4 ref(0.0f, 0.0f, 1.0f, 1.0f);

                for (int c = 0; c < 4; c++)
                    if (fabs(p[c] - ref[c]) > 0.01f)
                        pass = false;
            }
        }

        if (!pass)
        {
            m_resultCollector.fail("Pixel differences found.");
        }
    }
}

struct Programs
{
    void init(vk::SourceCollections &dst, TestConfig config) const
    {
        std::ostringstream vertexShader;

        vertexShader << "#version 450\n"
                        "highp float;\n"
                        "void main (void) {\n"
                        "\tif (gl_VertexIndex % 3 == 0) gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
                        "\tif (gl_VertexIndex % 3 == 1) gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
                        "\tif (gl_VertexIndex % 3 == 2) gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
                        "}\n";

        dst.glslSources.add("vert") << glu::VertexSource(vertexShader.str());

        std::ostringstream fragmentShader;

        fragmentShader << "#version 450\n"
                          "#extension GL_EXT_shader_realtime_clock : require\n"
                          "#extension GL_ARB_gpu_shader_int64 : require\n"
                          "precision highp float;\n"
                          "layout(set=0, binding=0, std430) buffer SSBO\n"
                          "{\n"
                          "\tuint64_t time_stamps[];\n"
                          "} ssbo;\n"
                          "layout(push_constant, std140) uniform PC\n"
                          "{\n"
                          "\tfloat width;\n"
                          "\tfloat height;\n"
                          "\tuint layer;\n"
                          "} pc;\n"
                          "layout(location = 0) out vec4 out_color;\n"
                          "void main()\n"
                          "{\n"
                          "\tint time_stamp_idx = int(gl_FragCoord.x) + int(gl_FragCoord.y) * int(pc.width);\n"
                          "\tssbo.time_stamps[time_stamp_idx] = clockRealtimeEXT();\n"
                          "\tout_color = vec4(0,0,1,1);\n"
                          "}\n";

        dst.glslSources.add("frag") << glu::FragmentSource(fragmentShader.str());

        if (config.layerCount > 1)
        {
            std::ostringstream geometryShader;
            geometryShader << "#version 450\n"
                              "layout (triangles) in;\n"
                              "layout (triangle_strip, max_vertices = 3) out;\n"
                              "layout(push_constant, std140) uniform PC\n"
                              "{\n"
                              "\tfloat width;\n"
                              "\tfloat height;\n"
                              "\tint  layer;\n"
                              "} pc;\n"
                              "void main()\n"
                              "{\n"
                              "\tgl_Layer = pc.layer;\n"
                              "\tgl_Position = gl_in[0].gl_Position;\n"
                              "\tEmitVertex();\n"
                              "\tgl_Position = gl_in[1].gl_Position;\n"
                              "\tEmitVertex();\n"
                              "\tgl_Position = gl_in[2].gl_Position;\n"
                              "\tEmitVertex();\n"
                              "\tEndPrimitive();\n"
                              "}\n";
            dst.glslSources.add("geom") << glu::GeometrySource(geometryShader.str());
        }
    }
};

class PerformanceCountersByRegionRenderPassTestInstance : public TestInstance
{
public:
    PerformanceCountersByRegionRenderPassTestInstance(Context &context, TestConfig config);
    ~PerformanceCountersByRegionRenderPassTestInstance(void);

    tcu::TestStatus iterate(void);

    PerformanceCountersByRegionContainer m_container1;
    PerformanceCountersByRegionContainer m_container2;
    PerformanceCountersByRegionContainer m_container3;
    tcu::ResultCollector m_resultCollector;
    const SharedGroupParams m_groupParams;
};

PerformanceCountersByRegionRenderPassTestInstance::PerformanceCountersByRegionRenderPassTestInstance(Context &context,
                                                                                                     TestConfig config)
    : TestInstance(context)
    , m_container1(context, config, m_resultCollector, 4096, 4096, 0.5f)
    , m_container2(context, config, m_resultCollector, 64, 64, 1.0f)
    , m_container3(context, config, m_resultCollector, 256, 256, 1.0f)
    , m_groupParams(config.groupParams)
{
}

PerformanceCountersByRegionRenderPassTestInstance::~PerformanceCountersByRegionRenderPassTestInstance()
{
}

void validate_container_timestamps(tcu::ResultCollector &resultCollector, std::vector<regionTimeStamps> &ts1,
                                   std::vector<regionTimeStamps> &ts2)
{
    uint64_t start1 = UINT64_MAX;
    uint64_t end1   = 0;
    uint64_t start2 = UINT64_MAX;
    uint64_t end2   = 0;

    for (uint32_t i = 0; i < ts2.size(); ++i)
    {
        start1 = std::min(start1, ts1[i].start);
        end1   = std::max(end1, ts1[i].end);
    }

    for (uint32_t k = 0; k < ts2.size(); ++k)
    {
        start2 = std::min(start2, ts2[k].start);
        end2   = std::max(end2, ts2[k].end);
    }

    if (!(start1 >= end2 || end1 <= start2))
    {
        for (uint32_t i = 0; i < ts1.size(); ++i)
        {
            for (uint32_t k = 0; k < ts2.size(); ++k)
            {
                if (!(ts1[i].start >= ts2[k].end || ts1[i].end <= ts2[k].start))
                {
                    resultCollector.fail("Region regionTimeStamps on different logical devices overlapped.");
                }
            }
        }
    }
}

tcu::TestStatus PerformanceCountersByRegionRenderPassTestInstance::iterate(void)
{
    m_container1.build(m_context, m_groupParams); // Large workload, low priority, with counters
    m_container2.build(m_context, m_groupParams); // Small workload, high priority
    m_container3.build(m_context, m_groupParams); // Small workload, high priority

    // Submit the command buffers to encourage an overlap between the large workload and small workloads.

    // submit the large workload with the first small workload
    m_container1.submitRendering();
    m_container2.submitRendering();

    m_container2.wait();

    // submit the second small workload as soon as the first small workload is finished
    m_container3.submitRendering();

    m_container1.wait();
    m_container3.wait();

    // Check the counter values are as expected and not affected by the high priority submissions.
    m_container1.validateCounters();

    std::vector<regionTimeStamps> ts1(m_container1.getNumRegions());
    m_container1.gatherPerRegionTimestamps(ts1);

    std::vector<regionTimeStamps> ts2(m_container2.getNumRegions());
    m_container2.gatherPerRegionTimestamps(ts2);

    // Check that no regions on different devices overlapped.
    validate_container_timestamps(m_resultCollector, ts1, ts2);

    std::vector<regionTimeStamps> ts3(m_container3.getNumRegions());
    m_container3.gatherPerRegionTimestamps(ts3);

    // Check that no regions on different devices overlapped.
    validate_container_timestamps(m_resultCollector, ts1, ts3);

    // Copy image results for validation
    m_container1.copyImageResults();

    // Check the color attachment contents.
    m_container1.validateAttachment();

    return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

std::string formatToName(VkFormat format)
{
    const std::string formatStr = de::toString(format);
    const std::string prefix    = "VK_FORMAT_";

    DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

    return de::toLower(formatStr.substr(prefix.length()));
}

void checkSupport(Context &context, TestConfig config)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    checkPipelineConstructionRequirements(vki, physicalDevice, config.groupParams->pipelineConstructionType);

    context.requireDeviceFunctionality("VK_KHR_buffer_device_address");
    context.requireDeviceFunctionality("VK_EXT_separate_stencil_usage");
    context.requireDeviceFunctionality("VK_ARM_performance_counters_by_region");
    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

    if (config.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

    if (config.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    if (isConstructionTypeLibrary(config.groupParams->pipelineConstructionType))
    {
        context.requireDeviceFunctionality("VK_KHR_pipeline_library");
        context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

        if (!context.getGraphicsPipelineLibraryFeaturesEXT().graphicsPipelineLibrary)
            TCU_THROW(NotSupportedError, "graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary required");
    }

    context.requireDeviceFunctionality("VK_KHR_shader_clock");
    const auto &shaderClockFeatures = context.getShaderClockFeatures();
    if (!shaderClockFeatures.shaderDeviceClock)
        TCU_THROW(NotSupportedError, "Shader device clock is not supported");

    VkPhysicalDeviceFeatures2 features2                                                = initVulkanStructure();
    VkPhysicalDevicePerformanceCountersByRegionFeaturesARM performanceCountersByRegion = initVulkanStructure();
    const auto addFeatures = makeStructChainAdder(&features2);

    addFeatures(&performanceCountersByRegion);

    vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

    if (!performanceCountersByRegion.performanceCountersByRegion)
        TCU_THROW(NotSupportedError, "VkPhysicalDevicePerformanceCountersByRegionFeaturesARM is not supported");
}

void initTests(tcu::TestCaseGroup *group, const SharedGroupParams groupParams)
{
    static const VkFormat formats[] = {
        VK_FORMAT_R8G8B8A8_UNORM,
    };
    tcu::TestContext &testCtx(group->getTestContext());

    for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
    {
        const VkFormat format(formats[formatNdx]);
        const std::string formatName(formatToName(format));
        de::MovePtr<tcu::TestCaseGroup> formatGroup(
            new tcu::TestCaseGroup(testCtx, formatName.c_str(), formatName.c_str()));
        de::MovePtr<tcu::TestCaseGroup> extFormatGroup(
            new tcu::TestCaseGroup(testCtx, formatName.c_str(), formatName.c_str()));
        const std::vector<CounterConfig> counters(1, {"Fragment warps", 0, 0, 256});

        for (int i = 1; i <= 2; ++i)
        {
            const TestConfig testConfig(format, groupParams, counters, i);
            const std::string testName("layers_" + std::to_string(i));

            formatGroup->addChild(new InstanceFactory1WithSupport<PerformanceCountersByRegionRenderPassTestInstance,
                                                                  TestConfig, FunctionSupport1<TestConfig>, Programs>(
                testCtx, testName.c_str(), testConfig,
                typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfig)));
        }

        group->addChild(formatGroup.release());
    }
}

} // namespace

tcu::TestCaseGroup *createRenderPassPerformanceCountersByRegionTests(tcu::TestContext &testCtx,
                                                                     const SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "performance_counters_by_region", initTests, groupParams);
}

} // namespace vkt

#endif
