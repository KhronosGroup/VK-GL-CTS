/*------------------------------------------------------------------------
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
 * \brief Sparse buffer tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktSparseResourcesBufferSparseBinding.hpp"
#include "vktSparseResourcesBufferSparseResidency.hpp"
#include "vktSparseResourcesBufferMemoryAliasing.hpp"
#include "vktSparseResourcesBufferRebind.hpp"

#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTestLog.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deMath.h"

#include <algorithm>
#include <string>
#include <vector>
#include <map>

using namespace vk;
using de::MovePtr;
using de::SharedPtr;
using de::UniquePtr;
using tcu::IVec2;
using tcu::IVec4;
using tcu::Vec4;

namespace vkt
{
namespace sparse
{
namespace
{

typedef SharedPtr<UniquePtr<Allocation>> AllocationSp;

enum
{
    RENDER_SIZE = 128,             //!< framebuffer size in pixels
    GRID_SIZE   = RENDER_SIZE / 8, //!< number of grid tiles in a row
};

enum TestFlagBits
{
    //   sparseBinding is implied
    TEST_FLAG_ALIASED              = 1u << 0, //!< sparseResidencyAliased
    TEST_FLAG_RESIDENCY            = 1u << 1, //!< sparseResidencyBuffer
    TEST_FLAG_NON_RESIDENT_STRICT  = 1u << 2, //!< residencyNonResidentStrict
    TEST_FLAG_ENABLE_DEVICE_GROUPS = 1u << 3, //!< device groups are enabled
    TEST_FLAG_TRANSFORM_FEEDBACK   = 1u << 4, //!< require transform feedback extension
};
typedef uint32_t TestFlags;

enum class BufferObjectType
{
    BO_TYPE_UNIFORM = 0,
    BO_TYPE_STORAGE
};

struct TestParams
{
    TestFlags flags;
    BufferObjectType bufferType;
};

//! SparseAllocationBuilder output. Owns the allocated memory.
struct SparseAllocation
{
    uint32_t numResourceChunks;
    VkDeviceSize resourceSize;                   //!< buffer size in bytes
    std::vector<AllocationSp> allocations;       //!< actual allocated memory
    std::vector<VkSparseMemoryBind> memoryBinds; //!< memory binds backing the resource
    uint32_t memoryType;                         //!< memory type (same for all allocations)
    uint32_t heapIndex;                          //!< memory heap index
};

//! Utility to lay out memory allocations for a sparse buffer, including holes and aliased regions.
//! Will allocate memory upon building.
class SparseAllocationBuilder
{
public:
    SparseAllocationBuilder(void);

    // \note "chunk" is the smallest (due to alignment) bindable amount of memory

    SparseAllocationBuilder &addMemoryHole(const uint32_t numChunks = 1u);
    SparseAllocationBuilder &addResourceHole(const uint32_t numChunks = 1u);
    SparseAllocationBuilder &addMemoryBind(const uint32_t numChunks = 1u);
    SparseAllocationBuilder &addAliasedMemoryBind(const uint32_t allocationNdx, const uint32_t chunkOffset,
                                                  const uint32_t numChunks = 1u);
    SparseAllocationBuilder &addMemoryAllocation(void);

    MovePtr<SparseAllocation> build(
        const InstanceInterface &instanceInterface, const VkPhysicalDevice physicalDevice, const DeviceInterface &vk,
        const VkDevice device, Allocator &allocator,
        VkBufferCreateInfo referenceCreateInfo,        //!< buffer size is ignored in this info
        const VkDeviceSize minChunkSize = 0ull) const; //!< make sure chunks are at least this big

private:
    struct MemoryBind
    {
        uint32_t allocationNdx;
        uint32_t resourceChunkNdx;
        uint32_t memoryChunkNdx;
        uint32_t numChunks;
    };

    uint32_t m_allocationNdx;
    uint32_t m_resourceChunkNdx;
    uint32_t m_memoryChunkNdx;
    std::vector<MemoryBind> m_memoryBinds;
    std::vector<uint32_t> m_chunksPerAllocation;
};

SparseAllocationBuilder::SparseAllocationBuilder(void) : m_allocationNdx(0), m_resourceChunkNdx(0), m_memoryChunkNdx(0)
{
    m_chunksPerAllocation.push_back(0);
}

SparseAllocationBuilder &SparseAllocationBuilder::addMemoryHole(const uint32_t numChunks)
{
    m_memoryChunkNdx += numChunks;
    m_chunksPerAllocation[m_allocationNdx] += numChunks;

    return *this;
}

SparseAllocationBuilder &SparseAllocationBuilder::addResourceHole(const uint32_t numChunks)
{
    m_resourceChunkNdx += numChunks;

    return *this;
}

SparseAllocationBuilder &SparseAllocationBuilder::addMemoryAllocation(void)
{
    DE_ASSERT(m_memoryChunkNdx != 0); // doesn't make sense to have an empty allocation

    m_allocationNdx += 1;
    m_memoryChunkNdx = 0;
    m_chunksPerAllocation.push_back(0);

    return *this;
}

SparseAllocationBuilder &SparseAllocationBuilder::addMemoryBind(const uint32_t numChunks)
{
    const MemoryBind memoryBind = {m_allocationNdx, m_resourceChunkNdx, m_memoryChunkNdx, numChunks};
    m_memoryBinds.push_back(memoryBind);

    m_resourceChunkNdx += numChunks;
    m_memoryChunkNdx += numChunks;
    m_chunksPerAllocation[m_allocationNdx] += numChunks;

    return *this;
}

SparseAllocationBuilder &SparseAllocationBuilder::addAliasedMemoryBind(const uint32_t allocationNdx,
                                                                       const uint32_t chunkOffset,
                                                                       const uint32_t numChunks)
{
    DE_ASSERT(allocationNdx <= m_allocationNdx);

    const MemoryBind memoryBind = {allocationNdx, m_resourceChunkNdx, chunkOffset, numChunks};
    m_memoryBinds.push_back(memoryBind);

    m_resourceChunkNdx += numChunks;

    return *this;
}

MovePtr<SparseAllocation> SparseAllocationBuilder::build(const InstanceInterface &instanceInterface,
                                                         const VkPhysicalDevice physicalDevice,
                                                         const DeviceInterface &vk, const VkDevice device,
                                                         Allocator &allocator, VkBufferCreateInfo referenceCreateInfo,
                                                         const VkDeviceSize minChunkSize) const
{

    MovePtr<SparseAllocation> sparseAllocation(new SparseAllocation());

    referenceCreateInfo.size = sizeof(uint32_t);
    const Unique<VkBuffer> refBuffer(createBuffer(vk, device, &referenceCreateInfo));
    const VkMemoryRequirements memoryRequirements = getBufferMemoryRequirements(vk, device, *refBuffer);
    const VkDeviceSize chunkSize                  = std::max(
        memoryRequirements.alignment, static_cast<VkDeviceSize>(deAlign64(minChunkSize, memoryRequirements.alignment)));
    const uint32_t memoryTypeNdx =
        findMatchingMemoryType(instanceInterface, physicalDevice, memoryRequirements, MemoryRequirement::Any);
    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        memoryRequirements.size,                // VkDeviceSize allocationSize;
        memoryTypeNdx,                          // uint32_t memoryTypeIndex;
    };

    for (std::vector<uint32_t>::const_iterator numChunksIter = m_chunksPerAllocation.begin();
         numChunksIter != m_chunksPerAllocation.end(); ++numChunksIter)
    {
        allocInfo.allocationSize = *numChunksIter * chunkSize;
        sparseAllocation->allocations.push_back(makeDeSharedPtr(allocator.allocate(allocInfo, (VkDeviceSize)0)));
    }

    for (std::vector<MemoryBind>::const_iterator memBindIter = m_memoryBinds.begin();
         memBindIter != m_memoryBinds.end(); ++memBindIter)
    {
        const Allocation &alloc       = **sparseAllocation->allocations[memBindIter->allocationNdx];
        const VkSparseMemoryBind bind = {
            memBindIter->resourceChunkNdx * chunkSize,                   // VkDeviceSize               resourceOffset;
            memBindIter->numChunks * chunkSize,                          // VkDeviceSize               size;
            alloc.getMemory(),                                           // VkDeviceMemory             memory;
            alloc.getOffset() + memBindIter->memoryChunkNdx * chunkSize, // VkDeviceSize               memoryOffset;
            (VkSparseMemoryBindFlags)0,                                  // VkSparseMemoryBindFlags    flags;
        };
        sparseAllocation->memoryBinds.push_back(bind);
        referenceCreateInfo.size = std::max(referenceCreateInfo.size, bind.resourceOffset + bind.size);
    }

    sparseAllocation->resourceSize      = referenceCreateInfo.size;
    sparseAllocation->numResourceChunks = m_resourceChunkNdx;
    sparseAllocation->memoryType        = memoryTypeNdx;
    sparseAllocation->heapIndex         = getHeapIndexForMemoryType(instanceInterface, physicalDevice, memoryTypeNdx);

    return sparseAllocation;
}

VkImageCreateInfo makeImageCreateInfo(const VkFormat format, const IVec2 &size, const VkImageUsageFlags usage)
{
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0,               // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        makeExtent3D(size.x(), size.y(), 1), // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    return imageParams;
}

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                      const VkPipelineLayout pipelineLayout, const VkRenderPass renderPass,
                                      const IVec2 renderSize, const VkPrimitiveTopology topology,
                                      const uint32_t stageCount, const VkPipelineShaderStageCreateInfo *pStages)
{
    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t binding;
        sizeof(Vec4),                // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate inputRate;
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
        0u,                            // uint32_t location;
        0u,                            // uint32_t binding;
        VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
        0u,                            // uint32_t offset;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                                   // const void*                                 pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags;
        1u,                             // uint32_t                                    vertexBindingDescriptionCount;
        &vertexInputBindingDescription, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        1u,                             // uint32_t                                    vertexAttributeDescriptionCount;
        &vertexInputAttributeDescription, // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                    // const void*                                 pNext;
        (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags     flags;
        topology,                                   // VkPrimitiveTopology                         topology;
        VK_FALSE, // VkBool32                                    primitiveRestartEnable;
    };

    const VkViewport viewport = makeViewport(renderSize);
    const VkRect2D scissor    = makeRect2D(renderSize);

    const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType;
        nullptr,                                               // const void*                                 pNext;
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags;
        1u,        // uint32_t                                    viewportCount;
        &viewport, // const VkViewport*                           pViewports;
        1u,        // uint32_t                                    scissorCount;
        &scissor,  // const VkRect2D*                             pScissors;
    };

    const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                          sType;
        nullptr,                                                    // const void*                              pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags  flags;
        VK_FALSE,                        // VkBool32                                 depthClampEnable;
        VK_FALSE,                        // VkBool32                                 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,            // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,               // VkCullModeFlags cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace frontFace;
        VK_FALSE,                        // VkBool32 depthBiasEnable;
        0.0f,                            // float depthBiasConstantFactor;
        0.0f,                            // float depthBiasClamp;
        0.0f,                            // float depthBiasSlopeFactor;
        1.0f,                            // float lineWidth;
    };

    const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0,                 // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    const VkStencilOpState stencilOpState = makeStencilOpState(VK_STENCIL_OP_KEEP,   // stencil fail
                                                               VK_STENCIL_OP_KEEP,   // depth & stencil pass
                                                               VK_STENCIL_OP_KEEP,   // depth only fail
                                                               VK_COMPARE_OP_ALWAYS, // compare op
                                                               0u,                   // compare mask
                                                               0u,                   // write mask
                                                               0u);                  // reference

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        (VkPipelineDepthStencilStateCreateFlags)0,                  // VkPipelineDepthStencilStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthTestEnable;
        VK_FALSE,                                                   // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_LESS,                                         // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        stencilOpState,                                             // VkStencilOpState front;
        stencilOpState,                                             // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        1.0f,                                                       // float maxDepthBounds;
    };

    const VkColorComponentFlags colorComponentsAll =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
        VK_FALSE,             // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
        colorComponentsAll,   // VkColorComponentFlags colorWriteMask;
    };

    const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineColorBlendStateCreateFlags)0,                  // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        1u,                                                       // uint32_t attachmentCount;
        &pipelineColorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f},           // float blendConstants[4];
    };

    const VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags flags;
        stageCount,                                      // uint32_t stageCount;
        pStages,                                         // const VkPipelineShaderStageCreateInfo* pStages;
        &vertexInputStateInfo,           // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
        &pipelineInputAssemblyStateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
        nullptr,                         // const VkPipelineTessellationStateCreateInfo* pTessellationState;
        &pipelineViewportStateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
        &pipelineRasterizationStateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
        &pipelineMultisampleStateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
        &pipelineDepthStencilStateInfo,  // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
        &pipelineColorBlendStateInfo,    // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
        nullptr,                         // const VkPipelineDynamicStateCreateInfo* pDynamicState;
        pipelineLayout,                  // VkPipelineLayout layout;
        renderPass,                      // VkRenderPass renderPass;
        0u,                              // uint32_t subpass;
        VK_NULL_HANDLE,                  // VkPipeline basePipelineHandle;
        0,                               // int32_t basePipelineIndex;
    };

    return createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineInfo);
}

//! Return true if there are any red (or all zero) pixels in the image
bool imageHasErrorPixels(const tcu::ConstPixelBufferAccess image)
{
    const Vec4 errorColor = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    const Vec4 blankColor = Vec4();

    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const Vec4 color = image.getPixel(x, y);
            if (color == errorColor || color == blankColor)
                return true;
        }

    return false;
}

class Renderer
{
public:
    typedef std::map<VkShaderStageFlagBits, const VkSpecializationInfo *> SpecializationMap;

    //! Use the delegate to bind descriptor sets, vertex buffers, etc. and make a draw call
    struct Delegate
    {
        virtual ~Delegate(void)
        {
        }
        virtual void rendererDraw(const VkPipelineLayout pipelineLayout, const VkCommandBuffer cmdBuffer) const = 0;
    };

    Renderer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator, const uint32_t queueFamilyIndex,
             const VkDescriptorSetLayout descriptorSetLayout, //!< may be NULL, if no descriptors are used
             BinaryCollection &binaryCollection, const std::string &vertexName, const std::string &fragmentName,
             const VkBuffer colorBuffer, const IVec2 &renderSize, const VkFormat colorFormat, const Vec4 &clearColor,
             const VkPrimitiveTopology topology, SpecializationMap specMap = SpecializationMap())
        : m_colorBuffer(colorBuffer)
        , m_renderSize(renderSize)
        , m_colorFormat(colorFormat)
        , m_colorSubresourceRange(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u))
        , m_clearColor(clearColor)
        , m_topology(topology)
        , m_descriptorSetLayout(descriptorSetLayout)
    {
        m_colorImage =
            makeImage(vk, device,
                      makeImageCreateInfo(m_colorFormat, m_renderSize,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
        m_colorImageAlloc = bindImage(vk, device, allocator, *m_colorImage, MemoryRequirement::Any);
        m_colorAttachment =
            makeImageView(vk, device, *m_colorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, m_colorSubresourceRange);

        m_vertexModule   = createShaderModule(vk, device, binaryCollection.get(vertexName), 0u);
        m_fragmentModule = createShaderModule(vk, device, binaryCollection.get(fragmentName), 0u);

        const VkPipelineShaderStageCreateInfo pShaderStages[] = {
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                             // const void* pNext;
                (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
                VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits stage;
                *m_vertexModule,                                     // VkShaderModule module;
                "main",                                              // const char* pName;
                specMap[VK_SHADER_STAGE_VERTEX_BIT],                 // const VkSpecializationInfo* pSpecializationInfo;
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                             // const void* pNext;
                (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
                VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits stage;
                *m_fragmentModule,                                   // VkShaderModule module;
                "main",                                              // const char* pName;
                specMap[VK_SHADER_STAGE_FRAGMENT_BIT],               // const VkSpecializationInfo* pSpecializationInfo;
            }};

        m_renderPass = makeRenderPass(vk, device, m_colorFormat);
        m_framebuffer =
            makeFramebuffer(vk, device, *m_renderPass, m_colorAttachment.get(), static_cast<uint32_t>(m_renderSize.x()),
                            static_cast<uint32_t>(m_renderSize.y()));
        m_pipelineLayout = makePipelineLayout(vk, device, m_descriptorSetLayout);
        m_pipeline       = makeGraphicsPipeline(vk, device, *m_pipelineLayout, *m_renderPass, m_renderSize, m_topology,
                                                DE_LENGTH_OF_ARRAY(pShaderStages), pShaderStages);
        m_cmdPool        = makeCommandPool(vk, device, queueFamilyIndex);
        m_cmdBuffer      = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    }

    void draw(const DeviceInterface &vk, const VkDevice device, const VkQueue queue, const Delegate &drawDelegate,
              const bool useDeviceGroups, const uint32_t deviceID) const
    {
        beginCommandBuffer(vk, *m_cmdBuffer);

        beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer,
                        makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), m_clearColor);

        vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        drawDelegate.rendererDraw(*m_pipelineLayout, *m_cmdBuffer);

        endRenderPass(vk, *m_cmdBuffer);

        copyImageToBuffer(vk, *m_cmdBuffer, *m_colorImage, m_colorBuffer, m_renderSize);

        endCommandBuffer(vk, *m_cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *m_cmdBuffer, 0U, nullptr, nullptr, 0U, nullptr, useDeviceGroups,
                              deviceID);
    }

private:
    const VkBuffer m_colorBuffer;
    const IVec2 m_renderSize;
    const VkFormat m_colorFormat;
    const VkImageSubresourceRange m_colorSubresourceRange;
    const Vec4 m_clearColor;
    const VkPrimitiveTopology m_topology;
    const VkDescriptorSetLayout m_descriptorSetLayout;

    Move<VkImage> m_colorImage;
    MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorAttachment;
    Move<VkShaderModule> m_vertexModule;
    Move<VkShaderModule> m_fragmentModule;
    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkPipeline> m_pipeline;
    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;

    // "deleted"
    Renderer(const Renderer &);
    Renderer &operator=(const Renderer &);
};

void bindSparseBuffer(const DeviceInterface &vk, const VkDevice device, const VkQueue sparseQueue,
                      const VkBuffer buffer, const SparseAllocation &sparseAllocation, const bool useDeviceGroups,
                      uint32_t resourceDevId, uint32_t memoryDeviceId)
{
    const VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo = {
        buffer,                                                     // VkBuffer                     buffer;
        static_cast<uint32_t>(sparseAllocation.memoryBinds.size()), // uint32_t                     bindCount;
        &sparseAllocation.memoryBinds[0],                           // const VkSparseMemoryBind*    pBinds;
    };

    const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo = {
        VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, //VkStructureType sType;
        nullptr,                                         //const void* pNext;
        resourceDevId,                                   //uint32_t resourceDeviceIndex;
        memoryDeviceId,                                  //uint32_t memoryDeviceIndex;
    };

    const VkBindSparseInfo bindInfo = {
        VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,                  // VkStructureType                             sType;
        useDeviceGroups ? &devGroupBindSparseInfo : nullptr, // const void*                                 pNext;
        0u,                          // uint32_t                                    waitSemaphoreCount;
        nullptr,                     // const VkSemaphore*                          pWaitSemaphores;
        1u,                          // uint32_t                                    bufferBindCount;
        &sparseBufferMemoryBindInfo, // const VkSparseBufferMemoryBindInfo*         pBufferBinds;
        0u,                          // uint32_t                                    imageOpaqueBindCount;
        nullptr,                     // const VkSparseImageOpaqueMemoryBindInfo*    pImageOpaqueBinds;
        0u,                          // uint32_t                                    imageBindCount;
        nullptr,                     // const VkSparseImageMemoryBindInfo*          pImageBinds;
        0u,                          // uint32_t                                    signalSemaphoreCount;
        nullptr,                     // const VkSemaphore*                          pSignalSemaphores;
    };

    const Unique<VkFence> fence(createFence(vk, device));

    VK_CHECK(vk.queueBindSparse(sparseQueue, 1u, &bindInfo, *fence));
    VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), VK_TRUE, ~0ull));
}

class SparseBufferTestInstance : public SparseResourcesBaseInstance, Renderer::Delegate
{
public:
    SparseBufferTestInstance(Context &context, const TestFlags flags)
        : SparseResourcesBaseInstance(context, (flags & TEST_FLAG_ENABLE_DEVICE_GROUPS) != 0)
        , m_aliased((flags & TEST_FLAG_ALIASED) != 0)
        , m_residency((flags & TEST_FLAG_RESIDENCY) != 0)
        , m_nonResidentStrict((flags & TEST_FLAG_NON_RESIDENT_STRICT) != 0)
        , m_renderSize(RENDER_SIZE, RENDER_SIZE)
        , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
        , m_colorBufferSize(m_renderSize.x() * m_renderSize.y() * tcu::getPixelSize(mapVkFormat(m_colorFormat)))
    {
        {
            QueueRequirementsVec requirements;
            requirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
            requirements.push_back(QueueRequirements(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 1u));

            createDeviceSupportingQueues(requirements, false, false, flags & TEST_FLAG_TRANSFORM_FEEDBACK);
        }

        const DeviceInterface &vk = getDeviceInterface();

        m_sparseQueue    = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0u);
        m_universalQueue = getQueue(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0u);

        m_sharedQueueFamilyIndices[0] = m_sparseQueue.queueFamilyIndex;
        m_sharedQueueFamilyIndices[1] = m_universalQueue.queueFamilyIndex;

        m_colorBuffer = makeBuffer(vk, getDevice(), m_colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        m_colorBufferAlloc =
            bindBuffer(vk, getDevice(), getAllocator(), *m_colorBuffer, MemoryRequirement::HostVisible);

        deMemset(m_colorBufferAlloc->getHostPtr(), 0, static_cast<std::size_t>(m_colorBufferSize));
        flushAlloc(vk, getDevice(), *m_colorBufferAlloc);
    }

protected:
    VkBufferCreateInfo getSparseBufferCreateInfo(const VkBufferUsageFlags usage) const
    {
        VkBufferCreateFlags flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
        if (m_residency)
            flags |= VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
        if (m_aliased)
            flags |= VK_BUFFER_CREATE_SPARSE_ALIASED_BIT;

        VkBufferCreateInfo referenceBufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,     // VkStructureType        sType;
            nullptr,                                  // const void*            pNext;
            flags,                                    // VkBufferCreateFlags    flags;
            0u,                                       // override later // VkDeviceSize           size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, // VkBufferUsageFlags     usage;
            VK_SHARING_MODE_EXCLUSIVE,                // VkSharingMode          sharingMode;
            0u,                                       // uint32_t               queueFamilyIndexCount;
            nullptr,                                  // const uint32_t*        pQueueFamilyIndices;
        };

        if (m_sparseQueue.queueFamilyIndex != m_universalQueue.queueFamilyIndex)
        {
            referenceBufferCreateInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
            referenceBufferCreateInfo.queueFamilyIndexCount = DE_LENGTH_OF_ARRAY(m_sharedQueueFamilyIndices);
            referenceBufferCreateInfo.pQueueFamilyIndices   = m_sharedQueueFamilyIndices;
        }

        return referenceBufferCreateInfo;
    }

    void draw(const VkPrimitiveTopology topology, const VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE,
              Renderer::SpecializationMap specMap = Renderer::SpecializationMap(), bool useDeviceGroups = false,
              uint32_t deviceID = 0)
    {
        const UniquePtr<Renderer> renderer(
            new Renderer(getDeviceInterface(), getDevice(), getAllocator(), m_universalQueue.queueFamilyIndex,
                         descriptorSetLayout, m_context.getBinaryCollection(), "vert", "frag", *m_colorBuffer,
                         m_renderSize, m_colorFormat, Vec4(1.0f, 0.0f, 0.0f, 1.0f), topology, specMap));

        renderer->draw(getDeviceInterface(), getDevice(), m_universalQueue.queueHandle, *this, useDeviceGroups,
                       deviceID);
    }

    virtual bool isResultCorrect(void) const
    {
        invalidateAlloc(getDeviceInterface(), getDevice(), *m_colorBufferAlloc);

        const tcu::ConstPixelBufferAccess resultImage(mapVkFormat(m_colorFormat), m_renderSize.x(), m_renderSize.y(),
                                                      1u, m_colorBufferAlloc->getHostPtr());

        m_context.getTestContext().getLog() << tcu::LogImageSet("Result", "Result")
                                            << tcu::LogImage("color0", "", resultImage) << tcu::TestLog::EndImageSet;

        return !imageHasErrorPixels(resultImage);
    }

    const bool m_aliased;
    const bool m_residency;
    const bool m_nonResidentStrict;

    Queue m_sparseQueue;
    Queue m_universalQueue;

private:
    const IVec2 m_renderSize;
    const VkFormat m_colorFormat;
    const VkDeviceSize m_colorBufferSize;

    Move<VkBuffer> m_colorBuffer;
    MovePtr<Allocation> m_colorBufferAlloc;

    uint32_t m_sharedQueueFamilyIndices[2];
};

void initProgramsDrawWithBufferObject(vk::SourceCollections &programCollection, const TestParams testParams)
{
    // Vertex shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) in vec4 in_position;\n"
            << "\n"
            << "out gl_PerVertex {\n"
            << "    vec4 gl_Position;\n"
            << "};\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    gl_Position = in_position;\n"
            << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
    }

    // Fragment shader
    {
        const TestFlags flags        = testParams.flags;
        const bool aliased           = (flags & TEST_FLAG_ALIASED) != 0;
        const bool residency         = (flags & TEST_FLAG_RESIDENCY) != 0;
        const bool nonResidentStrict = (flags & TEST_FLAG_NON_RESIDENT_STRICT) != 0;
        const std::string valueExpr =
            (aliased ? "ivec4(3*(ndx % nonAliasedSize) ^ 127, 0, 0, 0)" : "ivec4(3*ndx ^ 127, 0, 0, 0)");
        const bool isReadWriteOp          = (testParams.bufferType == BufferObjectType::BO_TYPE_STORAGE);
        const std::string bufferTypeStr   = isReadWriteOp ? "buffer" : "uniform";
        const std::string bufferLayoutStr = isReadWriteOp ? "std430" : "std140";
        const std::string volatileStr     = isReadWriteOp ? "volatile " : "";

        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) out vec4 o_color;\n"
            << "\n"
            << "layout(constant_id = 1) const int dataSize  = 1;\n"
            << "layout(constant_id = 2) const int chunkSize = 1;\n"
            << "\n"
            << "layout(set = 0, binding = 0, " << bufferLayoutStr << ") " << bufferTypeStr << " SparseBuffer {\n"
            << "    " << volatileStr << "ivec4 data[dataSize];\n"
            << "} buff;\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    const int fragNdx        = int(gl_FragCoord.x) + " << RENDER_SIZE << " * int(gl_FragCoord.y);\n"
            << "    const int pageSize       = " << RENDER_SIZE << " * " << RENDER_SIZE << ";\n"
            << "    const int numChunks      = dataSize / chunkSize;\n";

        if (aliased)
            src << "    const int nonAliasedSize = (numChunks > 1 ? dataSize - chunkSize : dataSize);\n";

        src << "    bool      ok             = true;\n"
            << "\n"
            << "    for (int ndx = fragNdx; ndx < dataSize; ndx += pageSize)\n"
            << "    {\n";

        src << "        ivec4 readData = buff.data[ndx];\n"
            << "\n";

        if (isReadWriteOp)
        {
            src << "        // Write a new value based on index\n"
                << "        ivec4 newData = ivec4(ndx * 2 + 1, ndx ^ 0x55, ndx, 1);\n"
                << "        buff.data[ndx] = newData;\n"
                << "        ivec4 verifyData = buff.data[ndx];\n"
                << "\n";
        }

        if (residency && nonResidentStrict)
        {
            // Accessing non-resident regions
            src << "        if (ndx >= chunkSize && ndx < 2 * chunkSize)\n"
                << "            ok = ok && (readData == ivec4(0))"
                << (isReadWriteOp ? " && (verifyData == ivec4(0))" : "") << ";\n"
                << "        else\n"
                << "            ok = ok && (readData == " + valueExpr + ")"
                << (isReadWriteOp ? " && (verifyData == newData)" : "") << ";\n";
        }
        else if (residency)
        {
            src << "        if (ndx >= chunkSize && ndx < 2*chunkSize)\n"
                << "            continue;\n"
                << "        ok = ok && (readData == " << valueExpr << ")"
                << (isReadWriteOp ? " && (verifyData == newData)" : "") << ";\n";
        }
        else
            src << "        ok = ok && (readData == " << valueExpr << ")"
                << (isReadWriteOp ? " && (verifyData == newData)" : "") << ";\n";

        src << "    }\n"
            << "\n"
            << "    if (ok)\n"
            << "        o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
            << "    else\n"
            << "        o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
            << "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
    }
}

//! Sparse buffer backing a UBO or SSBO
class BufferObjectTestInstance : public SparseBufferTestInstance
{
public:
    BufferObjectTestInstance(Context &context, const TestParams testParams)
        : SparseBufferTestInstance(context, testParams.flags)
        , m_bufferType(testParams.bufferType)
    {
    }

    void rendererDraw(const VkPipelineLayout pipelineLayout, const VkCommandBuffer cmdBuffer) const
    {
        const DeviceInterface &vk       = getDeviceInterface();
        const VkDeviceSize vertexOffset = 0ull;

        vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &vertexOffset);
        vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0u, 1u,
                                 &m_descriptorSet.get(), 0u, nullptr);
        vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    }

    tcu::TestStatus iterate(void)
    {
        const InstanceInterface &instance = m_context.getInstanceInterface();
        const DeviceInterface &vk         = getDeviceInterface();
        MovePtr<SparseAllocation> sparseAllocation;
        Move<VkBuffer> sparseBuffer;
        Move<VkBuffer> sparseBufferAliased;
        bool setupDescriptors                     = true;
        const VkBufferUsageFlags bufferUsageFlags = (m_bufferType == BufferObjectType::BO_TYPE_UNIFORM) ?
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT :
                                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        const uint32_t maxBufferTypeRange         = (m_bufferType == BufferObjectType::BO_TYPE_UNIFORM) ?
                                                        m_context.getDeviceProperties().limits.maxUniformBufferRange :
                                                        m_context.getDeviceProperties().limits.maxStorageBufferRange;
        const VkDescriptorType descriptorType     = (m_bufferType == BufferObjectType::BO_TYPE_UNIFORM) ?
                                                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                                                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

        // Go through all physical devices
        for (uint32_t physDevID = 0; physDevID < m_numPhysicalDevices; physDevID++)
        {
            const uint32_t firstDeviceID  = physDevID;
            const uint32_t secondDeviceID = (firstDeviceID + 1) % m_numPhysicalDevices;

            // Set up the sparse buffer
            {
                VkBufferCreateInfo referenceBufferCreateInfo = getSparseBufferCreateInfo(bufferUsageFlags);
                const VkDeviceSize minChunkSize = 512u; // make sure the smallest allocation is at least this big
                uint32_t numMaxChunks           = 0u;

                // Check how many chunks we can allocate given the alignment and size requirements of UBOs or SSBOs
                {
                    const UniquePtr<SparseAllocation> minAllocation(SparseAllocationBuilder().addMemoryBind().build(
                        instance, getPhysicalDevice(secondDeviceID), vk, getDevice(), getAllocator(),
                        referenceBufferCreateInfo, minChunkSize));

                    numMaxChunks =
                        deMaxu32(static_cast<uint32_t>(maxBufferTypeRange / minAllocation->resourceSize), 1u);
                }

                if (numMaxChunks < 4)
                {
                    sparseAllocation = SparseAllocationBuilder().addMemoryBind().build(
                        instance, getPhysicalDevice(secondDeviceID), vk, getDevice(), getAllocator(),
                        referenceBufferCreateInfo, minChunkSize);
                }
                else
                {
                    // Try to use a non-trivial memory allocation scheme to make it different from a non-sparse binding
                    SparseAllocationBuilder builder;
                    builder.addMemoryBind();

                    if (m_residency)
                        builder.addResourceHole();

                    builder.addMemoryAllocation().addMemoryHole().addMemoryBind();

                    if (m_aliased)
                        builder.addAliasedMemoryBind(0u, 0u);

                    sparseAllocation = builder.build(instance, getPhysicalDevice(secondDeviceID), vk, getDevice(),
                                                     getAllocator(), referenceBufferCreateInfo, minChunkSize);
                    DE_ASSERT(sparseAllocation->resourceSize <= maxBufferTypeRange);
                }

                if (firstDeviceID != secondDeviceID)
                {
                    VkPeerMemoryFeatureFlags peerMemoryFeatureFlags = (VkPeerMemoryFeatureFlags)0;
                    vk.getDeviceGroupPeerMemoryFeatures(getDevice(), sparseAllocation->heapIndex, firstDeviceID,
                                                        secondDeviceID, &peerMemoryFeatureFlags);

                    if (((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT) == 0) ||
                        ((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT) == 0))
                    {
                        TCU_THROW(NotSupportedError, "Peer memory does not support COPY_DST and GENERIC_SRC");
                    }
                }

                // Create the buffer
                referenceBufferCreateInfo.size = sparseAllocation->resourceSize;
                sparseBuffer                   = makeBuffer(vk, getDevice(), referenceBufferCreateInfo);
                bindSparseBuffer(vk, getDevice(), m_sparseQueue.queueHandle, *sparseBuffer, *sparseAllocation,
                                 usingDeviceGroups(), firstDeviceID, secondDeviceID);

                if (m_aliased)
                {
                    sparseBufferAliased = makeBuffer(vk, getDevice(), referenceBufferCreateInfo);
                    bindSparseBuffer(vk, getDevice(), m_sparseQueue.queueHandle, *sparseBufferAliased,
                                     *sparseAllocation, usingDeviceGroups(), firstDeviceID, secondDeviceID);
                }
            }

            // Set uniform data
            {
                const bool hasAliasedChunk   = (m_aliased && sparseAllocation->memoryBinds.size() > 1u);
                const VkDeviceSize chunkSize = sparseAllocation->resourceSize / sparseAllocation->numResourceChunks;
                const VkDeviceSize stagingBufferSize =
                    sparseAllocation->resourceSize - (hasAliasedChunk ? chunkSize : 0);
                const uint32_t numBufferEntries = static_cast<uint32_t>(stagingBufferSize / sizeof(IVec4));

                const Unique<VkBuffer> stagingBuffer(
                    makeBuffer(vk, getDevice(), stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
                const UniquePtr<Allocation> stagingBufferAlloc(
                    bindBuffer(vk, getDevice(), getAllocator(), *stagingBuffer, MemoryRequirement::HostVisible));

                {
                    // If aliased chunk is used, the staging buffer is smaller than the sparse buffer and we don't overwrite the last chunk
                    IVec4 *const pData = static_cast<IVec4 *>(stagingBufferAlloc->getHostPtr());
                    for (uint32_t i = 0; i < numBufferEntries; ++i)
                        pData[i] = IVec4(3 * i ^ 127, 0, 0, 0);

                    flushAlloc(vk, getDevice(), *stagingBufferAlloc);

                    const VkBufferCopy copyRegion = {
                        0ull,              // VkDeviceSize    srcOffset;
                        0ull,              // VkDeviceSize    dstOffset;
                        stagingBufferSize, // VkDeviceSize    size;
                    };

                    const Unique<VkCommandPool> cmdPool(
                        makeCommandPool(vk, getDevice(), m_universalQueue.queueFamilyIndex));
                    const Unique<VkCommandBuffer> cmdBuffer(
                        allocateCommandBuffer(vk, getDevice(), *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

                    beginCommandBuffer(vk, *cmdBuffer);
                    vk.cmdCopyBuffer(*cmdBuffer, *stagingBuffer, *sparseBuffer, 1u, &copyRegion);
                    endCommandBuffer(vk, *cmdBuffer);

                    submitCommandsAndWait(vk, getDevice(), m_universalQueue.queueHandle, *cmdBuffer, 0u, nullptr,
                                          nullptr, 0, nullptr, usingDeviceGroups(), firstDeviceID);
                    // Once the fence is signaled, the write is also available to the aliasing buffer.
                }
            }

            // Make sure that we don't try to access a larger range than is allowed. This only applies to a single chunk case.
            const uint32_t maxBufferRange =
                deMinu32(static_cast<uint32_t>(sparseAllocation->resourceSize), maxBufferTypeRange);

            // Descriptor sets
            {
                // Setup only once
                if (setupDescriptors)
                {
                    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                                .addSingleBinding(descriptorType, VK_SHADER_STAGE_FRAGMENT_BIT)
                                                .build(vk, getDevice());

                    m_descriptorPool =
                        DescriptorPoolBuilder()
                            .addType(descriptorType)
                            .build(vk, getDevice(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

                    m_descriptorSet  = makeDescriptorSet(vk, getDevice(), *m_descriptorPool, *m_descriptorSetLayout);
                    setupDescriptors = false;
                }

                const VkBuffer buffer                         = (m_aliased ? *sparseBufferAliased : *sparseBuffer);
                const VkDescriptorBufferInfo sparseBufferInfo = makeDescriptorBufferInfo(buffer, 0ull, maxBufferRange);

                DescriptorSetUpdateBuilder()
                    .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                                 &sparseBufferInfo)
                    .update(vk, getDevice());
            }

            // Vertex data
            {
                const Vec4 vertexData[] = {
                    Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
                    Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
                    Vec4(1.0f, -1.0f, 0.0f, 1.0f),
                    Vec4(1.0f, 1.0f, 0.0f, 1.0f),
                };

                const VkDeviceSize vertexBufferSize = sizeof(vertexData);

                m_vertexBuffer = makeBuffer(vk, getDevice(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
                m_vertexBufferAlloc =
                    bindBuffer(vk, getDevice(), getAllocator(), *m_vertexBuffer, MemoryRequirement::HostVisible);

                deMemcpy(m_vertexBufferAlloc->getHostPtr(), &vertexData[0], vertexBufferSize);
                flushAlloc(vk, getDevice(), *m_vertexBufferAlloc);
            }

            // Draw
            {
                std::vector<int32_t> specializationData;
                {
                    const uint32_t numBufferEntries   = maxBufferRange / static_cast<uint32_t>(sizeof(IVec4));
                    const uint32_t numEntriesPerChunk = numBufferEntries / sparseAllocation->numResourceChunks;

                    specializationData.push_back(numBufferEntries);
                    specializationData.push_back(numEntriesPerChunk);
                }

                const VkSpecializationMapEntry specMapEntries[] = {
                    {
                        1u,              // uint32_t    constantID;
                        0u,              // uint32_t    offset;
                        sizeof(int32_t), // size_t      size;
                    },
                    {
                        2u,              // uint32_t    constantID;
                        sizeof(int32_t), // uint32_t    offset;
                        sizeof(int32_t), // size_t      size;
                    },
                };

                const VkSpecializationInfo specInfo = {
                    DE_LENGTH_OF_ARRAY(specMapEntries),   // uint32_t                           mapEntryCount;
                    specMapEntries,                       // const VkSpecializationMapEntry*    pMapEntries;
                    sizeInBytes(specializationData),      // size_t                             dataSize;
                    getDataOrNullptr(specializationData), // const void*                        pData;
                };

                Renderer::SpecializationMap specMap;
                specMap[VK_SHADER_STAGE_FRAGMENT_BIT] = &specInfo;

                draw(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, *m_descriptorSetLayout, specMap, usingDeviceGroups(),
                     firstDeviceID);
            }

            if (!isResultCorrect())
                return tcu::TestStatus::fail("Some buffer values were incorrect");
        }
        return tcu::TestStatus::pass("Pass");
    }

private:
    const BufferObjectType m_bufferType;
    Move<VkBuffer> m_vertexBuffer;
    MovePtr<Allocation> m_vertexBufferAlloc;

    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
};

void initProgramsDrawGrid(vk::SourceCollections &programCollection, const TestFlags flags)
{
    DE_UNREF(flags);

    // Vertex shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) in  vec4 in_position;\n"
            << "layout(location = 0) out int  out_ndx;\n"
            << "\n"
            << "out gl_PerVertex {\n"
            << "    vec4 gl_Position;\n"
            << "};\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    gl_Position = in_position;\n"
            << "    out_ndx     = gl_VertexIndex;\n"
            << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
    }

    // Fragment shader
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) flat in  int  in_ndx;\n"
            << "layout(location = 0)      out vec4 o_color;\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    if (in_ndx % 2 == 0)\n"
            << "        o_color = vec4(vec3(1.0), 1.0);\n"
            << "    else\n"
            << "        o_color = vec4(vec3(0.75), 1.0);\n"
            << "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
    }
}

//! Generate vertex positions for a grid of tiles composed of two triangles each (6 vertices)
void generateGrid(void *pRawData, const float step, const float ox, const float oy, const uint32_t numX,
                  const uint32_t numY, const float z = 0.0f)
{
    typedef Vec4(*TilePtr)[6];

    TilePtr const pData = static_cast<TilePtr>(pRawData);
    {
        for (uint32_t iy = 0; iy < numY; ++iy)
            for (uint32_t ix = 0; ix < numX; ++ix)
            {
                const uint32_t ndx = ix + numX * iy;
                const float x      = ox + step * static_cast<float>(ix);
                const float y      = oy + step * static_cast<float>(iy);

                pData[ndx][0] = Vec4(x + step, y, z, 1.0f);
                pData[ndx][1] = Vec4(x, y, z, 1.0f);
                pData[ndx][2] = Vec4(x, y + step, z, 1.0f);

                pData[ndx][3] = Vec4(x, y + step, z, 1.0f);
                pData[ndx][4] = Vec4(x + step, y + step, z, 1.0f);
                pData[ndx][5] = Vec4(x + step, y, z, 1.0f);
            }
    }
}

//! Base test for a sparse buffer backing a vertex/index buffer
class DrawGridTestInstance : public SparseBufferTestInstance
{
public:
    DrawGridTestInstance(Context &context, const TestFlags flags, const VkBufferUsageFlags usage,
                         const VkDeviceSize minChunkSize)
        : SparseBufferTestInstance(context, flags)
        , m_bufferUsage(usage)
        , m_minChunkSize(minChunkSize)
        , m_perDrawBufferOffset(0)
        , m_stagingBufferSize(0)
    {
    }

    void createResources(uint32_t memoryDeviceIndex)
    {
        const InstanceInterface &instance            = m_context.getInstanceInterface();
        const DeviceInterface &vk                    = getDeviceInterface();
        VkBufferCreateInfo referenceBufferCreateInfo = getSparseBufferCreateInfo(m_bufferUsage);

        {
            // Allocate two chunks, each covering half of the viewport
            SparseAllocationBuilder builder;
            builder.addMemoryBind();

            if (m_residency)
                builder.addResourceHole();

            builder.addMemoryAllocation().addMemoryHole().addMemoryBind();

            if (m_aliased)
                builder.addAliasedMemoryBind(0u, 0u);

            m_sparseAllocation = builder.build(instance, getPhysicalDevice(memoryDeviceIndex), vk, getDevice(),
                                               getAllocator(), referenceBufferCreateInfo, m_minChunkSize);
        }

        // Create the buffer
        referenceBufferCreateInfo.size = m_sparseAllocation->resourceSize;
        m_sparseBuffer                 = makeBuffer(vk, getDevice(), referenceBufferCreateInfo);

        m_perDrawBufferOffset = m_sparseAllocation->resourceSize / m_sparseAllocation->numResourceChunks;
        m_stagingBufferSize   = 2 * m_perDrawBufferOffset;
        m_stagingBuffer       = makeBuffer(vk, getDevice(), m_stagingBufferSize,
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        m_stagingBufferAlloc =
            bindBuffer(vk, getDevice(), getAllocator(), *m_stagingBuffer, MemoryRequirement::HostVisible);
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk = getDeviceInterface();

        for (uint32_t physDevID = 0; physDevID < m_numPhysicalDevices; physDevID++)
        {
            const uint32_t firstDeviceID  = physDevID;
            const uint32_t secondDeviceID = (firstDeviceID + 1) % m_numPhysicalDevices;

            createResources(secondDeviceID);

            if (firstDeviceID != secondDeviceID)
            {
                VkPeerMemoryFeatureFlags peerMemoryFeatureFlags = (VkPeerMemoryFeatureFlags)0;
                vk.getDeviceGroupPeerMemoryFeatures(getDevice(), m_sparseAllocation->heapIndex, firstDeviceID,
                                                    secondDeviceID, &peerMemoryFeatureFlags);

                if (((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT) == 0) ||
                    ((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT) == 0))
                {
                    TCU_THROW(NotSupportedError, "Peer memory does not support COPY_DST and GENERIC_SRC");
                }
            }

            // Bind the memory
            bindSparseBuffer(vk, getDevice(), m_sparseQueue.queueHandle, *m_sparseBuffer, *m_sparseAllocation,
                             usingDeviceGroups(), firstDeviceID, secondDeviceID);

            initializeBuffers();

            // Upload to the sparse buffer
            {
                flushAlloc(vk, getDevice(), *m_stagingBufferAlloc);

                VkDeviceSize firstChunkOffset  = 0ull;
                VkDeviceSize secondChunkOffset = m_perDrawBufferOffset;

                if (m_residency)
                    secondChunkOffset += m_perDrawBufferOffset;

                if (m_aliased)
                    firstChunkOffset = secondChunkOffset + m_perDrawBufferOffset;

                const VkBufferCopy copyRegions[] = {
                    {
                        0ull,                  // VkDeviceSize    srcOffset;
                        firstChunkOffset,      // VkDeviceSize    dstOffset;
                        m_perDrawBufferOffset, // VkDeviceSize    size;
                    },
                    {
                        m_perDrawBufferOffset, // VkDeviceSize    srcOffset;
                        secondChunkOffset,     // VkDeviceSize    dstOffset;
                        m_perDrawBufferOffset, // VkDeviceSize    size;
                    },
                };

                const Unique<VkCommandPool> cmdPool(
                    makeCommandPool(vk, getDevice(), m_universalQueue.queueFamilyIndex));
                const Unique<VkCommandBuffer> cmdBuffer(
                    allocateCommandBuffer(vk, getDevice(), *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

                beginCommandBuffer(vk, *cmdBuffer);
                vk.cmdCopyBuffer(*cmdBuffer, *m_stagingBuffer, *m_sparseBuffer, DE_LENGTH_OF_ARRAY(copyRegions),
                                 copyRegions);
                endCommandBuffer(vk, *cmdBuffer);

                submitCommandsAndWait(vk, getDevice(), m_universalQueue.queueHandle, *cmdBuffer, 0u, nullptr, nullptr,
                                      0, nullptr, usingDeviceGroups(), firstDeviceID);
            }

            Renderer::SpecializationMap specMap;
            draw(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_NULL_HANDLE, specMap, usingDeviceGroups(), firstDeviceID);

            if (!isResultCorrect())
                return tcu::TestStatus::fail("Some buffer values were incorrect");
        }
        return tcu::TestStatus::pass("Pass");
    }

protected:
    virtual void initializeBuffers(void) = 0;

    const VkBufferUsageFlags m_bufferUsage;
    const VkDeviceSize m_minChunkSize;

    VkDeviceSize m_perDrawBufferOffset;

    VkDeviceSize m_stagingBufferSize;
    Move<VkBuffer> m_stagingBuffer;
    MovePtr<Allocation> m_stagingBufferAlloc;

    MovePtr<SparseAllocation> m_sparseAllocation;
    Move<VkBuffer> m_sparseBuffer;
};

//! Sparse buffer backing a vertex input buffer
class VertexBufferTestInstance : public DrawGridTestInstance
{
public:
    VertexBufferTestInstance(Context &context, const TestFlags flags)
        : DrawGridTestInstance(context, flags, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               GRID_SIZE * GRID_SIZE * 6 * sizeof(Vec4))
    {
    }

    void rendererDraw(const VkPipelineLayout pipelineLayout, const VkCommandBuffer cmdBuffer) const
    {
        DE_UNREF(pipelineLayout);

        m_context.getTestContext().getLog()
            << tcu::TestLog::Message
            << "Drawing a grid of triangles backed by a sparse vertex buffer. There should be no red pixels visible."
            << tcu::TestLog::EndMessage;

        const DeviceInterface &vk  = getDeviceInterface();
        const uint32_t vertexCount = 6 * (GRID_SIZE * GRID_SIZE) / 2;
        VkDeviceSize vertexOffset  = 0ull;

        vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_sparseBuffer.get(), &vertexOffset);
        vk.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);

        vertexOffset += m_perDrawBufferOffset * (m_residency ? 2 : 1);

        vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_sparseBuffer.get(), &vertexOffset);
        vk.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
    }

    void initializeBuffers(void)
    {
        uint8_t *pData   = static_cast<uint8_t *>(m_stagingBufferAlloc->getHostPtr());
        const float step = 2.0f / static_cast<float>(GRID_SIZE);

        // Prepare data for two draw calls
        generateGrid(pData, step, -1.0f, -1.0f, GRID_SIZE, GRID_SIZE / 2);
        generateGrid(pData + m_perDrawBufferOffset, step, -1.0f, 0.0f, GRID_SIZE, GRID_SIZE / 2);
    }
};

//! Sparse buffer backing an index buffer
class IndexBufferTestInstance : public DrawGridTestInstance
{
public:
    IndexBufferTestInstance(Context &context, const TestFlags flags)
        : DrawGridTestInstance(context, flags, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                               GRID_SIZE * GRID_SIZE * 6 * sizeof(uint32_t))
        , m_halfVertexCount(6 * (GRID_SIZE * GRID_SIZE) / 2)
    {
    }

    void rendererDraw(const VkPipelineLayout pipelineLayout, const VkCommandBuffer cmdBuffer) const
    {
        DE_UNREF(pipelineLayout);

        m_context.getTestContext().getLog()
            << tcu::TestLog::Message
            << "Drawing a grid of triangles from a sparse index buffer. There should be no red pixels visible."
            << tcu::TestLog::EndMessage;

        const DeviceInterface &vk       = getDeviceInterface();
        const VkDeviceSize vertexOffset = 0ull;
        VkDeviceSize indexOffset        = 0ull;

        vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &vertexOffset);

        vk.cmdBindIndexBuffer(cmdBuffer, *m_sparseBuffer, indexOffset, VK_INDEX_TYPE_UINT32);
        vk.cmdDrawIndexed(cmdBuffer, m_halfVertexCount, 1u, 0u, 0, 0u);

        indexOffset += m_perDrawBufferOffset * (m_residency ? 2 : 1);

        vk.cmdBindIndexBuffer(cmdBuffer, *m_sparseBuffer, indexOffset, VK_INDEX_TYPE_UINT32);
        vk.cmdDrawIndexed(cmdBuffer, m_halfVertexCount, 1u, 0u, 0, 0u);
    }

    void initializeBuffers(void)
    {
        // Vertex buffer
        const DeviceInterface &vk           = getDeviceInterface();
        const VkDeviceSize vertexBufferSize = 2 * m_halfVertexCount * sizeof(Vec4);
        m_vertexBuffer = makeBuffer(vk, getDevice(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        m_vertexBufferAlloc =
            bindBuffer(vk, getDevice(), getAllocator(), *m_vertexBuffer, MemoryRequirement::HostVisible);

        {
            const float step = 2.0f / static_cast<float>(GRID_SIZE);

            generateGrid(m_vertexBufferAlloc->getHostPtr(), step, -1.0f, -1.0f, GRID_SIZE, GRID_SIZE);

            flushAlloc(vk, getDevice(), *m_vertexBufferAlloc);
        }

        // Sparse index buffer
        for (uint32_t chunkNdx = 0u; chunkNdx < 2; ++chunkNdx)
        {
            uint8_t *const pData =
                static_cast<uint8_t *>(m_stagingBufferAlloc->getHostPtr()) + chunkNdx * m_perDrawBufferOffset;
            uint32_t *const pIndexData = reinterpret_cast<uint32_t *>(pData);
            const uint32_t ndxBase     = chunkNdx * m_halfVertexCount;

            for (uint32_t i = 0u; i < m_halfVertexCount; ++i)
                pIndexData[i] = ndxBase + i;
        }
    }

private:
    const uint32_t m_halfVertexCount;
    Move<VkBuffer> m_vertexBuffer;
    MovePtr<Allocation> m_vertexBufferAlloc;
};

//! Draw from a sparse indirect buffer
class IndirectBufferTestInstance : public DrawGridTestInstance
{
public:
    IndirectBufferTestInstance(Context &context, const TestFlags flags)
        : DrawGridTestInstance(context, flags, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, sizeof(VkDrawIndirectCommand))
    {
    }

    void rendererDraw(const VkPipelineLayout pipelineLayout, const VkCommandBuffer cmdBuffer) const
    {
        DE_UNREF(pipelineLayout);

        m_context.getTestContext().getLog()
            << tcu::TestLog::Message
            << "Drawing two triangles covering the whole viewport. There should be no red pixels visible."
            << tcu::TestLog::EndMessage;

        const DeviceInterface &vk       = getDeviceInterface();
        const VkDeviceSize vertexOffset = 0ull;
        VkDeviceSize indirectOffset     = 0ull;

        vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &vertexOffset);
        vk.cmdDrawIndirect(cmdBuffer, *m_sparseBuffer, indirectOffset, 1u, 0u);

        indirectOffset += m_perDrawBufferOffset * (m_residency ? 2 : 1);

        vk.cmdDrawIndirect(cmdBuffer, *m_sparseBuffer, indirectOffset, 1u, 0u);
    }

    void initializeBuffers(void)
    {
        // Vertex buffer
        const DeviceInterface &vk           = getDeviceInterface();
        const VkDeviceSize vertexBufferSize = 2 * 3 * sizeof(Vec4);
        m_vertexBuffer = makeBuffer(vk, getDevice(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        m_vertexBufferAlloc =
            bindBuffer(vk, getDevice(), getAllocator(), *m_vertexBuffer, MemoryRequirement::HostVisible);

        {
            generateGrid(m_vertexBufferAlloc->getHostPtr(), 2.0f, -1.0f, -1.0f, 1, 1);
            flushAlloc(vk, getDevice(), *m_vertexBufferAlloc);
        }

        // Indirect buffer
        for (uint32_t chunkNdx = 0u; chunkNdx < 2; ++chunkNdx)
        {
            uint8_t *const pData =
                static_cast<uint8_t *>(m_stagingBufferAlloc->getHostPtr()) + chunkNdx * m_perDrawBufferOffset;
            VkDrawIndirectCommand *const pCmdData = reinterpret_cast<VkDrawIndirectCommand *>(pData);

            pCmdData->firstVertex   = 3u * chunkNdx;
            pCmdData->firstInstance = 0u;
            pCmdData->vertexCount   = 3u;
            pCmdData->instanceCount = 1u;
        }
    }

private:
    Move<VkBuffer> m_vertexBuffer;
    MovePtr<Allocation> m_vertexBufferAlloc;
};

//! Use sparse transform feedback buffer
class TransformFeedbackTestInstance : public DrawGridTestInstance
{
public:
    TransformFeedbackTestInstance(Context &context, const TestFlags flags)
        : DrawGridTestInstance(context, flags,
                               VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               GRID_SIZE * GRID_SIZE * 6 * sizeof(uint32_t))
    {
    }
    ~TransformFeedbackTestInstance() = default;

    void rendererDraw(const VkPipelineLayout, const VkCommandBuffer cmdBuffer) const
    {
        const DeviceInterface &vk  = getDeviceInterface();
        const uint32_t vertexCount = 6 * GRID_SIZE * GRID_SIZE;
        VkDeviceSize vertexOffset  = 0ull;

        VkDeviceSize secondChunkOffset = m_perDrawBufferOffset;
        if (m_residency)
            secondChunkOffset += m_perDrawBufferOffset;

        vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &vertexOffset);
        vk.cmdBindTransformFeedbackBuffersEXT(cmdBuffer, 0, 1, &*m_sparseBuffer, &secondChunkOffset,
                                              &m_perDrawBufferOffset);

        vk.cmdBeginTransformFeedbackEXT(cmdBuffer, 0, 0, nullptr, nullptr);
        vk.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
        vk.cmdEndTransformFeedbackEXT(cmdBuffer, 0, 0, nullptr, nullptr);
    }

    void initializeBuffers(void)
    {
        // Create vertex buffer
        const auto device                   = getDevice();
        const DeviceInterface &vk           = getDeviceInterface();
        const VkDeviceSize vertexBufferSize = GRID_SIZE * GRID_SIZE * 6 * sizeof(Vec4);
        m_vertexBuffer      = makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        m_vertexBufferAlloc = bindBuffer(vk, device, getAllocator(), *m_vertexBuffer, MemoryRequirement::HostVisible);

        const float step = 2.0f / static_cast<float>(GRID_SIZE);
        generateGrid(m_vertexBufferAlloc->getHostPtr(), step, -1.0f, -1.0f, GRID_SIZE, GRID_SIZE);
        flushAlloc(vk, getDevice(), *m_vertexBufferAlloc);

        // Sparse buffer that will be used for transform feedback is filled with random data from staging buffer
    }

    virtual bool isResultCorrect(void) const
    {
        const auto device         = getDevice();
        const DeviceInterface &vk = getDeviceInterface();

        VkDeviceSize secondChunkOffset = m_perDrawBufferOffset;
        if (m_residency)
            secondChunkOffset += m_perDrawBufferOffset;

        const VkBufferCopy copyRegion{
            secondChunkOffset,     // VkDeviceSize    srcOffset;
            0ull,                  // VkDeviceSize    dstOffset;
            m_perDrawBufferOffset, // VkDeviceSize    size;
        };

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, m_universalQueue.queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        beginCommandBuffer(vk, *cmdBuffer);
        vk.cmdCopyBuffer(*cmdBuffer, *m_sparseBuffer, *m_stagingBuffer, 1u, &copyRegion);
        endCommandBuffer(vk, *cmdBuffer);

        submitCommandsAndWait(vk, device, m_universalQueue.queueHandle, *cmdBuffer, 0u, nullptr, nullptr, 0, nullptr,
                              false, 0);

        // verify selected number of items
        const uint32_t vertexCount = 6 * GRID_SIZE * GRID_SIZE;
        invalidateAlloc(vk, device, *m_stagingBufferAlloc);
        uint32_t *const pData = static_cast<uint32_t *>(m_stagingBufferAlloc->getHostPtr());
        for (uint32_t i = 0; i < vertexCount; i += (i < GRID_SIZE ? 1 : GRID_SIZE))
        {
            if (pData[i] != i)
                return false;
        }

        return true;
    }

private:
    Move<VkBuffer> m_vertexBuffer;
    MovePtr<Allocation> m_vertexBufferAlloc;
};

void initTransformFeedbackPrograms(vk::SourceCollections &programCollection, const TestFlags flags)
{
    DE_UNREF(flags);

    // Vertex shader
    std::string vertSrc = "#version 450\n"
                          "layout(location = 0) in vec4 in_position;\n"
                          "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0) out uint out_ndx;\n"
                          "out gl_PerVertex {\n"
                          "    vec4 gl_Position;\n"
                          "};\n"
                          "void main(void)\n"
                          "{\n"
                          "    gl_Position = in_position;\n"
                          "    out_ndx     = gl_VertexIndex;\n"
                          "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vertSrc);

    // Fragment shader
    std::string fragSrc = "#version 450\n"
                          "layout(location = 0) out vec4 o_color;\n"
                          "void main(void)\n"
                          "{\n"
                          "    o_color = vec4(1.0);\n"
                          "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(fragSrc);
}

//! Use sparse buffer for indirectdispatch
class IndirectDispatchTestInstance : public SparseResourcesBaseInstance
{
public:
    IndirectDispatchTestInstance(Context &context, const TestFlags flags);
    ~IndirectDispatchTestInstance() = default;

    tcu::TestStatus iterate(void);

protected:
    const bool m_residency;
    Queue m_sparseQueue;
    Queue m_computeQueue;
};

IndirectDispatchTestInstance::IndirectDispatchTestInstance(Context &context, const TestFlags flags)
    : SparseResourcesBaseInstance(context)
    , m_residency(flags & TEST_FLAG_RESIDENCY)
{
    QueueRequirementsVec requirements{QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u),
                                      QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u)};
    createDeviceSupportingQueues(requirements);

    m_sparseQueue  = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0u);
    m_computeQueue = getQueue(VK_QUEUE_COMPUTE_BIT, 0u);
}

tcu::TestStatus IndirectDispatchTestInstance::iterate(void)
{
    const DeviceInterface &vk = getDeviceInterface();
    const auto device         = getDevice();

    // create buffer that will be used as output for compute shader and as staging buffer for sparse buffer
    uint32_t outputItemCount     = 15u;
    VkDeviceSize inoutBufferSize = outputItemCount * sizeof(uint32_t);
    VkBufferUsageFlags inoutUsage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto bufferCreateInfo  = makeBufferCreateInfo(inoutBufferSize, inoutUsage);
    const auto inoutBuffer = createBuffer(vk, device, &bufferCreateInfo);
    const auto inoutBufferAlloc(bindBuffer(vk, device, getAllocator(), *inoutBuffer, MemoryRequirement::HostVisible));

    // create sparse buffer that will be used for indirect dispatch
    VkDeviceSize sparseBufferSize  = 1 << 18;
    VkBufferUsageFlags sparseUsage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferCreateFlags flags      = VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
    if (m_residency)
        flags |= VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
    bufferCreateInfo        = makeBufferCreateInfo(sparseBufferSize, sparseUsage, flags);
    const auto sparseBuffer = createBuffer(vk, device, &bufferCreateInfo);

    VkMemoryRequirements memoryRequirements = getBufferMemoryRequirements(vk, device, *sparseBuffer);
    auto sparseChunkSize                    = memoryRequirements.alignment;

    // allocate single chunk deliberately leaving hole at the beginning of resource
    memoryRequirements.size            = sparseChunkSize;
    de::MovePtr<Allocation> allocation = getAllocator().allocate(memoryRequirements, MemoryRequirement::Any);
    VkSparseMemoryBind sparseMemoryBind{sparseChunkSize,         // resourceOffset - define hole
                                        sparseChunkSize,         // size
                                        allocation->getMemory(), // memory
                                        0,                       // memoryOffset
                                        0};                      // flags

    const VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo{*sparseBuffer, 1, &sparseMemoryBind};
    VkBindSparseInfo bindInfo = initVulkanStructure();
    bindInfo.bufferBindCount  = 1;
    bindInfo.pBufferBinds     = &sparseBufferMemoryBindInfo;

    // bind memory to sparse buffer
    const Unique<VkFence> fence(createFence(vk, device));
    VK_CHECK(vk.queueBindSparse(m_sparseQueue.queueHandle, 1u, &bindInfo, *fence));
    VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), VK_TRUE, ~0ull));

    // copy data for indirect dispatch to output buffer
    const VkDispatchIndirectCommand indirectCommand{1u, outputItemCount, 1u};
    deMemcpy(inoutBufferAlloc->getHostPtr(), &indirectCommand, sizeof(indirectCommand));
    flushAlloc(vk, device, *inoutBufferAlloc);

    // create descriptor set
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));
    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo inoutBufferInfo = makeDescriptorBufferInfo(*inoutBuffer, 0ull, inoutBufferSize);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inoutBufferInfo)
        .update(vk, device);

    // create compute pipeline
    const auto shaderModule    = createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"));
    const auto pipelineLayout  = makePipelineLayout(vk, device, *descriptorSetLayout);
    const auto computePipeline = makeComputePipeline(vk, device, *pipelineLayout, *shaderModule);

    // create command buffer for compute
    const auto cmdPool(makeCommandPool(vk, device, m_computeQueue.queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    auto bp = VK_PIPELINE_BIND_POINT_COMPUTE;
    beginCommandBuffer(vk, *cmdBuffer);

    // wait for inout buffer beeing ready with dispatch values
    const auto inBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                         *inoutBuffer, 0ull, inoutBufferSize);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u,
                          &inBufferBarrier, 0u, nullptr);

    // copy indirect dispatch data to location in sparse buffer that has bound memory (we offset it by additional 4 bytes just for test)
    VkDeviceSize dispatchDataOffset = sparseChunkSize + 4u;
    const VkBufferCopy copyRegion{
        0ull,                 // VkDeviceSize    srcOffset;
        dispatchDataOffset,   // VkDeviceSize    dstOffset;
        3 * sizeof(uint32_t), // VkDeviceSize    size;
    };
    vk.cmdCopyBuffer(*cmdBuffer, *inoutBuffer, *sparseBuffer, 1u, &copyRegion);

    // wait for sparse buffer beeing ready with dispatch values
    const auto sparseBufferBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, *sparseBuffer,
                                dispatchDataOffset, sparseChunkSize);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 0u,
                          nullptr, 1u, &sparseBufferBarrier, 0u, nullptr);

    // dispatch compute shader but read dispatch parameters from sparse buffer
    vk.cmdBindPipeline(*cmdBuffer, bp, *computePipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, bp, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    vk.cmdDispatchIndirect(*cmdBuffer, *sparseBuffer, dispatchDataOffset);

    // wait for compute shader to finish writing to output buffer
    const auto outBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                          *inoutBuffer, 0ull, inoutBufferSize);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 1u, &outBufferBarrier, 0u, nullptr);

    // end recording
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, m_computeQueue.queueHandle, *cmdBuffer, 0u, nullptr, nullptr, 0, nullptr,
                          usingDeviceGroups(), 0);

    // verify output buffer
    invalidateAlloc(vk, device, *inoutBufferAlloc);
    const uint32_t *outputData = static_cast<const uint32_t *>(inoutBufferAlloc->getHostPtr());
    for (uint32_t i = 0; i < outputItemCount; ++i)
    {
        if (outputData[i] != (135 + i))
            return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

void initIndirectDispatchProgram(vk::SourceCollections &programCollection, const TestFlags flags)
{
    DE_UNREF(flags);

    std::string src = "#version 450\n"
                      "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
                      "layout(binding = 0, std430) writeonly buffer Output\n"
                      "{  uint r[]; };\n"
                      "void main(void)\n"
                      "{\n"
                      "    r[gl_GlobalInvocationID.y] = 135 + gl_GlobalInvocationID.y;\n"
                      "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(src);
}

//! Similar to the class in vktTestCaseUtil.hpp, but uses Arg0 directly rather than through a InstanceFunction1
template <typename Arg0>
class FunctionProgramsSimple1
{
public:
    typedef void (*Function)(vk::SourceCollections &dst, Arg0 arg0);
    FunctionProgramsSimple1(Function func) : m_func(func)
    {
    }
    void init(vk::SourceCollections &dst, const Arg0 &arg0) const
    {
        m_func(dst, arg0);
    }

private:
    const Function m_func;
};

void commonCheckSupport(Context &context, const TestFlags flags)
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

    if (flags & TEST_FLAG_RESIDENCY)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER);

    if (flags & TEST_FLAG_ALIASED)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED);

    if (flags & TEST_FLAG_NON_RESIDENT_STRICT &&
        !context.getDeviceProperties().sparseProperties.residencyNonResidentStrict)
        TCU_THROW(NotSupportedError, "Missing sparse property: residencyNonResidentStrict");

    if (flags & TEST_FLAG_TRANSFORM_FEEDBACK)
        context.requireDeviceFunctionality(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
}

void checkSupport(Context &context, const TestFlags flags)
{
    commonCheckSupport(context, flags);
}

void checkSupport(Context &context, const TestParams testParams)
{
    commonCheckSupport(context, testParams.flags);
}

#ifndef CTS_USES_VULKANSC

class NullAddressReadInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        bool useLocalInvocationIndex; // This may affect the implementation/compiler.
        bool useDescriptor;           // Instead of a buffer address for the read buffer.

        uint32_t getValueCount() const
        {
            return 64u;
        }

        uint32_t getWorkGroupSize() const
        {
            // We will launch a single workgroup with multiple invocations or multiple workgroups with a single
            // invocation depending on useLocalInvocationIndex.
            return (useLocalInvocationIndex ? getValueCount() : 1u);
        }

        uint32_t getWorkGroupCount() const
        {
            return (useLocalInvocationIndex ? 1u : getValueCount());
        }
    };

    NullAddressReadInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~NullAddressReadInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class NullAddressReadCase : public vkt::TestCase
{
public:
    NullAddressReadCase(tcu::TestContext &testCtx, const std::string &name,
                        const NullAddressReadInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~NullAddressReadCase(void) = default;

    TestInstance *createInstance(Context &context) const override
    {
        return new NullAddressReadInstance(context, m_params);
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

protected:
    const NullAddressReadInstance::Params m_params;
};

void NullAddressReadCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER);

    if (!m_params.useDescriptor)
        context.requireDeviceFunctionality("VK_KHR_buffer_device_address");

    const auto &sparseProperties = context.getDeviceProperties().sparseProperties;
    if (!sparseProperties.residencyNonResidentStrict)
        TCU_THROW(NotSupportedError, "residencyNonResidentStrict not supported");
}

void NullAddressReadCase::initPrograms(vk::SourceCollections &dst) const
{
    const auto wgSize     = m_params.getWorkGroupSize();
    const auto arrayIndex = (m_params.useLocalInvocationIndex ? "gl_LocalInvocationIndex" : "gl_WorkGroupID.x");

    std::ostringstream bufferDecls;
    std::string srcBufferExpr;
    std::string dstBufferExpr;

    if (m_params.useDescriptor)
    {
        bufferDecls << "layout (set=0, binding=0, std430) readonly buffer SrcBufferBlock {\n"
                    << "    uint values[];\n"
                    << "} srcBuffer;\n"
                    << "\n"
                    << "layout (set=0, binding=1, std430) writeonly buffer DstBufferBlock {\n"
                    << "    uint values[];\n"
                    << "} dstBuffer;\n"
                    << "\n";
        srcBufferExpr = "srcBuffer";
        dstBufferExpr = "dstBuffer";
    }
    else
    {
        bufferDecls << "layout (buffer_reference) buffer srcBuffer;\n"
                    << "layout (buffer_reference, buffer_reference_align=4, std430) readonly buffer srcBuffer\n"
                    << "{\n"
                    << "    uint values[];\n"
                    << "};\n"
                    << "\n"
                    << "layout (buffer_reference) buffer dstBuffer;\n"
                    << "layout (buffer_reference, buffer_reference_align=4, std430) writeonly buffer dstBuffer\n"
                    << "{\n"
                    << "    uint values[];\n"
                    << "};\n"
                    << "\n"
                    << "layout(push_constant, std430) uniform push_cb\n"
                    << "{\n"
                    << "    uvec2 srcBufferAddress;\n"
                    << "    uvec2 dstBufferAddress;\n"
                    << "} pc;\n"
                    << "\n";
        srcBufferExpr = "srcBuffer(pc.srcBufferAddress)";
        dstBufferExpr = "dstBuffer(pc.dstBufferAddress)";
    }

    std::ostringstream comp;
    comp << "#version 450\n"
         << "#extension GL_EXT_buffer_reference2 : require\n"
         << "#extension GL_EXT_buffer_reference_uvec2 : require\n"
         << "layout (local_size_x=" << wgSize << ", local_size_y=1, local_size_z=1) in;\n"
         << "\n"
         << bufferDecls.str() << "void main()\n"
         << "{\n"
         << "    const uint idx = " << arrayIndex << ";\n"
         << "    " << dstBufferExpr << ".values[idx] = " << srcBufferExpr << ".values[idx];\n"
         << "}\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus NullAddressReadInstance::iterate()
{
    const auto ctx        = m_context.getContextCommonData();
    const auto valueCount = m_params.getValueCount();
    const std::vector<uint32_t> emptyQueueFamilyIndexList;

    // Destination buffer, filled with non-zero values.
    std::vector<uint32_t> stagingValues(valueCount, std::numeric_limits<uint32_t>::max());
    const auto bufferSize = static_cast<VkDeviceSize>(de::dataSize(stagingValues));

    const auto dstBufferUsage = (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto dstBufferInfo  = vk::makeBufferCreateInfo(bufferSize, dstBufferUsage);
    BufferWithMemory dstBuffer(ctx.vkd, ctx.device, ctx.allocator, dstBufferInfo, MemoryRequirement::DeviceAddress);

    // Staging host-visible write buffer.
    const auto stagingDstBufferUsage = (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto stagingDstBufferInfo  = vk::makeBufferCreateInfo(bufferSize, stagingDstBufferUsage);
    BufferWithMemory stagingDstBuffer(ctx.vkd, ctx.device, ctx.allocator, stagingDstBufferInfo,
                                      MemoryRequirement::HostVisible);
    {
        auto &alloc = stagingDstBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(stagingValues), de::dataSize(stagingValues));
    }

    // Source buffer, sparse and bound to the null address, which should result in reads returning zeros.
    const auto srcBufferUsage = (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto srcBufferFlags = (VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT);
    const VkBufferCreateInfo srcBufferInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        srcBufferFlags,
        bufferSize, // Same size.
        srcBufferUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
    };
    const auto srcBuffer = createBuffer(ctx.vkd, ctx.device, &srcBufferInfo);
    // IMPORTANT: note we do not bind any memory to this buffer.

    // Pipeline, passing buffer addresses as push constants.
    struct PushConstants
    {
        tcu::UVec2 srcAddress;
        tcu::UVec2 dstAddress;
    };

    VkBufferDeviceAddressInfo srcAddressInfo = initVulkanStructure();
    VkBufferDeviceAddressInfo dstAddressInfo = initVulkanStructure();

    srcAddressInfo.buffer = *srcBuffer;
    dstAddressInfo.buffer = *dstBuffer;

    const auto srcBufferAddress = ctx.vkd.getBufferDeviceAddress(ctx.device, &srcAddressInfo);
    const auto dstBufferAddress = ctx.vkd.getBufferDeviceAddress(ctx.device, &dstAddressInfo);

    const auto shaderStages = VK_SHADER_STAGE_COMPUTE_BIT;
    const auto descType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    const PushConstants pcValue = {
        tcu::UVec2(static_cast<uint32_t>(srcBufferAddress & 0xFFFFFFFFull),
                   static_cast<uint32_t>((srcBufferAddress >> 32) & 0xFFFFFFFFull)),
        tcu::UVec2(static_cast<uint32_t>(dstBufferAddress & 0xFFFFFFFFull),
                   static_cast<uint32_t>((dstBufferAddress >> 32) & 0xFFFFFFFFull)),
    };
    const auto pcSize     = DE_SIZEOF32(pcValue);
    const auto pcRange    = makePushConstantRange(shaderStages, 0u, pcSize);
    const auto pcRangePtr = (m_params.useDescriptor ? nullptr : &pcRange);

    Move<VkDescriptorSetLayout> setLayout;

    if (m_params.useDescriptor)
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(descType, shaderStages);
        setLayoutBuilder.addSingleBinding(descType, shaderStages);
        setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    }

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, pcRangePtr);
    const auto compModule     = createShaderModule(ctx.vkd, ctx.device, m_context.getBinaryCollection().get("comp"));
    const auto pipeline       = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;

    if (m_params.useDescriptor)
    {
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(descType, 2u);
        descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        descriptorSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

        const auto srcBufferDescInfo = makeDescriptorBufferInfo(*srcBuffer, 0ull, VK_WHOLE_SIZE);
        const auto dstBufferDescInfo = makeDescriptorBufferInfo(*dstBuffer, 0ull, VK_WHOLE_SIZE);

        DescriptorSetUpdateBuilder updateBuilder;
        updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                  &srcBufferDescInfo);
        updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), descType,
                                  &dstBufferDescInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    const auto bufferCopy = makeBufferCopy(0ull, 0ull, bufferSize);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Prepare destination buffer with non-zero contents.
        ctx.vkd.cmdCopyBuffer(cmdBuffer, *stagingDstBuffer, *dstBuffer, 1u, &bufferCopy);

        // Transfer before other writes in the shader.
        const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &barrier);
    }
    {
        const auto wgCount = m_params.getWorkGroupCount();
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
        if (m_params.useDescriptor)
            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u,
                                          nullptr);
        else
            ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, shaderStages, 0u, pcSize, &pcValue);
        ctx.vkd.cmdDispatch(cmdBuffer, wgCount, 1u, 1u);
    }
    {
        // Copy values back to staging buffer.
        const auto preCopy = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopy);

        ctx.vkd.cmdCopyBuffer(cmdBuffer, *dstBuffer, *stagingDstBuffer, 1u, &bufferCopy);

        const auto postCopy = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &postCopy);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    vk::submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    {
        auto &alloc = stagingDstBuffer.getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        memcpy(de::dataOrNull(stagingValues), alloc.getHostPtr(), de::dataSize(stagingValues));
    }

    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    for (uint32_t i = 0u; i < valueCount; ++i)
    {
        const auto &result = stagingValues.at(i);

        if (result != 0u)
        {
            std::ostringstream msg;
            msg << "Unexpected non-zero value found in output buffer at position " << i << ": " << result;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            fail = true;
        }
    }

    if (fail)
        TCU_FAIL("Invalid values found in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

#endif // CTS_USES_VULKANSC

//! Convenience function to create a TestCase based on a freestanding initPrograms and a TestInstance implementation
template <typename TestInstanceT, typename Arg0>
TestCase *createTestInstanceWithPrograms(tcu::TestContext &testCtx, const std::string &name,
                                         typename FunctionProgramsSimple1<Arg0>::Function initPrograms, Arg0 arg0)
{
    return new InstanceFactory1WithSupport<TestInstanceT, Arg0, FunctionSupport1<Arg0>, FunctionProgramsSimple1<Arg0>>(
        testCtx, name, FunctionProgramsSimple1<Arg0>(initPrograms), arg0,
        typename FunctionSupport1<Arg0>::Args(checkSupport, arg0));
}

void populateTestGroup(tcu::TestCaseGroup *parentGroup)
{
    const struct
    {
        std::string name;
        TestFlags flags;
    } groups[] = {
        {
            "sparse_binding",
            0u,
        },
        {
            "sparse_binding_aliased",
            TEST_FLAG_ALIASED,
        },
        {
            "sparse_residency",
            TEST_FLAG_RESIDENCY,
        },
        {
            "sparse_residency_aliased",
            TEST_FLAG_RESIDENCY | TEST_FLAG_ALIASED,
        },
        {
            "sparse_residency_non_resident_strict",
            TEST_FLAG_RESIDENCY | TEST_FLAG_NON_RESIDENT_STRICT,
        },
    };

    const int numGroupsIncludingNonResidentStrict = DE_LENGTH_OF_ARRAY(groups);
    const int numGroupsDefaultList                = numGroupsIncludingNonResidentStrict - 1;
    std::string devGroupPrefix                    = "device_group_";

    // Transfer
    {
        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(parentGroup->getTestContext(), "transfer"));
        {
            MovePtr<tcu::TestCaseGroup> subGroup(
                new tcu::TestCaseGroup(parentGroup->getTestContext(), "sparse_binding"));
            addBufferSparseBindingTests(subGroup.get(), false);
            group->addChild(subGroup.release());

            MovePtr<tcu::TestCaseGroup> subGroupDeviceGroups(
                new tcu::TestCaseGroup(parentGroup->getTestContext(), "device_group_sparse_binding"));
            addBufferSparseBindingTests(subGroupDeviceGroups.get(), true);
            group->addChild(subGroupDeviceGroups.release());

            MovePtr<tcu::TestCaseGroup> subGroupRebind(new tcu::TestCaseGroup(parentGroup->getTestContext(), "rebind"));
            addBufferSparseRebindTests(subGroupRebind.get(), false);
            group->addChild(subGroupRebind.release());
        }
        parentGroup->addChild(group.release());
    }

    // SSBO
    {
        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(parentGroup->getTestContext(), "ssbo"));
        {
            MovePtr<tcu::TestCaseGroup> subGroup(
                new tcu::TestCaseGroup(parentGroup->getTestContext(), "sparse_binding_aliased"));
            addBufferSparseMemoryAliasingTests(subGroup.get(), false);
            group->addChild(subGroup.release());

            MovePtr<tcu::TestCaseGroup> subGroupDeviceGroups(
                new tcu::TestCaseGroup(parentGroup->getTestContext(), "device_group_sparse_binding_aliased"));
            addBufferSparseMemoryAliasingTests(subGroupDeviceGroups.get(), true);
            group->addChild(subGroupDeviceGroups.release());
        }
        {
            MovePtr<tcu::TestCaseGroup> subGroup(
                new tcu::TestCaseGroup(parentGroup->getTestContext(), "sparse_residency"));
            addBufferSparseResidencyTests(subGroup.get(), false);
            group->addChild(subGroup.release());

            MovePtr<tcu::TestCaseGroup> subGroupDeviceGroups(
                new tcu::TestCaseGroup(parentGroup->getTestContext(), "device_group_sparse_residency"));
            addBufferSparseResidencyTests(subGroupDeviceGroups.get(), true);
            group->addChild(subGroupDeviceGroups.release());
        }

        // Read and write sparse storage buffers in shaders
        {
            MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(parentGroup->getTestContext(), "read_write"));
            const TestParams testParams = {TestFlags(TEST_FLAG_RESIDENCY | TEST_FLAG_NON_RESIDENT_STRICT),
                                           BufferObjectType::BO_TYPE_STORAGE};
            subGroup->addChild(createTestInstanceWithPrograms<BufferObjectTestInstance>(
                subGroup->getTestContext(), "sparse_residency_non_resident_strict", initProgramsDrawWithBufferObject,
                testParams));
            group->addChild(subGroup.release());
        }
        parentGroup->addChild(group.release());
    }

    // UBO
    {
        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(parentGroup->getTestContext(), "ubo"));

        for (int groupNdx = 0u; groupNdx < numGroupsIncludingNonResidentStrict; ++groupNdx)
        {
            const TestParams testParams = {groups[groupNdx].flags, BufferObjectType::BO_TYPE_UNIFORM};
            group->addChild(createTestInstanceWithPrograms<BufferObjectTestInstance>(
                group->getTestContext(), groups[groupNdx].name.c_str(), initProgramsDrawWithBufferObject, testParams));
        }
        for (int groupNdx = 0u; groupNdx < numGroupsIncludingNonResidentStrict; ++groupNdx)
        {
            const TestParams testParams = {groups[groupNdx].flags | TEST_FLAG_ENABLE_DEVICE_GROUPS,
                                           BufferObjectType::BO_TYPE_UNIFORM};
            group->addChild(createTestInstanceWithPrograms<BufferObjectTestInstance>(
                group->getTestContext(), (devGroupPrefix + groups[groupNdx].name).c_str(),
                initProgramsDrawWithBufferObject, testParams));
        }
        parentGroup->addChild(group.release());
    }

    // Vertex buffer
    {
        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(parentGroup->getTestContext(), "vertex_buffer"));

        for (int groupNdx = 0u; groupNdx < numGroupsDefaultList; ++groupNdx)
        {
            group->addChild(createTestInstanceWithPrograms<VertexBufferTestInstance>(
                group->getTestContext(), groups[groupNdx].name.c_str(), initProgramsDrawGrid, groups[groupNdx].flags));
        }
        for (int groupNdx = 0u; groupNdx < numGroupsDefaultList; ++groupNdx)
        {
            group->addChild(createTestInstanceWithPrograms<VertexBufferTestInstance>(
                group->getTestContext(), (devGroupPrefix + groups[groupNdx].name).c_str(), initProgramsDrawGrid,
                groups[groupNdx].flags | TEST_FLAG_ENABLE_DEVICE_GROUPS));
        }

        parentGroup->addChild(group.release());
    }

    // Index buffer
    {
        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(parentGroup->getTestContext(), "index_buffer"));

        for (int groupNdx = 0u; groupNdx < numGroupsDefaultList; ++groupNdx)
        {
            group->addChild(createTestInstanceWithPrograms<IndexBufferTestInstance>(
                group->getTestContext(), groups[groupNdx].name.c_str(), initProgramsDrawGrid, groups[groupNdx].flags));
        }
        for (int groupNdx = 0u; groupNdx < numGroupsDefaultList; ++groupNdx)
        {
            group->addChild(createTestInstanceWithPrograms<IndexBufferTestInstance>(
                group->getTestContext(), (devGroupPrefix + groups[groupNdx].name).c_str(), initProgramsDrawGrid,
                groups[groupNdx].flags | TEST_FLAG_ENABLE_DEVICE_GROUPS));
        }

        parentGroup->addChild(group.release());
    }

    // Indirect buffer
    {
        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(parentGroup->getTestContext(), "indirect_buffer"));

        for (int groupNdx = 0u; groupNdx < numGroupsDefaultList; ++groupNdx)
        {
            group->addChild(createTestInstanceWithPrograms<IndirectBufferTestInstance>(
                group->getTestContext(), groups[groupNdx].name.c_str(), initProgramsDrawGrid, groups[groupNdx].flags));
        }
        for (int groupNdx = 0u; groupNdx < numGroupsDefaultList; ++groupNdx)
        {
            group->addChild(createTestInstanceWithPrograms<IndirectBufferTestInstance>(
                group->getTestContext(), (devGroupPrefix + groups[groupNdx].name).c_str(), initProgramsDrawGrid,
                groups[groupNdx].flags | TEST_FLAG_ENABLE_DEVICE_GROUPS));
        }

        parentGroup->addChild(group.release());
    }

    // Transform feedback - only sparse residency variant
    {
        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(parentGroup->getTestContext(), "transform_feedback"));
        group->addChild(createTestInstanceWithPrograms<TransformFeedbackTestInstance>(
            group->getTestContext(), std::string("sparse_residency"), initTransformFeedbackPrograms,
            TestFlags(TEST_FLAG_RESIDENCY | TEST_FLAG_TRANSFORM_FEEDBACK)));
        parentGroup->addChild(group.release());
    }

    // Indirect dispatch - only sparse residency variant
    {
        MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(parentGroup->getTestContext(), "indirect_dispatch"));
        group->addChild(createTestInstanceWithPrograms<IndirectDispatchTestInstance>(
            group->getTestContext(), std::string("sparse_residency"), initIndirectDispatchProgram,
            TestFlags(TEST_FLAG_RESIDENCY)));
        parentGroup->addChild(group.release());
    }

#ifndef CTS_USES_VULKANSC
    {
        auto &testCtx = parentGroup->getTestContext();
        de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc"));

        for (const auto useLocalInvocationIndex : {false, true})
            for (const auto useDescriptors : {false, true})
            {
                const NullAddressReadInstance::Params params{
                    useLocalInvocationIndex,
                    useDescriptors,
                };
                const auto testName = std::string("null_address_read") +
                                      (useLocalInvocationIndex ? "_local_inv_idx" : "") +
                                      (useDescriptors ? "_descriptors" : "");

                miscGroup->addChild(new NullAddressReadCase(testCtx, testName, params));
            }

        parentGroup->addChild(miscGroup.release());
    }
#endif // CTS_USES_VULKANSC
}

} // namespace

tcu::TestCaseGroup *createSparseBufferTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "buffer", populateTestGroup);
}

} // namespace sparse
} // namespace vkt
