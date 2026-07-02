/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief Robust Index Buffer Access Tests
 *//*--------------------------------------------------------------------*/

#include "vktRobustnessIndexAccessTests.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vktRobustnessUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "deUniquePtr.hpp"
#include <algorithm>
#include <numeric>
#include <limits>
#include <vector>

namespace vkt::robustness
{

using namespace vk;

enum TestMode
{
    TM_DRAW_INDEXED = 0,
    TM_DRAW_INDEXED_INDIRECT,
    TM_DRAW_INDEXED_INDIRECT_COUNT,
    TM_DRAW_MULTI_INDEXED,
};

enum OOTypes
{
    OO_NONE,
    OO_INDEX,
    OO_SIZE,
    OO_WHOLE_SIZE
};

enum class UsedStages
{
    VERT_FRAG = 0,
    VERT_GEOM_FRAG,
    VERT_TESS_FRAG,
    VERT_TESS_GEOM_FRAG
};

struct TestParams
{
    TestMode mode                 = TM_DRAW_INDEXED;
    OOTypes ooType                = OO_NONE;
    uint32_t leadingCount         = 0;
    uint32_t robustnessVersion    = 2;
    bool useDeviceAddressCommands = false;
    bool usePipelineRobustness    = false;
    UsedStages usedStages         = UsedStages::VERT_FRAG; // used by EXT_pipeline_robustness cases
    VkIndexType indexType         = VK_INDEX_TYPE_UINT32;
};

static const std::pair<std::string, TestMode> testModes[]{
    {"draw_indexed", TestMode::TM_DRAW_INDEXED},
    {"draw_indexed_indirect", TestMode::TM_DRAW_INDEXED_INDIRECT},
    {"draw_indexed_indirect_count", TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT},
    {"draw_multi_indexed", TestMode::TM_DRAW_MULTI_INDEXED},
};

// helper function that executes cmdCopyImageToBuffer or cmdCopyImageToMemoryKHR
static void copyImageToMemory(const DeviceInterface &vk, VkCommandBuffer cmdBuffer, VkImage image,
                              const VkExtent3D &imageExtent, VkBuffer buffer, VkDeviceAddress bufferAddress,
                              [[maybe_unused]] VkDeviceSize bufferSize)
{
    const VkImageSubresourceLayers colorSL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    if (bufferAddress == 0)
    {
        const VkBufferImageCopy copyRegion = makeBufferImageCopy(imageExtent, colorSL);
        vk.cmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1u, &copyRegion);
    }

#ifndef CTS_USES_VULKANSC
    if (bufferAddress)
    {
        VkDeviceAddressRangeKHR addressRange{bufferAddress, bufferSize};
        VkDeviceMemoryImageCopyKHR region = initVulkanStructure();
        region.addressRange               = addressRange;
        region.imageSubresource           = colorSL;
        region.imageLayout                = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        region.imageOffset                = makeOffset3D(0, 0, 0);
        region.imageExtent                = imageExtent;

        VkCopyDeviceMemoryImageInfoKHR copyMemoryInfo = initVulkanStructure();
        copyMemoryInfo.image                          = image;
        copyMemoryInfo.regionCount                    = 1u;
        copyMemoryInfo.pRegions                       = &region;
        vk.cmdCopyImageToMemoryKHR(cmdBuffer, &copyMemoryInfo);
    }
#endif // CTS_USES_VULKANSC
}

class DrawIndexedInstance : public vkt::TestInstance
{
public:
    DrawIndexedInstance(Context &context, InstanceWrapper &&instance, DeviceWrapper &&device,
                        const TestParams &testParams);

    virtual ~DrawIndexedInstance(void) = default;

    virtual tcu::TestStatus iterate(void);

protected:
    const InstanceWrapper m_instance;
    const DeviceWrapper m_device;
    const TestParams m_params;
};

DrawIndexedInstance::DrawIndexedInstance(Context &context, InstanceWrapper &&instance, DeviceWrapper &&device,
                                         const TestParams &testParams)
    : vkt::TestInstance(context)
    , m_instance(std::move(instance))
    , m_device(std::move(device))
    , m_params(testParams)
{
}

tcu::TestStatus DrawIndexedInstance::iterate(void)
{
    const DeviceInterface &vk       = m_device.getDriver();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    vk::Allocator &memAlloc         = m_device.getAllocator();

    // this is testsed - first index in index buffer is outside of bounds
    constexpr uint32_t oobFirstIndex = std::numeric_limits<uint32_t>::max() - 100u;

    const VkFormat colorFormat{VK_FORMAT_R8G8B8A8_UNORM};
    const tcu::UVec2 renderSize{16};
    VkPipelineDynamicStateCreateInfo *dynamicStatePtr = nullptr;
    void *pNext                                       = nullptr;

    VkBufferUsageFlags commonUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (m_params.useDeviceAddressCommands)
        commonUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    MemoryRequirement memReq = m_params.useDeviceAddressCommands ?
                                   MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress :
                                   MemoryRequirement::HostVisible;

    // create vertex buffer
    const std::vector<float> vertices{
        0.0f, -0.8f, 0.0f, 1.0f, 0.0f,  0.8f,  0.0f, 1.0f, 0.8f,  -0.8f, 0.0f, 1.0f,
        0.8f, 0.8f,  0.0f, 1.0f, -0.8f, -0.8f, 0.0f, 1.0f, -0.8f, 0.8f,  0.0f, 1.0f,
    };
    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(float);
    const auto vertexBufferInfo =
        makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | commonUsage);
    BufferWithMemory vertexBuffer(vk, *m_device, memAlloc, vertexBufferInfo, memReq);
    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertices.data(), vertices.size() * sizeof(float));
    flushAlloc(vk, *m_device, vertexBuffer.getAllocation());

    // prepare index data for index type variants
    const std::vector<uint32_t> index32 = {0, 1, 2, 3, 4, 5};
    const std::vector<uint16_t> index16 = {0, 1, 2, 3, 4, 5};
    const std::vector<uint8_t> index8   = {0, 1, 2, 3, 4, 5};
    const void *indexData               = index32.data();
    size_t indexDataBytes               = index32.size() * sizeof(uint32_t);

    // select index data based on index type
    if (m_params.indexType == VK_INDEX_TYPE_UINT16)
    {
        indexData      = index16.data();
        indexDataBytes = index16.size() * sizeof(uint16_t);
    }
    else if (m_params.indexType == VK_INDEX_TYPE_UINT8)
    {
        indexData      = index8.data();
        indexDataBytes = index8.size() * sizeof(uint8_t);
    }

    // create index buffer for 6 points
    // 4--0--2
    // |  |  |
    // 5--1--3
    VkDeviceSize indexBufferSize = static_cast<VkDeviceSize>(indexDataBytes);
    const auto indexBufferInfo = makeBufferCreateInfo(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | commonUsage);
    BufferWithMemory indexBuffer(vk, *m_device, memAlloc, indexBufferInfo, memReq);
    deMemcpy(indexBuffer.getAllocation().getHostPtr(), indexData, indexDataBytes);
    flushAlloc(vk, *m_device, indexBuffer.getAllocation());

    // create indirect buffer
    const vk::VkDrawIndexedIndirectCommand drawIndirectCommand{
        (uint32_t)index32.size(), // indexCount
        1u,                       // instanceCount
        oobFirstIndex,            // firstIndex
        0u,                       // vertexOffset
        0u,                       // firstInstance
    };
    VkDeviceSize indirectBufferSize = sizeof(drawIndirectCommand);
    const auto indirectBufferInfo =
        makeBufferCreateInfo(indirectBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | commonUsage);
    BufferWithMemory indirectBuffer(vk, *m_device, memAlloc, indirectBufferInfo, memReq);
    if ((m_params.mode == TM_DRAW_INDEXED_INDIRECT) || (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT))
    {
        deMemcpy(indirectBuffer.getAllocation().getHostPtr(), &drawIndirectCommand, sizeof(drawIndirectCommand));
        flushAlloc(vk, *m_device, indirectBuffer.getAllocation());
    }

    // create indirect count buffer
    VkDeviceSize indirectCountBufferSize = sizeof(uint32_t);
    const auto indirectCountBufferInfo =
        makeBufferCreateInfo(indirectCountBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | commonUsage);
    BufferWithMemory indirectCountBuffer(vk, *m_device, memAlloc, indirectCountBufferInfo, memReq);
    if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
    {
        *(reinterpret_cast<uint32_t *>(indirectCountBuffer.getAllocation().getHostPtr())) = 1;
        flushAlloc(vk, *m_device, indirectCountBuffer.getAllocation());
    }

    // create output buffer that will be used to read rendered image
    const VkDeviceSize outputBufferSize = renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
    const auto outputBufferInfo =
        makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | commonUsage);
    BufferWithMemory outputBuffer(vk, *m_device, memAlloc, outputBufferInfo, memReq);
    VkDeviceAddress outputBufferAddress = 0ull;

#ifndef CTS_USES_VULKANSC
    VkDynamicState dynamicState{VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE};
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo        = initVulkanStructure();
    VkPipelineRobustnessCreateInfoEXT pipelineRobustnessCreateInfo = initVulkanStructure();

    VkDeviceAddress vertexBufferAddress        = 0ull;
    VkDeviceAddress indexBufferAddress         = 0ull;
    VkDeviceAddress indirectBufferAddress      = 0ull;
    VkDeviceAddress indirectCountBufferAddress = 0ull;

    if (m_params.useDeviceAddressCommands)
    {
        dynamicStateCreateInfo.dynamicStateCount = 1u;
        dynamicStateCreateInfo.pDynamicStates    = &dynamicState;
        dynamicStatePtr                          = &dynamicStateCreateInfo;

        vertexBufferAddress        = getBufferDeviceAddress(vk, *m_device, *vertexBuffer);
        indexBufferAddress         = getBufferDeviceAddress(vk, *m_device, *indexBuffer);
        indirectBufferAddress      = getBufferDeviceAddress(vk, *m_device, *indirectBuffer);
        indirectCountBufferAddress = getBufferDeviceAddress(vk, *m_device, *indirectCountBuffer);
        outputBufferAddress        = getBufferDeviceAddress(vk, *m_device, *outputBuffer);
    }

    if (m_params.usePipelineRobustness)
    {
        pipelineRobustnessCreateInfo.storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED;
        pipelineRobustnessCreateInfo.uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED;
        pipelineRobustnessCreateInfo.vertexInputs   = (m_params.robustnessVersion == 1) ?
                                                          VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS :
                                                          VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2;
        pipelineRobustnessCreateInfo.images         = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DISABLED;
        pNext                                       = &pipelineRobustnessCreateInfo;
    }
#endif

    // create color buffer
    VkExtent3D imageExtent = makeExtent3D(renderSize.x(), renderSize.y(), 1u);
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
        nullptr,                                                               // const void* pNext;
        0u,                                                                    // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
        colorFormat,                                                           // VkFormat format;
        imageExtent,                                                           // VkExtent3D extent;
        1u,                                                                    // uint32_t mipLevels;
        1u,                                                                    // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
        0u,                                                                    // uint32_t queueFamilyIndexCount;
        nullptr,                                                               // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                             // VkImageLayout initialLayout;
    };
    const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    ImageWithMemory colorImage(vk, *m_device, memAlloc, imageCreateInfo, MemoryRequirement::Any);
    auto colorImageView = makeImageView(vk, *m_device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

    auto renderPass  = makeRenderPass(vk, *m_device, colorFormat);
    auto framebuffer = makeFramebuffer(vk, *m_device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initVulkanStructure();
    inputAssemblyState.topology                               = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    // create required shader modules
    auto &bc              = m_context.getBinaryCollection();
    auto vertShaderModule = createShaderModule(vk, *m_device, bc.get("vert"));
    auto fragShaderModule = createShaderModule(vk, *m_device, bc.get("frag"));
    Move<VkShaderModule> geomShaderModule;
    Move<VkShaderModule> tesscShaderModule;
    Move<VkShaderModule> tessehaderModule;

    VkPipelineShaderStageCreateInfo commonShaderStage = initVulkanStructure();
    commonShaderStage.pNext                           = pNext;
    commonShaderStage.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
    commonShaderStage.module                          = *vertShaderModule;
    commonShaderStage.pName                           = "main";

    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos{commonShaderStage, commonShaderStage};
    shaderStageCreateInfos[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStageCreateInfos[1].module = *fragShaderModule;

    if (m_params.usedStages == UsedStages::VERT_GEOM_FRAG || m_params.usedStages == UsedStages::VERT_TESS_GEOM_FRAG)
    {
        geomShaderModule = createShaderModule(vk, *m_device, bc.get("geom"));
        shaderStageCreateInfos.push_back(commonShaderStage);
        shaderStageCreateInfos.back().stage  = VK_SHADER_STAGE_GEOMETRY_BIT;
        shaderStageCreateInfos.back().module = *geomShaderModule;
    }
    if (m_params.usedStages == UsedStages::VERT_TESS_FRAG || m_params.usedStages == UsedStages::VERT_TESS_GEOM_FRAG)
    {
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

        tesscShaderModule = createShaderModule(vk, *m_device, bc.get("tessc"));
        shaderStageCreateInfos.push_back(commonShaderStage);
        shaderStageCreateInfos.back().stage  = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        shaderStageCreateInfos.back().module = *tesscShaderModule;

        tessehaderModule = createShaderModule(vk, *m_device, bc.get("tesse"));
        shaderStageCreateInfos.push_back(commonShaderStage);
        shaderStageCreateInfos.back().stage  = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        shaderStageCreateInfos.back().module = *tessehaderModule;
    }

    const auto viewport = makeViewport(renderSize);
    const auto scissors = makeRect2D(renderSize);
    auto pipelineLayout = makePipelineLayout(vk, *m_device, VK_NULL_HANDLE);

    // create pipeline
    VkPipelineViewportStateCreateInfo viewportState = initVulkanStructure();
    viewportState.viewportCount                     = 1;
    viewportState.pViewports                        = &viewport;
    viewportState.scissorCount                      = 1;
    viewportState.pScissors                         = &scissors;

    const VkVertexInputBindingDescription vertexInputBinding{0u, sizeof(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription vertexInputAttribute{0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u};
    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    vertexInputState.vertexBindingDescriptionCount        = 1u;
    vertexInputState.pVertexBindingDescriptions           = &vertexInputBinding;
    vertexInputState.vertexAttributeDescriptionCount      = 1u;
    vertexInputState.pVertexAttributeDescriptions         = &vertexInputAttribute;

    VkPipelineTessellationStateCreateInfo tessellationState = initVulkanStructure();
    tessellationState.patchControlPoints                    = 1u;

    VkPipelineRasterizationStateCreateInfo rasterizationState = initVulkanStructure();
    rasterizationState.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterizationState.lineWidth                              = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.minSampleShading                     = 1.0f;

    const VkStencilOpState stencilOpState                   = {};
    VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();
    depthStencilState.depthCompareOp                        = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.front                                 = stencilOpState;
    depthStencilState.back                                  = stencilOpState;
    depthStencilState.maxDepthBounds                        = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask                      = 0xFu;

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
    colorBlendState.attachmentCount                     = 1u;
    colorBlendState.pAttachments                        = &colorBlendAttachmentState;

    VkGraphicsPipelineCreateInfo pipelineInfo = initVulkanStructure();
    pipelineInfo.pNext                        = pNext;
    pipelineInfo.flags                        = 0;
    pipelineInfo.stageCount                   = static_cast<uint32_t>(shaderStageCreateInfos.size());
    pipelineInfo.pStages                      = shaderStageCreateInfos.data();
    pipelineInfo.pVertexInputState            = &vertexInputState;
    pipelineInfo.pInputAssemblyState          = &inputAssemblyState;
    pipelineInfo.pTessellationState           = &tessellationState;
    pipelineInfo.pViewportState               = &viewportState;
    pipelineInfo.pRasterizationState          = &rasterizationState;
    pipelineInfo.pMultisampleState            = &multisampleState;
    pipelineInfo.pDepthStencilState           = &depthStencilState;
    pipelineInfo.pColorBlendState             = &colorBlendState;
    pipelineInfo.pDynamicState                = dynamicStatePtr;
    pipelineInfo.layout                       = *pipelineLayout;
    pipelineInfo.renderPass                   = *renderPass;
    pipelineInfo.basePipelineIndex            = -1;

    auto graphicsPipeline = createGraphicsPipeline(vk, *m_device, VK_NULL_HANDLE, &pipelineInfo);
    auto cmdPool = createCommandPool(vk, *m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    auto cmdBuffer = allocateCommandBuffer(vk, *m_device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    // transition colorbuffer layout
    VkImageMemoryBarrier imageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorImage.get(), colorSRR);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u,
                          0u, 0u, 0u, 1u, &imageBarrier);

    const VkRect2D renderArea = makeRect2D(0, 0, renderSize.x(), renderSize.y());
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

    if (!m_params.useDeviceAddressCommands)
    {
        const VkDeviceSize vBuffOffset = 0;
        vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &vBuffOffset);
        vk.cmdBindIndexBuffer(*cmdBuffer, indexBuffer.get(), 0, m_params.indexType);

        // we will draw all points at index 0
        if (m_params.mode == TM_DRAW_INDEXED)
            vk.cmdDrawIndexed(*cmdBuffer, (uint32_t)index32.size(), 1, oobFirstIndex, 0, 0);
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT)
            vk.cmdDrawIndexedIndirect(*cmdBuffer, indirectBuffer.get(), 0, 1, 0);
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
            vk.cmdDrawIndexedIndirectCount(*cmdBuffer, indirectBuffer.get(), 0, indirectCountBuffer.get(), 0, 1,
                                           sizeof(VkDrawIndexedIndirectCommand));
        else if (m_params.mode == TM_DRAW_MULTI_INDEXED)
        {
#ifndef CTS_USES_VULKANSC
            VkMultiDrawIndexedInfoEXT indexInfo[]{
                {oobFirstIndex, 3, 0},
                {oobFirstIndex - 3, 3, 0},
            };
            vk.cmdDrawMultiIndexedEXT(*cmdBuffer, 2, indexInfo, 1, 0, sizeof(VkMultiDrawIndexedInfoEXT), nullptr);
#endif // CTS_USES_VULKANSC
        }
    }
#ifndef CTS_USES_VULKANSC
    if (m_params.useDeviceAddressCommands)
    {
        // use different valid addressFlags in some cases to test them
        VkAddressCommandFlagsKHR addressFlags = VK_ADDRESS_COMMAND_UNKNOWN_TRANSFORM_FEEDBACK_BUFFER_USAGE_BIT_KHR;
        if (m_params.mode == TM_DRAW_INDEXED)
            addressFlags |= VK_ADDRESS_COMMAND_UNKNOWN_STORAGE_BUFFER_USAGE_BIT_KHR;
        if (m_params.mode == TM_DRAW_INDEXED_INDIRECT)
            addressFlags |= VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;

        VkBindVertexBuffer3InfoKHR vertexBuffer3Info = initVulkanStructure();
        vertexBuffer3Info.setStride                  = true;
        vertexBuffer3Info.addressRange               = {vertexBufferAddress, vertexBufferSize, 4u * sizeof(float)};
        vertexBuffer3Info.addressFlags               = addressFlags;
        vk.cmdBindVertexBuffers3KHR(*cmdBuffer, 0, 1, &vertexBuffer3Info);

        VkBindIndexBuffer3InfoKHR bindIndexBuffer3Info = initVulkanStructure();
        bindIndexBuffer3Info.addressRange              = {indexBufferAddress, indexBufferSize};
        bindIndexBuffer3Info.indexType                 = m_params.indexType;
        bindIndexBuffer3Info.addressFlags              = addressFlags;
        vk.cmdBindIndexBuffer3KHR(*cmdBuffer, &bindIndexBuffer3Info);

        // we will draw all points at index 0
        if (m_params.mode == TM_DRAW_INDEXED)
            vk.cmdDrawIndexed(*cmdBuffer, (uint32_t)index32.size(), 1, oobFirstIndex, 0, 0);
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT)
        {
            VkDrawIndirect2InfoKHR drawIndirect2Info = initVulkanStructure();
            drawIndirect2Info.addressRange           = {indirectBufferAddress, indirectBufferSize, 0};
            drawIndirect2Info.addressFlags           = addressFlags;
            drawIndirect2Info.drawCount              = 1u;

            vk.cmdDrawIndexedIndirect2KHR(*cmdBuffer, &drawIndirect2Info);
        }
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
        {
            VkDrawIndirectCount2InfoKHR drawIndirectCount2Info = initVulkanStructure();
            drawIndirectCount2Info.addressRange                = {indirectBufferAddress, indirectBufferSize,
                                                                  sizeof(VkDrawIndexedIndirectCommand)};
            drawIndirectCount2Info.countAddressRange           = {indirectCountBufferAddress, indirectCountBufferSize};
            drawIndirectCount2Info.maxDrawCount                = 1;

            vk.cmdDrawIndexedIndirectCount2KHR(*cmdBuffer, &drawIndirectCount2Info);
        }
        else if (m_params.mode == TM_DRAW_MULTI_INDEXED)
            DE_ASSERT(false);
    }
#endif // CTS_USES_VULKANSC

    endRenderPass(vk, *cmdBuffer);

    // wait till data is transfered to image
    imageBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0u, 0u, 0u, 0u, 1u, &imageBarrier);

    // read back color image
    copyImageToMemory(vk, *cmdBuffer, *colorImage, imageExtent, *outputBuffer, outputBufferAddress, outputBufferSize);

    auto bufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                                 outputBuffer.get(), 0u, VK_WHOLE_SIZE);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, 0u, 1u,
                          &bufferBarrier, 0u, 0u);

    endCommandBuffer(vk, *cmdBuffer);

    VkQueue queue;
    vk.getDeviceQueue(*m_device, queueFamilyIndex, 0, &queue);
    submitCommandsAndWait(vk, *m_device, queue, *cmdBuffer);

    // for robustBufferAccess (the original feature) OOB access will return undefined value;
    // we can only expect that above drawing will be executed without errors (we can't expect any specific result)
    if (m_params.robustnessVersion < 2u)
        return tcu::TestStatus::pass("Pass");

    // get output buffer
    invalidateAlloc(vk, *m_device, outputBuffer.getAllocation());
    const tcu::TextureFormat resultFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess outputAccess(resultFormat, renderSize.x(), renderSize.y(), 1u,
                                             outputBuffer.getAllocation().getHostPtr());

    // for VK_EXT_robustness2 OOB access should return 0 and we can verify
    // that single fragment is drawn in the middle-top part of the image
    tcu::UVec4 expectedValue(51, 255, 127, 255);
    bool fragmentFound = false;

    for (uint32_t x = 0u; x < renderSize.x(); ++x)
        for (uint32_t y = 0u; y < renderSize.y(); ++y)
        {
            tcu::UVec4 pixel = outputAccess.getPixelUint(x, y, 0);

            if (tcu::boolAll(tcu::lessThan(tcu::absDiff(pixel, expectedValue), tcu::UVec4(2))))
            {
                if (fragmentFound)
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Expected single fragment with: " << expectedValue
                        << " color, got more, second at " << tcu::UVec2(x, y) << tcu::TestLog::EndMessage
                        << tcu::TestLog::Image("Result", "Result", outputAccess);
                    return tcu::TestStatus::fail("Fail");
                }
                else if ((y < 3) && (x > 5) && (x < 10))
                    fragmentFound = true;
                else
                {
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message
                        << "Expected fragment in the middle-top of the image, got at: " << tcu::UVec2(x, y)
                        << tcu::TestLog::EndMessage << tcu::TestLog::Image("Result", "Result", outputAccess);
                    return tcu::TestStatus::fail("Fail");
                }
            }
        }

    if (fragmentFound)
        return tcu::TestStatus::pass("Pass");
    return tcu::TestStatus::fail("Fail");
}

class DrawIndexedTestCase : public vkt::TestCase
{
public:
    DrawIndexedTestCase(tcu::TestContext &testContext, const std::string &name, const TestParams &params);

    virtual ~DrawIndexedTestCase(void) = default;

    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override;
    void initPrograms(SourceCollections &programCollection) const override;

protected:
    CustomDevice createTestDevice(Context &context, const InstanceWrapper &instance) const;
    const TestParams m_params;
};

DrawIndexedTestCase::DrawIndexedTestCase(tcu::TestContext &testContext, const std::string &name,
                                         const TestParams &params)

    : vkt::TestCase(testContext, name)
    , m_params(params)
{
}

void DrawIndexedTestCase::checkSupport(Context &context) const
{
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getDeviceFeatures().robustBufferAccess)
        TCU_THROW(NotSupportedError,
                  "VK_KHR_portability_subset: robustBufferAccess not supported by this implementation");

    if (m_params.mode == TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT)
        context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
    if (m_params.mode == TestMode::TM_DRAW_MULTI_INDEXED)
        context.requireDeviceFunctionality("VK_EXT_multi_draw");

    if (m_params.robustnessVersion == 2)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_robustness2") &&
            !context.isDeviceFunctionalitySupported("VK_EXT_robustness2"))

            TCU_THROW(NotSupportedError, "VK_KHR_robustness2 and VK_EXT_robustness2 are not supported");

        const auto &vki           = context.getInstanceInterface();
        const auto physicalDevice = context.getPhysicalDevice();

        VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = initVulkanStructure();
        VkPhysicalDeviceFeatures2 features2                        = initVulkanStructure(&robustness2Features);

        vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

        if (!robustness2Features.robustBufferAccess2)
            TCU_THROW(NotSupportedError, "robustBufferAccess2 not supported");
    }
    if (m_params.useDeviceAddressCommands)
        context.requireDeviceFunctionality("VK_KHR_device_address_commands");

    if (m_params.usePipelineRobustness)
        context.requireDeviceFunctionality("VK_EXT_pipeline_robustness");

    const bool useAllStages = (m_params.usedStages == UsedStages::VERT_TESS_GEOM_FRAG);
    if (useAllStages || (m_params.usedStages == UsedStages::VERT_GEOM_FRAG))
        context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (useAllStages || (m_params.usedStages == UsedStages::VERT_TESS_FRAG))
        context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if ((m_params.indexType == VK_INDEX_TYPE_UINT8) && !context.getIndexTypeUint8Features().indexTypeUint8)
        TCU_THROW(NotSupportedError, "indexTypeUint8 not supported");
}

CustomDevice DrawIndexedTestCase::createTestDevice(Context &context, const InstanceWrapper &instance) const
{
    VkPhysicalDeviceFeatures2 features2   = initVulkanStructure();
    features2.features.robustBufferAccess = true;

    void **nextPtr = &features2.pNext;

    VkPhysicalDeviceScalarBlockLayoutFeatures sblFeatures = initVulkanStructure();
    sblFeatures.scalarBlockLayout                         = true;

#ifndef CTS_USES_VULKANSC
    VkPhysicalDeviceMultiDrawFeaturesEXT multiDrawFeatures = initVulkanStructure();
    if (m_params.mode == TestMode::TM_DRAW_MULTI_INDEXED)
    {
        multiDrawFeatures.multiDraw = true;
        addToChainVulkanStructure(&nextPtr, multiDrawFeatures);
    }

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures  = initVulkanStructure();
    VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR addressCommandsFeatures = initVulkanStructure();
    if (m_params.useDeviceAddressCommands)
    {
        addressCommandsFeatures.deviceAddressCommands = true;
        addToChainVulkanStructure(&nextPtr, addressCommandsFeatures);
    }
#endif // CTS_USES_VULKANSC

    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = initVulkanStructure();
    if (m_params.robustnessVersion > 1u)
    {
        robustness2Features.robustBufferAccess2 = true;
        addToChainVulkanStructure(&nextPtr, robustness2Features);
    }

    VkPhysicalDeviceIndexTypeUint8Features indexTypeUint8Features = initVulkanStructure();
    if (m_params.indexType == VK_INDEX_TYPE_UINT8)
    {
        indexTypeUint8Features.indexTypeUint8 = true;
        addToChainVulkanStructure(&nextPtr, indexTypeUint8Features);
    }

    uint32_t apiVersion                               = context.getUsedApiVersion();
    VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure();
    if ((m_params.mode == TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT) && (apiVersion > VK_MAKE_API_VERSION(0, 1, 1, 0)))
    {
        vulkan12Features.drawIndirectCount = true;
        vulkan12Features.scalarBlockLayout = true;
        if (m_params.useDeviceAddressCommands)
            vulkan12Features.bufferDeviceAddress = true;

        addToChainVulkanStructure(&nextPtr, vulkan12Features);
    }
    else
    {
        addToChainVulkanStructure(&nextPtr, sblFeatures);

#ifndef CTS_USES_VULKANSC
        if (m_params.useDeviceAddressCommands)
        {
            bufferDeviceAddressFeatures.bufferDeviceAddress = true;
            addToChainVulkanStructure(&nextPtr, bufferDeviceAddressFeatures);
        }
#endif // CTS_USES_VULKANSC
    }

    return createRobustBufferAccessDevice(context, instance, &features2);
}

TestInstance *DrawIndexedTestCase::createInstance(Context &context) const
{
    InstanceWrapper instance(context);
    DeviceWrapper device(context);

    // when we are testing pipeline robustness we need to use default device
    // (robustBufferAccess should not be enabled)
    if (!m_params.usePipelineRobustness)
        device = createTestDevice(context, instance);

    return new DrawIndexedInstance(context, std::move(instance), std::move(device), m_params);
}

void DrawIndexedTestCase::initPrograms(SourceCollections &sourceCollections) const
{
    auto &glslSources = sourceCollections.glslSources;
    std::string vertexSource("#version 450\n"
                             "layout(location = 0) in vec4 inPosition;\n"
                             "void main(void)\n"
                             "{\n"
                             "\tgl_Position = inPosition;\n"
                             "\tgl_PointSize = 1.0;\n"
                             "}\n");
    glslSources.add("vert") << glu::VertexSource(vertexSource);

    std::string fragmentSource("#version 450\n"
                               "precision highp float;\n"
                               "layout(location = 0) out vec4 fragColor;\n"
                               "void main (void)\n"
                               "{\n"
                               "\tfragColor = vec4(0.2, 1.0, 0.5, 1.0);\n"
                               "}\n");
    glslSources.add("frag") << glu::FragmentSource(fragmentSource);

    if ((m_params.usedStages == UsedStages::VERT_TESS_FRAG) || (m_params.usedStages == UsedStages::VERT_TESS_GEOM_FRAG))
    {
        const std::string tessControlSource(
            "#version 450\n"
            "layout(vertices = 1) out;\n"
            "void main(void) {\n"
            "   gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
            "   gl_TessLevelOuter[0] = 1.0;\n"
            "   gl_TessLevelOuter[1] = 1.0;\n"
            "   gl_TessLevelOuter[2] = 1.0;\n"
            "}\n");
        glslSources.add("tessc") << glu::TessellationControlSource(tessControlSource);

        const std::string tessEvalSource("#version 450\n"
                                         "layout(triangles, point_mode) in;\n"
                                         "void main(void) {\n"
                                         "   gl_Position = gl_in[0].gl_Position;\n"
                                         "}\n");
        glslSources.add("tesse") << glu::TessellationEvaluationSource(tessEvalSource);
    }
    if ((m_params.usedStages == UsedStages::VERT_GEOM_FRAG) || (m_params.usedStages == UsedStages::VERT_TESS_GEOM_FRAG))
    {
        const std::string geometrySource("#version 450\n"
                                         "layout(points) in;\n"
                                         "layout(points, max_vertices = 1) out;\n"
                                         "void main(void) {\n"
                                         "   gl_Position = gl_in[0].gl_Position;\n"
                                         "   EmitVertex();\n"
                                         "   EndPrimitive();\n"
                                         "}\n");
        glslSources.add("geom") << glu::GeometrySource(geometrySource);
    }
}

class BindIndexBuffer2Instance : public vkt::TestInstance
{
public:
    BindIndexBuffer2Instance(Context &c, InstanceWrapper &&instance, DeviceWrapper &&device, const TestParams &params);
    virtual ~BindIndexBuffer2Instance(void) = default;

    virtual tcu::TestStatus iterate(void) override;

protected:
    InstanceWrapper m_instance;
    DeviceWrapper m_device;
    const TestParams m_params;

protected:
    inline const DeviceInterface &getDeviceInterface() const
    {
        return m_device.getDriver();
    }
    inline VkDevice getDevice() const
    {
        return *m_device;
    }
    inline VkPhysicalDevice getPhysicalDevice() const
    {
        return m_device.getPhysicalDevice();
    }
    inline Allocator &getAllocator()
    {
        return m_device.getAllocator();
    }
    VkQueue getQueue() const;
};

BindIndexBuffer2Instance::BindIndexBuffer2Instance(Context &c, InstanceWrapper &&instance, DeviceWrapper &&device,
                                                   const TestParams &params)
    : vkt::TestInstance(c)
    , m_instance(std::move(instance))
    , m_device(std::move(device))
    , m_params(params)
{
}

VkQueue BindIndexBuffer2Instance::getQueue() const
{
    VkQueue queue = VK_NULL_HANDLE;
    getDeviceInterface().getDeviceQueue(getDevice(), m_context.getUniversalQueueFamilyIndex(), 0, &queue);
    return queue;
}

class BindIndexBuffer2TestCase : public DrawIndexedTestCase
{
public:
    BindIndexBuffer2TestCase(tcu::TestContext &testContext, const std::string &name, const TestParams &params);
    ~BindIndexBuffer2TestCase(void) = default;

    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override;
    void initPrograms(SourceCollections &programs) const override;
};

BindIndexBuffer2TestCase::BindIndexBuffer2TestCase(tcu::TestContext &testContext, const std::string &name,
                                                   const TestParams &params)
    : DrawIndexedTestCase(testContext, name, params)
{
}

#ifdef CTS_USES_VULKANSC
#define DEPENDENT_MAINTENANCE_5_EXTENSION_NAME "VK_KHR_maintenance5"
#else
#define DEPENDENT_MAINTENANCE_5_EXTENSION_NAME VK_KHR_MAINTENANCE_5_EXTENSION_NAME
#endif

void BindIndexBuffer2TestCase::checkSupport(Context &context) const
{
    DrawIndexedTestCase::checkSupport(context);
    context.requireDeviceFunctionality(DEPENDENT_MAINTENANCE_5_EXTENSION_NAME);
}

void BindIndexBuffer2TestCase::initPrograms(SourceCollections &programs) const
{
    const std::string vertexSource("#version 450\n"
                                   "layout(location = 0) in vec4 inPosition;\n"
                                   "void main(void) {\n"
                                   "   gl_Position = inPosition;\n"
                                   "   gl_PointSize = 1.0;\n"
                                   "}\n");
    programs.glslSources.add("vert") << glu::VertexSource(vertexSource);

    const std::string fragmentSource("#version 450\n"
                                     "layout(location = 0) out vec4 fragColor;\n"
                                     "void main (void) {\n"
                                     "   fragColor = vec4(1.0);\n"
                                     "}\n");
    programs.glslSources.add("frag") << glu::FragmentSource(fragmentSource);
}

TestInstance *BindIndexBuffer2TestCase::createInstance(Context &context) const
{
    InstanceWrapper instance(context);
    DeviceWrapper device = createTestDevice(context, instance);
    return new BindIndexBuffer2Instance(context, std::move(instance), std::move(device), m_params);
}

tcu::TestStatus BindIndexBuffer2Instance::iterate(void)
{
    const DeviceInterface &vk     = this->getDeviceInterface();
    const VkDevice device         = this->getDevice();
    Allocator &allocator          = this->getAllocator();
    const VkQueue queue           = this->getQueue();
    const uint32_t queueFamilyIdx = m_context.getUniversalQueueFamilyIndex();
    tcu::TestLog &log             = m_context.getTestContext().getLog();

    const VkFormat colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
    const tcu::UVec2 renderSize{64, 64};
    const std::vector<VkViewport> viewports{makeViewport(renderSize)};
    const std::vector<VkRect2D> scissors{makeRect2D(renderSize)};
    VkPipelineDynamicStateCreateInfo *dynamicStatePtr = nullptr;

    // build vertices data
    std::vector<tcu::Vec4> vertices{// first triangle in 2nd quarter, it should not be drawn
                                    {-1.0f, 0.1f, 0.0f, 1.0f},
                                    {-1.0f, 1.0f, 0.0f, 1.0f},
                                    {-0.1f, 0.1f, 0.0f, 1.0f},

                                    // second triangle in 2nd quarter, it should not be drawn
                                    {-0.1f, 0.1f, 0.0f, 1.0f},
                                    {-1.0f, 1.0f, 0.0f, 1.0f},
                                    {-0.1f, 1.0f, 0.0f, 1.0f},

                                    // first triangle in 3rd quarter, it must be drawn
                                    {0.0f, -1.0f, 0.0f, 1.0f},
                                    {-1.0f, -1.0f, 0.0f, 1.0f},
                                    {-1.0f, 0.0f, 0.0f, 1.0f},

                                    // second triangle in 3rd quarter if robustness works as expected,
                                    // otherwise will be drawn in 1st quarter as well
                                    {0.0f, -1.0f, 0.0f, 1.0f},
                                    {-1.0f, 0.0f, 0.0f, 1.0f},
                                    {1.0f, 1.0f, 0.0f, 1.0f}};

    const VkBufferUsageFlags commonUsage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        (m_params.useDeviceAddressCommands ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0);

    MemoryRequirement memReq = m_params.useDeviceAddressCommands ?
                                   MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress :
                                   MemoryRequirement::HostVisible;

    // create vertex buffer
    const VkDeviceSize vertexBufferSize = vertices.size() * sizeof(tcu::Vec4);
    const VkBufferCreateInfo vertexBufferInfo =
        makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | commonUsage);
    BufferWithMemory vertexBuffer(vk, device, allocator, vertexBufferInfo, memReq);
    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertices.data(), (size_t)vertexBufferSize);
    flushAlloc(vk, device, vertexBuffer.getAllocation());

    // build index data
    const uint32_t leadingCount = m_params.leadingCount;
    std::vector<uint32_t> indices(leadingCount * 6 + 6);
    for (uint32_t j = 0; j < leadingCount; ++j)
        for (uint32_t k = 0; k < 6; ++k)
        {
            indices[j * 6 + k] = k;
        }
    std::iota(std::next(indices.begin(), (leadingCount * 6)), indices.end(), 6u);

    const uint32_t firstIndex        = 0;
    const uint32_t indexCount        = 6;
    const VkDeviceSize bindingOffset = leadingCount * 6 * sizeof(uint32_t);
    VkDeviceSize indexBindingSize    = 6 * sizeof(uint32_t);
    VkDeviceSize indexBufferSize     = indices.size() * sizeof(uint32_t);
    switch (m_params.ooType)
    {
    case OOTypes::OO_NONE:
        // default values already set
        break;
    case OOTypes::OO_INDEX:
        indices.back() = 33; // out of range index
        break;
    case OOTypes::OO_SIZE:
        indexBindingSize = 5 * sizeof(uint32_t);
        break;
    case OOTypes::OO_WHOLE_SIZE:
        indexBindingSize = VK_WHOLE_SIZE;
        indexBufferSize  = (indices.size() - 1) * sizeof(uint32_t);
        break;
    }

    // create index buffer
    const VkBufferCreateInfo indexBufferInfo =
        makeBufferCreateInfo(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | commonUsage);
    BufferWithMemory indexBuffer(vk, device, allocator, indexBufferInfo, memReq);
    deMemcpy(indexBuffer.getAllocation().getHostPtr(), indices.data(), size_t(indexBufferSize));
    flushAlloc(vk, device, indexBuffer.getAllocation());

    // create indirect buffer
    const VkDrawIndexedIndirectCommand drawIndirectCommand{
        indexCount, // indexCount
        1u,         // instanceCount
        firstIndex, // firstIndex
        0u,         // vertexOffset
        0u,         // firstInstance
    };
    const VkDeviceSize indirectBufferSize = sizeof(drawIndirectCommand);
    const VkBufferCreateInfo indirectBufferInfo =
        makeBufferCreateInfo(indirectBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | commonUsage);
    BufferWithMemory indirectBuffer(vk, *m_device, allocator, indirectBufferInfo, memReq);
    if ((m_params.mode == TM_DRAW_INDEXED_INDIRECT) || (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT))
    {
        deMemcpy(indirectBuffer.getAllocation().getHostPtr(), &drawIndirectCommand, indirectBufferSize);
        flushAlloc(vk, device, indirectBuffer.getAllocation());
    }

    // create indirect count buffer
    const VkDeviceSize indirectCountBufferSize = sizeof(uint32_t);
    const VkBufferCreateInfo indirectCountBufferInfo =
        makeBufferCreateInfo(indirectCountBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | commonUsage);
    BufferWithMemory indirectCountBuffer(vk, *m_device, allocator, indirectCountBufferInfo, memReq);
    if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
    {
        *static_cast<uint32_t *>(indirectCountBuffer.getAllocation().getHostPtr()) = 1u;
        flushAlloc(vk, device, indirectCountBuffer.getAllocation());
    }

    // create output buffer that will be used to read rendered image
    const VkDeviceSize outputBufferSize = renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
    const VkBufferCreateInfo outputBufferInfo =
        makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | commonUsage);
    BufferWithMemory outputBuffer(vk, device, allocator, outputBufferInfo, memReq);
    VkDeviceAddress outputBufferAddress = 0ull;

#ifndef CTS_USES_VULKANSC
    VkDynamicState dynamicState{VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE};
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = initVulkanStructure();

    VkDeviceAddress vertexBufferAddress        = 0ull;
    VkDeviceAddress indexBufferAddress         = 0ull;
    VkDeviceAddress indirectBufferAddress      = 0ull;
    VkDeviceAddress indirectCountBufferAddress = 0ull;

    if (m_params.useDeviceAddressCommands)
    {
        dynamicStateCreateInfo.dynamicStateCount = 1u;
        dynamicStateCreateInfo.pDynamicStates    = &dynamicState;
        dynamicStatePtr                          = &dynamicStateCreateInfo;

        vertexBufferAddress        = getBufferDeviceAddress(vk, *m_device, *vertexBuffer);
        indexBufferAddress         = getBufferDeviceAddress(vk, *m_device, *indexBuffer);
        indirectBufferAddress      = getBufferDeviceAddress(vk, *m_device, *indirectBuffer);
        indirectCountBufferAddress = getBufferDeviceAddress(vk, *m_device, *indirectCountBuffer);
        outputBufferAddress        = getBufferDeviceAddress(vk, *m_device, *outputBuffer);
    }
#endif

    // create color buffer
    const VkExtent3D imageExtent = makeExtent3D(renderSize.x(), renderSize.y(), 1u);
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
        nullptr,                                                               // const void* pNext;
        0u,                                                                    // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
        colorFormat,                                                           // VkFormat format;
        imageExtent,                                                           // VkExtent3D extent;
        1u,                                                                    // uint32_t mipLevels;
        1u,                                                                    // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
        0u,                                                                    // uint32_t queueFamilyIndexCount;
        nullptr,                                                               // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                             // VkImageLayout initialLayout;
    };
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    ImageWithMemory colorImage(vk, *m_device, allocator, imageCreateInfo, MemoryRequirement::Any);
    auto colorImageView = makeImageView(vk, *m_device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

    // create shader modules, renderpass, framebuffer and pipeline
    auto vertShaderModule                 = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    auto fragShaderModule                 = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"));
    Move<VkRenderPass> renderPass         = makeRenderPass(vk, device, colorFormat);
    Move<VkPipelineLayout> pipelineLayout = makePipelineLayout(vk, device);
    auto framebuffer = makeFramebuffer(vk, device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());
    Move<VkPipeline> graphicsPipeline = makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertShaderModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        *fragShaderModule, *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, 0, nullptr,
        nullptr, nullptr, nullptr, nullptr, dynamicStatePtr);

    auto cmdPool   = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIdx);
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    // transition colorbuffer layout
    VkImageMemoryBarrier imageBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorImage.get(), colorSRR);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u,
                          0u, 0u, 0u, 1u, &imageBarrier);

    const VkRect2D renderArea = makeRect2D(0, 0, renderSize.x(), renderSize.y());
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

    if (!m_params.useDeviceAddressCommands)
        vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &static_cast<const VkDeviceSize &>(0));

#ifndef CTS_USES_VULKANSC
    if (m_params.useDeviceAddressCommands)
    {
        VkBindVertexBuffer3InfoKHR vertexBuffer3Info = initVulkanStructure();
        vertexBuffer3Info.setStride                  = true;
        vertexBuffer3Info.addressRange               = {vertexBufferAddress, vertexBufferSize, sizeof(tcu::Vec4)};
        vk.cmdBindVertexBuffers3KHR(*cmdBuffer, 0, 1, &vertexBuffer3Info);

        VkBindIndexBuffer3InfoKHR bindIndexBuffer3Info = initVulkanStructure();
        bindIndexBuffer3Info.addressRange              = {indexBufferAddress + bindingOffset, indexBindingSize};
        bindIndexBuffer3Info.indexType                 = VK_INDEX_TYPE_UINT32;
        vk.cmdBindIndexBuffer3KHR(*cmdBuffer, &bindIndexBuffer3Info);
    }
    else
        vk.cmdBindIndexBuffer2(*cmdBuffer, indexBuffer.get(), bindingOffset, indexBindingSize, VK_INDEX_TYPE_UINT32);
#else
    DE_UNREF(bindingOffset);
    DE_UNREF(indexBindingSize);
#endif

    // we will draw all points at index 0
    if (!m_params.useDeviceAddressCommands)
    {
        switch (m_params.mode)
        {
        case TM_DRAW_INDEXED:
            vk.cmdDrawIndexed(*cmdBuffer, indexCount, 1u, firstIndex, 0, 0);
            break;

        case TM_DRAW_INDEXED_INDIRECT:
            vk.cmdDrawIndexedIndirect(*cmdBuffer, indirectBuffer.get(), 0, 1, uint32_t(sizeof(drawIndirectCommand)));
            break;

        case TM_DRAW_INDEXED_INDIRECT_COUNT:
            vk.cmdDrawIndexedIndirectCount(*cmdBuffer, indirectBuffer.get(), 0, indirectCountBuffer.get(), 0, 1,
                                           uint32_t(sizeof(drawIndirectCommand)));
            break;

        case TM_DRAW_MULTI_INDEXED:
#ifndef CTS_USES_VULKANSC
        {
            const VkMultiDrawIndexedInfoEXT indexInfo[/* { firstIndex, indexCount, vertexOffset } */]{
                {firstIndex + 3, 3, 0},
                {firstIndex, 3, 0},
            };
            vk.cmdDrawMultiIndexedEXT(*cmdBuffer, DE_LENGTH_OF_ARRAY(indexInfo), indexInfo, 1, 0,
                                      sizeof(VkMultiDrawIndexedInfoEXT), nullptr);
        }
#endif
        break;
        }
    }

#ifndef CTS_USES_VULKANSC
    if (m_params.useDeviceAddressCommands)
    {
        if (m_params.mode == TM_DRAW_INDEXED)
            vk.cmdDrawIndexed(*cmdBuffer, (uint32_t)indexCount, 1, firstIndex, 0, 0);
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT)
        {
            VkDrawIndirect2InfoKHR drawIndirect2Info = initVulkanStructure();
            drawIndirect2Info.addressRange = {indirectBufferAddress, indirectBufferSize, sizeof(drawIndirectCommand)};
            drawIndirect2Info.drawCount    = 1u;

            vk.cmdDrawIndexedIndirect2KHR(*cmdBuffer, &drawIndirect2Info);
        }
        else if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
        {
            VkDrawIndirectCount2InfoKHR drawIndirectCount2Info = initVulkanStructure();
            drawIndirectCount2Info.addressRange                = {indirectBufferAddress, indirectBufferSize,
                                                                  sizeof(drawIndirectCommand)};
            drawIndirectCount2Info.countAddressRange           = {indirectCountBufferAddress, indirectCountBufferSize};
            drawIndirectCount2Info.maxDrawCount                = 1;

            vk.cmdDrawIndexedIndirectCount2KHR(*cmdBuffer, &drawIndirectCount2Info);
        }
        else // TM_DRAW_MULTI_INDEXED
            DE_ASSERT(false);
    }
#endif // CTS_USES_VULKANSC

    endRenderPass(vk, *cmdBuffer);

    // wait till data is transfered to image
    imageBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorImage, colorSRR);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                          0u, 0u, 0u, 0u, 1u, &imageBarrier);

    // read back color image
    copyImageToMemory(vk, *cmdBuffer, *colorImage, imageExtent, *outputBuffer, outputBufferAddress, outputBufferSize);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // get output buffer
    invalidateAlloc(vk, device, outputBuffer.getAllocation());
    const tcu::TextureFormat resultFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess resultAccess(resultFormat, renderSize.x(), renderSize.y(), 1u,
                                             outputBuffer.getAllocation().getHostPtr());

    // neither one triangle should be drawn in the second quarter, they are omitted by the offset or the firstIndex parameters
    const tcu::Vec4 p11 = resultAccess.getPixel((1 * renderSize.x()) / 8, (5 * renderSize.y()) / 8);
    const tcu::Vec4 p12 = resultAccess.getPixel((3 * renderSize.x()) / 8, (7 * renderSize.y()) / 8);
    const bool c1       = p11.x() == clearColor.x() && p11.y() == clearColor.y() && p11.z() == clearColor.z() &&
                    p12.x() == clearColor.x() && p12.y() == clearColor.y() && p12.z() == clearColor.z();

    // small triangle in the third quarter must be drawn always
    const tcu::Vec4 p2 = resultAccess.getPixel((1 * renderSize.x()) / 8, (1 * renderSize.y()) / 8);
    const bool c2      = p2.x() != clearColor.x() && p2.y() != clearColor.y() && p2.z() != clearColor.z();

    // if robustness works, then the origin of coordinate system will be read in shader instead of a value that an index points (1,1)
    const tcu::Vec4 p3 = resultAccess.getPixel((3 * renderSize.x()) / 4, (3 * renderSize.y()) / 4);
    const bool c3      = p3.x() == clearColor.x() && p3.y() == clearColor.y() && p3.z() == clearColor.z();

    bool verdict = false;
    switch (m_params.ooType)
    {
    case OOTypes::OO_NONE:
        verdict = c1 && c2 && !c3;
        break;
    default:
        verdict = c1 && c2 && c3;
        break;
    }

    log << tcu::TestLog::ImageSet("Result", "") << tcu::TestLog::Image(std::to_string(m_params.mode), "", resultAccess)
        << tcu::TestLog::EndImageSet;
    return (*(verdict ? &tcu::TestStatus::pass : &tcu::TestStatus::fail))(std::string());
}

tcu::TestCaseGroup *createCmdBindIndexBuffer2Tests(tcu::TestContext &testCtx)
{
    const std::pair<std::string, OOTypes> OutOfTypes[]{
        {"oo_none", OOTypes::OO_NONE},
        {"oo_index", OOTypes::OO_INDEX},
        {"oo_size", OOTypes::OO_SIZE},
        {"oo_whole_size", OOTypes::OO_WHOLE_SIZE},
    };

    const uint32_t offsets[] = {0, 100};

    // Test access outside of the buffer with using the vkCmdBindIndexBuffer2 function from
    // VK_KHR_maintenance5 and with vkCmdBindIndexBuffer3KHR from VK_KHR_device_address_commands.
    de::MovePtr<tcu::TestCaseGroup> gRoot(new tcu::TestCaseGroup(testCtx, "bind_index_buffer2"));
    for (uint32_t offset : offsets)
    {
        de::MovePtr<tcu::TestCaseGroup> gOffset(
            new tcu::TestCaseGroup(testCtx, ("offset_" + std::to_string(offset)).c_str()));
        for (const auto &mode : testModes)
        {
            de::MovePtr<tcu::TestCaseGroup> gMode(new tcu::TestCaseGroup(testCtx, mode.first.c_str()));
            for (const auto &ooType : OutOfTypes)
            {
                TestParams p;
                p.mode         = mode.second;
                p.ooType       = ooType.second;
                p.leadingCount = offset;

                gMode->addChild(new BindIndexBuffer2TestCase(testCtx, ooType.first, p));

#ifndef CTS_USES_VULKANSC
                // skip testing VK_WHOLE_SIZE for device_address_commands, as it is not supported
                if (ooType.second == OOTypes::OO_WHOLE_SIZE)
                    continue;

                // limit number of tests repeated for device_address_commands
                if ((mode.second != TestMode::TM_DRAW_MULTI_INDEXED) && (offset == 100))
                {
                    p.useDeviceAddressCommands = true;
                    gMode->addChild(new BindIndexBuffer2TestCase(testCtx, ooType.first + "_device_address", p));
                }
#endif // CTS_USES_VULKANSC
            }
            gOffset->addChild(gMode.release());
        }
        gRoot->addChild(gOffset.release());
    }

    return gRoot.release();
}

tcu::TestCaseGroup *createIndexAccessTests(tcu::TestContext &testCtx)
{
    std::map<UsedStages, std::string> allStageCombinations{{UsedStages::VERT_FRAG, "vert_frag"},
                                                           {UsedStages::VERT_GEOM_FRAG, "vert_geom_frag"},
                                                           {UsedStages::VERT_TESS_FRAG, "vert_tess_frag"},
                                                           {UsedStages::VERT_TESS_GEOM_FRAG, "vert_tess_geom_frag"}};

    std::map<VkIndexType, std::string> typeCombinations{{VK_INDEX_TYPE_UINT8, "uint8"},
                                                        {VK_INDEX_TYPE_UINT16, "uint16"}};

    // Test access outside of the buffer for indices
    de::MovePtr<tcu::TestCaseGroup> indexAccessTests(new tcu::TestCaseGroup(testCtx, "index_access"));

    for (const auto &[n, mode] : testModes)
    {
        TestParams params;
        params.mode              = mode;
        params.robustnessVersion = 2;

        std::string name = n + "_2";
        indexAccessTests->addChild(new DrawIndexedTestCase(testCtx, name, params));

#ifndef CTS_USES_VULKANSC
        if (mode != TestMode::TM_DRAW_MULTI_INDEXED)
        {
            params.useDeviceAddressCommands = true;
            indexAccessTests->addChild(new DrawIndexedTestCase(testCtx, name + "_device_address", params));
        }

        // Test oob access for 8-bit and 16-bit indices
        params.usedStages            = UsedStages::VERT_FRAG;
        params.usePipelineRobustness = false;
        for (const auto &[indexType, postfix] : typeCombinations)
        {
            name                            = n + "_2_" + postfix;
            params.indexType                = indexType;
            params.useDeviceAddressCommands = false;
            indexAccessTests->addChild(new DrawIndexedTestCase(testCtx, name, params));

            // Test oob access for 8-bit and 16-bit indices with device address commands
            if (mode != TestMode::TM_DRAW_MULTI_INDEXED)
            {
                params.useDeviceAddressCommands = true;
                indexAccessTests->addChild(new DrawIndexedTestCase(testCtx, name + "_device_address", params));
            }
        }

        params.useDeviceAddressCommands = false;
        for (uint32_t robustnessVersion : {1, 2})
        {
            params.robustnessVersion = robustnessVersion;
            std::string versionStr   = std::to_string(robustnessVersion) + "_";

            // Test EXT_pipeline_robustness with all combinations of used stages and for both robustness versions
            for (auto &[usedStages, stagesName] : allStageCombinations)
            {
                name                         = n + "_pipeline_robustness_" + versionStr + stagesName;
                params.usedStages            = usedStages;
                params.usePipelineRobustness = true;
                indexAccessTests->addChild(new DrawIndexedTestCase(testCtx, name, params));
            }
        }
#endif // CTS_USES_VULKANSC
    }

    return indexAccessTests.release();
}

} // namespace vkt::robustness
