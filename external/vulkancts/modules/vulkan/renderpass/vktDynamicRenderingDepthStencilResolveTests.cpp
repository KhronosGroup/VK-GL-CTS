/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Vulkan Dynamic Rendering Depth Stencil Resolve Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicRenderingDepthStencilResolveTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deMath.h"

#include <limits>
#include <map>

using namespace vk;

using tcu::TestLog;
using tcu::Vec4;

typedef de::SharedPtr<vk::Unique<VkImage>> VkImageSp;
typedef de::SharedPtr<vk::Unique<VkImageView>> VkImageViewSp;
typedef de::SharedPtr<vk::Unique<VkBuffer>> VkBufferSp;
typedef de::SharedPtr<vk::Unique<VkPipeline>> VkPipelineSp;
typedef de::SharedPtr<Allocation> AllocationSp;

namespace vkt
{
namespace renderpass
{
namespace
{

template <typename T>
de::SharedPtr<T> safeSharedPtr(T *ptr)
{
    try
    {
        return de::SharedPtr<T>(ptr);
    }
    catch (...)
    {
        delete ptr;
        throw;
    }
}

VkImageAspectFlags aspectFlagsForFormat(VkFormat vkformat)
{
    const tcu::TextureFormat format(mapVkFormat(vkformat));
    VkImageAspectFlags aspectFlags =
        ((tcu::hasDepthComponent(format.order) ? static_cast<vk::VkImageAspectFlags>(vk::VK_IMAGE_ASPECT_DEPTH_BIT) :
                                                 0u) |
         (tcu::hasStencilComponent(format.order) ?
              static_cast<vk::VkImageAspectFlags>(vk::VK_IMAGE_ASPECT_STENCIL_BIT) :
              0u));
    return aspectFlags;
}

enum VerifyBuffer
{
    VB_DEPTH = 0,
    VB_STENCIL
};

struct TestConfig
{
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t imageLayers;
    uint32_t viewLayers;
    uint32_t resolveBaseLayer;
    VkRect2D renderArea;
    VkImageAspectFlags aspectFlag;
    uint32_t sampleCount;
    VkResolveModeFlagBits depthResolveMode;
    VkResolveModeFlagBits stencilResolveMode;
    VerifyBuffer verifyBuffer;
    VkClearDepthStencilValue clearValue;
    float depthExpectedValue;
    uint8_t stencilExpectedValue;
    bool separateDepthStencilLayouts;
    const SharedGroupParams groupParams;
};

float get16bitDepthComponent(uint8_t *pixelPtr)
{
    uint16_t *value = reinterpret_cast<uint16_t *>(pixelPtr);
    return static_cast<float>(*value) / 65535.0f;
}

float get24bitDepthComponent(uint8_t *pixelPtr)
{
    const bool littleEndian = (DE_ENDIANNESS == DE_LITTLE_ENDIAN);
    uint32_t value          = (((uint32_t)pixelPtr[0]) << (!littleEndian * 16u)) | (((uint32_t)pixelPtr[1]) << 8u) |
                     (((uint32_t)pixelPtr[2]) << (littleEndian * 16u));
    return static_cast<float>(value) / 16777215.0f;
}

float get32bitDepthComponent(uint8_t *pixelPtr)
{
    return *(reinterpret_cast<float *>(pixelPtr));
}

class DepthStencilResolveTest : public TestInstance
{
public:
    DepthStencilResolveTest(Context &context, TestConfig config);
    virtual ~DepthStencilResolveTest(void);

    virtual tcu::TestStatus iterate(void);

protected:
    bool isFeaturesSupported(void);
    VkSampleCountFlagBits sampleCountBitFromSampleCount(uint32_t count) const;

    VkImageSp createImage(VkFormat vkformat, uint32_t sampleCount, VkImageUsageFlags additionalUsage = 0u);
    AllocationSp createImageMemory(VkImageSp image);
    VkImageViewSp createImageView(VkImageSp image, VkFormat vkformat, uint32_t baseArrayLayer);
    AllocationSp createBufferMemory(void);
    VkBufferSp createBuffer(void);

    Move<VkPipelineLayout> createRenderPipelineLayout(void);
    Move<VkPipeline> createRenderPipeline(VkFormat vkformat, VkPipelineLayout renderPipelineLayout);

#ifndef CTS_USES_VULKANSC
    void beginSecondaryCommandBuffer(VkCommandBuffer cmdBuffer, VerifyBuffer attachmentType,
                                     VkRenderingFlagsKHR renderingFlags = 0) const;
#endif // CTS_USES_VULKANSC

    void submit(void);
    bool verifyDepth(void);
    bool verifyStencil(void);

protected:
    const TestConfig m_config;
    const bool m_featureSupported;

    const InstanceInterface &m_vki;
    const DeviceInterface &m_vkd;
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;

    const Unique<VkCommandPool> m_commandPool;

    VkImageSp m_multisampleImage;
    AllocationSp m_multisampleImageMemory;
    VkImageViewSp m_multisampleImageView;
    VkImageSp m_singlesampleImage;
    AllocationSp m_singlesampleImageMemory;
    VkImageViewSp m_singlesampleImageView;
    VkBufferSp m_buffer;
    AllocationSp m_bufferMemory;

    uint32_t m_numRenderPasses;
    Unique<VkPipelineLayout> m_renderPipelineLayout;
    Unique<VkPipeline> m_renderPipeline;
};

DepthStencilResolveTest::DepthStencilResolveTest(Context &context, TestConfig config)
    : TestInstance(context)
    , m_config(config)
    , m_featureSupported(isFeaturesSupported())
    , m_vki(context.getInstanceInterface())
    , m_vkd(context.getDeviceInterface())
    , m_device(context.getDevice())
    , m_physicalDevice(context.getPhysicalDevice())

    , m_commandPool(createCommandPool(context.getDeviceInterface(), context.getDevice(),
                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))

    , m_multisampleImage(createImage(m_config.format, m_config.sampleCount, VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    , m_multisampleImageMemory(createImageMemory(m_multisampleImage))
    , m_multisampleImageView(createImageView(m_multisampleImage, m_config.format, 0u))

    , m_singlesampleImage(
          createImage(m_config.format, 1, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    , m_singlesampleImageMemory(createImageMemory(m_singlesampleImage))
    , m_singlesampleImageView(createImageView(m_singlesampleImage, m_config.format, m_config.resolveBaseLayer))

    , m_buffer(createBuffer())
    , m_bufferMemory(createBufferMemory())

    , m_numRenderPasses(m_config.verifyBuffer == VB_DEPTH ? 1u : m_config.sampleCount)
    , m_renderPipelineLayout(createRenderPipelineLayout())
    , m_renderPipeline(createRenderPipeline(m_config.format, *m_renderPipelineLayout))
{
}

DepthStencilResolveTest::~DepthStencilResolveTest(void)
{
}

bool DepthStencilResolveTest::isFeaturesSupported()
{
    m_context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
    m_context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");
    if (m_config.imageLayers > 1)
        m_context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_config.separateDepthStencilLayouts)
        m_context.requireDeviceFunctionality("VK_KHR_separate_depth_stencil_layouts");

    VkPhysicalDeviceDepthStencilResolveProperties dsResolveProperties;
    deMemset(&dsResolveProperties, 0, sizeof(VkPhysicalDeviceDepthStencilResolveProperties));
    dsResolveProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
    dsResolveProperties.pNext = DE_NULL;

    VkPhysicalDeviceProperties2 deviceProperties;
    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties.pNext = &dsResolveProperties;

    // perform query to get supported float control properties
    const VkPhysicalDevice physicalDevice          = m_context.getPhysicalDevice();
    const vk::InstanceInterface &instanceInterface = m_context.getInstanceInterface();
    instanceInterface.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

    // check if both modes are supported
    VkResolveModeFlagBits depthResolveMode   = m_config.depthResolveMode;
    VkResolveModeFlagBits stencilResolveMode = m_config.stencilResolveMode;

    if ((depthResolveMode != VK_RESOLVE_MODE_NONE) &&
        !(depthResolveMode & dsResolveProperties.supportedDepthResolveModes))
        TCU_THROW(NotSupportedError, "Depth resolve mode not supported");

    if ((stencilResolveMode != VK_RESOLVE_MODE_NONE) &&
        !(stencilResolveMode & dsResolveProperties.supportedStencilResolveModes))
        TCU_THROW(NotSupportedError, "Stencil resolve mode not supported");

    // check if the implementation supports setting the depth and stencil resolve
    // modes to different values when one of those modes is VK_RESOLVE_MODE_NONE
    if (dsResolveProperties.independentResolveNone)
    {
        if ((!dsResolveProperties.independentResolve) && (depthResolveMode != stencilResolveMode) &&
            (depthResolveMode != VK_RESOLVE_MODE_NONE) && (stencilResolveMode != VK_RESOLVE_MODE_NONE))
            TCU_THROW(NotSupportedError, "Implementation doesn't support diferent resolve modes");
    }
    else if (!dsResolveProperties.independentResolve && (depthResolveMode != stencilResolveMode))
    {
        // when independentResolveNone and independentResolve are VK_FALSE then both modes must be the same
        TCU_THROW(NotSupportedError, "Implementation doesn't support diferent resolve modes");
    }

    return true;
}

VkSampleCountFlagBits DepthStencilResolveTest::sampleCountBitFromSampleCount(uint32_t count) const
{
    switch (count)
    {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    case 32:
        return VK_SAMPLE_COUNT_32_BIT;
    case 64:
        return VK_SAMPLE_COUNT_64_BIT;

    default:
        DE_FATAL("Invalid sample count");
        return (VkSampleCountFlagBits)0x0;
    }
}

VkImageSp DepthStencilResolveTest::createImage(VkFormat vkformat, uint32_t sampleCount,
                                               VkImageUsageFlags additionalUsage)
{
    const tcu::TextureFormat format(mapVkFormat(m_config.format));
    const VkImageTiling imageTiling(VK_IMAGE_TILING_OPTIMAL);
    VkSampleCountFlagBits sampleCountBit(sampleCountBitFromSampleCount(sampleCount));
    VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalUsage;

    VkImageFormatProperties imageFormatProperties;
    if (m_vki.getPhysicalDeviceImageFormatProperties(m_physicalDevice, m_config.format, VK_IMAGE_TYPE_2D, imageTiling,
                                                     usage, 0u,
                                                     &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Format not supported");
    }
    if (imageFormatProperties.sampleCounts < sampleCount)
    {
        TCU_THROW(NotSupportedError, "Sample count not supported");
    }
    if (imageFormatProperties.maxArrayLayers < m_config.imageLayers)
    {
        TCU_THROW(NotSupportedError, "Layers count not supported");
    }

    const VkExtent3D imageExtent = {m_config.width, m_config.height, 1u};

    if (!(tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order)))
        TCU_THROW(NotSupportedError, "Format can't be used as depth/stencil attachment");

    if (imageFormatProperties.maxExtent.width < imageExtent.width ||
        imageFormatProperties.maxExtent.height < imageExtent.height ||
        ((imageFormatProperties.sampleCounts & sampleCountBit) == 0) ||
        imageFormatProperties.maxArrayLayers < m_config.imageLayers)
    {
        TCU_THROW(NotSupportedError, "Image type not supported");
    }

    const VkImageCreateInfo pCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType
        DE_NULL,                             // const void*              pNext
        0u,                                  // VkImageCreateFlags       flags
        VK_IMAGE_TYPE_2D,                    // VkImageType              imageType
        vkformat,                            // VkFormat                 format
        imageExtent,                         // VkExtent3D               extent
        1u,                                  // uint32_t                 mipLevels
        m_config.imageLayers,                // uint32_t                 arrayLayers
        sampleCountBit,                      // VkSampleCountFlagBits    samples
        imageTiling,                         // VkImageTiling            tiling
        usage,                               // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
        0u,                                  // uint32_t                 queueFamilyIndexCount
        DE_NULL,                             // const uint32_t*          pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout            initialLayout
    };

    return safeSharedPtr(new Unique<VkImage>(vk::createImage(m_vkd, m_device, &pCreateInfo)));
}

AllocationSp DepthStencilResolveTest::createImageMemory(VkImageSp image)
{
    Allocator &allocator = m_context.getDefaultAllocator();

    de::MovePtr<Allocation> allocation(
        allocator.allocate(getImageMemoryRequirements(m_vkd, m_device, **image), MemoryRequirement::Any));
    VK_CHECK(m_vkd.bindImageMemory(m_device, **image, allocation->getMemory(), allocation->getOffset()));
    return safeSharedPtr(allocation.release());
}

VkImageViewSp DepthStencilResolveTest::createImageView(VkImageSp image, VkFormat vkformat, uint32_t baseArrayLayer)
{
    const VkImageSubresourceRange range = {
        aspectFlagsForFormat(vkformat), // VkImageAspectFlags    aspectMask
        0u,                             // uint32_t              baseMipLevel
        1u,                             // uint32_t              levelCount
        baseArrayLayer,                 // uint32_t              baseArrayLayer
        m_config.viewLayers             // uint32_t              layerCount
    };

    const VkImageViewCreateInfo pCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType            sType
        DE_NULL,                                  // const void*                pNext
        0u,                                       // VkImageViewCreateFlags     flags
        **image,                                  // VkImage                    image
        (m_config.viewLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY :
                                    VK_IMAGE_VIEW_TYPE_2D, // VkImageViewType            viewType
        vkformat,                                          // VkFormat                   format
        makeComponentMappingRGBA(),                        // VkComponentMapping         components
        range,                                             // VkImageSubresourceRange    subresourceRange
    };
    return safeSharedPtr(new Unique<VkImageView>(vk::createImageView(m_vkd, m_device, &pCreateInfo)));
}

Move<VkPipelineLayout> DepthStencilResolveTest::createRenderPipelineLayout(void)
{
    VkPushConstantRange pushConstant = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags    stageFlags
        0u,                           // uint32_t              offset
        4u                            // uint32_t              size
    };

    uint32_t pushConstantRangeCount          = 0u;
    VkPushConstantRange *pPushConstantRanges = DE_NULL;
    if (m_config.verifyBuffer == VB_STENCIL)
    {
        pushConstantRangeCount = 1u;
        pPushConstantRanges    = &pushConstant;
    }

    const VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                 sType
        DE_NULL,                                       // const void*                     pNext
        (vk::VkPipelineLayoutCreateFlags)0,            // VkPipelineLayoutCreateFlags     flags
        0u,                                            // uint32_t                        setLayoutCount
        DE_NULL,                                       // const VkDescriptorSetLayout*    pSetLayouts
        pushConstantRangeCount,                        // uint32_t                        pushConstantRangeCount
        pPushConstantRanges                            // const VkPushConstantRange*      pPushConstantRanges
    };

    return vk::createPipelineLayout(m_vkd, m_device, &createInfo);
}

Move<VkPipeline> DepthStencilResolveTest::createRenderPipeline(VkFormat format, VkPipelineLayout renderPipelineLayout)
{
    const bool testingStencil                    = (m_config.verifyBuffer == VB_STENCIL);
    const vk::BinaryCollection &binaryCollection = m_context.getBinaryCollection();

    const Unique<VkShaderModule> vertexShaderModule(
        createShaderModule(m_vkd, m_device, binaryCollection.get("quad-vert"), 0u));
    const Unique<VkShaderModule> fragmentShaderModule(
        createShaderModule(m_vkd, m_device, binaryCollection.get("quad-frag"), 0u));
    const Move<VkShaderModule> geometryShaderModule(
        m_config.imageLayers == 1 ? Move<VkShaderModule>() :
                                    createShaderModule(m_vkd, m_device, binaryCollection.get("quad-geom"), 0u));

    const VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        DE_NULL,                                                   // const void*                                 pNext
        (VkPipelineVertexInputStateCreateFlags)0u,                 // VkPipelineVertexInputStateCreateFlags       flags
        0u,      // uint32_t                                    vertexBindingDescriptionCount
        DE_NULL, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        0u,      // uint32_t                                    vertexAttributeDescriptionCount
        DE_NULL  // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };
    const tcu::UVec2 view(m_config.width, m_config.height);
    const std::vector<VkViewport> viewports(1, makeViewport(view));
    const std::vector<VkRect2D> scissors(1, m_config.renderArea);

    const VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        DE_NULL,
        (VkPipelineMultisampleStateCreateFlags)0u,
        sampleCountBitFromSampleCount(m_config.sampleCount),
        VK_FALSE,
        0.0f,
        DE_NULL,
        VK_FALSE,
        VK_FALSE,
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        DE_NULL,
        (VkPipelineDepthStencilStateCreateFlags)0u,

        VK_TRUE, // depthTestEnable
        VK_TRUE,
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        testingStencil, // stencilTestEnable
        {
            VK_STENCIL_OP_REPLACE, // failOp
            VK_STENCIL_OP_REPLACE, // passOp
            VK_STENCIL_OP_REPLACE, // depthFailOp
            VK_COMPARE_OP_ALWAYS,  // compareOp
            0xFFu,                 // compareMask
            0xFFu,                 // writeMask
            1                      // reference
        },
        {VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 1},
        0.0f,
        1.0f};

    std::vector<VkDynamicState> dynamicState;
    dynamicState.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType                      sType
        DE_NULL,                                              // const void*                          pNext
        (VkPipelineDynamicStateCreateFlags)0u,                // VkPipelineDynamicStateCreateFlags    flags
        static_cast<uint32_t>(dynamicState.size()),           // uint32_t                             dynamicStateCount
        &dynamicState[0]                                      // const VkDynamicState*                pDynamicStates
    };

    VkPipelineRenderingCreateInfoKHR dynamicRenderingInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, // VkStructureType    sType;
        DE_NULL,                                          // const void*        pNext;
        0,                                                // uint32_t           viewMask;
        0,                                                // uint32_t           colorAttachmentCount;
        DE_NULL,                                          // const VkFormat*    pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                              // VkFormat           depthAttachmentFormat;
        VK_FORMAT_UNDEFINED                               // VkFormat           stencilAttachmentFormat;
    };
    const tcu::TextureFormat deFormat(mapVkFormat(format));
    if (tcu::hasDepthComponent(deFormat.order) && m_config.verifyBuffer == VB_DEPTH)
        dynamicRenderingInfo.depthAttachmentFormat = format;
    if (tcu::hasStencilComponent(deFormat.order) && m_config.verifyBuffer != VB_DEPTH)
        dynamicRenderingInfo.stencilAttachmentFormat = format;

    return makeGraphicsPipeline(
        m_vkd,                // const DeviceInterface&                        vk
        m_device,             // const VkDevice                                device
        renderPipelineLayout, // const VkPipelineLayout                        pipelineLayout
        *vertexShaderModule,  // const VkShaderModule                          vertexShaderModule
        DE_NULL,              // const VkShaderModule                          tessellationControlShaderModule
        DE_NULL,              // const VkShaderModule                          tessellationEvalShaderModule
        m_config.imageLayers == 1 ?
            DE_NULL :
            *geometryShaderModule,           // const VkShaderModule                          geometryShaderModule
        *fragmentShaderModule,               // const VkShaderModule                          fragmentShaderModule
        VK_NULL_HANDLE,                      // const VkRenderPass                            renderPass
        viewports,                           // const std::vector<VkViewport>&                viewports
        scissors,                            // const std::vector<VkRect2D>&                  scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
        0u,                                  // const uint32_t                                subpass
        0u,                                  // const uint32_t                                patchControlPoints
        &vertexInputState,                   // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
        DE_NULL,            // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
        &multisampleState,  // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
        &depthStencilState, // const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
        DE_NULL,            // const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
        testingStencil ? &dynamicStateCreateInfo :
                         DE_NULL,               // const VkPipelineDynamicStateCreateInfo*       dynamicStateCreateInfo
        &dynamicRenderingInfo,                  // const void*                                   pNext
        static_cast<VkPipelineCreateFlags>(0)); // const VkPipelineCreateFlags                   pipelineCreateFlags
}

AllocationSp DepthStencilResolveTest::createBufferMemory(void)
{
    Allocator &allocator = m_context.getDefaultAllocator();
    de::MovePtr<Allocation> allocation(
        allocator.allocate(getBufferMemoryRequirements(m_vkd, m_device, **m_buffer), MemoryRequirement::HostVisible));
    VK_CHECK(m_vkd.bindBufferMemory(m_device, **m_buffer, allocation->getMemory(), allocation->getOffset()));
    return safeSharedPtr(allocation.release());
}

VkBufferSp DepthStencilResolveTest::createBuffer(void)
{
    const VkBufferUsageFlags bufferUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const tcu::TextureFormat textureFormat(mapVkFormat(m_config.format));
    const VkDeviceSize pixelSize(textureFormat.getPixelSize());
    const VkBufferCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                // VkStructureType        sType
        DE_NULL,                                                             // const void*            pNext
        0u,                                                                  // VkBufferCreateFlags    flags
        m_config.width * m_config.height * m_config.imageLayers * pixelSize, // VkDeviceSize           size
        bufferUsage,                                                         // VkBufferUsageFlags     usage
        VK_SHARING_MODE_EXCLUSIVE,                                           // VkSharingMode          sharingMode
        0u,     // uint32_t               queueFamilyIndexCount
        DE_NULL // const uint32_t*        pQueueFamilyIndices
    };
    return safeSharedPtr(new Unique<VkBuffer>(vk::createBuffer(m_vkd, m_device, &createInfo)));
}

#ifndef CTS_USES_VULKANSC
void DepthStencilResolveTest::beginSecondaryCommandBuffer(VkCommandBuffer cmdBuffer, VerifyBuffer attachmentType,
                                                          VkRenderingFlagsKHR renderingFlags) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    VkFormat depthFormat       = attachmentType == VB_DEPTH ? m_config.format : VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat     = attachmentType == VB_STENCIL ? m_config.format : VK_FORMAT_UNDEFINED;

    VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType                    sType
        DE_NULL,                                                         // const void*                        pNext
        renderingFlags,                                                  // VkRenderingFlagsKHR                flags
        0u,                                                              // uint32_t                            viewMask
        0u,            // uint32_t                            colorAttachmentCount
        DE_NULL,       // const VkFormat*                    pColorAttachmentFormats
        depthFormat,   // VkFormat                            depthAttachmentFormat
        stencilFormat, // VkFormat                            stencilAttachmentFormat
        sampleCountBitFromSampleCount(m_config.sampleCount) // VkSampleCountFlagBits            rasterizationSamples
    };

    const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);
    VkCommandBufferUsageFlags usageFlags                       = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    const VkCommandBufferBeginInfo commandBufBeginParams{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType                            sType
        DE_NULL,                                     // const void*                                pNext
        usageFlags,                                  // VkCommandBufferUsageFlags                flags
        &bufferInheritanceInfo,                      // const VkCommandBufferInheritanceInfo*    pInheritanceInfo
    };

    VK_CHECK(vkd.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

void DepthStencilResolveTest::submit(void)
{
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    Move<VkCommandBuffer> secCmdBuffer;

    const vk::VkImageSubresourceRange imageRange = {
        aspectFlagsForFormat(m_config.format), // VkImageAspectFlags    aspectMask
        0u,                                    // uint32_t              baseMipLevel
        VK_REMAINING_MIP_LEVELS,               // uint32_t              levelCount
        0u,                                    // uint32_t              baseArrayLayer
        VK_REMAINING_ARRAY_LAYERS,             // uint32_t              layerCount
    };

    const VkImageMemoryBarrier preClearBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
        DE_NULL,                                // const void*                pNext
        VK_ACCESS_NONE_KHR,                     // VkAccessFlags              srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags              dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout              oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout              newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                   srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                   dstQueueFamilyIndex
        **m_singlesampleImage,                  // VkImage                    image
        imageRange,                             // VkImageSubresourceRange    subresourceRange
    };

    const VkImageMemoryBarrier preRenderBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,           // VkStructureType            sType
        DE_NULL,                                          // const void*                pNext
        VK_ACCESS_TRANSFER_WRITE_BIT,                     // VkAccessFlags              srcAccessMask
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,     // VkAccessFlags              dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,             // VkImageLayout              oldLayout
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout              newLayout
        VK_QUEUE_FAMILY_IGNORED,                          // uint32_t                   srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                          // uint32_t                   dstQueueFamilyIndex
        **m_singlesampleImage,                            // VkImage                    image
        imageRange,                                       // VkImageSubresourceRange    subresourceRange
    };

    // Clearing resolve image
    {
        const Unique<VkCommandBuffer> clearCmdBuffer(
            allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        beginCommandBuffer(vkd, *clearCmdBuffer);
        vkd.cmdPipelineBarrier(*clearCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                               0u, DE_NULL, 0u, DE_NULL, 1u, &preClearBarrier);

        vkd.cmdClearDepthStencilImage(*clearCmdBuffer, **m_singlesampleImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      &m_config.clearValue, 1u, &imageRange);
        endCommandBuffer(vkd, *clearCmdBuffer);

        vk::submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *clearCmdBuffer);
    }

    bool testingDepth = (m_config.verifyBuffer == VB_DEPTH);
    if (testingDepth)
    {
        // Begin rendering
        VkClearValue clearVal;
        clearVal.depthStencil = m_config.clearValue;

        const VkRenderingAttachmentInfo depthAttachment = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,      // VkStructureType          sType
            DE_NULL,                                          // const void*              pNext
            **m_multisampleImageView,                         // VkImageView              imageView
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            imageLayout
            m_config.depthResolveMode,                        // VkResolveModeFlagBits    resolveMode
            **m_singlesampleImageView,                        // VkImageView              resolveImageView
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            resolveImageLayout
            VK_ATTACHMENT_LOAD_OP_CLEAR,                      // VkAttachmentLoadOp       loadOp
            VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp      storeOp
            clearVal                                          // VkClearValue             clearValue
        };

        VkRenderingInfoKHR renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO, // VkStructureType                     sType
            DE_NULL,                          // const void*                         pNext
            static_cast<VkRenderingFlags>(0), // VkRenderingFlags                    flags
            {
                // VkRect2D                            renderArea
                {0u, 0u},                         // VkOffset2D    offset;
                {m_config.width, m_config.height} // VkExtent2D    extent
            },
            m_config.viewLayers, // uint32_t                            layerCount
            0,                   // uint32_t                            viewMask
            0,                   // uint32_t                            colorAttachmentCount
            DE_NULL,             // const VkRenderingAttachmentInfo*    pColorAttachments
            &depthAttachment,    // const VkRenderingAttachmentInfo*    pDepthAttachment
            DE_NULL              // const VkRenderingAttachmentInfo*    pStencilAttachment
        };

        if (m_config.groupParams->useSecondaryCmdBuffer)
        {
            secCmdBuffer = allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

            // Record secondary command buffer
            if (m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            {
                beginSecondaryCommandBuffer(*secCmdBuffer, VB_DEPTH,
                                            VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
                vkd.cmdBeginRendering(*secCmdBuffer, &renderingInfo);
            }
            else
                beginSecondaryCommandBuffer(*secCmdBuffer, VB_DEPTH);

            vkd.cmdBindPipeline(*secCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
            vkd.cmdDraw(*secCmdBuffer, 6u, 1u, 0u, 0u);

            if (m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                vkd.cmdEndRendering(*secCmdBuffer);

            vk::endCommandBuffer(vkd, *secCmdBuffer);

            // Record primary command buffer
            beginCommandBuffer(vkd, *cmdBuffer);
            vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                       vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preRenderBarrier);

            if (!m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            {
                renderingInfo.flags = vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
                vkd.cmdBeginRendering(*cmdBuffer, &renderingInfo);
            }
            vkd.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);

            if (!m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                vkd.cmdEndRendering(*cmdBuffer);
        }
        else
        {
            // Record primary command buffer
            beginCommandBuffer(vkd, *cmdBuffer);
            vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                       vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preRenderBarrier);
            vkd.cmdBeginRendering(*cmdBuffer, &renderingInfo);
            vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
            vkd.cmdDraw(*cmdBuffer, 6u, 1u, 0u, 0u);
            vkd.cmdEndRendering(*cmdBuffer);
        }
    }
    else
    {
        beginCommandBuffer(vkd, *cmdBuffer);
        vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                               vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preRenderBarrier);
        if (m_config.groupParams->useSecondaryCmdBuffer)
        {
            secCmdBuffer = allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

            for (uint32_t i = 0; i < m_config.sampleCount; i++)
            {
                if (i == 0)
                {
                    // Begin rendering
                    VkClearValue clearVal;
                    clearVal.depthStencil = m_config.clearValue;

                    const VkRenderingAttachmentInfo stencilAttachment = {
                        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,      // VkStructureType          sType
                        DE_NULL,                                          // const void*              pNext
                        **m_multisampleImageView,                         // VkImageView              imageView
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            imageLayout
                        m_config.stencilResolveMode,                      // VkResolveModeFlagBits    resolveMode
                        **m_singlesampleImageView,                        // VkImageView              resolveImageView
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            resolveImageLayout
                        VK_ATTACHMENT_LOAD_OP_CLEAR,                      // VkAttachmentLoadOp       loadOp
                        VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp      storeOp
                        clearVal                                          // VkClearValue             clearValue
                    };

                    VkRenderingInfoKHR renderingInfo = {
                        VK_STRUCTURE_TYPE_RENDERING_INFO, // VkStructureType                     sType
                        DE_NULL,                          // const void*                         pNext
                        static_cast<VkRenderingFlags>(0), // VkRenderingFlags                    flags
                        {
                            // VkRect2D                            renderArea
                            {0u, 0u},                         // VkOffset2D    offset;
                            {m_config.width, m_config.height} // VkExtent2D    extent
                        },
                        m_config.viewLayers, // uint32_t                            layerCount
                        0,                   // uint32_t                            viewMask
                        0,                   // uint32_t                            colorAttachmentCount
                        DE_NULL,             // const VkRenderingAttachmentInfo*    pColorAttachments
                        DE_NULL,             // const VkRenderingAttachmentInfo*    pDepthAttachment
                        &stencilAttachment   // const VkRenderingAttachmentInfo*    pStencilAttachment
                    };

                    vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, 0, 0, 0, 0, 0);

                    if (m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                        beginSecondaryCommandBuffer(*secCmdBuffer, VB_STENCIL,
                                                    VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
                    else
                        beginSecondaryCommandBuffer(*secCmdBuffer, VB_STENCIL);

                    // Record secondary command buffer
                    if (m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                    {
                        vkd.cmdBeginRendering(*secCmdBuffer, &renderingInfo);
                    }
                    else
                    {
                        renderingInfo.flags = vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
                        vkd.cmdBeginRendering(*cmdBuffer, &renderingInfo);
                    }
                }

                // For stencil we can set reference value for just one sample at a time
                // so we need to do as many passes as there are samples, first half
                // of samples is initialized with 1 and second half with 255

                const uint32_t halfOfSamples    = m_config.sampleCount >> 1;
                const uint32_t stencilReference = 1 + 254 * (i >= halfOfSamples);

                vkd.cmdBindPipeline(*secCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
                vkd.cmdPushConstants(*secCmdBuffer, *m_renderPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                                     sizeof(i), &i);
                vkd.cmdSetStencilReference(*secCmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, stencilReference);
                vkd.cmdDraw(*secCmdBuffer, 6u, 1u, 0u, 0u);
                if (i == m_config.sampleCount - 1)
                {
                    if (m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                        vkd.cmdEndRendering(*secCmdBuffer);
                    vk::endCommandBuffer(vkd, *secCmdBuffer);
                    vkd.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);
                    if (!m_config.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                        vkd.cmdEndRendering(*cmdBuffer);
                }
            }
        }
        else
        {
            for (uint32_t i = 0; i < m_config.sampleCount; i++)
            {
                if (i == 0)
                {
                    // Begin rendering
                    VkClearValue clearVal;
                    clearVal.depthStencil = m_config.clearValue;

                    const VkRenderingAttachmentInfo stencilAttachment = {
                        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,      // VkStructureType          sType
                        DE_NULL,                                          // const void*              pNext
                        **m_multisampleImageView,                         // VkImageView              imageView
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            imageLayout
                        m_config.stencilResolveMode,                      // VkResolveModeFlagBits    resolveMode
                        **m_singlesampleImageView,                        // VkImageView              resolveImageView
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            resolveImageLayout
                        VK_ATTACHMENT_LOAD_OP_CLEAR,                      // VkAttachmentLoadOp       loadOp
                        VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp      storeOp
                        clearVal                                          // VkClearValue             clearValue
                    };

                    VkRenderingInfoKHR renderingInfo = {
                        VK_STRUCTURE_TYPE_RENDERING_INFO, // VkStructureType                     sType
                        DE_NULL,                          // const void*                         pNext
                        static_cast<VkRenderingFlags>(0), // VkRenderingFlags                    flags
                        {
                            // VkRect2D                            renderArea
                            {0u, 0u},                         // VkOffset2D    offset
                            {m_config.width, m_config.height} // VkExtent2D    extent
                        },
                        m_config.viewLayers, // uint32_t                            layerCount
                        0,                   // uint32_t                            viewMask
                        0,                   // uint32_t                            colorAttachmentCount
                        DE_NULL,             // const VkRenderingAttachmentInfo*    pColorAttachments
                        DE_NULL,             // const VkRenderingAttachmentInfo*    pDepthAttachment
                        &stencilAttachment   // const VkRenderingAttachmentInfo*    pStencilAttachment
                    };

                    vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, 0, 0, 0, 0, 0);
                    vkd.cmdBeginRendering(*cmdBuffer, &renderingInfo);
                }

                // For stencil we can set reference value for just one sample at a time
                // so we need to do as many passes as there are samples, first half
                // of samples is initialized with 1 and second half with 255

                const uint32_t halfOfSamples = m_config.sampleCount >> 1;
                uint32_t stencilReference    = 1 + 254 * (i >= halfOfSamples);
                vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
                vkd.cmdPushConstants(*cmdBuffer, *m_renderPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(i),
                                     &i);
                vkd.cmdSetStencilReference(*cmdBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, stencilReference);
                vkd.cmdDraw(*cmdBuffer, 6u, 1u, 0u, 0u);
                if (i == m_config.sampleCount - 1)
                    vkd.cmdEndRendering(*cmdBuffer);
            }
        }
    }

    // Memory barriers between rendering and copying
    {
        const VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            DE_NULL,

            // Note: as per the spec, depth/stencil *resolve* operations are synchronized using the color attachment write access.
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,

            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,

            **m_singlesampleImage,
            {(m_config.separateDepthStencilLayouts) ?
                 VkImageAspectFlags(testingDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT) :
                 aspectFlagsForFormat(m_config.format),
             0u, 1u, 0u, m_config.viewLayers}};

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                               DE_NULL, 0u, DE_NULL, 1u, &barrier);
    }

    // Copy image memory to buffers
    const VkBufferImageCopy region = {
        0u,
        0u,
        0u,
        {
            VkImageAspectFlags(testingDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT),
            0u,
            0u,
            m_config.viewLayers,
        },
        {0u, 0u, 0u},
        {m_config.width, m_config.height, 1u}};

    vkd.cmdCopyImageToBuffer(*cmdBuffer, **m_singlesampleImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_buffer, 1u,
                             &region);

    // Memory barriers between copies and host access
    {
        const VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                               DE_NULL,

                                               VK_ACCESS_TRANSFER_WRITE_BIT,
                                               VK_ACCESS_HOST_READ_BIT,

                                               VK_QUEUE_FAMILY_IGNORED,
                                               VK_QUEUE_FAMILY_IGNORED,

                                               **m_buffer,
                                               0u,
                                               VK_WHOLE_SIZE};

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL,
                               1u, &barrier, 0u, DE_NULL);
    }

    vk::endCommandBuffer(vkd, *cmdBuffer);

    vk::submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *cmdBuffer);
}

bool DepthStencilResolveTest::verifyDepth(void)
{
    // Invalidate allocation before attempting to read buffer memory.
    invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), *m_bufferMemory);

    uint32_t layerSize   = m_config.width * m_config.height;
    uint32_t valuesCount = layerSize * m_config.viewLayers;
    uint8_t *pixelPtr    = static_cast<uint8_t *>(m_bufferMemory->getHostPtr());

    const DeviceInterface &vkd(m_context.getDeviceInterface());
    invalidateMappedMemoryRange(vkd, m_context.getDevice(), m_bufferMemory->getMemory(), m_bufferMemory->getOffset(),
                                VK_WHOLE_SIZE);

    float expectedValue = m_config.depthExpectedValue;
    if (m_config.depthResolveMode == VK_RESOLVE_MODE_NONE)
        expectedValue = m_config.clearValue.depth;

    // depth data in buffer is tightly packed, ConstPixelBufferAccess
    // coludn't be used for depth value extraction as it cant interpret
    // formats containing just depth component

    typedef float (*DepthComponentGetterFn)(uint8_t *);
    VkFormat format                          = m_config.format;
    DepthComponentGetterFn getDepthComponent = &get16bitDepthComponent;
    uint32_t pixelStep                       = 2;
    float epsilon                            = 0.002f;

    if ((format == VK_FORMAT_X8_D24_UNORM_PACK32) || (format == VK_FORMAT_D24_UNORM_S8_UINT))
    {
        getDepthComponent = &get24bitDepthComponent;
        pixelStep         = 4;
    }
    else if ((format == VK_FORMAT_D32_SFLOAT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT))
    {
        getDepthComponent = &get32bitDepthComponent;
        pixelStep         = 4;
    }

    for (uint32_t valueIndex = 0; valueIndex < valuesCount; valueIndex++)
    {
        float depth = (*getDepthComponent)(pixelPtr);
        pixelPtr += pixelStep;

        // check if pixel data is outside of render area
        int32_t layerIndex   = valueIndex / layerSize;
        int32_t inLayerIndex = valueIndex % layerSize;
        int32_t x            = inLayerIndex % m_config.width;
        int32_t y            = (inLayerIndex - x) / m_config.width;
        int32_t x1           = m_config.renderArea.offset.x;
        int32_t y1           = m_config.renderArea.offset.y;
        int32_t x2           = x1 + m_config.renderArea.extent.width;
        int32_t y2           = y1 + m_config.renderArea.extent.height;
        if ((x < x1) || (x >= x2) || (y < y1) || (y >= y2))
        {
            // verify that outside of render area there are clear values
            float error = deFloatAbs(depth - m_config.clearValue.depth);
            if (error > epsilon)
            {
                m_context.getTestContext().getLog()
                    << TestLog::Message << "(" << x << ", " << y << ", layer: " << layerIndex
                    << ") is outside of render area but depth value is: " << depth << " (expected "
                    << m_config.clearValue.depth << ")" << TestLog::EndMessage;
                return false;
            }

            // value is correct, go to next one
            continue;
        }

        float error = deFloatAbs(depth - expectedValue);
        if (error > epsilon)
        {
            m_context.getTestContext().getLog()
                << TestLog::Message << "At (" << x << ", " << y << ", layer: " << layerIndex
                << ") depth value is: " << depth << " expected: " << expectedValue << TestLog::EndMessage;
            return false;
        }
    }
    m_context.getTestContext().getLog() << TestLog::Message << "Depth value is " << expectedValue
                                        << TestLog::EndMessage;

    return true;
}

bool DepthStencilResolveTest::verifyStencil(void)
{
    // Invalidate allocation before attempting to read buffer memory.
    invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), *m_bufferMemory);

    uint32_t layerSize   = m_config.width * m_config.height;
    uint32_t valuesCount = layerSize * m_config.viewLayers;
    uint8_t *pixelPtr    = static_cast<uint8_t *>(m_bufferMemory->getHostPtr());

    const DeviceInterface &vkd(m_context.getDeviceInterface());
    invalidateMappedMemoryRange(vkd, m_context.getDevice(), m_bufferMemory->getMemory(), m_bufferMemory->getOffset(),
                                VK_WHOLE_SIZE);

    // when stencil is tested we are discarding invocations and
    // because of that depth and stencil need to be tested separately

    uint8_t expectedValue = m_config.stencilExpectedValue;
    if (m_config.stencilResolveMode == VK_RESOLVE_MODE_NONE)
        expectedValue = static_cast<uint8_t>(m_config.clearValue.stencil);

    for (uint32_t valueIndex = 0; valueIndex < valuesCount; valueIndex++)
    {
        uint8_t stencil      = *pixelPtr++;
        int32_t layerIndex   = valueIndex / layerSize;
        int32_t inLayerIndex = valueIndex % layerSize;
        int32_t x            = inLayerIndex % m_config.width;
        int32_t y            = (inLayerIndex - x) / m_config.width;
        int32_t x1           = m_config.renderArea.offset.x;
        int32_t y1           = m_config.renderArea.offset.y;
        int32_t x2           = x1 + m_config.renderArea.extent.width;
        int32_t y2           = y1 + m_config.renderArea.extent.height;
        if ((x < x1) || (x >= x2) || (y < y1) || (y >= y2))
        {
            if (stencil != m_config.clearValue.stencil)
            {
                m_context.getTestContext().getLog()
                    << TestLog::Message << "(" << x << ", " << y << ", layer: " << layerIndex
                    << ") is outside of render area but stencil value is: " << stencil << " (expected "
                    << m_config.clearValue.stencil << ")" << TestLog::EndMessage;
                return false;
            }

            // value is correct, go to next one
            continue;
        }

        if (stencil != expectedValue)
        {
            m_context.getTestContext().getLog()
                << TestLog::Message << "At (" << x << ", " << y << ", layer: " << layerIndex
                << ") stencil value is: " << static_cast<uint32_t>(stencil)
                << " expected: " << static_cast<uint32_t>(expectedValue) << TestLog::EndMessage;
            return false;
        }
    }
    m_context.getTestContext().getLog() << TestLog::Message << "Stencil value is "
                                        << static_cast<uint32_t>(expectedValue) << TestLog::EndMessage;

    return true;
}

tcu::TestStatus DepthStencilResolveTest::iterate(void)
{
    submit();

    bool result = false;
    if (m_config.verifyBuffer == VB_DEPTH)
        result = verifyDepth();
    else
        result = verifyStencil();

    if (result)
        return tcu::TestStatus::pass("Pass");
    return tcu::TestStatus::fail("Fail");
}

struct Programs
{
    void init(vk::SourceCollections &dst, TestConfig config) const
    {
        // geometry shader is only needed in multi-layer framebuffer resolve tests
        if (config.imageLayers > 1)
        {
            const uint32_t layerCount = 3;

            std::ostringstream src;
            src << "#version 450\n"
                << "highp float;\n"
                << "\n"
                << "layout(triangles) in;\n"
                << "layout(triangle_strip, max_vertices = " << 3 * 2 * layerCount << ") out;\n"
                << "\n"
                << "in gl_PerVertex {\n"
                << "    vec4 gl_Position;\n"
                << "} gl_in[];\n"
                << "\n"
                << "out gl_PerVertex {\n"
                << "    vec4 gl_Position;\n"
                << "};\n"
                << "\n"
                << "void main (void) {\n"
                << "    for (int layerNdx = 0; layerNdx < " << layerCount << "; ++layerNdx) {\n"
                << "        for(int vertexNdx = 0; vertexNdx < gl_in.length(); vertexNdx++) {\n"
                << "            gl_Position = gl_in[vertexNdx].gl_Position;\n"
                << "            gl_Layer    = layerNdx;\n"
                << "            EmitVertex();\n"
                << "        };\n"
                << "        EndPrimitive();\n"
                << "    };\n"
                << "}\n";

            dst.glslSources.add("quad-geom") << glu::GeometrySource(src.str());
        }

        dst.glslSources.add("quad-vert") << glu::VertexSource(
            "#version 450\n"
            "out gl_PerVertex {\n"
            "\tvec4 gl_Position;\n"
            "};\n"
            "highp float;\n"
            "void main (void) {\n"
            "\tgl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
            "\t                   ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
            "}\n");

        if (config.verifyBuffer == VB_DEPTH)
        {
            dst.glslSources.add("quad-frag") << glu::FragmentSource(
                "#version 450\n"
                "precision highp float;\n"
                "precision highp int;\n"
                "void main (void)\n"
                "{\n"
                "  float sampleIndex = float(gl_SampleID);\n"          // sampleIndex is integer in range <0, 63>
                "  float valueIndex = round(mod(sampleIndex, 4.0));\n" // limit possible depth values - count to 4
                "  float value = valueIndex + 2.0;\n"                  // value is one of [2, 3, 4, 5]
                "  value = round(exp2(value));\n"                      // value is one of [4, 8, 16, 32]
                "  bool condition = (int(value) == 8);\n"            // select second sample value (to make it smallest)
                "  value = round(value - float(condition) * 6.0);\n" // value is one of [4, 2, 16, 32]
                "  gl_FragDepth = value / 100.0;\n"                  // sample depth is one of [0.04, 0.02, 0.16, 0.32]
                "}\n");
        }
        else
        {
            dst.glslSources.add("quad-frag") << glu::FragmentSource("#version 450\n"
                                                                    "precision highp float;\n"
                                                                    "precision highp int;\n"
                                                                    "layout(push_constant) uniform PushConstant {\n"
                                                                    "  highp int sampleID;\n"
                                                                    "} pushConstants;\n"
                                                                    "void main (void)\n"
                                                                    "{\n"
                                                                    "  if(gl_SampleID != pushConstants.sampleID)\n"
                                                                    "    discard;\n"
                                                                    "  gl_FragDepth = 0.5;\n"
                                                                    "}\n");
        }
    }
};

void initTests(tcu::TestCaseGroup *group, const SharedGroupParams groupParams)
{
    typedef InstanceFactory1<DepthStencilResolveTest, TestConfig, Programs> DSResolveTestInstance;

    struct FormatData
    {
        VkFormat format;
        const char *name;
        bool hasDepth;
        bool hasStencil;
    };
    FormatData formats[] = {
        {VK_FORMAT_D16_UNORM, "d16_unorm", true, false},
        {VK_FORMAT_X8_D24_UNORM_PACK32, "x8_d24_unorm_pack32", true, false},
        {VK_FORMAT_D32_SFLOAT, "d32_sfloat", true, false},
        {VK_FORMAT_S8_UINT, "s8_uint", false, true},
        {VK_FORMAT_D16_UNORM_S8_UINT, "d16_unorm_s8_uint", true, true},
        {VK_FORMAT_D24_UNORM_S8_UINT, "d24_unorm_s8_uint", true, true},
        {VK_FORMAT_D32_SFLOAT_S8_UINT, "d32_sfloat_s8_uint", true, true},
    };

    struct ResolveModeData
    {
        VkResolveModeFlagBits flag;
        std::string name;
    };
    ResolveModeData resolveModes[] = {
        {VK_RESOLVE_MODE_NONE, "none"},           {VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, "zero"},
        {VK_RESOLVE_MODE_AVERAGE_BIT, "average"}, {VK_RESOLVE_MODE_MIN_BIT, "min"},
        {VK_RESOLVE_MODE_MAX_BIT, "max"},
    };

    struct ImageTestData
    {
        const char *groupName;
        uint32_t width;
        uint32_t height;
        uint32_t imageLayers;
        VkRect2D renderArea;
        VkClearDepthStencilValue clearValue;
    };

    // NOTE: tests cant be executed for 1D and 3D images:
    // 1D images are not tested because acording to specification sampleCounts
    // will be set to VK_SAMPLE_COUNT_1_BIT when type is not VK_IMAGE_TYPE_2D
    // 3D images are not tested because VkFramebufferCreateInfo specification
    // states that: each element of pAttachments that is a 2D or 2D array image
    // view taken from a 3D image must not be a depth/stencil format
    const uint32_t sampleCounts[]       = {2u, 4u, 8u, 16u, 32u, 64u};
    const float depthExpectedValue[][6] = {
        // 2 samples    4            8            16            32            64
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},            // RESOLVE_MODE_NONE
        {0.04f, 0.04f, 0.04f, 0.04f, 0.04f, 0.04f},      // RESOLVE_MODE_SAMPLE_ZERO_BIT
        {0.03f, 0.135f, 0.135f, 0.135f, 0.135f, 0.135f}, // RESOLVE_MODE_AVERAGE_BIT
        {0.02f, 0.02f, 0.02f, 0.02f, 0.02f, 0.02f},      // RESOLVE_MODE_MIN_BIT
        {0.04f, 0.32f, 0.32f, 0.32f, 0.32f, 0.32f},      // RESOLVE_MODE_MAX_BIT
    };
    const uint8_t stencilExpectedValue[][6] = {
        // 2 samples    4        8        16        32        64
        {0u, 0u, 0u, 0u, 0u, 0u},             // RESOLVE_MODE_NONE
        {1u, 1u, 1u, 1u, 1u, 1u},             // RESOLVE_MODE_SAMPLE_ZERO_BIT
        {0u, 0u, 0u, 0u, 0u, 0u},             // RESOLVE_MODE_AVERAGE_BIT
        {1u, 1u, 1u, 1u, 1u, 1u},             // RESOLVE_MODE_MIN_BIT
        {255u, 255u, 255u, 255u, 255u, 255u}, // RESOLVE_MODE_MAX_BIT
    };

    tcu::TestContext &testCtx(group->getTestContext());

    ImageTestData imageData = {"image_2d_32_32", 32, 32, 1, {{0, 0}, {32, 32}}, {0.000f, 0x00}};

    // iterate over sampleCounts
    for (size_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
    {
        const uint32_t sampleCount(sampleCounts[sampleCountNdx]);
        const std::string sampleName("samples_" + de::toString(sampleCount));

        // create test group for sample count
        de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, sampleName.c_str()));

        // iterate over depth/stencil formats
        for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
        {
            const FormatData &formatData = formats[formatNdx];
            VkFormat format              = formatData.format;
            const char *formatName       = formatData.name;
            const bool hasDepth          = formatData.hasDepth;
            const bool hasStencil        = formatData.hasStencil;
            VkImageAspectFlags aspectFlags =
                (hasDepth * VK_IMAGE_ASPECT_DEPTH_BIT) | (hasStencil * VK_IMAGE_ASPECT_STENCIL_BIT);
            const int separateLayoutsLoopCount = (hasDepth && hasStencil) ? 2 : 1;

            for (int separateDepthStencilLayouts = 0; separateDepthStencilLayouts < separateLayoutsLoopCount;
                 ++separateDepthStencilLayouts)
            {
                const bool useSeparateDepthStencilLayouts = bool(separateDepthStencilLayouts);
                const std::string groupName =
                    std::string(formatName) + ((useSeparateDepthStencilLayouts) ? "_separate_layouts" : "");

                // create test group for format
                de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));

                // iterate over depth resolve modes
                for (size_t depthResolveModeNdx = 0; depthResolveModeNdx < DE_LENGTH_OF_ARRAY(resolveModes);
                     depthResolveModeNdx++)
                {
                    // iterate over stencil resolve modes
                    for (size_t stencilResolveModeNdx = 0; stencilResolveModeNdx < DE_LENGTH_OF_ARRAY(resolveModes);
                         stencilResolveModeNdx++)
                    {
                        // there is no average resolve mode for stencil - go to next iteration
                        ResolveModeData &sResolve = resolveModes[stencilResolveModeNdx];
                        if (sResolve.flag == VK_RESOLVE_MODE_AVERAGE_BIT)
                            continue;

                        // if pDepthStencilResolveAttachment is not NULL and does not have the value VK_ATTACHMENT_UNUSED,
                        // depthResolveMode and stencilResolveMode must not both be VK_RESOLVE_MODE_NONE_KHR
                        ResolveModeData &dResolve = resolveModes[depthResolveModeNdx];
                        if ((dResolve.flag == VK_RESOLVE_MODE_NONE) && (sResolve.flag == VK_RESOLVE_MODE_NONE))
                            continue;

                        // If there is no depth, the depth resolve mode should be NONE, or
                        // match the stencil resolve mode.
                        if (!hasDepth && (dResolve.flag != VK_RESOLVE_MODE_NONE) && (dResolve.flag != sResolve.flag))
                            continue;

                        // If there is no stencil, the stencil resolve mode should be NONE, or
                        // match the depth resolve mode.
                        if (!hasStencil && (sResolve.flag != VK_RESOLVE_MODE_NONE) && (dResolve.flag != sResolve.flag))
                            continue;

                        std::string baseName = "depth_" + dResolve.name + "_stencil_" + sResolve.name;

                        if (hasDepth)
                        {
                            std::string name     = baseName + "_testing_depth";
                            const char *testName = name.c_str();
                            float expectedValue  = depthExpectedValue[depthResolveModeNdx][sampleCountNdx];

                            const TestConfig testConfig = {format,
                                                           imageData.width,
                                                           imageData.height,
                                                           1u,
                                                           1u,
                                                           0u,
                                                           imageData.renderArea,
                                                           aspectFlags,
                                                           sampleCount,
                                                           dResolve.flag,
                                                           sResolve.flag,
                                                           VB_DEPTH,
                                                           imageData.clearValue,
                                                           expectedValue,
                                                           0u,
                                                           useSeparateDepthStencilLayouts,
                                                           groupParams};
                            formatGroup->addChild(new DSResolveTestInstance(testCtx, testName, testConfig));
                        }

                        if (hasStencil)
                        {
                            std::string name      = baseName + "_testing_stencil";
                            const char *testName  = name.c_str();
                            uint8_t expectedValue = stencilExpectedValue[stencilResolveModeNdx][sampleCountNdx];

                            const TestConfig testConfig = {format,
                                                           imageData.width,
                                                           imageData.height,
                                                           1u,
                                                           1u,
                                                           0u,
                                                           imageData.renderArea,
                                                           aspectFlags,
                                                           sampleCount,
                                                           dResolve.flag,
                                                           sResolve.flag,
                                                           VB_STENCIL,
                                                           imageData.clearValue,
                                                           0.0f,
                                                           expectedValue,
                                                           useSeparateDepthStencilLayouts,
                                                           groupParams};
                            formatGroup->addChild(new DSResolveTestInstance(testCtx, testName, testConfig));
                        }
                    }
                }
                sampleGroup->addChild(formatGroup.release());
            }
        }
        group->addChild(sampleGroup.release());
    }
}

} // namespace

tcu::TestCaseGroup *createDynamicRenderingDepthStencilResolveTests(tcu::TestContext &testCtx,
                                                                   const SharedGroupParams groupParams)
{
    // Depth/stencil resolve tests
    return createTestGroup(testCtx, "depth_stencil_resolve", initTests, groupParams);
};

} // namespace renderpass
} // namespace vkt
