/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC
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
 * \brief VK_EXT_primitives_generated_query Tests
 *//*--------------------------------------------------------------------*/

#include "vktPrimitivesGeneratedQueryTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTestLog.hpp"

#include <functional>
#include <map>

namespace vkt
{
namespace TransformFeedback
{
namespace
{
using namespace vk;

enum
{
    IMAGE_WIDTH  = 64,
    IMAGE_HEIGHT = IMAGE_WIDTH,
};

enum QueryReadType
{
    QUERY_READ_TYPE_GET,
    QUERY_READ_TYPE_COPY,

    QUERY_READ_TYPE_LAST
};

enum QueryResetType
{
    QUERY_RESET_TYPE_QUEUE,
    QUERY_RESET_TYPE_HOST,

    QUERY_RESET_TYPE_LAST
};

enum QueryResultType
{
    QUERY_RESULT_TYPE_32_BIT,
    QUERY_RESULT_TYPE_64_BIT,
    QUERY_RESULT_TYPE_PGQ_32_XFB_64,
    QUERY_RESULT_TYPE_PGQ_64_XFB_32,

    QUERY_RESULT_TYPE_LAST
};

enum ShaderStage
{
    SHADER_STAGE_VERTEX,
    SHADER_STAGE_TESSELLATION_EVALUATION,
    SHADER_STAGE_GEOMETRY,

    SHADER_STAGE_LAST
};

enum RasterizationCase
{
    RAST_CASE_DEFAULT,
    RAST_CASE_DISCARD,
    RAST_CASE_EMPTY_FRAG,
    RAST_CASE_NO_ATTACHMENT,
    RAST_CASE_COLOR_WRITE_DISABLE_STATIC,
    RAST_CASE_COLOR_WRITE_DISABLE_DYNAMIC,

    RAST_CASE_LAST
};

enum VertexStream
{
    VERTEX_STREAM_DEFAULT = -1,
    VERTEX_STREAM_0       = 0,
    VERTEX_STREAM_1       = 1,
};

enum CommandBufferCase
{
    CMD_BUF_CASE_SINGLE_DRAW,

    CMD_BUF_CASE_LAST
};

struct TestParameters
{
    QueryReadType queryReadType;
    QueryResetType queryResetType;
    QueryResultType queryResultType;
    ShaderStage shaderStage;
    bool transformFeedback;
    RasterizationCase rastCase;
    bool depthStencilAttachment;
    VkPrimitiveTopology primitiveTopology;
    VertexStream pgqStream;
    VertexStream xfbStream;
    CommandBufferCase cmdBufCase;

    bool pgqDefault(void) const
    {
        return pgqStream == VERTEX_STREAM_DEFAULT;
    }
    bool xfbDefault(void) const
    {
        return xfbStream == VERTEX_STREAM_DEFAULT;
    }
    uint32_t pgqStreamIndex(void) const
    {
        return pgqDefault() ? 0 : static_cast<uint32_t>(pgqStream);
    }
    uint32_t xfbStreamIndex(void) const
    {
        return xfbDefault() ? 0 : static_cast<uint32_t>(xfbStream);
    }
    bool multipleStreams(void) const
    {
        return pgqStreamIndex() != xfbStreamIndex();
    }
    bool nonZeroStreams(void) const
    {
        return (pgqStreamIndex() != 0) || (xfbStreamIndex() != 0);
    }
    bool rastDiscard(void) const
    {
        return rastCase == RAST_CASE_DISCARD;
    }
    bool colorAttachment(void) const
    {
        return !rastDiscard() && rastCase != RAST_CASE_NO_ATTACHMENT;
    }
    bool staticColorWriteDisable(void) const
    {
        return rastCase == RAST_CASE_COLOR_WRITE_DISABLE_STATIC;
    }
    bool dynamicColorWriteDisable(void) const
    {
        return rastCase == RAST_CASE_COLOR_WRITE_DISABLE_DYNAMIC;
    }
    bool colorWriteDisable(void) const
    {
        return staticColorWriteDisable() || dynamicColorWriteDisable();
    }
};

struct TopologyInfo
{
    uint32_t primitiveSize;                             // Size of the primitive.
    bool hasAdjacency;                                  // True if topology has adjacency.
    const char *inputString;                            // Layout qualifier identifier for geometry shader input.
    const char *outputString;                           // Layout qualifier identifier for geometry shader output.
    std::function<uint64_t(uint64_t)> getNumPrimitives; // Number of primitives generated.
    std::function<uint64_t(uint64_t)> getNumVertices;   // Number of vertices generated.
};

const std::map<VkPrimitiveTopology, TopologyInfo> topologyData = {
    {VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
     {1, false, "points", "points", [](uint64_t vtxCount) { return vtxCount; },
      [](uint64_t primCount) { return primCount; }}},
    {VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
     {2, false, "lines", "line_strip", [](uint64_t vtxCount) { return vtxCount / 2u; },
      [](uint64_t primCount) { return primCount * 2u; }}},
    {VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
     {2, false, "lines", "line_strip", [](uint64_t vtxCount) { return vtxCount - 1u; },
      [](uint64_t primCount) { return primCount + 1u; }}},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
     {3, false, "triangles", "triangle_strip", [](uint64_t vtxCount) { return vtxCount / 3u; },
      [](uint64_t primCount) { return primCount * 3u; }}},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
     {3, false, "triangles", "triangle_strip", [](uint64_t vtxCount) { return vtxCount - 2u; },
      [](uint64_t primCount) { return primCount + 2u; }}},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
     {3, false, "triangles", "triangle_strip", [](uint64_t vtxCount) { return vtxCount - 2u; },
      [](uint64_t primCount) { return primCount + 2u; }}},
    {VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
     {2, true, "lines_adjacency", "line_strip", [](uint64_t vtxCount) { return vtxCount / 4u; },
      [](uint64_t primCount) { return primCount * 4u; }}},
    {VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
     {2, true, "lines_adjacency", "line_strip", [](uint64_t vtxCount) { return vtxCount - 3u; },
      [](uint64_t primCount) { return primCount + 3u; }}},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
     {3, true, "triangles_adjacency", "triangle_strip", [](uint64_t vtxCount) { return vtxCount / 6u; },
      [](uint64_t primCount) { return primCount * 6u; }}},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
     {3, true, "triangles_adjacency", "triangle_strip", [](uint64_t vtxCount) { return (vtxCount - 4u) / 2u; },
      [](uint64_t primCount) { return primCount * 2u + 4; }}},
    {VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
     {3, false, "ERROR", "ERROR", [](uint64_t vtxCount) { return vtxCount / 3u; },
      [](uint64_t primCount) { return primCount * 3u; }}},
};

class PrimitivesGeneratedQueryTestInstance : public vkt::TestInstance
{
public:
    PrimitivesGeneratedQueryTestInstance(vkt::Context &context, const TestParameters &parameters)
        : vkt::TestInstance(context)
        , m_parameters(parameters)
    {
    }

private:
    tcu::TestStatus iterate(void);
    VkFormat selectDepthStencilFormat(void);
    Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                          const VkRenderPass renderPass);
    void fillVertexBuffer(tcu::Vec2 *vertices, const uint64_t primitivesGenerated);
    const TestParameters m_parameters;
};

tcu::TestStatus PrimitivesGeneratedQueryTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = m_context.getUniversalQueue();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const VkFormat colorFormat = m_parameters.colorAttachment() ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_UNDEFINED;
    Move<VkImage> colorImage;
    de::MovePtr<Allocation> colorImageAllocation;

    if (m_parameters.colorAttachment())
    {
        const VkImageCreateInfo colorImageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,        // VkStructureType            sType
            DE_NULL,                                    // const void*                pNext
            0u,                                         // VkImageCreateFlags        flags
            VK_IMAGE_TYPE_2D,                           // VkImageType                imageType
            colorFormat,                                // VkFormat                    format
            makeExtent3D(IMAGE_WIDTH, IMAGE_HEIGHT, 1), // VkExtent3D                extent
            1u,                                         // uint32_t                    mipLevels
            1u,                                         // uint32_t                    arrayLayers
            VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits    samples
            VK_IMAGE_TILING_OPTIMAL,                    // VkImageTiling            tiling
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,        // VkImageUsageFlags        usage
            VK_SHARING_MODE_EXCLUSIVE,                  // VkSharingMode            sharingMode
            0u,                                         // uint32_t                    queueFamilyIndexCount
            DE_NULL,                                    // const uint32_t*            pQueueFamilyIndices
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout            initialLayout
        };

        colorImage           = makeImage(vk, device, colorImageCreateInfo);
        colorImageAllocation = bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any);
    }

    const VkFormat dsFormat = m_parameters.depthStencilAttachment ?
                                  PrimitivesGeneratedQueryTestInstance::selectDepthStencilFormat() :
                                  VK_FORMAT_UNDEFINED;

    if (m_parameters.depthStencilAttachment && dsFormat == VK_FORMAT_UNDEFINED)
        return tcu::TestStatus::fail("VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT feature must be supported for at "
                                     "least one of VK_FORMAT_D24_UNORM_S8_UINT and VK_FORMAT_D32_SFLOAT_S8_UINT.");

    Move<VkImage> dsImage;
    de::MovePtr<Allocation> dsImageAllocation;

    if (m_parameters.depthStencilAttachment)
    {
        const VkImageCreateInfo dsImageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,         // VkStructureType            sType
            DE_NULL,                                     // const void*                pNext
            0u,                                          // VkImageCreateFlags        flags
            VK_IMAGE_TYPE_2D,                            // VkImageType                imageType
            dsFormat,                                    // VkFormat                    format
            makeExtent3D(IMAGE_WIDTH, IMAGE_HEIGHT, 1),  // VkExtent3D                extent
            1u,                                          // uint32_t                    mipLevels
            1u,                                          // uint32_t                    arrayLayers
            VK_SAMPLE_COUNT_1_BIT,                       // VkSampleCountFlagBits    samples
            VK_IMAGE_TILING_OPTIMAL,                     // VkImageTiling            tiling
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, // VkImageUsageFlags        usage
            VK_SHARING_MODE_EXCLUSIVE,                   // VkSharingMode            sharingMode
            0u,                                          // uint32_t                    queueFamilyIndexCount
            DE_NULL,                                     // const uint32_t*            pQueueFamilyIndices
            VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout            initialLayout
        };

        dsImage           = makeImage(vk, device, dsImageCreateInfo);
        dsImageAllocation = bindImage(vk, device, allocator, *dsImage, MemoryRequirement::Any);
    }

    const VkDeviceSize primitivesGenerated = 32;
    const uint32_t baseMipLevel            = 0;
    const uint32_t levelCount              = 1;
    const uint32_t baseArrayLayer          = 0;
    const uint32_t layerCount              = 1;

    Move<VkImageView> colorImageView;
    Move<VkImageView> dsImageView;
    std::vector<VkImageView> imageViews;

    if (m_parameters.colorAttachment())
    {
        const VkImageSubresourceRange colorSubresourceRange =
            makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, baseMipLevel, levelCount, baseArrayLayer, layerCount);
        colorImageView =
            makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange);
        imageViews.push_back(*colorImageView);
    }

    if (m_parameters.depthStencilAttachment)
    {
        const VkImageSubresourceRange dsSubresourceRange =
            makeImageSubresourceRange((VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT), baseMipLevel,
                                      levelCount, baseArrayLayer, layerCount);
        dsImageView = makeImageView(vk, device, *dsImage, VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSubresourceRange);
        imageViews.push_back(*dsImageView);
    }

    const Unique<VkRenderPass> renderPass(
        makeRenderPass(vk, device, colorFormat, dsFormat, VK_ATTACHMENT_LOAD_OP_DONT_CARE));
    const Unique<VkFramebuffer> framebuffer(makeFramebuffer(vk, device, *renderPass, (uint32_t)imageViews.size(),
                                                            imageViews.data(), IMAGE_WIDTH, IMAGE_HEIGHT));
    const Unique<VkPipeline> pipeline(
        PrimitivesGeneratedQueryTestInstance::makeGraphicsPipeline(vk, device, *renderPass));
    Move<VkBuffer> vtxBuffer;
    de::MovePtr<Allocation> vtxBufferAlloc;

    {
        const VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        const std::vector<uint32_t> queueFamilyIndices(1, queueFamilyIndex);
        const VkDeviceSize vtxBufferSize =
            topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesGenerated) * sizeof(tcu::Vec2);
        const VkBufferCreateInfo createInfo = makeBufferCreateInfo(vtxBufferSize, usage, queueFamilyIndices);

        vtxBuffer = createBuffer(vk, device, &createInfo);
        vtxBufferAlloc =
            allocator.allocate(getBufferMemoryRequirements(vk, device, *vtxBuffer), MemoryRequirement::HostVisible);
    }

    const VkCommandPoolCreateFlags cmdPoolCreateFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    const VkCommandBufferLevel cmdBufferLevel         = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    const Unique<VkCommandPool> cmdPool(createCommandPool(vk, device, cmdPoolCreateFlags, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, cmdBufferLevel));

    const bool pgq64                           = (m_parameters.queryResultType == QUERY_RESULT_TYPE_64_BIT ||
                        m_parameters.queryResultType == QUERY_RESULT_TYPE_PGQ_64_XFB_32);
    const bool xfb64                           = (m_parameters.queryResultType == QUERY_RESULT_TYPE_64_BIT ||
                        m_parameters.queryResultType == QUERY_RESULT_TYPE_PGQ_32_XFB_64);
    const size_t pgqResultSize                 = pgq64 ? sizeof(uint64_t) : sizeof(uint32_t);
    const size_t xfbResultSize                 = xfb64 ? sizeof(uint64_t) * 2 : sizeof(uint32_t) * 2;
    const VkQueryResultFlags pgqResultWidthBit = pgq64 ? VK_QUERY_RESULT_64_BIT : 0;
    const VkQueryResultFlags xfbResultWidthBit = xfb64 ? VK_QUERY_RESULT_64_BIT : 0;
    const VkQueryResultFlags pgqResultFlags    = VK_QUERY_RESULT_WAIT_BIT | pgqResultWidthBit;
    const VkQueryResultFlags xfbResultFlags    = VK_QUERY_RESULT_WAIT_BIT | xfbResultWidthBit;

    const uint32_t queryIndex = 0;
    const uint32_t queryCount = 1;

    std::vector<uint8_t> pgqResults(pgqResultSize, 255u);
    std::vector<uint8_t> xfbResults(xfbResultSize, 255u);

    const VkQueryPoolCreateInfo pgqCreateInfo = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType                    sType
        DE_NULL,                                  // const void*                        pNext
        0u,                                       // VkQueryPoolCreateFlags            flags
        VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT,   // VkQueryType                        queryType
        queryCount,                               // uint32_t                            queryCount
        0u,                                       // VkQueryPipelineStatisticFlags    pipelineStatistics
    };

    const Unique<VkQueryPool> pgqPool(createQueryPool(vk, device, &pgqCreateInfo));
    Move<VkQueryPool> xfbPool;

    if (m_parameters.transformFeedback)
    {
        const VkQueryPoolCreateInfo xfbCreateInfo = {
            VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,    // VkStructureType                    sType
            DE_NULL,                                     // const void*                        pNext
            0u,                                          // VkQueryPoolCreateFlags            flags
            VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT, // VkQueryType                        queryType
            queryCount,                                  // uint32_t                            queryCount
            0u,                                          // VkQueryPipelineStatisticFlags    pipelineStatistics
        };

        xfbPool = createQueryPool(vk, device, &xfbCreateInfo);
    }

    Move<VkBuffer> pgqResultsBuffer;
    Move<VkBuffer> xfbResultsBuffer;
    de::MovePtr<Allocation> pgqResultsBufferAlloc;
    de::MovePtr<Allocation> xfbResultsBufferAlloc;

    if (m_parameters.queryReadType == QUERY_READ_TYPE_COPY)
    {
        const VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        const std::vector<uint32_t> queueFamilyIndices(1, queueFamilyIndex);
        const VkBufferCreateInfo pgqBufferCreateInfo = makeBufferCreateInfo(pgqResultSize, usage, queueFamilyIndices);

        pgqResultsBuffer      = createBuffer(vk, device, &pgqBufferCreateInfo);
        pgqResultsBufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *pgqResultsBuffer),
                                                   MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(device, *pgqResultsBuffer, pgqResultsBufferAlloc->getMemory(),
                                     pgqResultsBufferAlloc->getOffset()));

        if (m_parameters.transformFeedback)
        {
            const VkBufferCreateInfo xfbBufferCreateInfo =
                makeBufferCreateInfo(xfbResultSize, usage, queueFamilyIndices);

            xfbResultsBuffer      = createBuffer(vk, device, &xfbBufferCreateInfo);
            xfbResultsBufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *xfbResultsBuffer),
                                                       MemoryRequirement::HostVisible);

            VK_CHECK(vk.bindBufferMemory(device, *xfbResultsBuffer, xfbResultsBufferAlloc->getMemory(),
                                         xfbResultsBufferAlloc->getOffset()));
        }
    }

    const VkDeviceSize primitivesWritten = primitivesGenerated - 3;
    const VkDeviceSize verticesWritten =
        topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesWritten);
    const VkDeviceSize primitiveSize =
        m_parameters.nonZeroStreams() ? 1 : topologyData.at(m_parameters.primitiveTopology).primitiveSize;
    const VkDeviceSize bytesPerVertex = 4 * sizeof(float);
    const VkDeviceSize xfbBufferSize  = primitivesWritten * primitiveSize * bytesPerVertex;
    Move<VkBuffer> xfbBuffer;
    de::MovePtr<Allocation> xfbBufferAlloc;

    if (m_parameters.transformFeedback)
    {
        const VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
        const std::vector<uint32_t> queueFamilyIndices(1, queueFamilyIndex);
        const VkBufferCreateInfo createInfo = makeBufferCreateInfo(xfbBufferSize, usage, queueFamilyIndices);

        xfbBuffer = createBuffer(vk, device, &createInfo);
        xfbBufferAlloc =
            allocator.allocate(getBufferMemoryRequirements(vk, device, *xfbBuffer), MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(device, *xfbBuffer, xfbBufferAlloc->getMemory(), xfbBufferAlloc->getOffset()));
    }

    fillVertexBuffer(static_cast<tcu::Vec2 *>(vtxBufferAlloc.get()->getHostPtr()), primitivesGenerated);

    VK_CHECK(vk.bindBufferMemory(device, *vtxBuffer, vtxBufferAlloc->getMemory(), vtxBufferAlloc->getOffset()));

    beginCommandBuffer(vk, *cmdBuffer);
    {
        const VkDeviceSize vertexBufferOffset = static_cast<VkDeviceSize>(0);

        // After query pool creation, each query must be reset before it is used.
        if (m_parameters.queryResetType == QUERY_RESET_TYPE_QUEUE)
        {
            vk.cmdResetQueryPool(*cmdBuffer, *pgqPool, queryIndex, queryCount);

            if (m_parameters.transformFeedback)
                vk.cmdResetQueryPool(*cmdBuffer, *xfbPool, queryIndex, queryCount);
        }

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

        vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vtxBuffer.get(), &vertexBufferOffset);

        beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(IMAGE_WIDTH, IMAGE_HEIGHT)));
        {
            const VkQueryControlFlags queryControlFlags = 0;

            if (m_parameters.pgqDefault())
                vk.cmdBeginQuery(*cmdBuffer, *pgqPool, queryIndex, queryControlFlags);
            else
                vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *pgqPool, queryIndex, queryControlFlags,
                                           m_parameters.pgqStreamIndex());

            const uint32_t firstCounterBuffer        = 0;
            const uint32_t counterBufferCount        = 0;
            const VkBuffer *counterBuffers           = DE_NULL;
            const VkDeviceSize *counterBufferOffsets = DE_NULL;

            if (m_parameters.transformFeedback)
            {
                const uint32_t firstBinding = 0;
                const uint32_t bindingCount = 1;
                const VkDeviceSize offset   = 0;

                vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, firstBinding, bindingCount, &*xfbBuffer, &offset,
                                                      &xfbBufferSize);

                if (m_parameters.xfbDefault())
                    vk.cmdBeginQuery(*cmdBuffer, *xfbPool, queryIndex, queryControlFlags);
                else
                    vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *xfbPool, queryIndex, queryControlFlags,
                                               m_parameters.xfbStreamIndex());

                vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, firstCounterBuffer, counterBufferCount, counterBuffers,
                                                counterBufferOffsets);
            }

            if (m_parameters.dynamicColorWriteDisable())
            {
                const uint32_t attachmentCount   = 1;
                const VkBool32 colorWriteEnables = VK_FALSE;

                vk.cmdSetColorWriteEnableEXT(*cmdBuffer, attachmentCount, &colorWriteEnables);
            }

            const uint32_t vertexCount = static_cast<uint32_t>(
                topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesGenerated));
            const uint32_t instanceCount = 1u;
            const uint32_t firstVertex   = 0u;
            const uint32_t firstInstance = 0u;

            vk.cmdDraw(*cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

            if (m_parameters.pgqDefault())
                vk.cmdEndQuery(*cmdBuffer, *pgqPool, queryIndex);
            else
                vk.cmdEndQueryIndexedEXT(*cmdBuffer, *pgqPool, queryIndex, m_parameters.pgqStreamIndex());

            if (m_parameters.transformFeedback)
            {
                if (m_parameters.xfbDefault())
                    vk.cmdEndQuery(*cmdBuffer, *xfbPool, queryIndex);
                else
                    vk.cmdEndQueryIndexedEXT(*cmdBuffer, *xfbPool, queryIndex, m_parameters.xfbStreamIndex());

                vk.cmdEndTransformFeedbackEXT(*cmdBuffer, firstCounterBuffer, counterBufferCount, counterBuffers,
                                              counterBufferOffsets);
            }
        }
        endRenderPass(vk, *cmdBuffer);

        if (m_parameters.queryReadType == QUERY_READ_TYPE_COPY)
        {
            VkBufferMemoryBarrier bufferBarrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType
                DE_NULL,                                 // const void*        pNext
                VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags    srcAccessMask
                VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags    dstAccessMask
                VK_QUEUE_FAMILY_IGNORED,                 // uint32_t            srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                 // uint32_t            dstQueueFamilyIndex
                *pgqResultsBuffer,                       // VkBuffer            buffer
                0u,                                      // VkDeviceSize        offset
                VK_WHOLE_SIZE                            // VkDeviceSize        size
            };

            vk.cmdCopyQueryPoolResults(*cmdBuffer, *pgqPool, queryIndex, queryCount, *pgqResultsBuffer, 0u,
                                       pgqResultSize, pgqResultFlags);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                                  DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);

            if (m_parameters.transformFeedback)
            {
                bufferBarrier.buffer = *xfbResultsBuffer;
                vk.cmdCopyQueryPoolResults(*cmdBuffer, *xfbPool, queryIndex, queryCount, *xfbResultsBuffer, 0u,
                                           xfbResultSize, xfbResultFlags);
                vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                                      DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
            }
        }
    }
    endCommandBuffer(vk, *cmdBuffer);

    // After query pool creation, each query must be reset before it is used.
    if (m_parameters.queryResetType == QUERY_RESET_TYPE_HOST)
    {
        vk.resetQueryPool(device, *pgqPool, queryIndex, queryCount);

        if (m_parameters.transformFeedback)
            vk.resetQueryPool(device, *xfbPool, queryIndex, queryCount);
    }

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    if (m_parameters.queryReadType == QUERY_READ_TYPE_COPY)
    {
        invalidateAlloc(vk, device, *pgqResultsBufferAlloc);
        deMemcpy(pgqResults.data(), pgqResultsBufferAlloc->getHostPtr(), pgqResults.size());

        if (m_parameters.transformFeedback)
        {
            invalidateAlloc(vk, device, *xfbResultsBufferAlloc);
            deMemcpy(xfbResults.data(), xfbResultsBufferAlloc->getHostPtr(), xfbResults.size());
        }
    }
    else
    {
        vk.getQueryPoolResults(device, *pgqPool, queryIndex, queryCount, pgqResults.size(), pgqResults.data(),
                               pgqResults.size(), pgqResultFlags);

        if (m_parameters.transformFeedback)
            vk.getQueryPoolResults(device, *xfbPool, queryIndex, queryCount, xfbResults.size(), xfbResults.data(),
                                   xfbResults.size(), xfbResultFlags);
    }

    // Validate counters.
    {
        union QueryResults
        {
            uint32_t elements32[2];
            uint64_t elements64[2];
        };

        const QueryResults *pgqCounters = reinterpret_cast<QueryResults *>(pgqResults.data());
        const QueryResults *xfbCounters = reinterpret_cast<QueryResults *>(xfbResults.data());
        const uint64_t pgqGenerated =
            pgq64 ? pgqCounters->elements64[0] : static_cast<uint64_t>(pgqCounters->elements32[0]);
        const uint64_t xfbWritten =
            xfb64 ? xfbCounters->elements64[0] : static_cast<uint64_t>(xfbCounters->elements32[0]);
        const uint64_t xfbGenerated =
            xfb64 ? xfbCounters->elements64[1] : static_cast<uint64_t>(xfbCounters->elements32[1]);
        tcu::TestLog &log = m_context.getTestContext().getLog();

        log << tcu::TestLog::Message << "primitivesGenerated: " << primitivesGenerated << "\n"
            << "primitivesWritten: " << primitivesWritten << "\n"
            << "verticesWritten: " << verticesWritten << "\n"
            << "xfbBufferSize: " << xfbBufferSize << "\n"
            << tcu::TestLog::EndMessage;

        log << tcu::TestLog::Message << "PGQ: Generated " << pgqGenerated << tcu::TestLog::EndMessage;

        if (m_parameters.transformFeedback)
            log << tcu::TestLog::Message << "XFB: Written " << xfbWritten << ", generated " << xfbGenerated
                << tcu::TestLog::EndMessage;

        if (pgqGenerated != primitivesGenerated)
        {
            const std::string message = std::string("pgqGenerated == ") + de::toString(pgqGenerated) + ", expected " +
                                        de::toString(primitivesGenerated);
            return tcu::TestStatus::fail(message);
        }

        if (m_parameters.transformFeedback)
        {
            if (xfbGenerated != primitivesGenerated)
            {
                const std::string message = std::string("xfbGenerated == ") + de::toString(xfbGenerated) +
                                            ", expected " + de::toString(primitivesGenerated);
                return tcu::TestStatus::fail(message);
            }

            if (xfbWritten != primitivesWritten)
            {
                const std::string message = std::string("xfbWritten == ") + de::toString(xfbWritten) + ", expected " +
                                            de::toString(primitivesWritten);
                return tcu::TestStatus::fail(message);
            }

            if (xfbWritten != (primitivesGenerated - 3))
            {
                const std::string message = std::string("xfbWritten == ") + de::toString(xfbWritten) + ", expected " +
                                            de::toString(primitivesGenerated - 3);
                return tcu::TestStatus::fail(message);
            }
        }
    }

    return tcu::TestStatus::pass("Counters OK");
}

VkFormat PrimitivesGeneratedQueryTestInstance::selectDepthStencilFormat(void)
{
    constexpr VkFormat formats[] = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};

    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

    for (VkFormat format : formats)
    {
        const VkFormatFeatureFlags features =
            getPhysicalDeviceFormatProperties(vki, physicalDevice, format).optimalTilingFeatures;

        if (features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }

    return VK_FORMAT_UNDEFINED;
}

Move<VkPipeline> PrimitivesGeneratedQueryTestInstance::makeGraphicsPipeline(const DeviceInterface &vk,
                                                                            const VkDevice device,
                                                                            const VkRenderPass renderPass)
{
    const VkDescriptorSetLayout descriptorSetLayout = DE_NULL;
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, descriptorSetLayout));
    const std::vector<VkViewport> viewports(1, makeViewport(makeExtent2D(IMAGE_WIDTH, IMAGE_HEIGHT)));
    const std::vector<VkRect2D> scissors(1, makeRect2D(makeExtent2D(IMAGE_WIDTH, IMAGE_HEIGHT)));
    const uint32_t subpass            = 0u;
    const uint32_t patchControlPoints = topologyData.at(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST).primitiveSize;
    const Unique<VkShaderModule> vertModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
    Move<VkShaderModule> tescModule;
    Move<VkShaderModule> teseModule;
    Move<VkShaderModule> geomModule;
    Move<VkShaderModule> fragModule;
    VkVertexInputBindingDescription bindingDescription;
    VkVertexInputAttributeDescription attributeDescription;

    if (m_parameters.shaderStage == SHADER_STAGE_TESSELLATION_EVALUATION)
    {
        tescModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("tesc"), 0u);
        teseModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("tese"), 0u);
    }

    if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY)
        geomModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0u);

    if (!m_parameters.rastDiscard())
        fragModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);

    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(tcu::Vec2);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    attributeDescription.binding  = 0;
    attributeDescription.location = 0;
    attributeDescription.format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescription.offset   = 0;

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                            sType
        DE_NULL,                                                   // const void*                                pNext
        0u,                                                        // VkPipelineVertexInputStateCreateFlags    flags
        1u,                    // uint32_t                                    vertexBindingDescriptionCount
        &bindingDescription,   // const VkVertexInputBindingDescription*    pVertexBindingDescriptions
        1u,                    // uint32_t                                    vertexAttributeDescriptionCount
        &attributeDescription, // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                            sType
        DE_NULL,                                                    // const void*                                pNext
        0,                                                          // VkPipelineRasterizationStateCreateFlags    flags
        VK_FALSE, // VkBool32                                    depthClampEnable
        (m_parameters.rastDiscard() ? VK_TRUE :
                                      VK_FALSE), // VkBool32                                    rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,                    // VkPolygonMode                            polygonMode
        VK_CULL_MODE_NONE,                       // VkCullModeFlags                            cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE,         // VkFrontFace                                frontFace
        VK_FALSE,                                // VkBool32                                    depthBiasEnable
        0.0f,                                    // float                                    depthBiasConstantFactor
        0.0f,                                    // float                                    depthBiasClamp
        0.0f,                                    // float                                    depthBiasSlopeFactor
        1.0f                                     // float                                    lineWidth
    };

    const VkStencilOpState stencilOpState = {
        VK_STENCIL_OP_KEEP,   // VkStencilOp    failOp
        VK_STENCIL_OP_KEEP,   // VkStencilOp    passOp
        VK_STENCIL_OP_KEEP,   // VkStencilOp    depthFailOp
        VK_COMPARE_OP_ALWAYS, // VkCompareOp    compareOp
        0xFFu,                // uint32_t        compareMask
        0xFFu,                // uint32_t        writeMask
        0,                    // uint32_t        reference
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, //    VkStructureType                            sType
        DE_NULL,            //    const void*                                pNext
        0,                  //    VkPipelineDepthStencilStateCreateFlags    flags
        VK_TRUE,            //    VkBool32                                depthTestEnable
        VK_TRUE,            //    VkBool32                                depthWriteEnable
        VK_COMPARE_OP_LESS, //    VkCompareOp                                depthCompareOp
        VK_FALSE,           //    VkBool32                                depthBoundsTestEnable
        VK_FALSE,           //    VkBool32                                stencilTestEnable
        stencilOpState,     //    VkStencilOpState                        front
        stencilOpState,     //    VkStencilOpState                        back
        0.0f,               //    float                                    minDepthBounds
        1.0f,               //    float                                    maxDepthBounds
    };

    const VkBool32 colorWriteEnables = VK_FALSE;

    const VkPipelineColorWriteCreateInfoEXT colorWriteCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT, // VkStructureType sType;
        DE_NULL,                                                // const void* pNext;
        1,                                                      // uint32_t attachmentCount;
        &colorWriteEnables                                      // const VkBool32* pColorWriteEnables;
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,                // VkBool32                    blendEnable
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstColorBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                colorBlendOp
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstAlphaBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT // VkColorComponentFlags    colorWriteMask
            | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
        &colorWriteCreateInfo,      // const void*                                    pNext
        0,                          // VkPipelineColorBlendStateCreateFlags            flags
        VK_FALSE,                   // VkBool32                                        logicOpEnable
        VK_LOGIC_OP_NO_OP,          // VkLogicOp                                    logicOp
        1,                          // uint32_t                                        attachmentCount
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState*    pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f}    // float                                        blendConstants[4]
    };

    const VkDynamicState dynamicStates = VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT;

    const VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType                        sType
        DE_NULL,                                              // const void*                            pNext
        0u,                                                   // VkPipelineDynamicStateCreateFlags    flags
        1u,            // uint32_t                                dynamicStateCount
        &dynamicStates // const VkDynamicState*                pDynamicStates
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    return vk::makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertModule, *tescModule, *teseModule, *geomModule, *fragModule, renderPass,
        viewports, scissors, m_parameters.primitiveTopology, subpass, patchControlPoints, &vertexInputStateCreateInfo,
        &rasterizationStateCreateInfo, &multisampleStateCreateInfo,
        m_parameters.depthStencilAttachment ? &depthStencilStateCreateInfo : DE_NULL,
        m_parameters.staticColorWriteDisable() ? &colorBlendStateCreateInfo : DE_NULL,
        m_parameters.dynamicColorWriteDisable() ? &pipelineDynamicStateCreateInfo : DE_NULL);
}

void PrimitivesGeneratedQueryTestInstance::fillVertexBuffer(tcu::Vec2 *vertices, const uint64_t primitivesGenerated)
{
    const float step = 1.0f / static_cast<float>(primitivesGenerated);

    switch (m_parameters.primitiveTopology)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
    {
        for (uint32_t prim = 0; prim < primitivesGenerated; ++prim)
        {
            vertices[prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 0.0f);
        }
        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    {
        for (uint32_t prim = 0; prim < primitivesGenerated; ++prim)
        {
            vertices[2 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
            vertices[2 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
        }
        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    {
        vertices[0] = tcu::Vec2(-1.0f, -1.0f);
        vertices[1] = tcu::Vec2(-1.0f, 1.0f);

        for (uint32_t prim = 1; prim < primitivesGenerated; ++prim)
        {
            if (prim % 2 == 0)
            {
                vertices[1 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
            }
            else
            {
                vertices[1 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
            }
        }
        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    {
        vertices[0] = tcu::Vec2(-1.0f, 1.0f);
        vertices[1] = tcu::Vec2(-1.0f, -1.0f);
        vertices[2] = tcu::Vec2(-1.0f + 2.0f * step, 1.0f);

        for (uint32_t prim = 1; prim < primitivesGenerated; ++prim)
        {
            if (prim % 2 == 0)
            {
                vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
            }
            else
            {
                vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
            }
        }
        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    {
        vertices[0] = tcu::Vec2(0.0f, -1.0f);

        for (uint32_t prim = 0; prim < primitivesGenerated + 1; ++prim)
        {
            vertices[1 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f + 2.0f * (float)prim * step);
        }
        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    {
        for (uint32_t prim = 0; prim < primitivesGenerated; ++prim)
        {
            vertices[4 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
            vertices[4 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 0.5f);
            vertices[4 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -0.5f);
            vertices[4 * prim + 3] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
        }
        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
    {
        vertices[0] = tcu::Vec2(-1.0f, 0.0f);
        vertices[1] = tcu::Vec2(-1.0f, -1.0f);
        vertices[2] = tcu::Vec2(-1.0f, 1.0f);

        for (uint32_t prim = 1; prim < primitivesGenerated; ++prim)
        {
            if (prim % 2 == 0)
            {
                vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
            }
            else
            {
                vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
            }
        }

        vertices[2 + primitivesGenerated] = tcu::Vec2(1.0f, 0.0f);

        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
    {
        for (uint32_t prim = 0; prim < primitivesGenerated; ++prim)
        {
            if (prim % 2 == 0)
            {
                vertices[3 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, 1.0f);
                vertices[3 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -1.0f);
                vertices[3 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
            }
            else
            {
                vertices[3 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
                vertices[3 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, 1.0f);
                vertices[3 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, 1.0f);
            }
        }
        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    {
        for (uint32_t prim = 0; prim < primitivesGenerated; ++prim)
        {
            if (prim % 2 == 0)
            {
                vertices[6 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, 1.0f);
                vertices[6 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, 1.0f);
                vertices[6 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -1.0f);
                vertices[6 * prim + 3] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -1.0f);
                vertices[6 * prim + 4] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
                vertices[6 * prim + 5] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
            }
            else
            {
                vertices[6 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
                vertices[6 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
                vertices[6 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, 1.0f);
                vertices[6 * prim + 3] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, 1.0f);
                vertices[6 * prim + 4] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, 1.0f);
                vertices[6 * prim + 5] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, 1.0f);
            }
        }
        break;
    }
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
    {
        vertices[0] = tcu::Vec2(-1.0f, 1.0f);
        vertices[1] = tcu::Vec2(-1.0f, 1.0f);
        vertices[2] = tcu::Vec2(-1.0f, -1.0f);
        vertices[3] = tcu::Vec2(-1.0f, -1.0f);
        vertices[4] = tcu::Vec2(-1.0f + 2.0f * step, 1.0f);
        vertices[5] = tcu::Vec2(-1.0f + 2.0f * step, 1.0f);

        for (uint32_t prim = 1; prim < primitivesGenerated; ++prim)
        {
            if (prim % 2 == 0)
            {
                vertices[5 + prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
                vertices[5 + prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
            }
            else
            {
                vertices[5 + prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
                vertices[5 + prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
            }
        }
        break;
    }
    default:
        TCU_THROW(InternalError, "Unrecognized primitive topology");
    }
}

class PrimitivesGeneratedQueryTestCase : public vkt::TestCase
{
public:
    PrimitivesGeneratedQueryTestCase(tcu::TestContext &context, const char *name, const char *description,
                                     const TestParameters &parameters)
        : TestCase(context, name, description)
        , m_parameters(parameters)
    {
    }

private:
    void checkSupport(vkt::Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new PrimitivesGeneratedQueryTestInstance(context, m_parameters);
    }

    const TestParameters m_parameters;
};

void PrimitivesGeneratedQueryTestCase::checkSupport(vkt::Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_primitives_generated_query");
    context.requireDeviceFunctionality("VK_EXT_transform_feedback");

    const VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT &pgqFeatures =
        context.getPrimitivesGeneratedQueryFeaturesEXT();
    const VkPhysicalDeviceTransformFeedbackFeaturesEXT &xfbFeatures     = context.getTransformFeedbackFeaturesEXT();
    const VkPhysicalDeviceTransformFeedbackPropertiesEXT &xfbProperties = context.getTransformFeedbackPropertiesEXT();

    if (pgqFeatures.primitivesGeneratedQuery != VK_TRUE)
        TCU_THROW(NotSupportedError, "VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT not supported");

    if (m_parameters.rastDiscard() && (pgqFeatures.primitivesGeneratedQueryWithRasterizerDiscard != VK_TRUE))
        TCU_THROW(NotSupportedError, "primitivesGeneratedQueryWithRasterizerDiscard not supported");

    if (m_parameters.queryResetType == QUERY_RESET_TYPE_HOST)
        context.requireDeviceFunctionality("VK_EXT_host_query_reset");

    if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY ||
        topologyData.at(m_parameters.primitiveTopology).hasAdjacency)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_parameters.shaderStage == SHADER_STAGE_TESSELLATION_EVALUATION)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if (m_parameters.nonZeroStreams())
    {
        const uint32_t requiredStreams = de::max(m_parameters.pgqStreamIndex(), m_parameters.xfbStreamIndex());

        if (m_parameters.pgqStreamIndex() > 0 && pgqFeatures.primitivesGeneratedQueryWithNonZeroStreams != VK_TRUE)
            TCU_THROW(NotSupportedError, "primitivesGeneratedQueryWithNonZeroStreams not supported");

        if (xfbProperties.maxTransformFeedbackStreams <= requiredStreams)
            TCU_THROW(NotSupportedError, "Required amount of XFB streams not supported");
    }

    if (m_parameters.transformFeedback)
    {
        if (xfbFeatures.transformFeedback != VK_TRUE)
            TCU_THROW(NotSupportedError, "transformFeedback not supported");

        if (xfbProperties.transformFeedbackQueries != VK_TRUE)
            TCU_THROW(NotSupportedError, "transformFeedbackQueries not supported");
    }

    if (m_parameters.colorWriteDisable())
    {
        context.requireDeviceFunctionality("VK_EXT_color_write_enable");

        if (context.getColorWriteEnableFeaturesEXT().colorWriteEnable != VK_TRUE)
            TCU_THROW(NotSupportedError, "colorWriteEnable not supported");
    }
}

void PrimitivesGeneratedQueryTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // Vertex shader.
    {
        const bool vertXfb = (m_parameters.transformFeedback && m_parameters.shaderStage == SHADER_STAGE_VERTEX);
        std::ostringstream src;

        src << "#version 450\n";
        src << "layout(location=0) in vec2 inPosition;\n";

        if (vertXfb)
            src << "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n";

        src << "void main (void)\n"
               "{\n";

        if (m_parameters.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST &&
            m_parameters.shaderStage == SHADER_STAGE_VERTEX)
            src << "    gl_PointSize = 1.0;\n";

        src << "     gl_Position = vec4(inPosition, 0, 1);\n";

        if (vertXfb)
            src << "    out0 = vec4(42);\n";

        src << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
    }

    // Tessellation shaders.
    if (m_parameters.shaderStage == SHADER_STAGE_TESSELLATION_EVALUATION)
    {
        std::stringstream tescSrc;
        std::stringstream teseSrc;

        tescSrc << "#version 450\n"
                   "#extension GL_EXT_tessellation_shader : require\n"
                   "layout(vertices = "
                << topologyData.at(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST).primitiveSize
                << ") out;\n"
                   "void main (void)\n"
                   "{\n"
                   "    gl_TessLevelInner[0] = 1.0;\n"
                   "    gl_TessLevelInner[1] = 1.0;\n"
                   "    gl_TessLevelOuter[0] = 1.0;\n"
                   "    gl_TessLevelOuter[1] = 1.0;\n"
                   "    gl_TessLevelOuter[2] = 1.0;\n"
                   "    gl_TessLevelOuter[3] = 1.0;\n"
                   "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                   "}\n";

        teseSrc << "#version 450\n"
                   "#extension GL_EXT_tessellation_shader : require\n"
                   "layout(triangles) in;\n";

        if (m_parameters.transformFeedback)
            teseSrc << "layout(xfb_buffer = 0, xfb_offset = 0, location = 0) out vec4 out0;\n";

        teseSrc << "void main (void)\n"
                   "{\n";

        if (m_parameters.transformFeedback)
            teseSrc << "    out0 = vec4(42);\n";

        teseSrc << "    vec4 p0 = gl_TessCoord.x * gl_in[0].gl_Position;\n"
                   "    vec4 p1 = gl_TessCoord.y * gl_in[1].gl_Position;\n"
                   "    vec4 p2 = gl_TessCoord.z * gl_in[2].gl_Position;\n"
                   "    gl_Position = p0 + p1 + p2;\n"
                   "}\n";

        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tescSrc.str());
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(teseSrc.str());
    }

    // Geometry shader.
    if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY)
    {
        const bool outputPoints =
            m_parameters.nonZeroStreams() || m_parameters.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        const char *const inputTopology = topologyData.at(m_parameters.primitiveTopology).inputString;
        const char *const outputTopology =
            outputPoints ? "points" : topologyData.at(m_parameters.primitiveTopology).outputString;
        const VkDeviceSize outputPrimSize =
            outputPoints ? 1 : topologyData.at(m_parameters.primitiveTopology).primitiveSize;
        const VkDeviceSize maxVertices = m_parameters.multipleStreams() ? outputPrimSize * 2 : outputPrimSize;
        const std::string pgqEmitCommand =
            m_parameters.nonZeroStreams() ?
                std::string("EmitStreamVertex(") + de::toString(m_parameters.pgqStreamIndex()) + ")" :
                "EmitVertex()";
        const std::string xfbEmitCommand =
            m_parameters.nonZeroStreams() ?
                std::string("EmitStreamVertex(") + de::toString(m_parameters.xfbStreamIndex()) + ")" :
                "EmitVertex()";
        const std::string pgqEndCommand =
            m_parameters.nonZeroStreams() ?
                std::string("EndStreamPrimitive(") + de::toString(m_parameters.pgqStreamIndex()) + ")" :
                "EndPrimitive()";
        const std::string xfbEndCommand =
            m_parameters.nonZeroStreams() ?
                std::string("EndStreamPrimitive(") + de::toString(m_parameters.xfbStreamIndex()) + ")" :
                "EndPrimitive()";
        std::ostringstream src;

        src << "#version 450\n"
               "layout("
            << inputTopology
            << ") in;\n"
               "layout("
            << outputTopology << ", max_vertices = " << maxVertices << ") out;\n";

        if (m_parameters.transformFeedback)
            src << "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0, stream = "
                << m_parameters.xfbStreamIndex() << ") out vec4 xfb;\n";

        src << "void main (void)\n"
               "{\n";

        if (outputPoints)
            src << "    gl_PointSize = 1.0;\n";

        if (m_parameters.transformFeedback)
            src << "    xfb = vec4(42);\n";

        for (VkDeviceSize i = 0; i < outputPrimSize; i++)
            src << "    " << pgqEmitCommand << ";\n";

        src << "    " << pgqEndCommand << ";\n";

        if (m_parameters.transformFeedback && m_parameters.multipleStreams())
        {
            for (VkDeviceSize i = 0; i < outputPrimSize; i++)
                src << "    " << xfbEmitCommand << ";\n";

            src << "    " << xfbEndCommand << ";\n";
        }

        src << "}\n";

        programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
    }

    // Fragment shader.
    if (!m_parameters.rastDiscard())
    {
        std::ostringstream src;

        if (m_parameters.rastCase == RAST_CASE_EMPTY_FRAG)
        {
            src << "#version 450\n"
                   "void main (void) {}\n";
        }
        else
        {
            src << "#version 450\n"
                   "layout(location = 0) out vec4 out0;\n"
                   "void main (void)\n"
                   "{\n"
                   "    out0 = vec4(0.0, 1.0, 0.0, 1.0);\n"
                   "}\n";
        }

        programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
    }
}

void testGenerator(tcu::TestCaseGroup *pgqGroup)
{
    constexpr struct ReadType
    {
        QueryReadType type;
        const char *name;
        const char *desc;
    } readTypes[] = {
        {QUERY_READ_TYPE_GET, "get", "Tests for vkGetQueryPoolResults"},
        {QUERY_READ_TYPE_COPY, "copy", "Tests for vkCmdCopyQueryPoolResults"},
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(readTypes) == QUERY_READ_TYPE_LAST);

    constexpr struct ResetType
    {
        QueryResetType type;
        const char *name;
        const char *desc;
    } resetTypes[] = {
        {QUERY_RESET_TYPE_QUEUE, "queue_reset", "Tests for vkCmdResetQueryPool"},
        {QUERY_RESET_TYPE_HOST, "host_reset", "Tests for vkResetQueryPool"},
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(resetTypes) == QUERY_RESET_TYPE_LAST);

    constexpr struct ResultTypes
    {
        QueryResultType type;
        const char *name;
        const char *desc;
    } resultTypes[] = {
        {QUERY_RESULT_TYPE_32_BIT, "32bit", "Tests for default query result size"},
        {QUERY_RESULT_TYPE_64_BIT, "64bit", "Tests for VK_QUERY_RESULT_64_BIT"},
        {QUERY_RESULT_TYPE_PGQ_32_XFB_64, "pgq_32bit_xfb_64bit",
         "Tests for PGQ without and XFBQ with VK_QUERY_RESULT_64_BIT"},
        {QUERY_RESULT_TYPE_PGQ_64_XFB_32, "pgq_64bit_xfb_32bit",
         "Tests for PGQ with and XFBQ without VK_QUERY_RESULT_64_BIT"},
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(resultTypes) == QUERY_RESULT_TYPE_LAST);

    constexpr struct Shader
    {
        ShaderStage stage;
        const char *name;
        const char *desc;
    } shaderStages[] = {
        {SHADER_STAGE_VERTEX, "vert", "Vertex shader tests"},
        {SHADER_STAGE_TESSELLATION_EVALUATION, "tese", "Tessellation evaluation shader tests"},
        {SHADER_STAGE_GEOMETRY, "geom", "Geometry shader tests"},
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(shaderStages) == SHADER_STAGE_LAST);

    constexpr struct TransformFeedbackState
    {
        bool enable;
        const char *name;
        const char *desc;
    } transformFeedbackStates[] = {
        {false, "no_xfb", "Tests without transform feedback"},
        {true, "xfb", "Tests for comparing PGQ results against transform feedback query results"},
    };

    constexpr struct RastCase
    {
        RasterizationCase type;
        bool dsAttachment;
        const char *name;
        const char *desc;
    } rastCases[] = {
        {RAST_CASE_DISCARD, false, "no_rast", "Tests with rasterizer discard"},
        {RAST_CASE_DEFAULT, false, "rast", "Tests without rasterizer discard"},
        {RAST_CASE_EMPTY_FRAG, false, "empty_frag", "Tests with an empty fragment shader"},
        {RAST_CASE_NO_ATTACHMENT, false, "no_attachment", "Tests with an attachmentless render pass"},
        {RAST_CASE_COLOR_WRITE_DISABLE_STATIC, false, "color_write_disable_static",
         "Tests disabling color output using VkPipelineColorWriteCreateInfoEXT"},
        {RAST_CASE_COLOR_WRITE_DISABLE_STATIC, true, "color_write_disable_static_ds",
         "Tests disabling color output using VkPipelineColorWriteCreateInfoEXT with a depth stencil attachment"},
        {RAST_CASE_COLOR_WRITE_DISABLE_DYNAMIC, false, "color_write_disable_dynamic",
         "Tests disabling color output using vkCmdSetColorWriteEnableEXT"},
        {RAST_CASE_COLOR_WRITE_DISABLE_DYNAMIC, true, "color_write_disable_dynamic_ds",
         "Tests disabling color output using vkCmdSetColorWriteEnableEXT with a depth stencil attachment"},
    };

    constexpr struct Topology
    {
        VkPrimitiveTopology type;
        const char *name;
        const char *desc;
    } topologies[] = {
        {VK_PRIMITIVE_TOPOLOGY_POINT_LIST, "point_list", "Tests for separate point primitives"},
        {VK_PRIMITIVE_TOPOLOGY_LINE_LIST, "line_list", "Tests for separate line primitives"},
        {VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, "line_strip",
         "Tests for connected line primitives with consecutive lines sharing a vertex"},
        {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, "triangle_list", "Tests for separate triangle primitives"},
        {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, "triangle_strip",
         "Tests for connected triangle primitives with consecutive triangles sharing an edge"},
        {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, "triangle_fan",
         "Tests for connected triangle primitives with all triangles sharing a common vertex"},
        {VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, "line_list_with_adjacency",
         "Tests for separate line primitives with adjacency"},
        {VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, "line_strip_with_adjacency",
         "Tests for connected line primitives with adjacency, with consecutive primitives sharing three vertices"},
        {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, "triangle_list_with_adjacency",
         "Tests for separate triangle primitives with adjacency"},
        {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, "triangle_strip_with_adjacency",
         "Tests for connected triangle primitives with adjacency, with consecutive triangles sharing an edge"},
        {VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, "patch_list", "Tests for separate patch primitives"},
    };

    // Tests for vkCmdBeginQueryIndexedEXT and vkCmdEndQueryIndexedEXT.
    constexpr struct StreamIndex
    {
        VertexStream index;
        const char *name;
    } streamIndices[] = {
        {VERTEX_STREAM_DEFAULT, "default"},
        {VERTEX_STREAM_0, "0"},
        {VERTEX_STREAM_1, "1"},
    };

    constexpr struct CmdBufCase
    {
        CommandBufferCase type;
        const char *name;
        const char *desc;
    } cmdBufCases[] = {
        {CMD_BUF_CASE_SINGLE_DRAW, "single_draw", "Test single draw call"},
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(cmdBufCases) == CMD_BUF_CASE_LAST);

    tcu::TestContext &testCtx = pgqGroup->getTestContext();

    for (const ReadType &read : readTypes)
    {
        de::MovePtr<tcu::TestCaseGroup> readGroup(new tcu::TestCaseGroup(testCtx, read.name, read.desc));

        for (const ResetType &reset : resetTypes)
        {
            de::MovePtr<tcu::TestCaseGroup> resetGroup(new tcu::TestCaseGroup(testCtx, reset.name, reset.desc));

            for (const ResultTypes &result : resultTypes)
            {
                de::MovePtr<tcu::TestCaseGroup> resultGroup(new tcu::TestCaseGroup(testCtx, result.name, result.desc));

                for (const Shader &shader : shaderStages)
                {
                    de::MovePtr<tcu::TestCaseGroup> shaderGroup(
                        new tcu::TestCaseGroup(testCtx, shader.name, shader.desc));

                    for (const TransformFeedbackState &xfbState : transformFeedbackStates)
                    {
                        de::MovePtr<tcu::TestCaseGroup> xfbGroup(
                            new tcu::TestCaseGroup(testCtx, xfbState.name, xfbState.desc));

                        // Only test multiple result types with XFB enabled.
                        if ((result.type == QUERY_RESULT_TYPE_PGQ_32_XFB_64 ||
                             result.type == QUERY_RESULT_TYPE_PGQ_64_XFB_32) &&
                            !xfbState.enable)
                            continue;

                        for (const RastCase &rastCase : rastCases)
                        {
                            de::MovePtr<tcu::TestCaseGroup> rastGroup(
                                new tcu::TestCaseGroup(testCtx, rastCase.name, rastCase.desc));

                            // Skip uninteresting cases
                            if ((rastCase.type > RAST_CASE_DISCARD) &&
                                ((read.type != QUERY_READ_TYPE_GET) || (reset.type != QUERY_RESET_TYPE_QUEUE) ||
                                 (result.type != QUERY_RESULT_TYPE_32_BIT)))
                            {
                                continue;
                            }

                            for (const Topology &topology : topologies)
                            {
                                de::MovePtr<tcu::TestCaseGroup> topologyGroup(
                                    new tcu::TestCaseGroup(testCtx, topology.name, topology.desc));

                                // Only test patch lists with tessellation shaders.
                                if ((topology.type == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST &&
                                     shader.stage != SHADER_STAGE_TESSELLATION_EVALUATION) ||
                                    ((topology.type != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST &&
                                      shader.stage == SHADER_STAGE_TESSELLATION_EVALUATION)))
                                {
                                    continue;
                                }

                                // Only test adjacency topologies with geometry shaders.
                                if (shader.stage != SHADER_STAGE_GEOMETRY &&
                                    topologyData.at(topology.type).hasAdjacency)
                                    continue;

                                for (const StreamIndex &pgqStream : streamIndices)
                                {
                                    for (const StreamIndex &xfbStream : streamIndices)
                                    {
                                        const std::string streamGroupName =
                                            std::string("pgq_") + pgqStream.name +
                                            (xfbState.enable ? std::string("_xfb_") + xfbStream.name : "");
                                        const bool pgqDefault = (pgqStream.index == VERTEX_STREAM_DEFAULT);
                                        const bool xfbDefault = (xfbStream.index == VERTEX_STREAM_DEFAULT);
                                        const std::string pgqDescStr =
                                            std::string("PGQ on ") + (pgqDefault ? "default " : "") +
                                            std::string("vertex stream ") + (pgqDefault ? "" : pgqStream.name);
                                        const std::string xfbDescStr =
                                            std::string("XFB on ") + (xfbDefault ? "default " : "") +
                                            std::string("vertex stream ") + (xfbDefault ? "" : xfbStream.name);
                                        const std::string streamGroupDesc =
                                            std::string("Tests for ") + pgqDescStr +
                                            (xfbState.enable ? (std::string(" and ") + xfbDescStr) : "");
                                        de::MovePtr<tcu::TestCaseGroup> streamGroup(new tcu::TestCaseGroup(
                                            testCtx, streamGroupName.c_str(), streamGroupDesc.c_str()));

                                        // Only test nondefault vertex streams with geometry shaders.
                                        if ((pgqStream.index != VERTEX_STREAM_DEFAULT ||
                                             xfbStream.index != VERTEX_STREAM_DEFAULT) &&
                                            shader.stage != SHADER_STAGE_GEOMETRY)
                                            continue;

                                        // Skip nondefault vertex streams for XFB when not enabled.
                                        if (!xfbState.enable && xfbStream.index != VERTEX_STREAM_DEFAULT)
                                            continue;

                                        for (const CmdBufCase &cmdBufCase : cmdBufCases)
                                        {
                                            const TestParameters parameters = {
                                                read.type,             // QueryReadType        queryReadType
                                                reset.type,            // QueryResetType        queryResetType
                                                result.type,           // QueryResultType        queryResultType
                                                shader.stage,          // ShaderStage            shaderStage
                                                xfbState.enable,       // bool                transformFeedback
                                                rastCase.type,         // RasterizationCase    rastCase
                                                rastCase.dsAttachment, // bool                depthStencilAttachment
                                                topology.type,         // VkPrimitiveTopology    primitiveTopology
                                                pgqStream.index,       // VertexStreamIndex    pgqStreamIndex
                                                xfbStream.index,       // VertexStreamIndex    xfbStreamIndex
                                                cmdBufCase.type,       // CommandBufferCase    cmdBufCase
                                            };

                                            streamGroup->addChild(new PrimitivesGeneratedQueryTestCase(
                                                testCtx, cmdBufCase.name, cmdBufCase.desc, parameters));
                                        }

                                        topologyGroup->addChild(streamGroup.release());
                                    }
                                }

                                rastGroup->addChild(topologyGroup.release());
                            }

                            xfbGroup->addChild(rastGroup.release());
                        }

                        shaderGroup->addChild(xfbGroup.release());
                    }

                    resultGroup->addChild(shaderGroup.release());
                }

                resetGroup->addChild(resultGroup.release());
            }

            readGroup->addChild(resetGroup.release());
        }

        pgqGroup->addChild(readGroup.release());
    }
}

} // namespace

tcu::TestCaseGroup *createPrimitivesGeneratedQueryTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "primitives_generated_query", "Primitives Generated Query Tests", testGenerator);
}

} // namespace TransformFeedback
} // namespace vkt
