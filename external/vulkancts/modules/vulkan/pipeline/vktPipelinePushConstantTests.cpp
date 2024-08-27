/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 ARM Limited.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief PushConstant Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelinePushConstantTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"

#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

#include "deMemory.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

enum
{
    TRIANGLE_COUNT  = 2,
    MAX_RANGE_COUNT = 5
};

enum RangeSizeCase
{
    SIZE_CASE_4 = 0,
    SIZE_CASE_8,
    SIZE_CASE_12,
    SIZE_CASE_16,
    SIZE_CASE_32,
    SIZE_CASE_36,
    SIZE_CASE_48,
    SIZE_CASE_128,
    SIZE_CASE_256,
    SIZE_CASE_MAX,
    SIZE_CASE_UNSUPPORTED
};

enum CommandType
{
    CMD_BIND_PIPELINE_GRAPHICS = 0,
    CMD_BIND_PIPELINE_COMPUTE,
    CMD_PUSH_CONSTANT,
    CMD_DRAW,
    CMD_DISPATCH,
    CMD_UNSUPPORTED
};

enum PushConstantUseStageType
{
    PC_USE_STAGE_NONE   = 0x00000000,
    PC_USE_STAGE_VERTEX = VK_SHADER_STAGE_VERTEX_BIT,
    PC_USE_STAGE_TESC   = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    PC_USE_STAGE_TESE   = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    PC_USE_STAGE_GEOM   = VK_SHADER_STAGE_GEOMETRY_BIT,
    PC_USE_STAGE_FRAG   = VK_SHADER_STAGE_FRAGMENT_BIT,
    PC_USE_STAGE_ALL    = VK_SHADER_STAGE_ALL
};

typedef uint32_t PushConstantUseStage;

struct CommandData
{
    CommandType cType;
    int32_t rangeNdx;
};

struct PushConstantData
{
    struct PushConstantRange
    {
        VkShaderStageFlags shaderStage;
        uint32_t offset;
        uint32_t size;
    } range;
    struct PushConstantUpdate
    {
        uint32_t offset;
        uint32_t size;
    } update;
};

// These values will be loaded from push constants and used as an index
static const uint32_t DYNAMIC_VEC_INDEX = 2u;
static const uint32_t DYNAMIC_MAT_INDEX = 0u;
static const uint32_t DYNAMIC_ARR_INDEX = 3u;

// These reference values will be compared in the shader to ensure the correct index was read
static const float DYNAMIC_VEC_CONSTANT = 0.25f;
static const float DYNAMIC_MAT_CONSTANT = 0.50f;
static const float DYNAMIC_ARR_CONSTANT = 0.75f;

enum IndexType
{
    INDEX_TYPE_CONST_LITERAL = 0,
    INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR,

    INDEX_TYPE_LAST
};

enum ComputeTestType
{
    CTT_SIMPLE = 0,
    CTT_UNINITIALIZED,

    CTT_LAST
};

std::string getShaderStageNameStr(VkShaderStageFlags stageFlags)
{
    const VkShaderStageFlags shaderStages[] = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                               VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                               VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};

    const char *shaderStageNames[] = {
        "VK_SHADER_STAGE_VERTEX_BIT",
        "VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT",
        "VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT",
        "VK_SHADER_STAGE_GEOMETRY_BIT",
        "VK_SHADER_STAGE_FRAGMENT_BIT",
    };

    std::stringstream shaderStageStr;

    for (uint32_t stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(shaderStages); stageNdx++)
    {
        if (stageFlags & shaderStages[stageNdx])
        {
            if (!(shaderStageStr.str().empty()))
                shaderStageStr << " | ";

            shaderStageStr << shaderStageNames[stageNdx];
        }
    }

    return shaderStageStr.str();
}

std::vector<Vertex4RGBA> createQuad(const float size, const tcu::Vec4 &color)
{
    std::vector<Vertex4RGBA> vertices;

    const Vertex4RGBA lowerLeftVertex  = {tcu::Vec4(-size, -size, 0.0f, 1.0f), color};
    const Vertex4RGBA lowerRightVertex = {tcu::Vec4(size, -size, 0.0f, 1.0f), color};
    const Vertex4RGBA UpperLeftVertex  = {tcu::Vec4(-size, size, 0.0f, 1.0f), color};
    const Vertex4RGBA UpperRightVertex = {tcu::Vec4(size, size, 0.0f, 1.0f), color};

    vertices.push_back(lowerLeftVertex);
    vertices.push_back(lowerRightVertex);
    vertices.push_back(UpperLeftVertex);
    vertices.push_back(UpperLeftVertex);
    vertices.push_back(lowerRightVertex);
    vertices.push_back(UpperRightVertex);

    return vertices;
}

void pushConstants(const DeviceInterface &vk, VkCommandBuffer cmdBuffer, VkPipelineLayout layout,
                   VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *pValues,
                   bool pushConstant2)
{
    if (pushConstant2)
    {
#ifndef CTS_USES_VULKANSC
        vk::VkPushConstantsInfoKHR pushConstantInfo = {
            VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            layout,                                    // VkPipelineLayout layout;
            stageFlags,                                // VkShaderStageFlags stageFlags;
            offset,                                    // uint32_t offset;
            size,                                      // uint32_t size;
            pValues,                                   // const void* pValues;
        };
        vk.cmdPushConstants2KHR(cmdBuffer, &pushConstantInfo);
#endif
    }
    else
    {
        vk.cmdPushConstants(cmdBuffer, layout, stageFlags, offset, size, pValues);
    }
}

class PushConstantGraphicsTestInstance : public vkt::TestInstance
{
public:
    PushConstantGraphicsTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                     const uint32_t rangeCount,
                                     const PushConstantData pushConstantRange[MAX_RANGE_COUNT],
                                     const bool multipleUpdate, const IndexType indexType, const bool pushConstant2,
                                     const PushConstantUseStage pcUsedStage = PC_USE_STAGE_ALL,
                                     const bool sizeQueriedFromDevice       = false);
    virtual ~PushConstantGraphicsTestInstance(void);
    void init(void);
    virtual tcu::TestStatus iterate(void);
    virtual std::vector<VkPushConstantRange> getPushConstantRanges(void)                         = 0;
    virtual void updatePushConstants(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) = 0;
    virtual void setReferenceColor(tcu::Vec4 initColor)                                          = 0;
    void createShaderModule(const DeviceInterface &vk, VkDevice device, const BinaryCollection &programCollection,
                            const char *name, ShaderWrapper *module);
    tcu::TestStatus verifyImage(void);

protected:
    const PipelineConstructionType m_pipelineConstructionType;
    std::vector<Vertex4RGBA> m_vertices;
    const uint32_t m_rangeCount;
    PushConstantData m_pushConstantRange[MAX_RANGE_COUNT];
    const IndexType m_indexType;
    const PushConstantUseStage m_pcUsedStage;
    bool m_pushConstant2;
    bool m_sizeQueriedFromDevice;

private:
    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;
    const bool m_multipleUpdate;

    VkImageCreateInfo m_colorImageCreateInfo;
    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorAttachmentView;
    RenderPassWrapper m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModule;
    ShaderWrapper m_geometryShaderModule;
    ShaderWrapper m_tessControlShaderModule;
    ShaderWrapper m_tessEvaluationShaderModule;

    VkShaderStageFlags m_shaderFlags;
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStage;

    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    Move<VkBuffer> m_uniformBuffer;
    de::MovePtr<Allocation> m_uniformBufferAlloc;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorSet> m_descriptorSet;

    PipelineLayoutWrapper m_preRasterizationStatePipelineLayout;
    PipelineLayoutWrapper m_fragmentStatePipelineLayout;
    GraphicsPipelineWrapper m_graphicsPipeline;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

void PushConstantGraphicsTestInstance::createShaderModule(const DeviceInterface &vk, VkDevice device,
                                                          const BinaryCollection &programCollection, const char *name,
                                                          ShaderWrapper *module)
{
    *module = ShaderWrapper(vk, device, programCollection.get(name), 0);
}

PushConstantGraphicsTestInstance::PushConstantGraphicsTestInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, const uint32_t rangeCount,
    const PushConstantData pushConstantRange[MAX_RANGE_COUNT], bool multipleUpdate, IndexType indexType,
    const bool pushConstant2, const PushConstantUseStage pcUsedStage, const bool sizeQueriedFromDevice)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_rangeCount(rangeCount)
    , m_indexType(indexType)
    , m_pcUsedStage(pcUsedStage)
    , m_pushConstant2(pushConstant2)
    , m_sizeQueriedFromDevice(sizeQueriedFromDevice)
    , m_renderSize(32, 32)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_multipleUpdate(multipleUpdate)
    , m_shaderFlags(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
    , m_graphicsPipeline(m_context.getInstanceInterface(), m_context.getDeviceInterface(),
                         m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getDeviceExtensions(),
                         pipelineConstructionType)
{
    deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

void PushConstantGraphicsTestInstance::init(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    if (m_sizeQueriedFromDevice)
    {
        const VkPhysicalDeviceLimits &limits = m_context.getDeviceProperties().limits;
        m_pushConstantRange[0].range.size    = limits.maxPushConstantsSize;
        m_pushConstantRange[0].update.size   = limits.maxPushConstantsSize;
    }
    const std::vector<VkPushConstantRange> pushConstantRanges = getPushConstantRanges();
    bool useTessellation                                      = false;
    bool useGeometry                                          = false;

    // Create color image
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            nullptr,                                                               // const void* pNext;
            0u,                                                                    // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
            m_colorFormat,                                                         // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u},                              // VkExtent3D extent;
            1u,                                                                    // uint32_t mipLevels;
            1u,                                                                    // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            1u,                                                                    // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
        };

        m_colorImageCreateInfo = colorImageParams;
        m_colorImage           = createImage(vk, vkDevice, &m_colorImageCreateInfo);

        // Allocate and bind color image memory
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));
    }

    // Create color attachment view
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,    // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            0u,                                          // VkImageViewCreateFlags flags;
            *m_colorImage,                               // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                       // VkImageViewType viewType;
            m_colorFormat,                               // VkFormat format;
            componentMappingRGBA,                        // VkChannelMapping channels;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    // Create render pass
    m_renderPass = RenderPassWrapper(m_pipelineConstructionType, vk, vkDevice, m_colorFormat);

    // Create framebuffer
    {
        const VkImageView attachmentBindInfos[1] = {*m_colorAttachmentView};

        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            1u,                                        // uint32_t attachmentCount;
            attachmentBindInfos,                       // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
    }

    // Create pipeline layout
    {
        // create descriptor set layout
        m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                    .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                                    .build(vk, vkDevice);

        // create descriptor pool
        m_descriptorPool = DescriptorPoolBuilder()
                               .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
                               .build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        // create uniform buffer
        const VkBufferCreateInfo uniformBufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags    flags
            16u,                                  // VkDeviceSize size;
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_uniformBuffer      = createBuffer(vk, vkDevice, &uniformBufferCreateInfo);
        m_uniformBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_uniformBuffer),
                                                 MemoryRequirement::HostVisible);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_uniformBuffer, m_uniformBufferAlloc->getMemory(),
                                     m_uniformBufferAlloc->getOffset()));

        const tcu::Vec4 value = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
        deMemcpy(m_uniformBufferAlloc->getHostPtr(), &value, 16u);
        flushAlloc(vk, vkDevice, *m_uniformBufferAlloc);

        // create and update descriptor set
        const VkDescriptorSetAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                             sType;
            nullptr,                                        // const void*                                 pNext;
            *m_descriptorPool,         // VkDescriptorPool                            descriptorPool;
            1u,                        // uint32_t                                    setLayoutCount;
            &(*m_descriptorSetLayout), // const VkDescriptorSetLayout*                pSetLayouts;
        };
        m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &allocInfo);

        const VkDescriptorBufferInfo descriptorInfo =
            makeDescriptorBufferInfo(*m_uniformBuffer, (VkDeviceSize)0u, (VkDeviceSize)16u);

        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorInfo)
            .update(vk, vkDevice);

        // create pipeline layout
#ifndef CTS_USES_VULKANSC
        VkPipelineLayoutCreateFlags pipelineLayoutFlags =
            (vk::isConstructionTypeLibrary(m_pipelineConstructionType)) ?
                uint32_t(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT) :
                0u;
#else
        VkPipelineLayoutCreateFlags pipelineLayoutFlags = 0u;
#endif // CTS_USES_VULKANSC
        VkPipelineLayoutCreateInfo pipelineLayoutParams{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            pipelineLayoutFlags,                           // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t setLayoutCount;
            &(*m_descriptorSetLayout),                     // const VkDescriptorSetLayout* pSetLayouts;
            (uint32_t)pushConstantRanges.size(),           // uint32_t pushConstantRangeCount;
            &pushConstantRanges.front()                    // const VkPushConstantRange* pPushConstantRanges;
        };

        m_preRasterizationStatePipelineLayout =
            PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
        pipelineLayoutParams.setLayoutCount = 0u;
        pipelineLayoutParams.pSetLayouts    = nullptr;
        m_fragmentStatePipelineLayout =
            PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
    }

    // Create shaders
    {
        for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
        {
            if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_GEOMETRY_BIT)
            {
                m_shaderFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
            }
            if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
            {
                m_shaderFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            }
            if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
            {
                m_shaderFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            }
        }

        VkPhysicalDeviceFeatures features = m_context.getDeviceFeatures();

        createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_vert", &m_vertexShaderModule);
        if (m_shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
            m_shaderFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        {
            if (features.tessellationShader == VK_FALSE)
            {
                TCU_THROW(NotSupportedError, "Tessellation Not Supported");
            }
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_tesc", &m_tessControlShaderModule);
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_tese",
                               &m_tessEvaluationShaderModule);
            useTessellation = true;
        }
        if (m_shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
        {
            if (features.geometryShader == VK_FALSE)
            {
                TCU_THROW(NotSupportedError, "Geometry Not Supported");
            }
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_geom", &m_geometryShaderModule);
            useGeometry = true;
        }
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_frag", &m_fragmentShaderModule);
    }

    // Create pipeline
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBA),        // uint32_t strideInBytes;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate stepRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
            {
                0u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                0u                             // uint32_t offsetInBytes;
            },
            {
                1u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                offsetof(Vertex4RGBA, color),  // uint32_t offset;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // vkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t bindingCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t attributeCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPrimitiveTopology topology = (m_shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ?
                                                 VK_PRIMITIVE_TOPOLOGY_PATCH_LIST :
                                                 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
        const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

        uint32_t specializationData = m_pushConstantRange[0].range.size;

        const VkSpecializationMapEntry specializationMapEntry = {
            0,                          // uint32_t constantID
            0,                          // uint32_t offset
            sizeof(specializationData), // uint32_t size
        };
        const VkSpecializationInfo specializationInfo = {
            1,                          // uint32_t    mapEntryCount
            &specializationMapEntry,    // const void* pMapEntries
            sizeof(specializationData), // size_t      dataSize
            &specializationData         // const void* pData
        };

        m_graphicsPipeline.setMonolithicPipelineLayout(m_preRasterizationStatePipelineLayout)
            .setDefaultRasterizationState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultColorBlendState()
            .setDefaultTopology(topology)
            .setupVertexInputState(&vertexInputStateParams)
            .setupPreRasterizationShaderState(viewports, scissors, m_preRasterizationStatePipelineLayout, *m_renderPass,
                                              0u, m_vertexShaderModule, nullptr,
                                              useTessellation ? m_tessControlShaderModule : ShaderWrapper(),
                                              useTessellation ? m_tessEvaluationShaderModule : ShaderWrapper(),
                                              useGeometry ? m_geometryShaderModule : ShaderWrapper(),
                                              m_sizeQueriedFromDevice ? &specializationInfo : DE_NULL)
            .setupFragmentShaderState(m_fragmentStatePipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule)
            .setupFragmentOutputState(*m_renderPass)
            .buildPipeline();
    }

    // Create vertex buffer
    {
        m_vertices = createQuad(1.0f, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));

        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                    // VkStructureType sType;
            nullptr,                                                 // const void* pNext;
            0u,                                                      // VkBufferCreateFlags flags;
            (VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()), // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,                       // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                               // VkSharingMode sharingMode;
            1u,                                                      // uint32_t queueFamilyCount;
            &queueFamilyIndex                                        // const uint32_t* pQueueFamilyIndices;
        };

        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    {
        const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
                           attachmentClearValue);

        // Update push constant values
        updatePushConstants(*m_cmdBuffer, *m_preRasterizationStatePipelineLayout);

        // draw quad
        const VkDeviceSize triangleOffset = (m_vertices.size() / TRIANGLE_COUNT) * sizeof(Vertex4RGBA);

        for (int triangleNdx = 0; triangleNdx < TRIANGLE_COUNT; triangleNdx++)
        {
            VkDeviceSize vertexBufferOffset = triangleOffset * triangleNdx;

            if (m_multipleUpdate)
            {
                pushConstants(vk, *m_cmdBuffer, *m_preRasterizationStatePipelineLayout,
                              m_pushConstantRange[0].range.shaderStage, m_pushConstantRange[0].range.offset,
                              m_pushConstantRange[0].range.size, &triangleNdx, m_pushConstant2);
            }

            m_graphicsPipeline.bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     *m_preRasterizationStatePipelineLayout, 0, 1, &(*m_descriptorSet), 0, nullptr);

            vk.cmdDraw(*m_cmdBuffer, (uint32_t)(m_vertices.size() / TRIANGLE_COUNT), 1, 0, 0);
        }

        m_renderPass.end(vk, *m_cmdBuffer);
        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

PushConstantGraphicsTestInstance::~PushConstantGraphicsTestInstance(void)
{
}

tcu::TestStatus PushConstantGraphicsTestInstance::iterate(void)
{
    init();

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyImage();
}

tcu::TestStatus PushConstantGraphicsTestInstance::verifyImage(void)
{
    const tcu::TextureFormat tcuColorFormat = mapVkFormat(m_colorFormat);
    const tcu::TextureFormat tcuDepthFormat = tcu::TextureFormat();
    const ColorVertexShader vertexShader;
    const ColorFragmentShader fragmentShader(tcuColorFormat, tcuDepthFormat);
    const rr::Program program(&vertexShader, &fragmentShader);
    ReferenceRenderer refRenderer(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
    bool compareOk = false;

    // Render reference image
    {
        if (m_shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
        {
            m_vertices =
                createQuad(((m_pcUsedStage & PC_USE_STAGE_GEOM) ? 0.5f : 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
        }

        setReferenceColor(m_vertices[0].color);

        if (m_multipleUpdate)
        {
            for (size_t vertexNdx = 0; vertexNdx < 3; vertexNdx++)
            {
                m_vertices[vertexNdx].color.xyz() = tcu::Vec3(0.0f, 1.0f, 0.0f);
            }
            for (size_t vertexNdx = 3; vertexNdx < m_vertices.size(); vertexNdx++)
            {
                m_vertices[vertexNdx].color.xyz() = tcu::Vec3(0.0f, 0.0f, 1.0f);
            }
        }

        for (int triangleNdx = 0; triangleNdx < TRIANGLE_COUNT; triangleNdx++)
        {
            rr::RenderState renderState(refRenderer.getViewportState(),
                                        m_context.getDeviceProperties().limits.subPixelPrecisionBits);

            refRenderer.draw(renderState, rr::PRIMITIVETYPE_TRIANGLES,
                             std::vector<Vertex4RGBA>(m_vertices.begin() + triangleNdx * 3,
                                                      m_vertices.begin() + (triangleNdx + 1) * 3));
        }
    }

    // Compare result with reference image
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice vkDevice         = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::MovePtr<tcu::TextureLevel> result = readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator,
                                                                    *m_colorImage, m_colorFormat, m_renderSize);

        compareOk = tcu::intThresholdPositionDeviationCompare(
            m_context.getTestContext().getLog(), "IntImageCompare", "Image comparison", refRenderer.getAccess(),
            result->getAccess(), tcu::UVec4(2, 2, 2, 2), tcu::IVec3(1, 1, 0), true, tcu::COMPARE_LOG_RESULT);
    }

    if (compareOk)
        return tcu::TestStatus::pass("Result image matches reference");
    else
        return tcu::TestStatus::fail("Image mismatch");
}

class PushConstantGraphicsDisjointInstance : public PushConstantGraphicsTestInstance
{
public:
    PushConstantGraphicsDisjointInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                         const uint32_t rangeCount,
                                         const PushConstantData pushConstantRange[MAX_RANGE_COUNT],
                                         const bool multipleUpdate, const IndexType indexType, const bool pushConstant2,
                                         const PushConstantUseStage pcUsedStage = PC_USE_STAGE_ALL,
                                         const bool sizeQueriedFromDevice       = false);
    virtual ~PushConstantGraphicsDisjointInstance(void);
    std::vector<VkPushConstantRange> getPushConstantRanges(void);
    void updatePushConstants(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);
    void setReferenceColor(tcu::Vec4 initColor);
};

PushConstantGraphicsDisjointInstance::PushConstantGraphicsDisjointInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, const uint32_t rangeCount,
    const PushConstantData pushConstantRange[MAX_RANGE_COUNT], const bool multipleUpdate, const IndexType indexType,
    const bool pushConstant2, const PushConstantUseStage pcUsedStage, const bool sizeQueriedFromDevice)
    : PushConstantGraphicsTestInstance(context, pipelineConstructionType, rangeCount, pushConstantRange, multipleUpdate,
                                       indexType, pushConstant2, pcUsedStage, sizeQueriedFromDevice)
{
    deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

PushConstantGraphicsDisjointInstance::~PushConstantGraphicsDisjointInstance(void)
{
}

std::vector<VkPushConstantRange> PushConstantGraphicsDisjointInstance::getPushConstantRanges(void)
{
    std::vector<VkPushConstantRange> pushConstantRanges;

    for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
    {
        const VkPushConstantRange pushConstantRange = {m_pushConstantRange[rangeNdx].range.shaderStage,
                                                       m_pushConstantRange[rangeNdx].range.offset,
                                                       m_pushConstantRange[rangeNdx].range.size};

        pushConstantRanges.push_back(pushConstantRange);
    }

    return pushConstantRanges;
}

void PushConstantGraphicsDisjointInstance::updatePushConstants(VkCommandBuffer cmdBuffer,
                                                               VkPipelineLayout pipelineLayout)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    std::vector<tcu::Vec4> color(m_sizeQueriedFromDevice ? ((m_pushConstantRange[0].range.size + 12) / 16) : 16,
                                 tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    std::vector<tcu::Vec4> allOnes(8, tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    switch (m_indexType)
    {
    case INDEX_TYPE_CONST_LITERAL:
        // Do nothing
        break;
    case INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR:
        // Stick our dynamic index at the beginning of a vector
        color[0] = tcu::Vec4(float(DYNAMIC_VEC_INDEX), float(DYNAMIC_MAT_INDEX), float(DYNAMIC_ARR_INDEX), 1.0f);

        // Place our reference values at each type offset

        // vec4[i]
        DE_ASSERT(DYNAMIC_VEC_INDEX <= 3);
        color[1]                    = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        color[1][DYNAMIC_VEC_INDEX] = DYNAMIC_VEC_CONSTANT;

        // mat2[i][0]
        DE_ASSERT(DYNAMIC_MAT_INDEX <= 1);
        color[2]                        = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        color[2][DYNAMIC_MAT_INDEX * 2] = DYNAMIC_MAT_CONSTANT;

        // float[i]
        DE_ASSERT(DYNAMIC_ARR_INDEX <= 3);
        color[3]                    = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        color[3][DYNAMIC_ARR_INDEX] = DYNAMIC_ARR_CONSTANT;
        break;
    default:
        DE_FATAL("Unhandled IndexType");
        break;
    }

    const uint32_t kind = 2u;
    const void *value   = nullptr;

    for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
    {
        value = (m_pushConstantRange[rangeNdx].range.size == 4u) ? (void *)(&kind) : (void *)(&color[0]);

        pushConstants(vk, cmdBuffer, pipelineLayout, m_pushConstantRange[rangeNdx].range.shaderStage,
                      m_pushConstantRange[rangeNdx].range.offset, m_pushConstantRange[rangeNdx].range.size, value,
                      m_pushConstant2);

        if (m_pushConstantRange[rangeNdx].update.size < m_pushConstantRange[rangeNdx].range.size)
        {
            value = (void *)(&allOnes[0]);
            pushConstants(vk, cmdBuffer, pipelineLayout, m_pushConstantRange[rangeNdx].range.shaderStage,
                          m_pushConstantRange[rangeNdx].update.offset, m_pushConstantRange[rangeNdx].update.size, value,
                          m_pushConstant2);
        }
    }
}

void PushConstantGraphicsDisjointInstance::setReferenceColor(tcu::Vec4 initColor)
{
    DE_UNREF(initColor);

    const tcu::Vec4 color = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);

    for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
    {
        if (m_pushConstantRange[rangeNdx].update.size < m_pushConstantRange[rangeNdx].range.size)
        {
            for (size_t vertexNdx = 0; vertexNdx < m_vertices.size(); vertexNdx++)
            {
                m_vertices[vertexNdx].color.xyzw() = color;
            }
        }
    }
}

class PushConstantGraphicsOverlapTestInstance : public PushConstantGraphicsTestInstance
{
public:
    PushConstantGraphicsOverlapTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                            const uint32_t rangeCount,
                                            const PushConstantData pushConstantRange[MAX_RANGE_COUNT],
                                            const bool multipleUpdate, const IndexType indexType,
                                            const bool pushConstant2,
                                            const PushConstantUseStage pcUsedStage = PC_USE_STAGE_ALL);
    virtual ~PushConstantGraphicsOverlapTestInstance(void);
    std::vector<VkPushConstantRange> getPushConstantRanges(void);
    std::vector<VkPushConstantRange> getPushConstantUpdates(void);
    void updatePushConstants(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);
    void setReferenceColor(tcu::Vec4 initColor);

private:
    const std::vector<float> m_colorData;
    std::vector<float> m_referenceData;
};

std::vector<float> generateColorData(uint32_t numBytes)
{
    DE_ASSERT(numBytes % 4u == 0u);

    std::vector<float> colorData;

    deRandom random;
    deRandom_init(&random, numBytes);

    for (uint32_t elementNdx = 0u; elementNdx < numBytes / 4u; elementNdx++)
        colorData.push_back(deRandom_getFloat(&random));

    return colorData;
}

PushConstantGraphicsOverlapTestInstance::PushConstantGraphicsOverlapTestInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, const uint32_t rangeCount,
    const PushConstantData pushConstantRange[MAX_RANGE_COUNT], const bool multipleUpdate, const IndexType indexType,
    const bool pushConstant2, const PushConstantUseStage pcUsedStage)
    : PushConstantGraphicsTestInstance(context, pipelineConstructionType, rangeCount, pushConstantRange, multipleUpdate,
                                       indexType, pushConstant2, pcUsedStage)
    , m_colorData(generateColorData(256u))
{
    deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

PushConstantGraphicsOverlapTestInstance::~PushConstantGraphicsOverlapTestInstance(void)
{
}

std::vector<VkPushConstantRange> PushConstantGraphicsOverlapTestInstance::getPushConstantRanges(void)
{
    // Find push constant ranges for each shader stage
    const VkShaderStageFlags shaderStages[] = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    std::vector<VkPushConstantRange> pushConstantRanges;

    m_context.getTestContext().getLog() << tcu::TestLog::Section("Ranges", "Push constant ranges");

    for (uint32_t stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(shaderStages); stageNdx++)
    {
        uint32_t firstByte = ~0u;
        uint32_t lastByte  = 0u;

        for (uint32_t rangeNdx = 0u; rangeNdx < m_rangeCount; rangeNdx++)
        {
            if (m_pushConstantRange[rangeNdx].range.shaderStage & shaderStages[stageNdx])
            {
                firstByte = deMinu32(firstByte, m_pushConstantRange[rangeNdx].range.offset);
                lastByte  = deMaxu32(lastByte, m_pushConstantRange[rangeNdx].range.offset +
                                                   m_pushConstantRange[rangeNdx].range.size);
            }
        }

        if (firstByte != ~0u)
        {
            const VkPushConstantRange pushConstantRange = {
                shaderStages[stageNdx], // VkShaderStageFlags    stageFlags
                firstByte,              // uint32_t              offset
                lastByte - firstByte    // uint32_t              size
            };

            pushConstantRanges.push_back(pushConstantRange);

            m_context.getTestContext().getLog()
                << tcu::TestLog::Message << "VkShaderStageFlags    stageFlags    "
                << getShaderStageNameStr(shaderStages[stageNdx]) << ",\n"
                << "uint32_t              offset        " << pushConstantRange.offset << ",\n"
                << "uint32_t              size          " << pushConstantRange.size << "\n"
                << tcu::TestLog::EndMessage;
        }
    }

    m_context.getTestContext().getLog() << tcu::TestLog::EndSection;

    return pushConstantRanges;
}

std::vector<VkPushConstantRange> PushConstantGraphicsOverlapTestInstance::getPushConstantUpdates(void)
{
    VkShaderStageFlags lastStageFlags = (VkShaderStageFlags)~0u;
    std::vector<VkPushConstantRange> pushConstantUpdates;

    // Find matching shader stages for every 4 byte chunk
    for (uint32_t offset = 0u; offset < 128u; offset += 4u)
    {
        VkShaderStageFlags stageFlags = (VkShaderStageFlags)0u;
        bool updateRange              = false;

        // For each byte in the range specified by offset and size and for each push constant range that overlaps that byte,
        // stageFlags must include all stages in that push constant range's VkPushConstantRange::stageFlags
        for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
        {
            const uint32_t rangeStart = m_pushConstantRange[rangeNdx].range.offset;
            const uint32_t rangeEnd   = rangeStart + m_pushConstantRange[rangeNdx].range.size;

            const uint32_t updateStart = m_pushConstantRange[rangeNdx].update.offset;
            const uint32_t updateEnd   = updateStart + m_pushConstantRange[rangeNdx].update.size;

            updateRange |= (updateStart <= offset && updateEnd >= offset + 4u);

            DE_ASSERT(rangeEnd <= 128u);

            if (rangeStart <= offset && rangeEnd >= offset + 4u)
                stageFlags |= m_pushConstantRange[rangeNdx].range.shaderStage;
        }

        // Skip chunks with no updates
        if (!stageFlags || !updateRange)
            continue;

        // Add new update entry
        if (stageFlags != lastStageFlags)
        {
            const VkPushConstantRange update = {
                stageFlags, // VkShaderStageFlags    stageFlags;
                offset,     // uint32_t              offset;
                4u          // uint32_t              size;
            };

            pushConstantUpdates.push_back(update);
            lastStageFlags = stageFlags;
        }
        // Increase current update entry size
        else
        {
            DE_ASSERT(pushConstantUpdates.size() > 0u);
            pushConstantUpdates.back().size += 4u;
        }
    }

    return pushConstantUpdates;
}

void PushConstantGraphicsOverlapTestInstance::updatePushConstants(VkCommandBuffer cmdBuffer,
                                                                  VkPipelineLayout pipelineLayout)
{
    const DeviceInterface &vk                                  = m_context.getDeviceInterface();
    const std::vector<VkPushConstantRange> pushConstantUpdates = getPushConstantUpdates();

    m_referenceData.resize(m_colorData.size(), 0.0f);

    m_context.getTestContext().getLog() << tcu::TestLog::Section("Updates", "Push constant updates");

    for (uint32_t pushNdx = 0u; pushNdx < pushConstantUpdates.size(); pushNdx++)
    {
        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << "VkShaderStageFlags    stageFlags    "
            << getShaderStageNameStr(pushConstantUpdates[pushNdx].stageFlags) << ",\n"
            << "uint32_t              offset        " << pushConstantUpdates[pushNdx].offset << ",\n"
            << "uint32_t              size          " << pushConstantUpdates[pushNdx].size << ",\n"
            << "const void*           pValues       " << &m_colorData[pushConstantUpdates[pushNdx].offset / 2u] << "\n"
            << tcu::TestLog::EndMessage;

        pushConstants(vk, cmdBuffer, pipelineLayout, pushConstantUpdates[pushNdx].stageFlags,
                      pushConstantUpdates[pushNdx].offset, pushConstantUpdates[pushNdx].size,
                      &m_colorData[pushConstantUpdates[pushNdx].offset / 2u], m_pushConstant2);

        // Copy push constant values to reference buffer
        DE_ASSERT((pushConstantUpdates[pushNdx].offset / 2u + pushConstantUpdates[pushNdx].size) <
                  4u * m_colorData.size());

        if (m_pcUsedStage & pushConstantUpdates[pushNdx].stageFlags)
            deMemcpy(&m_referenceData.at(pushConstantUpdates[pushNdx].offset / 4u),
                     &m_colorData.at(pushConstantUpdates[pushNdx].offset / 2u), pushConstantUpdates[pushNdx].size);
    }

    m_context.getTestContext().getLog() << tcu::TestLog::EndSection;
}

void PushConstantGraphicsOverlapTestInstance::setReferenceColor(tcu::Vec4 initColor)
{
    tcu::Vec4 expectedColor = initColor;

    // Calculate reference color
    for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
    {
        const uint32_t offset = m_pushConstantRange[rangeNdx].range.offset / 4u;
        const uint32_t size   = m_pushConstantRange[rangeNdx].range.size / 4u;

        if (m_pcUsedStage & m_pushConstantRange[rangeNdx].range.shaderStage)
        {
            const uint32_t numComponents = (size < 4u) ? size : 4u;
            const uint32_t colorNdx      = (offset + size - numComponents);

            for (uint32_t componentNdx = 0u; componentNdx < numComponents; componentNdx++)
                expectedColor[componentNdx] += m_referenceData[colorNdx + componentNdx];
        }
    }

    expectedColor = tcu::min(tcu::mod(expectedColor, tcu::Vec4(2.0f)), 2.0f - tcu::mod(expectedColor, tcu::Vec4(2.0f)));

    for (size_t vertexNdx = 0; vertexNdx < m_vertices.size(); vertexNdx++)
    {
        m_vertices[vertexNdx].color.xyzw() = expectedColor;
    }
}

class PushConstantGraphicsTest : public vkt::TestCase
{
public:
    PushConstantGraphicsTest(tcu::TestContext &testContext, const std::string &name,
                             const PipelineConstructionType pipelineConstructionType, const uint32_t rangeCount,
                             const PushConstantData pushConstantRange[MAX_RANGE_COUNT], const bool multipleUpdate,
                             const IndexType indexType, const bool pushConstant2,
                             const PushConstantUseStage pcUsedStage = PC_USE_STAGE_ALL,
                             const bool sizeQueriedFromDevice       = false);
    virtual ~PushConstantGraphicsTest(void);

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const = 0;
    virtual TestInstance *createInstance(Context &context) const          = 0;
    RangeSizeCase getRangeSizeCase(uint32_t rangeSize) const;

protected:
    const PipelineConstructionType m_pipelineConstructionType;
    const uint32_t m_rangeCount;
    PushConstantData m_pushConstantRange[MAX_RANGE_COUNT];
    const bool m_multipleUpdate;
    const IndexType m_indexType;
    const bool m_pushConstant2;
    const PushConstantUseStage m_pcUsedStage;
    const bool m_sizeQueriedFromDevice;
};

PushConstantGraphicsTest::PushConstantGraphicsTest(tcu::TestContext &testContext, const std::string &name,
                                                   const PipelineConstructionType pipelineConstructionType,
                                                   const uint32_t rangeCount,
                                                   const PushConstantData pushConstantRange[MAX_RANGE_COUNT],
                                                   const bool multipleUpdate, const IndexType indexType,
                                                   const bool pushConstant2, const PushConstantUseStage pcUsedStage,
                                                   const bool sizeQueriedFromDevice)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_rangeCount(rangeCount)
    , m_multipleUpdate(multipleUpdate)
    , m_indexType(indexType)
    , m_pushConstant2(pushConstant2)
    , m_pcUsedStage(pcUsedStage)
    , m_sizeQueriedFromDevice(sizeQueriedFromDevice)
{
    deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

PushConstantGraphicsTest::~PushConstantGraphicsTest(void)
{
}

void PushConstantGraphicsTest::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);

    if (m_pushConstant2)
        context.requireDeviceFunctionality("VK_KHR_maintenance6");

    const VkPhysicalDeviceLimits &limits = context.getDeviceProperties().limits;

    if (!m_sizeQueriedFromDevice)
    {
        for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
        {
            if (m_pushConstantRange[rangeNdx].range.size > limits.maxPushConstantsSize)
            {
                TCU_THROW(NotSupportedError,
                          "PushConstant size " + std::to_string(m_pushConstantRange[rangeNdx].range.size) +
                              " exceeds device limit " + std::to_string(limits.maxPushConstantsSize));
            }
        }
    }
}

RangeSizeCase PushConstantGraphicsTest::getRangeSizeCase(uint32_t rangeSize) const
{
    if (m_sizeQueriedFromDevice)
    {
        return SIZE_CASE_MAX;
    }

    switch (rangeSize)
    {
    case 8:
        return SIZE_CASE_8;
    case 4:
        return SIZE_CASE_4;
    case 12:
        return SIZE_CASE_12;
    case 16:
        return SIZE_CASE_16;
    case 32:
        return SIZE_CASE_32;
    case 36:
        return SIZE_CASE_36;
    case 48:
        return SIZE_CASE_48;
    case 128:
        return SIZE_CASE_128;
    case 256:
        return SIZE_CASE_256;
    default:
        DE_FATAL("Range size unsupported yet");
        return SIZE_CASE_UNSUPPORTED;
    }
}

class PushConstantGraphicsDisjointTest : public PushConstantGraphicsTest
{
public:
    PushConstantGraphicsDisjointTest(tcu::TestContext &testContext, const std::string &name,
                                     const PipelineConstructionType pipelineConstructionType, const uint32_t rangeCount,
                                     const PushConstantData pushConstantRange[MAX_RANGE_COUNT],
                                     const bool multipleUpdate, const IndexType indexType, const bool pushConstant2,
                                     const PushConstantUseStage pcUseStage = PC_USE_STAGE_ALL,
                                     const bool sizeQueriedFromDevice      = false);
    virtual ~PushConstantGraphicsDisjointTest(void);

    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
};

PushConstantGraphicsDisjointTest::PushConstantGraphicsDisjointTest(
    tcu::TestContext &testContext, const std::string &name, const PipelineConstructionType pipelineConstructionType,
    const uint32_t rangeCount, const PushConstantData pushConstantRange[MAX_RANGE_COUNT], const bool multipleUpdate,
    const IndexType indexType, const bool pushConstant2, const PushConstantUseStage pcUseStage,
    const bool sizeQueriedFromDevice)
    : PushConstantGraphicsTest(testContext, name, pipelineConstructionType, rangeCount, pushConstantRange,
                               multipleUpdate, indexType, pushConstant2, pcUseStage, sizeQueriedFromDevice)
{
}

PushConstantGraphicsDisjointTest::~PushConstantGraphicsDisjointTest(void)
{
}

void PushConstantGraphicsDisjointTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream vertexSrc;
    std::ostringstream fragmentSrc;
    std::ostringstream geometrySrc;
    std::ostringstream tessControlSrc;
    std::ostringstream tessEvaluationSrc;

    for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
    {
        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_VERTEX_BIT)
        {
            vertexSrc << "#version 450\n"
                      << "layout(location = 0) in highp vec4 position;\n"
                      << "layout(location = 1) in highp vec4 color;\n"
                      << "layout(location = 0) out highp vec4 vtxColor;\n"
                      << "out gl_PerVertex { vec4 gl_Position; };\n";

            if (m_pcUsedStage & PC_USE_STAGE_VERTEX)
            {
                if (m_sizeQueriedFromDevice)
                {
                    vertexSrc << "layout (constant_id = 0) const uint MaxPushConstantSize = 128;\n";
                }

                vertexSrc << "layout(push_constant) uniform Material {\n";

                switch (m_indexType)
                {
                case INDEX_TYPE_CONST_LITERAL:
                    switch (getRangeSizeCase(m_pushConstantRange[rangeNdx].range.size))
                    {
                    case SIZE_CASE_4:
                        vertexSrc << "int kind;\n"
                                  << "} matInst;\n";
                        break;
                    case SIZE_CASE_16:
                        vertexSrc << "vec4 color;\n"
                                  << "} matInst;\n"
                                  << "layout(std140, binding = 0) uniform UniformBuf {\n"
                                  << "vec4 element;\n"
                                  << "} uniformBuf;\n";
                        break;
                    case SIZE_CASE_32:
                        vertexSrc << "vec4 color[2];\n"
                                  << "} matInst;\n";
                        break;
                    case SIZE_CASE_48:
                        vertexSrc << "int unused1;\n"
                                  << "vec4 unused2;\n"
                                  << "vec4 color;\n"
                                  << "} matInst;\n";
                        break;
                    case SIZE_CASE_128:
                        vertexSrc << "vec4 color[8];\n"
                                  << "} matInst;\n";
                        break;
                    case SIZE_CASE_256:
                        vertexSrc << "vec4 color[16];\n"
                                  << "} matInst;\n";
                        break;
                    case SIZE_CASE_MAX:
                        vertexSrc << "vec4 color[(MaxPushConstantSize + 12) / 16];\n"
                                  << "} matInst;\n";
                        break;
                    default:
                        DE_FATAL("Not implemented yet");
                        break;
                    }
                    break;
                case INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR:
                    vertexSrc << "    layout(offset = 0)  vec4 index; \n"
                              << "    layout(offset = 16) vec4 vecType; \n"
                              << "    layout(offset = 32) mat2 matType; \n"
                              << "    layout(offset = 48) float[4] arrType; \n"
                              << "} matInst;\n";
                    break;
                default:
                    DE_FATAL("Unhandled IndexType");
                    break;
                }
            }

            vertexSrc << "void main()\n"
                      << "{\n"
                      << "    gl_Position = position;\n";

            if (m_pcUsedStage & PC_USE_STAGE_VERTEX)
            {
                switch (m_indexType)
                {
                case INDEX_TYPE_CONST_LITERAL:
                    switch (getRangeSizeCase(m_pushConstantRange[rangeNdx].range.size))
                    {
                    case SIZE_CASE_4:
                        vertexSrc << "switch (matInst.kind) {\n"
                                  << "case 0: vtxColor = vec4(0.0, 1.0, 0, 1.0); break;\n"
                                  << "case 1: vtxColor = vec4(0.0, 0.0, 1.0, 1.0); break;\n"
                                  << "case 2: vtxColor = vec4(1.0, 0.0, 0, 1.0); break;\n"
                                  << "default: vtxColor = color; break;}\n"
                                  << "}\n";
                        break;
                    case SIZE_CASE_16:
                        vertexSrc << "vtxColor = (matInst.color + uniformBuf.element) * 0.5;\n"
                                  << "}\n";
                        break;
                    case SIZE_CASE_32:
                        vertexSrc << "vtxColor = (matInst.color[0] + matInst.color[1]) * 0.5;\n"
                                  << "}\n";
                        break;
                    case SIZE_CASE_48:
                        vertexSrc << "vtxColor = matInst.color;\n"
                                  << "}\n";
                        break;
                    case SIZE_CASE_128:
                        vertexSrc << "vec4 color = vec4(0.0, 0, 0, 0.0);\n"
                                  << "for (int i = 0; i < 8; i++)\n"
                                  << "{\n"
                                  << "  color = color + matInst.color[i];\n"
                                  << "}\n"
                                  << "vtxColor = color * 0.125;\n"
                                  << "}\n";
                        break;
                    case SIZE_CASE_256:
                        vertexSrc << "vec4 color = vec4(0.0, 0, 0, 0.0);\n"
                                  << "for (int i = 0; i < 16; i++)\n"
                                  << "{\n"
                                  << "  color = color + matInst.color[i];\n"
                                  << "}\n"
                                  << "vtxColor = color * 0.0625;\n"
                                  << "}\n";
                        break;
                    case SIZE_CASE_MAX:
                        vertexSrc << "vec4 color = vec4(0.0, 0, 0, 0.0);\n"
                                  << "for (int i = 0; i < (MaxPushConstantSize / 16); i++)\n"
                                  << "{\n"
                                  << "  color = color + matInst.color[i];\n"
                                  << "}\n"
                                  << "for (int i = 0; i < ((MaxPushConstantSize % 16) / 4); i++)\n"
                                  << "{\n"
                                  << "  switch (i) {\n"
                                  << "  case 0: if (matInst.color[(MaxPushConstantSize + 12) / 16 - 1].x != 1.0) color "
                                     "= vec4(0.0); break;\n"
                                  << "  case 1: if (matInst.color[(MaxPushConstantSize + 12) / 16 - 1].y != 0.0) color "
                                     "= vec4(0.0); break;\n"
                                  << "  case 2: if (matInst.color[(MaxPushConstantSize + 12) / 16 - 1].z != 0.0) color "
                                     "= vec4(0.0); break;\n"
                                  << "  default: break;}\n"
                                  << "}\n"
                                  << "vtxColor = color / (MaxPushConstantSize / 16);\n"
                                  << "}\n";
                        break;
                    default:
                        DE_FATAL("Not implemented yet");
                        break;
                    }
                    break;
                case INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR:
                {
                    vertexSrc << "    vtxColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                              // Mix in gl_Position to (hopefully) prevent optimizing our index away
                              << "    int vec_selector = int(abs(gl_Position.x) * 0.0000001 + 0);\n"
                              << "    int mat_selector = int(abs(gl_Position.x) * 0.0000001 + 1);\n"
                              << "    int arr_selector = int(abs(gl_Position.x) * 0.0000001 + 2);\n";

                    // Use the dynamic index to pull our real index value from push constants
                    // Then use that value to index into three variable types
                    std::string vecValue = "matInst.vecType[int(matInst.index[vec_selector])]";
                    std::string matValue = "matInst.matType[int(matInst.index[mat_selector])][0]";
                    std::string arrValue = "matInst.arrType[int(matInst.index[arr_selector])]";

                    // Test vector indexing
                    vertexSrc << "    if (" << vecValue << " != " << DYNAMIC_VEC_CONSTANT << ")\n"
                              << "        vtxColor += vec4(0.0, 0.5, 0.0, 1.0);\n";

                    // Test matrix indexing
                    vertexSrc << "    if (" << matValue << " != " << DYNAMIC_MAT_CONSTANT << ")\n"
                              << "        vtxColor += vec4(0.0, 0.0, 0.5, 1.0);\n";

                    // Test array indexing
                    vertexSrc << "    if (" << arrValue << " != " << DYNAMIC_ARR_CONSTANT << ")\n"
                              << "        vtxColor = vec4(0.0, 0.5, 0.5, 1.0);\n";

                    vertexSrc << "}\n";
                }
                break;
                default:
                    DE_FATAL("Unhandled IndexType");
                    break;
                }
            }
            else
            {
                vertexSrc << "    vtxColor = color;\n";
                vertexSrc << "}\n";
            }
            sourceCollections.glslSources.add("color_vert") << glu::VertexSource(vertexSrc.str());
        }

        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        {
            tessControlSrc << "#version 450\n"
                           << "layout (vertices = 3) out;\n";

            if (m_pcUsedStage & PC_USE_STAGE_TESC)
            {
                tessControlSrc << "layout(push_constant) uniform TessLevel {\n"
                               << "    layout(offset = 24) int level;\n"
                               << "} tessLevel;\n";
            }

            tessControlSrc << "layout(location = 0) in highp vec4 color[];\n"
                           << "layout(location = 0) out highp vec4 vtxColor[];\n"
                           << "in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
                           << "out gl_PerVertex { vec4 gl_Position; } gl_out[];\n"
                           << "void main()\n"
                           << "{\n"
                           << "  gl_TessLevelInner[0] "
                           << ((m_pcUsedStage & PC_USE_STAGE_TESC) ? "= tessLevel.level;\n" : "= 2.0;\n")
                           << "  gl_TessLevelOuter[0] "
                           << ((m_pcUsedStage & PC_USE_STAGE_TESC) ? "= tessLevel.level;\n" : "= 2.0;\n")
                           << "  gl_TessLevelOuter[1] "
                           << ((m_pcUsedStage & PC_USE_STAGE_TESC) ? "= tessLevel.level;\n" : "= 2.0;\n")
                           << "  gl_TessLevelOuter[2] "
                           << ((m_pcUsedStage & PC_USE_STAGE_TESC) ? "= tessLevel.level;\n" : "= 2.0;\n")
                           << "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                           << "  vtxColor[gl_InvocationID] = color[gl_InvocationID];\n"
                           << "}\n";

            sourceCollections.glslSources.add("color_tesc") << glu::TessellationControlSource(tessControlSrc.str());
        }

        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        {
            tessEvaluationSrc << "#version 450\n"
                              << "layout (triangles) in;\n";

            if (m_pcUsedStage & PC_USE_STAGE_TESE)
            {
                tessEvaluationSrc << "layout(push_constant) uniform Material {\n"
                                  << "    layout(offset = 32) vec4 color;\n"
                                  << "} matInst;\n";
            }
            tessEvaluationSrc << "layout(location = 0) in highp vec4 color[];\n"
                              << "layout(location = 0) out highp vec4 vtxColor;\n"
                              << "in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
                              << "out gl_PerVertex { vec4 gl_Position; };\n"
                              << "void main()\n"
                              << "{\n"
                              << "  gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * "
                                 "gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position;\n";

            if (m_pcUsedStage & PC_USE_STAGE_TESE)
                tessEvaluationSrc << "  vtxColor = matInst.color;\n";
            else
                tessEvaluationSrc << "  vtxColor = gl_TessCoord.x * color[0] + gl_TessCoord.y * color[1] + "
                                     "gl_TessCoord.z * color[2];\n";

            tessEvaluationSrc << "}\n";

            sourceCollections.glslSources.add("color_tese")
                << glu::TessellationEvaluationSource(tessEvaluationSrc.str());
        }

        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_GEOMETRY_BIT)
        {
            geometrySrc << "#version 450\n"
                        << "layout(triangles) in;\n"
                        << "layout(triangle_strip, max_vertices=3) out;\n";

            if (m_pcUsedStage & PC_USE_STAGE_GEOM)
            {
                geometrySrc << "layout(push_constant) uniform Material {\n"
                            << "    layout(offset = 20) int kind;\n"
                            << "} matInst;\n";
            }

            geometrySrc << "layout(location = 0) in highp vec4 color[];\n"
                        << "layout(location = 0) out highp vec4 vtxColor;\n"
                        << "in gl_PerVertex { vec4 gl_Position; } gl_in[];\n"
                        << "out gl_PerVertex { vec4 gl_Position; };\n"
                        << "void main()\n"
                        << "{\n"
                        << "  for(int i=0; i<3; i++)\n"
                        << "  {\n";

            if (m_pcUsedStage & PC_USE_STAGE_GEOM)
                geometrySrc << "    gl_Position.xyz = gl_in[i].gl_Position.xyz / matInst.kind;\n";
            else
                geometrySrc << "    gl_Position.xyz = gl_in[i].gl_Position.xyz;\n";

            geometrySrc << "    gl_Position.w = gl_in[i].gl_Position.w;\n"
                        << "    vtxColor = color[i];\n"
                        << "    EmitVertex();\n"
                        << "  }\n"
                        << "  EndPrimitive();\n"
                        << "}\n";

            sourceCollections.glslSources.add("color_geom") << glu::GeometrySource(geometrySrc.str());
        }

        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            fragmentSrc << "#version 450\n"
                        << "layout(location = 0) in highp vec4 vtxColor;\n"
                        << "layout(location = 0) out highp vec4 fragColor;\n";

            if (m_pcUsedStage & PC_USE_STAGE_FRAG)
            {
                fragmentSrc << "layout(push_constant) uniform Material {\n";

                switch (m_indexType)
                {
                case INDEX_TYPE_CONST_LITERAL:
                    if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_VERTEX_BIT)
                    {
                        fragmentSrc << "    layout(offset = 0) int kind; \n"
                                    << "} matInst;\n";
                    }
                    else
                    {
                        fragmentSrc << "    layout(offset = 16) int kind;\n"
                                    << "} matInst;\n";
                    }

                    fragmentSrc << "void main (void)\n"
                                << "{\n"
                                << "    switch (matInst.kind) {\n"
                                << "    case 0: fragColor = vec4(0, 1.0, 0, 1.0); break;\n"
                                << "    case 1: fragColor = vec4(0, 0.0, 1.0, 1.0); break;\n"
                                << "    case 2: fragColor = vtxColor; break;\n"
                                << "    default: fragColor = vec4(1.0, 1.0, 1.0, 1.0); break;}\n"
                                << "}\n";
                    break;
                case INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR:
                {
                    fragmentSrc << "    layout(offset = 0)  vec4 index; \n"
                                << "    layout(offset = 16) vec4 vecType; \n"
                                << "    layout(offset = 32) mat2 matType; \n"
                                << "    layout(offset = 48) float[4] arrType; \n"
                                << "} matInst;\n";

                    fragmentSrc << "void main (void)\n"
                                << "{\n"
                                << "    fragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"

                                // Mix in gl_FragCoord to (hopefully) prevent optimizing our index away
                                << "    int vec_selector = int(gl_FragCoord.x * 0.0000001 + 0);\n"
                                << "    int mat_selector = int(gl_FragCoord.x * 0.0000001 + 1);\n"
                                << "    int arr_selector = int(gl_FragCoord.x * 0.0000001 + 2);\n";

                    // Use the dynamic index to pull our real index value from push constants
                    // Then use that value to index into three variable types
                    std::string vecValue = "matInst.vecType[int(matInst.index[vec_selector])]";
                    std::string matValue = "matInst.matType[int(matInst.index[mat_selector])][0]";
                    std::string arrValue = "matInst.arrType[int(matInst.index[arr_selector])]";

                    // Test vector indexing
                    fragmentSrc << "    if (" << vecValue << " != " << DYNAMIC_VEC_CONSTANT << ")\n"
                                << "        fragColor += vec4(0.0, 0.5, 0.0, 1.0);\n";

                    // Test matrix indexing
                    fragmentSrc << "    if (" << matValue << " != " << DYNAMIC_MAT_CONSTANT << ")\n"
                                << "        fragColor += vec4(0.0, 0.0, 0.5, 1.0);\n";

                    // Test array indexing
                    fragmentSrc << "    if (" << arrValue << " != " << DYNAMIC_ARR_CONSTANT << ")\n"
                                << "        fragColor = vec4(0.0, 0.5, 0.5, 1.0);\n";

                    fragmentSrc << "}\n";
                }
                break;
                default:
                    DE_FATAL("Unhandled IndexType");
                    break;
                }
            }
            else
            {
                fragmentSrc << "void main (void)\n"
                            << "{\n"
                            << "    fragColor = vtxColor;\n"
                            << "}\n";
            }

            sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(fragmentSrc.str());
        }
    }

    // add a pass through fragment shader if it's not activated in push constant ranges
    if (fragmentSrc.str().empty())
    {
        fragmentSrc << "#version 450\n"
                    << "layout(location = 0) in highp vec4 vtxColor;\n"
                    << "layout(location = 0) out highp vec4 fragColor;\n"
                    << "void main (void)\n"
                    << "{\n"
                    << "    fragColor = vtxColor;\n"
                    << "}\n";

        sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(fragmentSrc.str());
    }
}

TestInstance *PushConstantGraphicsDisjointTest::createInstance(Context &context) const
{
    return new PushConstantGraphicsDisjointInstance(context, m_pipelineConstructionType, m_rangeCount,
                                                    m_pushConstantRange, m_multipleUpdate, m_indexType, m_pushConstant2,
                                                    m_pcUsedStage, m_sizeQueriedFromDevice);
}

class PushConstantGraphicsOverlapTest : public PushConstantGraphicsTest
{
public:
    PushConstantGraphicsOverlapTest(tcu::TestContext &testContext, const std::string &name,
                                    const PipelineConstructionType pipelineConstructionType, const uint32_t rangeCount,
                                    const PushConstantData pushConstantRange[MAX_RANGE_COUNT], const bool pushConstant2,
                                    const PushConstantUseStage pcUsedStage = PC_USE_STAGE_ALL);
    virtual ~PushConstantGraphicsOverlapTest(void);
    std::string getPushConstantDeclarationStr(VkShaderStageFlags shaderStage) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
};

PushConstantGraphicsOverlapTest::PushConstantGraphicsOverlapTest(
    tcu::TestContext &testContext, const std::string &name, const PipelineConstructionType pipelineConstructionType,
    const uint32_t rangeCount, const PushConstantData pushConstantRange[MAX_RANGE_COUNT], const bool pushConstant2,
    const PushConstantUseStage pcUsedStage)
    : PushConstantGraphicsTest(testContext, name, pipelineConstructionType, rangeCount, pushConstantRange, false,
                               INDEX_TYPE_CONST_LITERAL, pushConstant2, pcUsedStage)
{
}

PushConstantGraphicsOverlapTest::~PushConstantGraphicsOverlapTest(void)
{
}

std::string PushConstantGraphicsOverlapTest::getPushConstantDeclarationStr(VkShaderStageFlags shaderStage) const
{
    std::stringstream src;

    src << "layout(push_constant) uniform Material\n"
        << "{\n";

    for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
    {
        if (m_pushConstantRange[rangeNdx].range.shaderStage & shaderStage)
        {
            switch (getRangeSizeCase(m_pushConstantRange[rangeNdx].range.size))
            {
            case SIZE_CASE_4:
                src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") float color;\n";
                break;
            case SIZE_CASE_8:
                src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec2 color;\n";
                break;
            case SIZE_CASE_12:
                src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec3 color;\n";
                break;
            case SIZE_CASE_16:
                src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec4 color;\n";
                break;
            case SIZE_CASE_32:
                src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec4 color[2];\n";
                break;
            case SIZE_CASE_36:
                src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") int unused1;\n"
                    << "    layout(offset = " << (m_pushConstantRange[rangeNdx].range.offset + 4) << ") vec4 unused2;\n"
                    << "    layout(offset = " << (m_pushConstantRange[rangeNdx].range.offset + 20) << ") vec4 color;\n";
                break;
            case SIZE_CASE_128:
                src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec4 color[8];\n";
                break;
            default:
                DE_FATAL("Not implemented");
                break;
            }
        }
    }

    src << "} matInst;\n";

    return src.str();
}

std::string getSwizzleStr(uint32_t size)
{
    switch (size)
    {
    case 4:
        return ".x";
    case 8:
        return ".xy";
    case 12:
        return ".xyz";
    case 16:
    case 32:
    case 36:
    case 128:
        return "";
    default:
        DE_FATAL("Not implemented");
        return "";
    }
}

std::string getColorReadStr(uint32_t size)
{
    // Always read the last element from array types
    const std::string arrayNdx     = (size == 128u) ? "[7]" : (size == 32u) ? "[1]" : "";
    const std::string colorReadStr = getSwizzleStr(size) + " += matInst.color" + arrayNdx + ";\n";

    return colorReadStr;
}

void PushConstantGraphicsOverlapTest::initPrograms(SourceCollections &sourceCollections) const
{
    for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
    {
        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_VERTEX_BIT)
        {
            const std::string source =
                "#version 450\n"
                "layout(location = 0) in highp vec4 position;\n"
                "layout(location = 1) in highp vec4 inColor;\n"
                "layout(location = 0) out highp vec4 vtxColor;\n"
                "out gl_PerVertex\n"
                "{\n"
                "    vec4 gl_Position;\n"
                "};\n" +
                ((m_pcUsedStage & PC_USE_STAGE_VERTEX) ? getPushConstantDeclarationStr(VK_SHADER_STAGE_VERTEX_BIT) :
                                                         "\n") +
                "void main()\n"
                "{\n"
                "    gl_Position = position;\n"
                "    vec4 color = inColor;\n" +
                ((m_pcUsedStage & PC_USE_STAGE_VERTEX) ?
                     "    color" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) :
                     "\n") +
                "    vtxColor = color;\n"
                "}\n";

            sourceCollections.glslSources.add("color_vert") << glu::VertexSource(source);
        }

        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        {
            const std::string source = "#version 450\n"
                                       "layout (vertices = 3) out;\n" +
                                       ((m_pcUsedStage & PC_USE_STAGE_TESC) ?
                                            getPushConstantDeclarationStr(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) :
                                            "\n") +
                                       "layout(location = 0) in highp vec4 color[];\n"
                                       "layout(location = 0) out highp vec4 vtxColor[];\n"
                                       "in gl_PerVertex\n"
                                       "{\n"
                                       "    vec4 gl_Position;\n"
                                       "} gl_in[gl_MaxPatchVertices];\n"
                                       "out gl_PerVertex\n"
                                       "{\n"
                                       "    vec4 gl_Position;\n"
                                       "} gl_out[];\n"
                                       "void main()\n"
                                       "{\n"
                                       "    gl_TessLevelInner[0] = 2.0;\n"
                                       "    gl_TessLevelOuter[0] = 2.0;\n"
                                       "    gl_TessLevelOuter[1] = 2.0;\n"
                                       "    gl_TessLevelOuter[2] = 2.0;\n"
                                       "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                                       "    vec4 outColor = color[gl_InvocationID];\n" +
                                       ((m_pcUsedStage & PC_USE_STAGE_TESC) ?
                                            "    outColor" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) :
                                            "\n") +
                                       "    vtxColor[gl_InvocationID] = outColor;\n"
                                       "}\n";

            sourceCollections.glslSources.add("color_tesc") << glu::TessellationControlSource(source);
        }

        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        {
            const std::string source =
                "#version 450\n"
                "layout (triangles) in;\n" +
                ((m_pcUsedStage & PC_USE_STAGE_TESE) ?
                     getPushConstantDeclarationStr(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) :
                     "\n") +
                "layout(location = 0) in highp vec4 color[];\n"
                "layout(location = 0) out highp vec4 vtxColor;\n"
                "in gl_PerVertex\n"
                "{\n"
                "    vec4 gl_Position;\n"
                "} gl_in[gl_MaxPatchVertices];\n"
                "out gl_PerVertex\n"
                "{\n"
                "    vec4 gl_Position;\n"
                "};\n"
                "void main()\n"
                "{\n"
                "    gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + "
                "gl_TessCoord.z * gl_in[2].gl_Position;\n"
                "    vtxColor = gl_TessCoord.x * color[0] + gl_TessCoord.y * color[1] + gl_TessCoord.z * color[2];\n" +
                ((m_pcUsedStage & PC_USE_STAGE_TESE) ?
                     "    vtxColor" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) :
                     "\n") +
                "}\n";

            sourceCollections.glslSources.add("color_tese") << glu::TessellationEvaluationSource(source);
        }

        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_GEOMETRY_BIT)
        {
            const std::string source =
                "#version 450\n"
                "layout(triangles) in;\n"
                "layout(triangle_strip, max_vertices=3) out;\n" +
                ((m_pcUsedStage & PC_USE_STAGE_GEOM) ? getPushConstantDeclarationStr(VK_SHADER_STAGE_GEOMETRY_BIT) :
                                                       "\n") +
                "layout(location = 0) in highp vec4 color[];\n"
                "layout(location = 0) out highp vec4 vtxColor;\n"
                "in gl_PerVertex\n"
                "{\n"
                "    vec4 gl_Position;\n"
                "} gl_in[];\n"
                "out gl_PerVertex\n"
                "{\n"
                "    vec4 gl_Position;\n"
                "};\n"
                "void main()\n"
                "{\n"
                "    for(int i = 0; i < 3; i++)\n"
                "    {\n"
                "        gl_Position.xyz = gl_in[i].gl_Position.xyz" +
                ((m_pcUsedStage & PC_USE_STAGE_GEOM) ? "/2.0;\n" : ";\n") +
                "        gl_Position.w = gl_in[i].gl_Position.w;\n"
                "        vtxColor = color[i];\n" +
                ((m_pcUsedStage & PC_USE_STAGE_GEOM) ?
                     "        vtxColor" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) :
                     "\n") +
                "        EmitVertex();\n"
                "    }\n"
                "    EndPrimitive();\n"
                "}\n";

            sourceCollections.glslSources.add("color_geom") << glu::GeometrySource(source);
        }

        if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            const std::string source =
                "#version 450\n"
                "layout(location = 0) in highp vec4 vtxColor;\n"
                "layout(location = 0) out highp vec4 fragColor;\n" +
                ((m_pcUsedStage & PC_USE_STAGE_FRAG) ? getPushConstantDeclarationStr(VK_SHADER_STAGE_FRAGMENT_BIT) :
                                                       "\n") +
                "void main (void)\n"
                "{\n"
                "    fragColor = vtxColor;\n" +
                ((m_pcUsedStage & PC_USE_STAGE_FRAG) ?
                     "    fragColor" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) :
                     "\n") +
                "    fragColor = min(mod(fragColor, 2.0), 2.0 - mod(fragColor, 2.0));\n"
                "}\n";

            sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(source);
        }
    }
}

TestInstance *PushConstantGraphicsOverlapTest::createInstance(Context &context) const
{
    return new PushConstantGraphicsOverlapTestInstance(context, m_pipelineConstructionType, m_rangeCount,
                                                       m_pushConstantRange, false, INDEX_TYPE_CONST_LITERAL,
                                                       m_pushConstant2, m_pcUsedStage);
}

class PushConstantComputeTest : public vkt::TestCase
{
public:
    PushConstantComputeTest(tcu::TestContext &testContext, const std::string &name, const ComputeTestType testType,
                            const PushConstantData pushConstantRange);
    virtual ~PushConstantComputeTest(void);
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const ComputeTestType m_testType;
    const PushConstantData m_pushConstantRange;
};

class PushConstantComputeTestInstance : public vkt::TestInstance
{
public:
    PushConstantComputeTestInstance(Context &context, const ComputeTestType testType,
                                    const PushConstantData pushConstantRange);
    virtual ~PushConstantComputeTestInstance(void);
    virtual tcu::TestStatus iterate(void);

private:
    const ComputeTestType m_testType;
    const PushConstantData m_pushConstantRange;

    Move<VkBuffer> m_outBuffer;
    de::MovePtr<Allocation> m_outBufferAlloc;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorSet> m_descriptorSet;

    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkPipeline> m_computePipelines;

    Move<VkShaderModule> m_computeShaderModule;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

PushConstantComputeTest::PushConstantComputeTest(tcu::TestContext &testContext, const std::string &name,
                                                 const ComputeTestType testType,
                                                 const PushConstantData pushConstantRange)
    : vkt::TestCase(testContext, name)
    , m_testType(testType)
    , m_pushConstantRange(pushConstantRange)
{
}

PushConstantComputeTest::~PushConstantComputeTest(void)
{
}

TestInstance *PushConstantComputeTest::createInstance(Context &context) const
{
    return new PushConstantComputeTestInstance(context, m_testType, m_pushConstantRange);
}

void PushConstantComputeTest::checkSupport(Context &context) const
{
    if (CTT_UNINITIALIZED == m_testType)
        context.requireDeviceFunctionality("VK_KHR_maintenance4");
}

void PushConstantComputeTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream computeSrc;

    computeSrc << "#version 450\n"
               << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
               << "layout(std140, set = 0, binding = 0) writeonly buffer Output {\n"
               << "  vec4 elements[];\n"
               << "} outData;\n"
               << "layout(push_constant) uniform Material{\n"
               << "  vec4 element;\n"
               << "} matInst;\n"
               << "void main (void)\n"
               << "{\n"
               << "  outData.elements[gl_GlobalInvocationID.x] = matInst.element;\n"
               << "}\n";

    sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc.str());
}

PushConstantComputeTestInstance::PushConstantComputeTestInstance(Context &context, const ComputeTestType testType,
                                                                 const PushConstantData pushConstantRange)
    : vkt::TestInstance(context)
    , m_testType(testType)
    , m_pushConstantRange(pushConstantRange)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

    // Create pipeline layout
    {
        // create push constant range
        VkPushConstantRange pushConstantRanges;
        pushConstantRanges.stageFlags = m_pushConstantRange.range.shaderStage;
        pushConstantRanges.offset     = m_pushConstantRange.range.offset;
        pushConstantRanges.size       = m_pushConstantRange.range.size;

        // create descriptor set layout
        m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                    .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                    .build(vk, vkDevice);

        // create descriptor pool
        m_descriptorPool = DescriptorPoolBuilder()
                               .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
                               .build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        // create uniform buffer
        const VkDeviceSize bufferSize             = sizeof(tcu::Vec4) * 8;
        const VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags    flags
            bufferSize,                           // VkDeviceSize size;
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_outBuffer = createBuffer(vk, vkDevice, &bufferCreateInfo);
        m_outBufferAlloc =
            memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_outBuffer), MemoryRequirement::HostVisible);
        VK_CHECK(
            vk.bindBufferMemory(vkDevice, *m_outBuffer, m_outBufferAlloc->getMemory(), m_outBufferAlloc->getOffset()));

        // create and update descriptor set
        const VkDescriptorSetAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                             sType;
            nullptr,                                        // const void*                                 pNext;
            *m_descriptorPool,         // VkDescriptorPool                            descriptorPool;
            1u,                        // uint32_t                                    setLayoutCount;
            &(*m_descriptorSetLayout), // const VkDescriptorSetLayout*                pSetLayouts;
        };
        m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &allocInfo);

        const VkDescriptorBufferInfo descriptorInfo =
            makeDescriptorBufferInfo(*m_outBuffer, (VkDeviceSize)0u, bufferSize);

        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
            .update(vk, vkDevice);

        // create pipeline layout
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t descriptorSetCount;
            &(*m_descriptorSetLayout),                     // const VkDescriptorSetLayout* pSetLayouts;
            1u,                                            // uint32_t pushConstantRangeCount;
            &pushConstantRanges                            // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
    }

    // create pipeline
    {
        m_computeShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("compute"), 0);

        const VkPipelineShaderStageCreateInfo stageCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            *m_computeShaderModule,                              // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr                                              // const VkSpecializationInfo* pSpecializationInfo;
        };

        const VkComputePipelineCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                             sType;
            nullptr,                                        // const void*                                 pNext;
            0u,                                             // VkPipelineCreateFlags                       flags;
            stageCreateInfo,                                // VkPipelineShaderStageCreateInfo             stage;
            *m_pipelineLayout,                              // VkPipelineLayout                            layout;
            VK_NULL_HANDLE, // VkPipeline                                  basePipelineHandle;
            0u,             // int32_t                                     basePipelineIndex;
        };

        m_computePipelines = createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &createInfo);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    {
        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipelines);
        vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, 1,
                                 &(*m_descriptorSet), 0, nullptr);

        // update push constant
        if (CTT_UNINITIALIZED != m_testType)
        {
            tcu::Vec4 value = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
            vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout, m_pushConstantRange.range.shaderStage,
                                m_pushConstantRange.range.offset, m_pushConstantRange.range.size, &value);
        }

        vk.cmdDispatch(*m_cmdBuffer, 8, 1, 1);

        const VkBufferMemoryBarrier buf_barrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //    VkStructureType    sType;
            nullptr,                                 //    const void*        pNext;
            VK_ACCESS_SHADER_WRITE_BIT,              //    VkAccessFlags      srcAccessMask;
            VK_ACCESS_HOST_READ_BIT,                 //    VkAccessFlags      dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                 //    uint32_t           srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                 //    uint32_t           dstQueueFamilyIndex;
            *m_outBuffer,                            //    VkBuffer           buffer;
            0,                                       //    VkDeviceSize       offset;
            VK_WHOLE_SIZE                            //    VkDeviceSize       size;
        };

        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                              nullptr, 1, &buf_barrier, 0, nullptr);

        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

PushConstantComputeTestInstance::~PushConstantComputeTestInstance(void)
{
}

tcu::TestStatus PushConstantComputeTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    // The test should run without crashing when reading that undefined value.
    // The actual value is not important, test just shouldn't crash.
    if (CTT_UNINITIALIZED == m_testType)
        return tcu::TestStatus::pass("pass");

    invalidateAlloc(vk, vkDevice, *m_outBufferAlloc);

    // verify result
    std::vector<tcu::Vec4> expectValue(8, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    if (deMemCmp((void *)(&expectValue[0]), m_outBufferAlloc->getHostPtr(), (size_t)(sizeof(tcu::Vec4) * 8)))
    {
        return tcu::TestStatus::fail("Image mismatch");
    }
    return tcu::TestStatus::pass("result image matches with reference");
}

class PushConstantLifetimeTest : public vkt::TestCase
{
public:
    PushConstantLifetimeTest(tcu::TestContext &testContext, const std::string &name,
                             const PipelineConstructionType pipelineConstructionType,
                             const PushConstantData pushConstantRange[MAX_RANGE_COUNT],
                             const std::vector<CommandData> &cmdList);

    virtual ~PushConstantLifetimeTest(void);

    virtual void checkSupport(Context &context) const;

    virtual void initPrograms(SourceCollections &sourceCollections) const;

    virtual TestInstance *createInstance(Context &context) const;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    PushConstantData m_pushConstantRange[MAX_RANGE_COUNT];
    std::vector<CommandData> m_cmdList;
};

class PushConstantLifetimeTestInstance : public vkt::TestInstance
{
public:
    PushConstantLifetimeTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                     const PushConstantData pushConstantRange[MAX_RANGE_COUNT],
                                     const std::vector<CommandData> &cmdList);

    virtual ~PushConstantLifetimeTestInstance(void);

    virtual tcu::TestStatus iterate(void);

    void init(void);

    tcu::TestStatus verify(bool verifyGraphics, bool verifyCompute);

private:
    PushConstantData m_pushConstantRange[MAX_RANGE_COUNT];
    PipelineConstructionType m_pipelineConstructionType;
    std::vector<CommandData> m_cmdList;

    std::vector<Vertex4RGBA> m_vertices;

    const tcu::UVec2 m_renderSize;
    const VkFormat m_colorFormat;

    VkImageCreateInfo m_colorImageCreateInfo;
    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorAttachmentView;
    RenderPassWrapper m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModule;
    ShaderWrapper m_computeShaderModule;

    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStage;

    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    Move<VkBuffer> m_outBuffer;
    de::MovePtr<Allocation> m_outBufferAlloc;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorSet> m_descriptorSet;

    PipelineLayoutWrapper m_pipelineLayout[3];
    GraphicsPipelineWrapper m_graphicsPipeline[3];
    Move<VkPipeline> m_computePipeline[3];

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

PushConstantLifetimeTest::PushConstantLifetimeTest(tcu::TestContext &testContext, const std::string &name,
                                                   const PipelineConstructionType pipelineConstructionType,
                                                   const PushConstantData pushConstantRange[MAX_RANGE_COUNT],
                                                   const std::vector<CommandData> &cmdList)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_cmdList(cmdList)
{
    deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

PushConstantLifetimeTest::~PushConstantLifetimeTest(void)
{
}

void PushConstantLifetimeTest::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
}

void PushConstantLifetimeTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream vertexSrc;

    vertexSrc << "#version 450\n"
              << "layout(location = 0) in highp vec4 position;\n"
              << "layout(location = 1) in highp vec4 inColor;\n"
              << "layout(location = 0) out highp vec4 vtxColor;\n"
              << "out gl_PerVertex\n"
              << "{\n"
              << "  vec4 gl_Position;\n"
              << "};\n"
              << "layout(push_constant) uniform Material {\n"
              << "    layout(offset = 16) vec4 color;\n"
              << "}matInst;\n"
              << "void main()\n"
              << "{\n"
              << "    gl_Position = position;\n"
              << "    vtxColor = vec4(inColor.x + matInst.color.x,\n"
              << "                    inColor.y - matInst.color.y,\n"
              << "                    inColor.z + matInst.color.z,\n"
              << "                    inColor.w + matInst.color.w);\n"
              << "}\n";

    sourceCollections.glslSources.add("color_vert_lt") << glu::VertexSource(vertexSrc.str());

    std::ostringstream fragmentSrc;

    fragmentSrc << "#version 450\n"
                << "layout(location = 0) in highp vec4 vtxColor;\n"
                << "layout(location = 0) out highp vec4 fragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    fragColor = vtxColor;\n"
                << "}\n";

    sourceCollections.glslSources.add("color_frag_lt") << glu::FragmentSource(fragmentSrc.str());

    std::ostringstream computeSrc;

    computeSrc << "#version 450\n"
               << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
               << "layout(std140, set = 0, binding = 0) writeonly buffer Output {\n"
               << "  vec4 elements[];\n"
               << "} outData;\n"
               << "layout(push_constant) uniform Material{\n"
               << "    layout(offset = 16) vec4 element;\n"
               << "} matInst;\n"
               << "void main (void)\n"
               << "{\n"
               << "  outData.elements[gl_GlobalInvocationID.x] = matInst.element;\n"
               << "}\n";

    sourceCollections.glslSources.add("compute_lt") << glu::ComputeSource(computeSrc.str());
}

TestInstance *PushConstantLifetimeTest::createInstance(Context &context) const
{
    return new PushConstantLifetimeTestInstance(context, m_pipelineConstructionType, m_pushConstantRange, m_cmdList);
}

PushConstantLifetimeTestInstance::PushConstantLifetimeTestInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType,
    const PushConstantData pushConstantRange[MAX_RANGE_COUNT], const std::vector<CommandData> &cmdList)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_cmdList(cmdList)
    , m_renderSize(32, 32)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_graphicsPipeline{{context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                          context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                         {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                          context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType},
                         {context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                          context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType}}
{
    deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

void PushConstantLifetimeTestInstance::init(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};

    // Create color image
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            nullptr,                                                               // const void* pNext;
            0u,                                                                    // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
            m_colorFormat,                                                         // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u},                              // VkExtent3D extent;
            1u,                                                                    // uint32_t mipLevels;
            1u,                                                                    // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            1u,                                                                    // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,         // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
        };

        m_colorImageCreateInfo = colorImageParams;
        m_colorImage           = createImage(vk, vkDevice, &m_colorImageCreateInfo);

        // Allocate and bind color image memory
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));
    }

    // Create color attachment view
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,    // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            0u,                                          // VkImageViewCreateFlags flags;
            *m_colorImage,                               // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                       // VkImageViewType viewType;
            m_colorFormat,                               // VkFormat format;
            componentMappingRGBA,                        // VkChannelMapping channels;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    // Create render pass
    m_renderPass = RenderPassWrapper(m_pipelineConstructionType, vk, vkDevice, m_colorFormat);

    // Create framebuffer
    {
        const VkImageView attachmentBindInfos[1] = {*m_colorAttachmentView};

        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            1u,                                        // uint32_t attachmentCount;
            attachmentBindInfos,                       // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
    }

    // Create data for pipeline layout
    {
        // create descriptor set layout
        m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                    .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                    .build(vk, vkDevice);

        // create descriptor pool
        m_descriptorPool = DescriptorPoolBuilder()
                               .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
                               .build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        // create storage buffer
        const VkDeviceSize bufferSize             = sizeof(tcu::Vec4) * 8;
        const VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags                flags
            bufferSize,                           // VkDeviceSize size;
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_outBuffer = createBuffer(vk, vkDevice, &bufferCreateInfo);
        m_outBufferAlloc =
            memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_outBuffer), MemoryRequirement::HostVisible);
        VK_CHECK(
            vk.bindBufferMemory(vkDevice, *m_outBuffer, m_outBufferAlloc->getMemory(), m_outBufferAlloc->getOffset()));

        // create and update descriptor set
        const VkDescriptorSetAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            *m_descriptorPool,                              // VkDescriptorPool descriptorPool;
            1u,                                             // uint32_t setLayoutCount;
            &(*m_descriptorSetLayout),                      // const VkDescriptorSetLayout* pSetLayouts;
        };
        m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &allocInfo);

        const VkDescriptorBufferInfo descriptorInfo =
            makeDescriptorBufferInfo(*m_outBuffer, (VkDeviceSize)0u, bufferSize);

        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
            .update(vk, vkDevice);

        // create push constant ranges
        const VkPushConstantRange pushConstantRanges[]{
            {m_pushConstantRange[0].range.shaderStage, m_pushConstantRange[0].range.offset,
             m_pushConstantRange[0].range.size},
            {m_pushConstantRange[1].range.shaderStage, m_pushConstantRange[1].range.offset,
             m_pushConstantRange[1].range.size}};

        const VkPipelineLayoutCreateInfo pipelineLayoutParams[]{
            {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
             nullptr,                                       // const void* pNext;
             0u,                                            // VkPipelineLayoutCreateFlags flags;
             1u,                                            // uint32_t descriptorSetCount;
             &(*m_descriptorSetLayout),                     // const VkDescriptorSetLayout* pSetLayouts;
             1u,                                            // uint32_t pushConstantRangeCount;
             &(pushConstantRanges[0])},
            {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
             nullptr,                                       // const void* pNext;
             0u,                                            // VkPipelineLayoutCreateFlags flags;
             1u,                                            // uint32_t descriptorSetCount;
             &(*m_descriptorSetLayout),                     // const VkDescriptorSetLayout* pSetLayouts;
             1u,                                            // uint32_t pushConstantRangeCount;
             &(pushConstantRanges[1])}};

        m_pipelineLayout[0] =
            PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &(pipelineLayoutParams[0]));
        m_pipelineLayout[1] =
            PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &(pipelineLayoutParams[1]));
    }

    m_vertexShaderModule   = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert_lt"), 0);
    m_fragmentShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag_lt"), 0);

    // Create graphics pipelines
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription{
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBA),        // uint32_t strideInBytes;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate stepRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[]{
            {
                0u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                0u                             // uint32_t offsetInBytes;
            },
            {
                1u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                offsetof(Vertex4RGBA, color),  // uint32_t offset;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // vkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t bindingCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t attributeCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
        const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

        m_graphicsPipeline[0]
            .setDefaultRasterizationState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultColorBlendState()
            .setDefaultTopology(topology)
            .setupVertexInputState(&vertexInputStateParams)
            .setupPreRasterizationShaderState(viewports, scissors, (m_pipelineLayout[0]), *m_renderPass, 0u,
                                              m_vertexShaderModule)
            .setupFragmentShaderState((m_pipelineLayout[0]), *m_renderPass, 0u, m_fragmentShaderModule)
            .setupFragmentOutputState(*m_renderPass)
            .setMonolithicPipelineLayout((m_pipelineLayout[0]))
            .buildPipeline();

        m_graphicsPipeline[1]
            .setDefaultRasterizationState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setDefaultColorBlendState()
            .setDefaultTopology(topology)
            .setupVertexInputState(&vertexInputStateParams)
            .setupPreRasterizationShaderState(viewports, scissors, (m_pipelineLayout[1]), *m_renderPass, 0u,
                                              m_vertexShaderModule)
            .setupFragmentShaderState((m_pipelineLayout[1]), *m_renderPass, 0u, m_fragmentShaderModule)
            .setupFragmentOutputState(*m_renderPass)
            .setMonolithicPipelineLayout((m_pipelineLayout[1]))
            .buildPipeline();
    }

    // Create vertex buffer
    {
        m_vertices = createQuad(1.0f, tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f));

        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                    // VkStructureType sType;
            nullptr,                                                 // const void* pNext;
            0u,                                                      // VkBufferCreateFlags flags;
            (VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()), // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,                       // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                               // VkSharingMode sharingMode;
            1u,                                                      // uint32_t queueFamilyCount;
            &queueFamilyIndex                                        // const uint32_t* pQueueFamilyIndices;
        };

        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create compute pipelines
    {
        m_computeShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("compute_lt"), 0);

        const VkPipelineShaderStageCreateInfo stageCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
            m_computeShaderModule.getModule(),                   // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr                                              // const VkSpecializationInfo* pSpecializationInfo;
        };

        if (m_pushConstantRange[0].range.shaderStage & VK_SHADER_STAGE_COMPUTE_BIT)
        {
            const VkComputePipelineCreateInfo computePipelineLayoutParams = {
                VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0u,                                             // VkPipelineCreateFlags flags;
                stageCreateInfo,                                // VkPipelineShaderStageCreateInfo stage;
                *m_pipelineLayout[0],                           // VkPipelineLayout layout;
                VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
                0u,                                             // int32_t basePipelineIndex;
            };

            m_computePipeline[0] = createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &computePipelineLayoutParams);
        }
        if (m_pushConstantRange[1].range.shaderStage & VK_SHADER_STAGE_COMPUTE_BIT)
        {
            const VkComputePipelineCreateInfo computePipelineLayoutParams = {
                VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0u,                                             // VkPipelineCreateFlags flags;
                stageCreateInfo,                                // VkPipelineShaderStageCreateInfo stage;
                *m_pipelineLayout[1],                           // VkPipelineLayout layout;
                VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
                0u,                                             // int32_t basePipelineIndex;
            };

            m_computePipeline[1] = createComputePipeline(vk, vkDevice, VK_NULL_HANDLE, &computePipelineLayoutParams);
        }
    }
}

PushConstantLifetimeTestInstance::~PushConstantLifetimeTestInstance(void)
{
}

tcu::TestStatus PushConstantLifetimeTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = m_context.getUniversalQueue();

    bool verifyGraphics = false;
    bool verifyCompute  = false;

    init();

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    // Create command buffer
    {
        const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

        // Set push constant value
        tcu::Vec4 value[2] = {{0.25f, 0.75f, 0.75f, 1.0f}, {0.25f, 0.75f, 0.75f, 1.0f}};

        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        for (size_t ndx = 0; ndx < m_cmdList.size(); ndx++)
        {
            const VkPushConstantRange pushConstantRange{m_pushConstantRange[m_cmdList[ndx].rangeNdx].range.shaderStage,
                                                        m_pushConstantRange[m_cmdList[ndx].rangeNdx].range.offset,
                                                        m_pushConstantRange[m_cmdList[ndx].rangeNdx].range.size};

            switch (m_cmdList[ndx].cType)
            {
            case CMD_PUSH_CONSTANT:
            {
                vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout[m_cmdList[ndx].rangeNdx],
                                    pushConstantRange.stageFlags, pushConstantRange.offset, pushConstantRange.size,
                                    &value);
                break;
            }
            case CMD_BIND_PIPELINE_COMPUTE:
            {
                vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                         *m_pipelineLayout[m_cmdList[ndx].rangeNdx], 0, 1u, &(*m_descriptorSet), 0,
                                         nullptr);

                vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                   *m_computePipeline[m_cmdList[ndx].rangeNdx]);
                break;
            }
            case CMD_BIND_PIPELINE_GRAPHICS:
            {
                m_graphicsPipeline[m_cmdList[ndx].rangeNdx].bind(*m_cmdBuffer);
                break;
            }
            case CMD_DRAW:
            {
                const VkDeviceSize bufferOffset = 0;

                const VkImageMemoryBarrier prePassBarrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    0,                                          // VkAccessFlags srcAccessMask;
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags dstAccessMask;
                    VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout oldLayout;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
                    *m_colorImage,                              // VkImage image;
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
                };

                vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                      &prePassBarrier);

                m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
                                   attachmentClearValue);

                vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &bufferOffset);
                vk.cmdDraw(*m_cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);

                m_renderPass.end(vk, *m_cmdBuffer);

                const VkImageMemoryBarrier postPassBarrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags srcAccessMask;
                    VK_ACCESS_TRANSFER_WRITE_BIT,               // VkAccessFlags dstAccessMask;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout oldLayout;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
                    *m_colorImage,                              // VkImage image;
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
                };

                vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      0, 0, nullptr, 0, nullptr, 1, &postPassBarrier);

                verifyGraphics = true;
                break;
            }
            case CMD_DISPATCH:
            {

                vk.cmdDispatch(*m_cmdBuffer, 8, 1, 1);

                const VkBufferMemoryBarrier outputBarrier = {
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
                    nullptr,                                 // const void* pNext;
                    VK_ACCESS_SHADER_WRITE_BIT,              // VkAccessFlags srcAccessMask;
                    VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
                    VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
                    *m_outBuffer,                            // VkBuffer buffer;
                    0,                                       // VkDeviceSize offset;
                    VK_WHOLE_SIZE                            // VkDeviceSize size;
                };

                vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                                      0, nullptr, 1, &outputBarrier, 0, nullptr);

                verifyCompute = true;
                break;
            }
            case CMD_UNSUPPORTED:
                break;
            default:
                break;
            }
        }

        endCommandBuffer(vk, *m_cmdBuffer);
    }

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verify(verifyGraphics, verifyCompute);
}

tcu::TestStatus PushConstantLifetimeTestInstance::verify(bool verifyGraphics, bool verifyCompute)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();

    const tcu::TextureFormat tcuColorFormat = mapVkFormat(m_colorFormat);
    const tcu::TextureFormat tcuDepthFormat = tcu::TextureFormat();
    const ColorVertexShader vertexShader;
    const ColorFragmentShader fragmentShader(tcuColorFormat, tcuDepthFormat);
    const rr::Program program(&vertexShader, &fragmentShader);
    ReferenceRenderer refRenderer(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);

    bool graphicsOk = !verifyGraphics;
    bool computeOk  = !verifyCompute;

    // Compare result with reference image
    if (verifyGraphics)
    {
        // Render reference image
        {
            rr::RenderState renderState(refRenderer.getViewportState(),
                                        m_context.getDeviceProperties().limits.subPixelPrecisionBits);
            refRenderer.draw(renderState, rr::PRIMITIVETYPE_TRIANGLES, m_vertices);
        }

        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::MovePtr<tcu::TextureLevel> result = readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator,
                                                                    *m_colorImage, m_colorFormat, m_renderSize);

        graphicsOk = tcu::intThresholdPositionDeviationCompare(
            m_context.getTestContext().getLog(), "IntImageCompare", "Image comparison", refRenderer.getAccess(),
            result->getAccess(), tcu::UVec4(2, 2, 2, 2), tcu::IVec3(1, 1, 0), true, tcu::COMPARE_LOG_RESULT);
    }

    // Compare compute output
    if (verifyCompute)
    {
        invalidateAlloc(vk, vkDevice, *m_outBufferAlloc);

        // verify result
        std::vector<tcu::Vec4> expectValue(8, tcu::Vec4(0.25f, 0.75f, 0.75f, 1.0f));
        if (deMemCmp((void *)(&expectValue[0]), m_outBufferAlloc->getHostPtr(), (size_t)(sizeof(tcu::Vec4) * 8)))
            computeOk = false;
        else
            computeOk = true;
    }

    if (!graphicsOk)
        return tcu::TestStatus::fail("Image mismatch");

    if (!computeOk)
        return tcu::TestStatus::fail("Wrong output value");

    return tcu::TestStatus::pass("Result image matches reference");
}

// The overwrite-values cases will use a 2x2 storage image and 4 separate draws or dispatches to store the color of each pixel in
// the image. The color will be calculated as baseColor*multiplier+colorOffset, and the base color, multiplier, color component
// offsets and coords will be changed with multiple push commands before each draw/dispatch, to verify overwriting multiple ranges
// works as expected.

struct OverwritePushConstants
{
    tcu::IVec4 coords; // We will only use the first two components, but an IVec4 eases matching alignments.
    tcu::UVec4 baseColor;
    tcu::UVec4 multiplier;
    uint32_t colorOffsets[4];
    tcu::UVec4 transparentGreen;
};

struct OverwriteTestParams
{
    PipelineConstructionType pipelineConstructionType;
    OverwritePushConstants pushConstantValues[4];
    VkPipelineBindPoint bindPoint;
};

class OverwriteTestCase : public vkt::TestCase
{
public:
    OverwriteTestCase(tcu::TestContext &testCtx, const std::string &name, const OverwriteTestParams &params);
    virtual ~OverwriteTestCase(void)
    {
    }

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

protected:
    OverwriteTestParams m_params;
};

class OverwriteTestInstance : public vkt::TestInstance
{
public:
    OverwriteTestInstance(Context &context, const OverwriteTestParams &params);
    virtual ~OverwriteTestInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    OverwriteTestParams m_params;
};

OverwriteTestCase::OverwriteTestCase(tcu::TestContext &testCtx, const std::string &name,
                                     const OverwriteTestParams &params)
    : vkt::TestCase(testCtx, name)
    , m_params(params)
{
}

void OverwriteTestCase::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_params.pipelineConstructionType);
}

void OverwriteTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream shader;

    shader << "#version 450\n"
           << "layout (push_constant, std430) uniform PushConstants {\n"
           << "    ivec4   coords;\n" // Note we will only use the .xy swizzle.
           << "    uvec4   baseColor;\n"
           << "    uvec4   multiplier;\n"
           << "    uint    colorOffsets[4];\n"
           << "    uvec4   transparentGreen;\n"
           << "} pc;\n"
           << "layout(rgba8ui, set=0, binding=0) uniform uimage2D simage;\n"
           << "void main() {\n"
           << "    uvec4   colorOffsets = uvec4(pc.colorOffsets[0], pc.colorOffsets[1], pc.colorOffsets[2], "
              "pc.colorOffsets[3]);\n"
           << "    uvec4   finalColor   = pc.baseColor * pc.multiplier + colorOffsets + pc.transparentGreen;\n"
           << "    imageStore(simage, pc.coords.xy, finalColor);\n"
           << "}\n";

    if (m_params.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        programCollection.glslSources.add("comp") << glu::ComputeSource(shader.str());
    }
    else if (m_params.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
        std::ostringstream vert;
        vert << "#version 450\n"
             << "\n"
             << "void main()\n"
             << "{\n"
             // Full-screen clockwise triangle strip with 4 vertices.
             << "    const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
             << "    const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
             << "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
             << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
        programCollection.glslSources.add("frag") << glu::FragmentSource(shader.str());
    }
    else
        DE_ASSERT(false);
}

TestInstance *OverwriteTestCase::createInstance(Context &context) const
{
    return new OverwriteTestInstance(context, m_params);
}

OverwriteTestInstance::OverwriteTestInstance(Context &context, const OverwriteTestParams &params)
    : vkt::TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus OverwriteTestInstance::iterate(void)
{
    const auto &vki       = m_context.getInstanceInterface();
    const auto &vkd       = m_context.getDeviceInterface();
    const auto physDevice = m_context.getPhysicalDevice();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto queue      = m_context.getUniversalQueue();
    const auto qIndex     = m_context.getUniversalQueueFamilyIndex();
    const bool isComp     = (m_params.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE);

    const VkShaderStageFlags stageFlags = (isComp ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT);
    const VkPipelineStageFlags writeStages =
        (isComp ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    const auto imageFormat = VK_FORMAT_R8G8B8A8_UINT;
    const auto imageExtent = makeExtent3D(2u, 2u, 1u);

    // Storage image.
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                            // VkStructureType sType;
        nullptr,                                                        // const void* pNext;
        0u,                                                             // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                               // VkImageType imageType;
        imageFormat,                                                    // VkFormat format;
        imageExtent,                                                    // VkExtent3D extent;
        1u,                                                             // uint32_t mipLevels;
        1u,                                                             // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                          // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                        // VkImageTiling tiling;
        (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                      // VkSharingMode sharingMode;
        0u,                                                             // uint32_t queueFamilyIndexCount;
        nullptr,                                                        // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                      // VkImageLayout initialLayout;
    };
    ImageWithMemory storageImage(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any);
    const auto subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto storageImageView =
        makeImageView(vkd, device, storageImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, subresourceRange);

    // Buffer to copy output pixels to.
    const auto tcuFormat  = mapVkFormat(imageFormat);
    const auto pixelSize  = static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat));
    const auto bufferSize = pixelSize * imageExtent.width * imageExtent.height * imageExtent.depth;

    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory transferBuffer(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible);

    // Descriptor set layout and pipeline layout.
    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stageFlags);
    const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

    const VkPushConstantRange pcRange = {
        stageFlags,                                            // VkShaderStageFlags stageFlags;
        0u,                                                    // uint32_t offset;
        static_cast<uint32_t>(sizeof(OverwritePushConstants)), // uint32_t size;
    };
    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, vkd, device, 1u,
                                               &descriptorSetLayout.get(), 1u, &pcRange);

    // Descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet  = makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

    DescriptorSetUpdateBuilder updateBuilder;
    const auto descriptorImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, storageImageView.get(), VK_IMAGE_LAYOUT_GENERAL);
    updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo);
    updateBuilder.update(vkd, device);

    // Command pool and set.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    // Pipeline.
    const std::vector<VkViewport> viewports(1, makeViewport(imageExtent));
    const std::vector<VkRect2D> scissors(1, makeRect2D(imageExtent));

    ShaderWrapper vertModule;
    ShaderWrapper fragModule;
    ShaderWrapper compModule;

    RenderPassWrapper renderPass;
    Move<VkFramebuffer> framebuffer;
    Move<VkPipeline> pipeline;
    GraphicsPipelineWrapper pipelineWrapper(vki, vkd, physDevice, device, m_context.getDeviceExtensions(),
                                            m_params.pipelineConstructionType);

    if (isComp)
    {
        compModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);
        pipeline   = makeComputePipeline(vkd, device, pipelineLayout.get(), compModule.getModule());
    }
    else
    {
        vertModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
        fragModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

        const VkPipelineVertexInputStateCreateInfo inputState = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
            nullptr, // const void*                                 pNext
            0u,      // VkPipelineVertexInputStateCreateFlags       flags
            0u,      // uint32_t                                    vertexBindingDescriptionCount
            nullptr, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
            0u,      // uint32_t                                    vertexAttributeDescriptionCount
            nullptr, // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
        };
        renderPass = RenderPassWrapper(m_params.pipelineConstructionType, vkd, device);
        renderPass.createFramebuffer(vkd, device, 0u, nullptr, nullptr, imageExtent.width, imageExtent.height);

        const VkPipelineColorBlendStateCreateInfo colorBlendState{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
            nullptr,                 // const void*                                    pNext
            0u,                      // VkPipelineColorBlendStateCreateFlags            flags
            VK_FALSE,                // VkBool32                                        logicOpEnable
            VK_LOGIC_OP_CLEAR,       // VkLogicOp                                    logicOp
            0u,                      // uint32_t                                        attachmentCount
            nullptr,                 // const VkPipelineColorBlendAttachmentState*    pAttachments
            {0.0f, 0.0f, 0.0f, 0.0f} // float                                        blendConstants[4]
        };

        pipelineWrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setupVertexInputState(&inputState)
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule)
            .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
            .setupFragmentOutputState(*renderPass, 0u, &colorBlendState)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline();
    }

    // Offsets and sizes.
    const struct
    {
        size_t offset;
        size_t size;
    } pcPush[] = {
        // Push members doing some back-and-forth in the range.
        {offsetof(OverwritePushConstants, baseColor), sizeof(OverwritePushConstants::baseColor)},
        {offsetof(OverwritePushConstants, coords), sizeof(OverwritePushConstants::coords)},
        {offsetof(OverwritePushConstants, colorOffsets), sizeof(OverwritePushConstants::colorOffsets)},
        {offsetof(OverwritePushConstants, multiplier), sizeof(OverwritePushConstants::multiplier)},
        {offsetof(OverwritePushConstants, transparentGreen), sizeof(OverwritePushConstants::transparentGreen)},
    };

    beginCommandBuffer(vkd, cmdBuffer);

    // Transition layout for storage image.
    const auto preImageBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VK_IMAGE_LAYOUT_GENERAL, storageImage.get(), subresourceRange);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, writeStages, 0u, 0u, nullptr, 0u, nullptr, 1u,
                           &preImageBarrier);

    vkd.cmdBindDescriptorSets(cmdBuffer, m_params.bindPoint, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u,
                              nullptr);

    if (!isComp)
    {
        pipelineWrapper.bind(cmdBuffer);
        renderPass.begin(vkd, cmdBuffer, scissors[0]);
    }
    else
        vkd.cmdBindPipeline(cmdBuffer, m_params.bindPoint, pipeline.get());

    for (int pcIndex = 0; pcIndex < DE_LENGTH_OF_ARRAY(m_params.pushConstantValues); ++pcIndex)
    {
        const auto &pc = m_params.pushConstantValues[pcIndex];

        // Push all structure members separately.
        for (int pushIdx = 0; pushIdx < DE_LENGTH_OF_ARRAY(pcPush); ++pushIdx)
        {
            const auto &push    = pcPush[pushIdx];
            const void *dataPtr = reinterpret_cast<const void *>(reinterpret_cast<const char *>(&pc) + push.offset);
            vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), stageFlags, static_cast<uint32_t>(push.offset),
                                 static_cast<uint32_t>(push.size), dataPtr);
        }

        // Draw or dispatch.
        if (isComp)
            vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
        else
            vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }

    if (!isComp)
        renderPass.end(vkd, cmdBuffer);

    // Copy storage image to output buffer.
    const auto postImageBarrier =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, storageImage.get(), subresourceRange);
    vkd.cmdPipelineBarrier(cmdBuffer, writeStages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u,
                           &postImageBarrier);

    const auto copyRegion =
        makeBufferImageCopy(imageExtent, makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
    vkd.cmdCopyImageToBuffer(cmdBuffer, storageImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transferBuffer.get(),
                             1u, &copyRegion);

    const auto bufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                                       transferBuffer.get(), 0ull, bufferSize);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                           &bufferBarrier, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Verify output colors match.
    auto &bufferAlloc         = transferBuffer.getAllocation();
    const void *bufferHostPtr = bufferAlloc.getHostPtr();
    invalidateAlloc(vkd, device, bufferAlloc);

    const int iWidth  = static_cast<int>(imageExtent.width);
    const int iHeight = static_cast<int>(imageExtent.height);
    const int iDepth  = static_cast<int>(imageExtent.depth);

    tcu::ConstPixelBufferAccess outputAccess(tcuFormat, iWidth, iHeight, iDepth, bufferHostPtr);

    for (int pixelIdx = 0; pixelIdx < DE_LENGTH_OF_ARRAY(m_params.pushConstantValues); ++pixelIdx)
    {
        const auto &pc = m_params.pushConstantValues[pixelIdx];
        const tcu::UVec4 expectedValue =
            pc.baseColor * pc.multiplier +
            tcu::UVec4(pc.colorOffsets[0], pc.colorOffsets[1], pc.colorOffsets[2], pc.colorOffsets[3]) +
            pc.transparentGreen;
        const tcu::UVec4 outputValue = outputAccess.getPixelUint(pc.coords.x(), pc.coords.y());

        if (expectedValue != outputValue)
        {
            std::ostringstream msg;
            msg << "Unexpected value in output image at coords " << pc.coords << ": found " << outputValue
                << " and expected " << expectedValue;
            TCU_FAIL(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

void addOverwriteCase(tcu::TestCaseGroup *group, tcu::TestContext &testCtx,
                      PipelineConstructionType pipelineConstructionType, VkPipelineBindPoint bindPoint)
{
    const OverwritePushConstants pushConstants[4] = {
        //    coords                        baseColor                    multiplier                            colorOffsets                transparentGreen
        {tcu::IVec4(0, 0, 0, 0),
         tcu::UVec4(1u, 0u, 0u, 0u),
         tcu::UVec4(2u, 2u, 2u, 2u),
         {128u, 129u, 130u, 131u},
         tcu::UVec4(0u, 1u, 0u, 0u)},
        {tcu::IVec4(0, 1, 0, 0),
         tcu::UVec4(0u, 1u, 0u, 0u),
         tcu::UVec4(4u, 4u, 4u, 4u),
         {132u, 133u, 134u, 135u},
         tcu::UVec4(0u, 1u, 0u, 0u)},
        {tcu::IVec4(1, 0, 0, 0),
         tcu::UVec4(0u, 0u, 1u, 0u),
         tcu::UVec4(8u, 8u, 8u, 8u),
         {136u, 137u, 138u, 139u},
         tcu::UVec4(0u, 1u, 0u, 0u)},
        {tcu::IVec4(1, 1, 0, 0),
         tcu::UVec4(0u, 0u, 0u, 1u),
         tcu::UVec4(16u, 16u, 16u, 16u),
         {140u, 141u, 142u, 143u},
         tcu::UVec4(0u, 1u, 0u, 0u)},
    };

    OverwriteTestParams testParams;

    DE_ASSERT(DE_LENGTH_OF_ARRAY(pushConstants) == DE_LENGTH_OF_ARRAY(testParams.pushConstantValues));
    for (int pixelIdx = 0; pixelIdx < DE_LENGTH_OF_ARRAY(pushConstants); ++pixelIdx)
        testParams.pushConstantValues[pixelIdx] = pushConstants[pixelIdx];

    testParams.pipelineConstructionType = pipelineConstructionType;
    testParams.bindPoint                = bindPoint;

    // Test push constant range overwrites
    group->addChild(new OverwriteTestCase(testCtx, "overwrite", testParams));
}

} // namespace

tcu::TestCaseGroup *createPushConstantTests(tcu::TestContext &testCtx,
                                            PipelineConstructionType pipelineConstructionType)
{
    static const struct
    {
        const char *name;
        uint32_t count;
        PushConstantData range[MAX_RANGE_COUNT];
        bool hasMultipleUpdates;
        IndexType indexType;
    } graphicsParams[] = {
        // test range size is 4 bytes(minimum valid size)
        {"range_size_4", 1u, {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 4}, {0, 4}}}, false, INDEX_TYPE_CONST_LITERAL},
        // test range size is 16 bytes, and together with a normal uniform
        {"range_size_16", 1u, {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}}}, false, INDEX_TYPE_CONST_LITERAL},
        // test range size is 128 bytes(maximum valid size in Vulkan 1.3)
        {"range_size_128", 1u, {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 128}, {0, 128}}}, false, INDEX_TYPE_CONST_LITERAL},
        // test range size is 256 bytes(maximum valid size in Vulkan 1.4)
        {"range_size_256", 1u, {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 256}, {0, 256}}}, false, INDEX_TYPE_CONST_LITERAL},
        // test range size is max bytes queried from driver and will be overwritten
        {"range_size_max",
         1u,
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 0xFFFF}, {0, 0xFFFF}}},
         false,
         INDEX_TYPE_CONST_LITERAL},
        // test range count, including all valid shader stage in graphics pipeline, and also multiple shader stages share one single range
        {"count_2_shaders_vert_frag",
         2u,
         {
             {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
             {{VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4}, {16, 4}},
         },
         false,
         INDEX_TYPE_CONST_LITERAL},
        // test range count is 3, use vertex, geometry and fragment shaders
        {"count_3_shaders_vert_geom_frag",
         3u,
         {
             {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
             {{VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4}, {16, 4}},
             {{VK_SHADER_STAGE_GEOMETRY_BIT, 20, 4}, {20, 4}},
         },
         false,
         INDEX_TYPE_CONST_LITERAL},
        // test range count is 5, use vertex, tessellation, geometry and fragment shaders
        {"count_5_shaders_vert_tess_geom_frag",
         5u,
         {
             {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
             {{VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4}, {16, 4}},
             {{VK_SHADER_STAGE_GEOMETRY_BIT, 20, 4}, {20, 4}},
             {{VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 24, 4}, {24, 4}},
             {{VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 32, 16}, {32, 16}},
         },
         false,
         INDEX_TYPE_CONST_LITERAL},
        // test range count is 1, vertex and fragment shaders share one range
        {"count_1_shader_vert_frag",
         1u,
         {{{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4}, {0, 4}}},
         false,
         INDEX_TYPE_CONST_LITERAL},
        // test data partial update and multiple times update
        {"data_update_partial_1",
         1u,
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {4, 24}}},
         false,
         INDEX_TYPE_CONST_LITERAL},
        // test partial update of the values
        {"data_update_partial_2",
         1u,
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 48}, {32, 16}}},
         false,
         INDEX_TYPE_CONST_LITERAL},
        // test multiple times update of the values
        {"data_update_multiple", 1u, {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 4}, {0, 4}}}, true, INDEX_TYPE_CONST_LITERAL},
        // dynamically uniform indexing of vertex, matrix, and array in vertex shader
        {"dynamic_index_vert",
         1u,
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 64}, {0, 64}}},
         false,
         INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR},
        // dynamically uniform indexing of vertex, matrix, and array in fragment shader
        {"dynamic_index_frag",
         1u,
         {{{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 64}, {0, 64}}},
         false,
         INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR}};

    static const struct
    {
        const char *name;
        uint32_t count;
        PushConstantData range[MAX_RANGE_COUNT];
    } overlapGraphicsParams[] = {
        // overlapping range count is 2, use vertex and fragment shaders
        {"overlap_2_shaders_vert_frag",
         2u,
         {
             {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
             {{VK_SHADER_STAGE_FRAGMENT_BIT, 12, 36}, {12, 36}},
         }},
        // overlapping range count is 3, use vertex, geometry and fragment shaders
        {"overlap_3_shaders_vert_geom_frag",
         3u,
         {{{VK_SHADER_STAGE_VERTEX_BIT, 12, 36}, {12, 36}},
          {{VK_SHADER_STAGE_GEOMETRY_BIT, 0, 32}, {16, 16}},
          {{VK_SHADER_STAGE_FRAGMENT_BIT, 20, 4}, {20, 4}}}},
        // overlapping range count is 4, use vertex, tessellation and fragment shaders
        {"overlap_4_shaders_vert_tess_frag",
         4u,
         {{{VK_SHADER_STAGE_VERTEX_BIT, 8, 4}, {8, 4}},
          {{VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, 128}, {52, 76}},
          {{VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 56, 8}, {56, 8}},
          {{VK_SHADER_STAGE_FRAGMENT_BIT, 60, 36}, {60, 36}}}},
        // overlapping range count is 5, use vertex, tessellation, geometry and fragment shaders
        {"overlap_5_shaders_vert_tess_geom_frag",
         5u,
         {{{VK_SHADER_STAGE_VERTEX_BIT, 40, 8}, {40, 8}},
          {{VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 32, 12}, {32, 12}},
          {{VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 48, 16}, {48, 16}},
          {{VK_SHADER_STAGE_GEOMETRY_BIT, 28, 36}, {28, 36}},
          {{VK_SHADER_STAGE_FRAGMENT_BIT, 56, 8}, {60, 4}}}}};

    static const struct
    {
        const char *name;
        ComputeTestType type;
        PushConstantData range;
    } computeParams[] = {
        // test compute pipeline
        {
            "simple_test",
            CTT_SIMPLE,
            {{VK_SHADER_STAGE_COMPUTE_BIT, 0, 16}, {0, 16}},
        },
        // test push constant that is dynamically unused
        {
            "uninitialized",
            CTT_UNINITIALIZED,
            {{VK_SHADER_STAGE_COMPUTE_BIT, 0, 16}, {0, 16}},
        },
    };

    static const struct
    {
        const char *name;
        PushConstantData range[MAX_RANGE_COUNT];
        std::vector<CommandData> cmdList;
    } lifetimeParams[] = {
        // bind different layout with the same range
        {"push_range0_bind_layout1",
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}, {{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}},
         {
             {CMD_PUSH_CONSTANT, 0},
             {CMD_BIND_PIPELINE_GRAPHICS, 1},
             {CMD_DRAW, -1},
         }},
        // bind layout with same range then push different range
        {"push_range1_bind_layout1_push_range0",
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}, {{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}},
         {
             {CMD_PUSH_CONSTANT, 1},
             {CMD_BIND_PIPELINE_GRAPHICS, 1},
             {CMD_DRAW, -1},
             {CMD_PUSH_CONSTANT, 0},
             {CMD_DRAW, -1},
         }},
        // same range same layout then same range from a different layout and same range from the same layout
        {"push_range0_bind_layout0_push_range1_push_range0",
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}, {{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}},
         {
             {CMD_PUSH_CONSTANT, 0},
             {CMD_BIND_PIPELINE_GRAPHICS, 0},
             {CMD_PUSH_CONSTANT, 1},
             {CMD_PUSH_CONSTANT, 0},
             {CMD_DRAW, -1},
         }},
        // same range same layout then diff range and same range update
        {"push_range0_bind_layout0_push_diff_overlapping_range1_push_range0",
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}, {{VK_SHADER_STAGE_VERTEX_BIT, 16, 32}, {16, 32}}},
         {
             {CMD_PUSH_CONSTANT, 0},
             {CMD_BIND_PIPELINE_GRAPHICS, 0},
             {CMD_PUSH_CONSTANT, 1},
             {CMD_PUSH_CONSTANT, 0},
             {CMD_DRAW, -1},
         }},
        // update push constant bind different layout with the same range then bind correct layout
        {"push_range0_bind_layout1_bind_layout0",
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}, {{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}},
         {
             {CMD_PUSH_CONSTANT, 0},
             {CMD_BIND_PIPELINE_GRAPHICS, 1},
             {CMD_BIND_PIPELINE_GRAPHICS, 0},
             {CMD_DRAW, -1},
         }},
        // update push constant then bind different layout with overlapping range then bind correct layout
        {"push_range0_bind_layout1_overlapping_range_bind_layout0",
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}, {{VK_SHADER_STAGE_VERTEX_BIT, 16, 32}, {16, 32}}},
         {
             {CMD_PUSH_CONSTANT, 0},
             {CMD_BIND_PIPELINE_GRAPHICS, 1},
             {CMD_BIND_PIPELINE_GRAPHICS, 0},
             {CMD_DRAW, -1},
         }},
        // bind different layout with different range then update push constant and bind correct layout
        {"bind_layout1_push_range0_bind_layout0",
         {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 32}, {0, 32}}, {{VK_SHADER_STAGE_VERTEX_BIT, 16, 32}, {16, 32}}},
         {
             {CMD_BIND_PIPELINE_GRAPHICS, 1},
             {CMD_PUSH_CONSTANT, 0},
             {CMD_BIND_PIPELINE_GRAPHICS, 0},
             {CMD_DRAW, -1},
         }},
        // change pipeline same range, bind then push, stages vertex and compute
        {"pipeline_change_same_range_bind_push_vert_and_comp",
         {{{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, 32}, {0, 32}},
          {{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, 32}, {0, 32}}},
         {
             {CMD_BIND_PIPELINE_GRAPHICS, 0},
             {CMD_BIND_PIPELINE_COMPUTE, 1},
             {CMD_PUSH_CONSTANT, 0},
             {CMD_DRAW, -1},
             {CMD_PUSH_CONSTANT, 1},
             {CMD_DISPATCH, -1},
         }},
        // change pipeline different range overlapping, bind then push, stages vertex and compute
        {"pipeline_change_diff_range_bind_push_vert_and_comp",
         {{{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, 32}, {0, 32}},
          {{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 16, 32}, {16, 32}}},
         {
             {CMD_BIND_PIPELINE_GRAPHICS, 0},
             {CMD_BIND_PIPELINE_COMPUTE, 1},
             {CMD_PUSH_CONSTANT, 0},
             {CMD_DRAW, -1},
             {CMD_PUSH_CONSTANT, 1},
             {CMD_DISPATCH, -1},
         }}};

    de::MovePtr<tcu::TestCaseGroup> pushConstantTests(new tcu::TestCaseGroup(testCtx, "push_constant"));

    de::MovePtr<tcu::TestCaseGroup> graphicsTests(new tcu::TestCaseGroup(testCtx, "graphics_pipeline"));
    for (int cmd = 0; cmd < 2; ++cmd)
    {
        bool pushConstant2 = cmd != 0;
#ifdef CTS_USES_VULKANSC
        if (pushConstant2)
            continue;
#endif
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(graphicsParams); ndx++)
        {
            std::string name = graphicsParams[ndx].name;
            if (pushConstant2)
                name += "_command2";
            graphicsTests->addChild(new PushConstantGraphicsDisjointTest(
                testCtx, name.c_str(), pipelineConstructionType, graphicsParams[ndx].count, graphicsParams[ndx].range,
                graphicsParams[ndx].hasMultipleUpdates, graphicsParams[ndx].indexType, pushConstant2, PC_USE_STAGE_ALL,
                graphicsParams[ndx].range[0].range.size == 0xFFFF));
        }

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(overlapGraphicsParams); ndx++)
        {
            std::string name = overlapGraphicsParams[ndx].name;
            if (pushConstant2)
                name += "_command2";
            graphicsTests->addChild(new PushConstantGraphicsOverlapTest(
                testCtx, name.c_str(), pipelineConstructionType, overlapGraphicsParams[ndx].count,
                overlapGraphicsParams[ndx].range, pushConstant2));
        }
    }
    addOverwriteCase(graphicsTests.get(), testCtx, pipelineConstructionType, VK_PIPELINE_BIND_POINT_GRAPHICS);

    // tests for unused push constants:
    // push constants that are specified in pipeline layout but not used in shaders
    {
        {
            static const struct
            {
                const char *name;
                uint32_t count;
                PushConstantData range[MAX_RANGE_COUNT];
                bool hasMultipleUpdates;
                IndexType indexType;
                PushConstantUseStage pcUsedStages;
            } unusedDisjointPCTestParams[] = {
                // test range size is 4 bytes(minimum valid size)
                // no shader stage using push constants
                {"unused_disjoint_1",
                 1u,
                 {{{VK_SHADER_STAGE_VERTEX_BIT, 0, 4}, {0, 4}}},
                 false,
                 INDEX_TYPE_CONST_LITERAL,
                 PC_USE_STAGE_NONE

                },
                // test range count, including all valid shader stage in graphics pipeline
                // vertex shader using push constants, fragment shader not using push constants
                {"unused_disjoint_2",
                 2u,
                 {
                     {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
                     {{VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4}, {16, 4}},
                 },
                 false,
                 INDEX_TYPE_CONST_LITERAL,
                 PC_USE_STAGE_VERTEX},
                // test range count is 3, use vertex, geometry and fragment shaders
                // no shader stage using push constants
                {"unused_disjoint_3",
                 3u,
                 {
                     {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
                     {{VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4}, {16, 4}},
                     {{VK_SHADER_STAGE_GEOMETRY_BIT, 20, 4}, {20, 4}},
                 },
                 false,
                 INDEX_TYPE_CONST_LITERAL,
                 PC_USE_STAGE_NONE},
                // test range count is 3, use vertex, geometry and fragment shaders
                // geometry shader using push constants, vertex and fragment shader not using push constants
                {"unused_disjoint_4",
                 3u,
                 {
                     {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
                     {{VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4}, {16, 4}},
                     {{VK_SHADER_STAGE_GEOMETRY_BIT, 20, 4}, {20, 4}},
                 },
                 false,
                 INDEX_TYPE_CONST_LITERAL,
                 PC_USE_STAGE_GEOM},
                // test range count is 5, use vertex, tessellation, geometry and fragment shaders
                // no shader stage using push constants
                {"unused_disjoint_5",
                 5u,
                 {
                     {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
                     {{VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4}, {16, 4}},
                     {{VK_SHADER_STAGE_GEOMETRY_BIT, 20, 4}, {20, 4}},
                     {{VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 24, 4}, {24, 4}},
                     {{VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 32, 16}, {32, 16}},
                 },
                 false,
                 INDEX_TYPE_CONST_LITERAL,
                 PC_USE_STAGE_NONE},
                // test range count is 5, use vertex, tessellation, geometry and fragment shaders
                // tess shader stages using push constants, vertex, geometry and fragment shader not using push constants
                {"unused_disjoint_6",
                 5u,
                 {
                     {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
                     {{VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4}, {16, 4}},
                     {{VK_SHADER_STAGE_GEOMETRY_BIT, 20, 4}, {20, 4}},
                     {{VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 24, 4}, {24, 4}},
                     {{VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 32, 16}, {32, 16}},
                 },
                 false,
                 INDEX_TYPE_CONST_LITERAL,
                 PC_USE_STAGE_TESC | PC_USE_STAGE_TESE}};

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(unusedDisjointPCTestParams); ndx++)
            {
                graphicsTests->addChild(new PushConstantGraphicsDisjointTest(
                    testCtx, unusedDisjointPCTestParams[ndx].name, pipelineConstructionType,
                    unusedDisjointPCTestParams[ndx].count, unusedDisjointPCTestParams[ndx].range,
                    unusedDisjointPCTestParams[ndx].hasMultipleUpdates, unusedDisjointPCTestParams[ndx].indexType,
                    unusedDisjointPCTestParams[ndx].pcUsedStages));
            }
        }
        {
            static const struct
            {
                const char *name;
                uint32_t count;
                PushConstantData range[MAX_RANGE_COUNT];
                PushConstantUseStage pcUsedStages;

            } unusedOverlapPCTestParams[] = {
                // overlapping range count is 2, use vertex and fragment shaders
                // no shader stage using push constants
                {"unused_overlap_1",
                 2u,
                 {
                     {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
                     {{VK_SHADER_STAGE_FRAGMENT_BIT, 12, 36}, {12, 36}},
                 },
                 PC_USE_STAGE_NONE},
                // overlapping range count is 2, use vertex and fragment shaders
                // vertex shader using push constants, fragment shader not using push constants
                {"unused_overlap_2",
                 2u,
                 {
                     {{VK_SHADER_STAGE_VERTEX_BIT, 0, 16}, {0, 16}},
                     {{VK_SHADER_STAGE_FRAGMENT_BIT, 12, 36}, {12, 36}},
                 },
                 PC_USE_STAGE_VERTEX},
                // overlapping range count is 3, use vertex, geometry and fragment shaders
                // no shader stage using push constants
                {"unused_overlap_3",
                 3u,
                 {{{VK_SHADER_STAGE_VERTEX_BIT, 12, 36}, {12, 36}},
                  {{VK_SHADER_STAGE_GEOMETRY_BIT, 0, 32}, {16, 16}},
                  {{VK_SHADER_STAGE_FRAGMENT_BIT, 20, 4}, {20, 4}}},
                 PC_USE_STAGE_NONE},
                // overlapping range count is 3, use vertex, geometry and fragment shaders
                // geometry shader using push constants, vertex and fragment shader not using push constants
                {"unused_overlap_4",
                 3u,
                 {{{VK_SHADER_STAGE_VERTEX_BIT, 12, 36}, {12, 36}},
                  {{VK_SHADER_STAGE_GEOMETRY_BIT, 0, 32}, {16, 16}},
                  {{VK_SHADER_STAGE_FRAGMENT_BIT, 20, 4}, {20, 4}}},
                 PC_USE_STAGE_GEOM},
                // overlapping range count is 5, use vertex, tessellation, geometry and fragment shaders
                // no shader stage using push constants
                {"unused_overlap_5",
                 5u,
                 {{{VK_SHADER_STAGE_VERTEX_BIT, 40, 8}, {40, 8}},
                  {{VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 32, 12}, {32, 12}},
                  {{VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 48, 16}, {48, 16}},
                  {{VK_SHADER_STAGE_GEOMETRY_BIT, 28, 36}, {28, 36}},
                  {{VK_SHADER_STAGE_FRAGMENT_BIT, 56, 8}, {60, 4}}},
                 PC_USE_STAGE_NONE},
                // overlapping range count is 5, use vertex, tessellation, geometry and fragment shaders
                // tess shader stages using push constants, vertex, geometry and fragment shader not using push constants
                {"unused_overlap_6",
                 5u,
                 {{{VK_SHADER_STAGE_VERTEX_BIT, 40, 8}, {40, 8}},
                  {{VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 32, 12}, {32, 12}},
                  {{VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 48, 16}, {48, 16}},
                  {{VK_SHADER_STAGE_GEOMETRY_BIT, 28, 36}, {28, 36}},
                  {{VK_SHADER_STAGE_FRAGMENT_BIT, 56, 8}, {60, 4}}},
                 PC_USE_STAGE_TESC | PC_USE_STAGE_TESE}};

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(unusedOverlapPCTestParams); ndx++)
            {
                graphicsTests->addChild(new PushConstantGraphicsOverlapTest(
                    testCtx, unusedOverlapPCTestParams[ndx].name, pipelineConstructionType,
                    unusedOverlapPCTestParams[ndx].count, unusedOverlapPCTestParams[ndx].range,
                    unusedOverlapPCTestParams[ndx].pcUsedStages));
            }
        }
    }

    pushConstantTests->addChild(graphicsTests.release());

    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        de::MovePtr<tcu::TestCaseGroup> computeTests(new tcu::TestCaseGroup(testCtx, "compute_pipeline"));
        for (const auto &params : computeParams)
        {
            computeTests->addChild(new PushConstantComputeTest(testCtx, params.name, params.type, params.range));
        }
        addOverwriteCase(computeTests.get(), testCtx, pipelineConstructionType, VK_PIPELINE_BIND_POINT_COMPUTE);
        pushConstantTests->addChild(computeTests.release());
    }

    de::MovePtr<tcu::TestCaseGroup> lifetimeTests(new tcu::TestCaseGroup(testCtx, "lifetime"));
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(lifetimeParams); ndx++)
    {
        lifetimeTests->addChild(new PushConstantLifetimeTest(testCtx, lifetimeParams[ndx].name,
                                                             pipelineConstructionType, lifetimeParams[ndx].range,
                                                             lifetimeParams[ndx].cmdList));
    }
    pushConstantTests->addChild(lifetimeTests.release());

    return pushConstantTests.release();
}

} // namespace pipeline
} // namespace vkt
