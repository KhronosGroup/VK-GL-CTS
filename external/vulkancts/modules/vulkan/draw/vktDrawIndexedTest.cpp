/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Draw Indexed Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawIndexedTest.hpp"

#include "tcuDefs.hpp"
#include "tcuVectorType.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"

#include "vktDrawBaseClass.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkRef.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTestCase.hpp"
#include "tcuVectorUtil.hpp"
#include "rrRenderer.hpp"

#include <numeric>

namespace vkt
{
namespace Draw
{
namespace
{

enum
{
    VERTEX_OFFSET_DEFAULT   = 13,
    VERTEX_OFFSET_MINUS_ONE = -1,
    VERTEX_OFFSET_NEGATIVE  = -13,
};

enum class IndexBindOffset
{
    DEFAULT  = 0,
    POSITIVE = 16, // Must be aligned to the index data type size.
};

enum class MemoryBindOffset
{
    DEFAULT  = 0,
    POSITIVE = 16, // Will be rounded up to the alignment requirement.
};

enum TestType
{
    TEST_TYPE_NON_MAINTENANCE_6 = 0,
    TEST_TYPE_MAINTENANCE6_INDEXED,
    TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT,
    //    TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT_KHR,
    TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT,
#ifndef CTS_USES_VULKANSC
    TEST_TYPE_MAINTENANCE6_MULTI_INDEXED_EXT,
#endif
    TEST_TYPE_LAST
};

enum
{
    RENDER_WIDTH_SMALLEST  = 1,
    RENDER_HEIGHT_SMALLEST = 1,
    RENDER_WIDTH_DEFAULT   = 256,
    RENDER_HEIGHT_DEFAULT  = 256,
};

struct TestSpec2 : TestSpecBase
{
    const int32_t vertexOffset;
    const vk::VkDeviceSize bindIndexBufferOffset;
    const vk::VkDeviceSize memoryBindOffset;
    const TestType testType;
    bool useMaintenance5Ext;
    const bool nullDescriptor;
    const bool bindIndexBuffer2;
    const bool testDrawCount;

    TestSpec2(const ShaderMap &shaders_, vk::VkPrimitiveTopology topology_, SharedGroupParams groupParams_,
              int32_t vertexOffset_, vk::VkDeviceSize bindIndexBufferOffset_, vk::VkDeviceSize memoryBindOffset_,
              TestType testType_, bool useMaintenance5Ext_, bool nullDescriptor_, bool bindIndexBuffer2_,
              bool testDrawCount_ = false)
        : TestSpecBase{shaders_, topology_, groupParams_}
        , vertexOffset(vertexOffset_)
        , bindIndexBufferOffset(bindIndexBufferOffset_)
        , memoryBindOffset(memoryBindOffset_)
        , testType(testType_)
        , useMaintenance5Ext(useMaintenance5Ext_)
        , nullDescriptor(nullDescriptor_)
        , bindIndexBuffer2(bindIndexBuffer2_)
        , testDrawCount(testDrawCount_)
    {
    }
};

class DrawIndexed : public DrawTestsBaseClass
{
public:
    typedef TestSpec2 TestSpec;

    DrawIndexed(Context &context, TestSpec testSpec);
    virtual void initialize(void);
    virtual tcu::TestStatus iterate(void);

protected:
    void cmdBindIndexBufferImpl(vk::VkCommandBuffer commandBuffer, vk::VkBuffer indexBuffer, vk::VkDeviceSize offset,
                                vk::VkDeviceSize size, vk::VkIndexType indexType);
    std::vector<uint32_t> m_indexes;
    de::SharedPtr<Buffer> m_indexBuffer;
    const TestSpec m_testSpec;
};

void DrawIndexed::initialize(void)
{
    const vk::VkDevice device       = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const auto viewMask             = getDefaultViewMask();
    const auto multiview            = (viewMask != 0u);

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto descriptorSetLayout = layoutBuilder.build(m_vk, device);

    PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

    if (m_testSpec.testDrawCount)
    {
        pipelineLayoutCreateInfo.setLayoutCount = 1u;
        pipelineLayoutCreateInfo.pSetLayouts    = &descriptorSetLayout.get();
    }

    m_pipelineLayout = vk::createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo);

    const vk::VkExtent3D targetImageExtent = {m_renderWidth, m_renderHeight, 1};
    const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, targetImageExtent, 1,
                                                m_layers, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
                                                vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                    vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                    vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    m_colorTargetImage = Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(),
                                               m_context.getUniversalQueueFamilyIndex());

    const ImageSubresourceRange colorSRR(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_layers);
    const auto imageViewType = (multiview ? vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY : vk::VK_IMAGE_VIEW_TYPE_2D);
    const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), imageViewType, m_colorAttachmentFormat,
                                                  colorSRR);
    m_colorTargetView = vk::createImageView(m_vk, device, &colorTargetViewInfo);

    // create renderpass and framebuffer only when we are not using dynamic rendering
    if (!m_groupParams->useDynamicRendering)
    {
        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(AttachmentDescription(
            m_colorAttachmentFormat, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
            vk::VK_ATTACHMENT_STORE_OP_STORE, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE, vk::VK_ATTACHMENT_STORE_OP_STORE,
            vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL));

        const vk::VkAttachmentReference colorAttachmentReference{0, vk::VK_IMAGE_LAYOUT_GENERAL};

        renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, nullptr, 1,
                                                           &colorAttachmentReference, nullptr, AttachmentReference(), 0,
                                                           nullptr));

        const std::vector<uint32_t> viewMasks(1u, viewMask);

        const vk::VkRenderPassMultiviewCreateInfo multiviewCreateInfo = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                 // const void* pNext;
            de::sizeU32(viewMasks),                                  // uint32_t subpassCount;
            de::dataOrNull(viewMasks),                               // const uint32_t* pViewMasks;
            0u,                                                      // uint32_t dependencyCount;
            nullptr,                                                 // const int32_t* pViewOffsets;
            de::sizeU32(viewMasks),                                  // uint32_t correlationMaskCount;
            de::dataOrNull(viewMasks),                               // const uint32_t* pCorrelationMasks;
        };

        if (multiview)
            renderPassCreateInfo.pNext = &multiviewCreateInfo;

        m_renderPass = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);

        // create framebuffer
        std::vector<vk::VkImageView> colorAttachments{*m_colorTargetView};
        const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, colorAttachments, m_renderWidth,
                                                          m_renderHeight, 1);
        m_framebuffer = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);
    }

    const vk::VkVertexInputBindingDescription vertexInputBindingDescription = {
        0,
        sizeof(VertexElementData),
        vk::VK_VERTEX_INPUT_RATE_VERTEX,
    };

    const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u}, // VertexElementData::position
        {1u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT,
         static_cast<uint32_t>(sizeof(tcu::Vec4))}, // VertexElementData::color
        {2u, 0u, vk::VK_FORMAT_R32_SINT,
         static_cast<uint32_t>(sizeof(tcu::Vec4)) * 2} // VertexElementData::refVertexIndex
    };

    m_vertexInputState = m_testSpec.testDrawCount ?
                             PipelineCreateInfo::VertexInputState(0, nullptr, 0, nullptr) :
                             PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription,
                                                                  DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),
                                                                  vertexInputAttributeDescriptions);

    const vk::VkDeviceSize dataSize = m_data.size() * sizeof(VertexElementData);
    m_vertexBuffer =
        Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(dataSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                               m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_vertexBuffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, &m_data[0], static_cast<size_t>(dataSize));

    vk::flushAlloc(m_vk, device, m_vertexBuffer->getBoundMemory());

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    m_cmdPool   = vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);
    m_cmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    if (m_groupParams->useSecondaryCmdBuffer)
        m_secCmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    initPipeline(device);
}

class DrawInstancedIndexed : public DrawIndexed
{
public:
    DrawInstancedIndexed(Context &context, TestSpec testSpec);
    virtual tcu::TestStatus iterate(void);
};

DrawIndexed::DrawIndexed(Context &context, TestSpec testSpec)
    : DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT],
                         testSpec.groupParams, testSpec.topology, 1u,
                         testSpec.testDrawCount ? RENDER_WIDTH_SMALLEST : RENDER_WIDTH_DEFAULT,
                         testSpec.testDrawCount ? RENDER_HEIGHT_SMALLEST : RENDER_HEIGHT_DEFAULT)
    , m_testSpec(testSpec)
{
    if (testSpec.testType == TEST_TYPE_NON_MAINTENANCE_6)
    {
        // When using a positive vertex offset, the strategy is:
        // - Storing vertices with that offset in the vertex buffer.
        // - Using indices normally as if they were stored at the start of the buffer.
        //
        // When using a negative vertex offset, the strategy is:
        // - Store vertices at the start of the vertex buffer.
        // - Increase indices by abs(offset) so when substracting it, it results in the regular positions.

        const uint32_t indexOffset =
            (m_testSpec.vertexOffset < 0 ? static_cast<uint32_t>(-m_testSpec.vertexOffset) : 0u);

        switch (m_topology)
        {
        case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
            m_indexes.push_back(0 + indexOffset);
            m_indexes.push_back(0 + indexOffset);
            m_indexes.push_back(2 + indexOffset);
            m_indexes.push_back(0 + indexOffset);
            m_indexes.push_back(6 + indexOffset);
            m_indexes.push_back(6 + indexOffset);
            m_indexes.push_back(0 + indexOffset);
            m_indexes.push_back(7 + indexOffset);
            break;
        case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
            m_indexes.push_back(0 + indexOffset);
            m_indexes.push_back(0 + indexOffset);
            m_indexes.push_back(2 + indexOffset);
            m_indexes.push_back(0 + indexOffset);
            m_indexes.push_back(6 + indexOffset);
            m_indexes.push_back(5 + indexOffset);
            m_indexes.push_back(0 + indexOffset);
            m_indexes.push_back(7 + indexOffset);
            break;

        case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        case vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        case vk::VK_PRIMITIVE_TOPOLOGY_LAST:
            DE_FATAL("Topology not implemented");
            break;
        default:
            DE_FATAL("Unknown topology");
            break;
        }
    }

    // This works for both positive and negative vertex offsets.
    for (int unusedIdx = 0; unusedIdx < testSpec.vertexOffset; unusedIdx++)
    {
        m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
    }

    int vertexIndex = (testSpec.vertexOffset >= 0 ? testSpec.vertexOffset : 0);

    m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
    m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
    m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, -0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
    m_data.push_back(VertexElementData(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
    m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, -0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
    m_data.push_back(VertexElementData(tcu::Vec4(0.3f, 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
    m_data.push_back(VertexElementData(tcu::Vec4(0.3f, -0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));
    m_data.push_back(VertexElementData(tcu::Vec4(0.3f, 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), vertexIndex++));

    m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));

    initialize();
}

void DrawIndexed::cmdBindIndexBufferImpl(vk::VkCommandBuffer commandBuffer, vk::VkBuffer indexBuffer,
                                         vk::VkDeviceSize offset, vk::VkDeviceSize dataSize, vk::VkIndexType indexType)
{
#ifndef CTS_USES_VULKANSC
    if (m_testSpec.useMaintenance5Ext)
        m_vk.cmdBindIndexBuffer2(commandBuffer, indexBuffer, offset, dataSize, indexType);
    else
#endif
    {
        DE_UNREF(dataSize);
        m_vk.cmdBindIndexBuffer(commandBuffer, indexBuffer, offset, indexType);
    }
}

tcu::TestStatus DrawIndexed::iterate(void)
{
    tcu::TestLog &log         = m_context.getTestContext().getLog();
    const auto &vki           = m_context.getInstanceInterface();
    const auto physDev        = m_context.getPhysicalDevice();
    const vk::VkQueue queue   = m_context.getUniversalQueue();
    const vk::VkDevice device = m_context.getDevice();
    const auto memProps       = vk::getPhysicalDeviceMemoryProperties(vki, physDev);
    const auto atomSize       = m_context.getDeviceProperties().limits.nonCoherentAtomSize;
    const auto dataSize       = static_cast<vk::VkDeviceSize>(de::dataSize(m_indexes));
    const auto bufferSize     = dataSize + m_testSpec.bindIndexBufferOffset;
    vk::SimpleAllocator allocator(m_vk, device, memProps,
                                  vk::SimpleAllocator::OptionalOffsetParams({atomSize, m_testSpec.memoryBindOffset}));

    m_indexBuffer =
        Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
                               allocator, vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_indexBuffer->getBoundMemory().getHostPtr());

    deMemset(ptr, 0xFF, static_cast<size_t>(m_testSpec.bindIndexBufferOffset));
    deMemcpy(ptr + m_testSpec.bindIndexBufferOffset, de::dataOrNull(m_indexes), de::dataSize(m_indexes));
    vk::flushAlloc(m_vk, device, m_indexBuffer->getBoundMemory());

    const vk::VkDeviceSize vertexBufferOffset = 0;
    const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
    const vk::VkBuffer indexBuffer            = m_indexBuffer->object();

#ifndef CTS_USES_VULKANSC
    if (m_groupParams->useSecondaryCmdBuffer)
    {
        // record secondary command buffer
        if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        {
            beginSecondaryCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
            beginDynamicRender(*m_secCmdBuffer);
        }
        else
            beginSecondaryCmdBuffer(m_vk);

        m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
        cmdBindIndexBufferImpl(*m_secCmdBuffer, indexBuffer, m_testSpec.bindIndexBufferOffset, dataSize,
                               vk::VK_INDEX_TYPE_UINT32);
        m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        m_vk.cmdDrawIndexed(*m_secCmdBuffer, 6, 1, 2, m_testSpec.vertexOffset, 0);

        if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            endDynamicRender(*m_secCmdBuffer);

        endCommandBuffer(m_vk, *m_secCmdBuffer);

        // record primary command buffer
        beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
        preRenderBarriers();

        if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            beginDynamicRender(*m_cmdBuffer, vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

        m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

        if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            endDynamicRender(*m_cmdBuffer);

        endCommandBuffer(m_vk, *m_cmdBuffer);
    }
    else if (m_groupParams->useDynamicRendering)
    {
        beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
        preRenderBarriers();
        beginDynamicRender(*m_cmdBuffer);

        m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
        cmdBindIndexBufferImpl(*m_cmdBuffer, indexBuffer, m_testSpec.bindIndexBufferOffset, dataSize,
                               vk::VK_INDEX_TYPE_UINT32);
        m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 1, 2, m_testSpec.vertexOffset, 0);

        endDynamicRender(*m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);
    }
#endif // CTS_USES_VULKANSC

    if (!m_groupParams->useDynamicRendering)
    {
        beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
        preRenderBarriers();
        beginLegacyRender(*m_cmdBuffer);

        m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
        cmdBindIndexBufferImpl(*m_cmdBuffer, indexBuffer, m_testSpec.bindIndexBufferOffset, dataSize,
                               vk::VK_INDEX_TYPE_UINT32);
        m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
        m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 1, 2, m_testSpec.vertexOffset, 0);

        endLegacyRender(*m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);
    }

    submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

    // Validation
    tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat),
                                  (int)(0.5f + static_cast<float>(m_renderWidth)),
                                  (int)(0.5f + static_cast<float>(m_renderHeight)));
    referenceFrame.allocLevel(0);

    const int32_t frameWidth  = referenceFrame.getWidth();
    const int32_t frameHeight = referenceFrame.getHeight();

    tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    ReferenceImageCoordinates refCoords;

    for (int y = 0; y < frameHeight; y++)
    {
        const float yCoord = (float)(y / (0.5 * frameHeight)) - 1.0f;

        for (int x = 0; x < frameWidth; x++)
        {
            const float xCoord = (float)(x / (0.5 * frameWidth)) - 1.0f;

            if ((yCoord >= refCoords.bottom && yCoord <= refCoords.top && xCoord >= refCoords.left &&
                 xCoord <= refCoords.right))
                referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
        }
    }

    const vk::VkOffset3D zeroOffset = {0, 0, 0};
    const tcu::ConstPixelBufferAccess renderedFrame =
        m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset,
                                        m_renderWidth, m_renderHeight, vk::VK_IMAGE_ASPECT_COLOR_BIT);

    qpTestResult res = QP_TEST_RESULT_PASS;

    if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame, 0.05f,
                           tcu::COMPARE_LOG_RESULT))
    {
        res = QP_TEST_RESULT_FAIL;
    }

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

DrawInstancedIndexed::DrawInstancedIndexed(Context &context, TestSpec testSpec) : DrawIndexed(context, testSpec)
{
}

tcu::TestStatus DrawInstancedIndexed::iterate(void)
{
    tcu::TestLog &log                 = m_context.getTestContext().getLog();
    const auto &vki                   = m_context.getInstanceInterface();
    const auto physDev                = m_context.getPhysicalDevice();
    const vk::VkQueue queue           = m_context.getUniversalQueue();
    const vk::VkDevice device         = m_context.getDevice();
    const auto memProps               = vk::getPhysicalDeviceMemoryProperties(vki, physDev);
    const auto dataSize               = static_cast<vk::VkDeviceSize>(de::dataSize(m_indexes));
    const vk::VkDeviceSize bufferSize = dataSize + m_testSpec.bindIndexBufferOffset;
    const auto atomSize               = m_context.getDeviceProperties().limits.nonCoherentAtomSize;
    vk::SimpleAllocator allocator(m_vk, device, memProps,
                                  vk::SimpleAllocator::OptionalOffsetParams({atomSize, m_testSpec.memoryBindOffset}));

    beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
    preRenderBarriers();

#ifndef CTS_USES_VULKANSC
    if (m_groupParams->useDynamicRendering)
        beginDynamicRender(*m_cmdBuffer);
    else
        beginLegacyRender(*m_cmdBuffer);
#else
    beginLegacyRender(*m_cmdBuffer);
#endif // CTS_USES_VULKANSC

    m_indexBuffer =
        Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
                               allocator, vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_indexBuffer->getBoundMemory().getHostPtr());

    deMemset(ptr, 0xFF, static_cast<size_t>(m_testSpec.bindIndexBufferOffset));
    deMemcpy(ptr + m_testSpec.bindIndexBufferOffset, de::dataOrNull(m_indexes), de::dataSize(m_indexes));
    vk::flushAlloc(m_vk, device, m_indexBuffer->getBoundMemory());

    const vk::VkDeviceSize vertexBufferOffset = 0;
    const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
    const vk::VkBuffer indexBuffer            = m_indexBuffer->object();

    m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
    cmdBindIndexBufferImpl(*m_cmdBuffer, indexBuffer, m_testSpec.bindIndexBufferOffset, dataSize,
                           vk::VK_INDEX_TYPE_UINT32);
    m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    switch (m_topology)
    {
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        m_vk.cmdDrawIndexed(*m_cmdBuffer, 6, 4, 2, m_testSpec.vertexOffset, 2);
        break;
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        m_vk.cmdDrawIndexed(*m_cmdBuffer, 4, 4, 2, m_testSpec.vertexOffset, 2);
        break;
    case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
    case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
    case vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
    case vk::VK_PRIMITIVE_TOPOLOGY_LAST:
        DE_FATAL("Topology not implemented");
        break;
    default:
        DE_FATAL("Unknown topology");
        break;
    }

#ifndef CTS_USES_VULKANSC
    if (m_groupParams->useDynamicRendering)
        endDynamicRender(*m_cmdBuffer);
    else
        endLegacyRender(*m_cmdBuffer);
#else
    endLegacyRender(*m_cmdBuffer);
#endif // CTS_USES_VULKANSC

    endCommandBuffer(m_vk, *m_cmdBuffer);

    submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

    // Validation
    VK_CHECK(m_vk.queueWaitIdle(queue));

    tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat),
                                  (int)(0.5f + static_cast<float>(m_renderWidth)),
                                  (int)(0.5f + static_cast<float>(m_renderHeight)));
    referenceFrame.allocLevel(0);

    const int32_t frameWidth  = referenceFrame.getWidth();
    const int32_t frameHeight = referenceFrame.getHeight();

    tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    ReferenceImageInstancedCoordinates refInstancedCoords;

    for (int y = 0; y < frameHeight; y++)
    {
        const float yCoord = (float)(y / (0.5 * frameHeight)) - 1.0f;

        for (int x = 0; x < frameWidth; x++)
        {
            const float xCoord = (float)(x / (0.5 * frameWidth)) - 1.0f;

            if ((yCoord >= refInstancedCoords.bottom && yCoord <= refInstancedCoords.top &&
                 xCoord >= refInstancedCoords.left && xCoord <= refInstancedCoords.right))
                referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
        }
    }

    const vk::VkOffset3D zeroOffset = {0, 0, 0};
    const tcu::ConstPixelBufferAccess renderedFrame =
        m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset,
                                        m_renderWidth, m_renderHeight, vk::VK_IMAGE_ASPECT_COLOR_BIT);

    qpTestResult res = QP_TEST_RESULT_PASS;

    if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame, 0.05f,
                           tcu::COMPARE_LOG_RESULT))
    {
        res = QP_TEST_RESULT_FAIL;
    }

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

class DrawIndexedMaintenance6 : public DrawIndexed
{
public:
    DrawIndexedMaintenance6(Context &context, TestSpec testSpec);
    virtual tcu::TestStatus iterate(void);
};

DrawIndexedMaintenance6::DrawIndexedMaintenance6(Context &context, TestSpec testSpec) : DrawIndexed(context, testSpec)
{
}

// Reference renderer shaders
class PassthruVertShader : public rr::VertexShader
{
public:
    PassthruVertShader(void) : rr::VertexShader(2, 1)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~PassthruVertShader()
    {
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            packets[packetNdx]->position =
                rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx);

            tcu::Vec4 color =
                rr::readVertexAttribFloat(inputs[1], packets[packetNdx]->instanceNdx, packets[packetNdx]->vertexNdx);

            packets[packetNdx]->outputs[0] = color;
        }
    }
};

class PassthruFragShader : public rr::FragmentShader
{
public:
    PassthruFragShader(void) : rr::FragmentShader(1, 1)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~PassthruFragShader()
    {
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::FragmentPacket &packet = packets[packetNdx];
            for (uint32_t fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
            {
                tcu::Vec4 color = rr::readVarying<float>(packet, context, 0, fragNdx);
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            }
        }
    }
};

tcu::TestStatus DrawIndexedMaintenance6::iterate(void)
{
    tcu::TestLog &log         = m_context.getTestContext().getLog();
    const auto &vki           = m_context.getInstanceInterface();
    const auto physDev        = m_context.getPhysicalDevice();
    const vk::VkQueue queue   = m_context.getUniversalQueue();
    const vk::VkDevice device = m_context.getDevice();
    const auto memProps       = vk::getPhysicalDeviceMemoryProperties(vki, physDev);
    const auto atomSize       = m_context.getDeviceProperties().limits.nonCoherentAtomSize;
    vk::SimpleAllocator allocator(m_vk, device, memProps,
                                  vk::SimpleAllocator::OptionalOffsetParams({atomSize, m_testSpec.memoryBindOffset}));

    beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
    preRenderBarriers();

#ifndef CTS_USES_VULKANSC
    if (m_groupParams->useDynamicRendering)
        beginDynamicRender(*m_cmdBuffer);
    else
        beginLegacyRender(*m_cmdBuffer);
#else
    beginLegacyRender(*m_cmdBuffer);
#endif // CTS_USES_VULKANSC

    const uint32_t indexCount = m_testSpec.nullDescriptor ? 3 : 0;

    const vk::VkDrawIndexedIndirectCommand drawParams = {indexCount, 1, 0, 0, 0};

    const auto drawParamsBuffer = Buffer::createAndAlloc(
        m_vk, device,
        BufferCreateInfo(sizeof(vk::VkDrawIndexedIndirectCommand), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), allocator,
        vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(drawParamsBuffer->getBoundMemory().getHostPtr());

    deMemcpy(ptr, &drawParams, sizeof(vk::VkDrawIndexedIndirectCommand));
    vk::flushAlloc(m_vk, device, drawParamsBuffer->getBoundMemory());

    const auto countBuffer = Buffer::createAndAlloc(
        m_vk, device, BufferCreateInfo(sizeof(uint32_t), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), allocator,
        vk::MemoryRequirement::HostVisible);

    ptr = reinterpret_cast<uint8_t *>(countBuffer->getBoundMemory().getHostPtr());

    deMemset(ptr, 1, 1);
    vk::flushAlloc(m_vk, device, countBuffer->getBoundMemory());

    const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
    const vk::VkDeviceSize vertexBufferOffset = 0;
    if (!m_testSpec.testDrawCount)
        m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

#ifndef CTS_USES_VULKANSC
    if (m_testSpec.bindIndexBuffer2)
    {
        m_vk.cmdBindIndexBuffer2(*m_cmdBuffer, VK_NULL_HANDLE, 0, 0, vk::VK_INDEX_TYPE_UINT32);
    }
    else
#endif
    {
        m_vk.cmdBindIndexBuffer(*m_cmdBuffer, VK_NULL_HANDLE, 0, vk::VK_INDEX_TYPE_UINT32);
    }

    de::MovePtr<vk::BufferWithMemory> ssboBuffer;
    const auto ssboBufferSize = static_cast<vk::VkDeviceSize>(sizeof(uint32_t));
    // Output SSBO
    const auto ssboBufferInfo = makeBufferCreateInfo(ssboBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    ssboBuffer                = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
        m_vk, device, m_context.getDefaultAllocator(), ssboBufferInfo, vk::MemoryRequirement::HostVisible));
    auto &ssboBufferAlloc     = ssboBuffer->getAllocation();

    deMemset(ssboBufferAlloc.getHostPtr(), 0, static_cast<size_t>(ssboBufferSize));
    flushAlloc(m_vk, device, ssboBufferAlloc);

    // Descriptor pool
    vk::Move<vk::VkDescriptorPool> descriptorPool;
    vk::DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    descriptorPool = poolBuilder.build(m_vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto descriptorSetLayout = layoutBuilder.build(m_vk, device);

    // Descriptor set
    vk::Move<vk::VkDescriptorSet> descriptorSet;
    descriptorSet            = makeDescriptorSet(m_vk, device, descriptorPool.get(), descriptorSetLayout.get());
    const auto ssboWriteInfo = makeDescriptorBufferInfo(ssboBuffer->get(), 0ull, ssboBufferSize);
    vk::DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                              vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboWriteInfo);
    updateBuilder.update(m_vk, device);

    m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    if (m_testSpec.testDrawCount)
        m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u,
                                   &descriptorSet.get(), 0u, nullptr);

    switch (m_testSpec.testType)
    {
    case TEST_TYPE_MAINTENANCE6_INDEXED:
    {
        m_vk.cmdDrawIndexed(*m_cmdBuffer, indexCount, 1, 0, 0, 0);

        break;
    }
    case TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT:
    {
        m_vk.cmdDrawIndexedIndirect(*m_cmdBuffer, drawParamsBuffer->object(), 0, 1,
                                    sizeof(vk::VkDrawIndexedIndirectCommand));

        break;
    }
    case TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT:
    {
        m_vk.cmdDrawIndexedIndirectCount(*m_cmdBuffer, drawParamsBuffer->object(), 0, countBuffer->object(), 0, 1,
                                         sizeof(vk::VkDrawIndexedIndirectCommand));

        break;
    }
#ifndef CTS_USES_VULKANSC
    case TEST_TYPE_MAINTENANCE6_MULTI_INDEXED_EXT:
    {
        const vk::VkMultiDrawIndexedInfoEXT indexInfo = {0, indexCount, 0};
        const int32_t vertexOffset                    = 0;

        m_vk.cmdDrawMultiIndexedEXT(*m_cmdBuffer, 1, &indexInfo, 1, 0, sizeof(vk::VkMultiDrawIndexedInfoEXT),
                                    &vertexOffset);

        break;
    }
#endif
    default:
    {
        DE_FATAL("Unknown test type");
        break;
    }
    }

#ifndef CTS_USES_VULKANSC
    if (m_groupParams->useDynamicRendering)
        endDynamicRender(*m_cmdBuffer);
    else
        endLegacyRender(*m_cmdBuffer);
#else
    endLegacyRender(*m_cmdBuffer);
#endif // CTS_USES_VULKANSC

    const auto ssboBarrier = vk::makeMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT);
    m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u,
                            1u, &ssboBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(m_vk, *m_cmdBuffer);

    submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

    // Validation
    VK_CHECK(m_vk.queueWaitIdle(queue));

    tcu::TextureLevel refImage(vk::mapVkFormat(m_colorAttachmentFormat),
                               (int)(0.5f + static_cast<float>(m_renderWidth)),
                               (int)(0.5f + static_cast<float>(m_renderHeight)));
    tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    if (m_testSpec.nullDescriptor)
    {
        std::vector<tcu::Vec4> vertices;
        std::vector<tcu::Vec4> colors;

        // Draw just the first point
        if (m_testSpec.testDrawCount)
        {
            const tcu::Vec4 center = tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f);
            vertices.push_back(center);
            colors.push_back(tcu::RGBA::blue().toVec());
        }
        else
        {
            vertices.push_back(m_data[0].position);
            colors.push_back(m_data[0].color);
        }

        {
            const PassthruVertShader vertShader;
            const PassthruFragShader fragShader;
            const rr::Program program(&vertShader, &fragShader);
            const rr::MultisamplePixelBufferAccess colorBuffer =
                rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(refImage.getAccess());
            const rr::RenderTarget renderTarget(colorBuffer);
            const rr::RenderState renderState((rr::ViewportState(colorBuffer)),
                                              m_context.getDeviceProperties().limits.subPixelPrecisionBits);
            const rr::Renderer renderer;

            const rr::VertexAttrib vertexAttribs[] = {
                rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &vertices[0]),
                rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &colors[0])};
            renderer.draw(rr::DrawCommand(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs),
                                          &vertexAttribs[0],
                                          rr::PrimitiveList(rr::PRIMITIVETYPE_POINTS, (uint32_t)vertices.size(), 0)));
        }
    }

    const vk::VkOffset3D zeroOffset = {0, 0, 0};
    const tcu::ConstPixelBufferAccess renderedFrame =
        m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset,
                                        m_renderWidth, m_renderHeight, vk::VK_IMAGE_ASPECT_COLOR_BIT);

    qpTestResult res = QP_TEST_RESULT_PASS;

    if (m_testSpec.testDrawCount)
    {
        if (!tcu::intThresholdCompare(log, "Result", "Image comparison result", refImage.getAccess(), renderedFrame,
                                      tcu::UVec4(0, 0, 0, 0), tcu::COMPARE_LOG_ON_ERROR))
            res = QP_TEST_RESULT_FAIL;

        if (res == QP_TEST_RESULT_PASS)
        {
            // Get stored counters.
            auto &ssboAlloc = ssboBuffer->getAllocation();
            invalidateAlloc(m_vk, device, ssboAlloc);

            uint32_t ssboCounter = 0;
            deMemcpy(&ssboCounter, ssboAlloc.getHostPtr(), ssboBufferSize);

            uint32_t expectedCounter = indexCount;
            if (ssboCounter != expectedCounter)
                res = QP_TEST_RESULT_FAIL;
        }
    }
    else
    {
        if (!tcu::intThresholdPositionDeviationCompare(log, "Result", "Image comparison result", refImage.getAccess(),
                                                       renderedFrame,
                                                       tcu::UVec4(4u),      // color threshold
                                                       tcu::IVec3(1, 1, 0), // position deviation tolerance
                                                       true,                // don't check the pixels at the boundary
                                                       tcu::COMPARE_LOG_ON_ERROR))
        {
            res = QP_TEST_RESULT_FAIL;
        }
    }

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

void checkSupport(Context &context, DrawIndexed::TestSpec testSpec)
{
    if (testSpec.groupParams->useDynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    if (testSpec.testType != TEST_TYPE_NON_MAINTENANCE_6)
    {
        context.requireDeviceFunctionality("VK_KHR_maintenance6");

        if (testSpec.nullDescriptor)
        {
            vk::VkPhysicalDeviceFeatures2 features2                        = vk::initVulkanStructure();
            vk::VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = vk::initVulkanStructure();

            features2.pNext = &robustness2Features;

            context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

            if (robustness2Features.nullDescriptor == VK_FALSE)
            {
                TCU_THROW(NotSupportedError, "robustness2 nullDescriptor is not supported");
            }

            DE_ASSERT(features2.features.robustBufferAccess);
        }

        if (testSpec.bindIndexBuffer2)
        {
            context.requireDeviceFunctionality("VK_KHR_maintenance5");
        }

#ifndef CTS_USES_VULKANSC
        if (testSpec.testType == TEST_TYPE_MAINTENANCE6_MULTI_INDEXED_EXT)
        {
            context.requireDeviceFunctionality("VK_EXT_multi_draw");
        }
#endif

        if (testSpec.testType == TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT)
        {
            context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
        }

        if (testSpec.testDrawCount)
        {
            const auto features =
                getPhysicalDeviceFeatures(context.getInstanceInterface(), context.getPhysicalDevice());
            if (!features.fragmentStoresAndAtomics)
                TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics is supported");
        }
    }
#ifndef CTS_USES_VULKANSC
    if (testSpec.useMaintenance5Ext)
        context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
#endif
}

// Pseudorandom 8-bit index multi-draws. The goal of these tests is trying different variations of the following
// pattern:
//
// * vkCmdBindIndexBuffer(8-bit indices)
// * vkCmdDrawIndexed()
// * vkCmdBindIndexBuffer(8-bit indices)
// * vkCmdDrawIndexed()
//
// The mechanism uses a large framebuffer and draws one point over each pixel. To use multiple draws, the total number
// of pixels is divided into 8 blocks of pseudorandom size. Sometimes the blocks are sorted in increasing size order and
// sometimes they're left as-is. For each block we also pseudorandomly decide if we will use a new command buffer or
// reuse the last one.
struct Multibind8BitParams
{
    uint32_t caseIndex;
    bool sortSizes;
};

class Multibind8BitInstance : public vkt::TestInstance
{
public:
    Multibind8BitInstance(Context &context, const Multibind8BitParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~Multibind8BitInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Multibind8BitParams m_params;
};

class Multibind8BitCase : public vkt::TestCase
{
public:
    Multibind8BitCase(tcu::TestContext &testCtx, const std::string &name, const Multibind8BitParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~Multibind8BitCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new Multibind8BitInstance(context, m_params);
    }

protected:
    const Multibind8BitParams m_params;
};

void Multibind8BitCase::checkSupport(Context &context) const
{
    const auto &index8Features = context.getIndexTypeUint8Features();
    if (!index8Features.indexTypeUint8)
        TCU_THROW(NotSupportedError, "indexTypeUint8 not supported");
}

void bluePointsPrograms(vk::SourceCollections &programCollection)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void Multibind8BitCase::initPrograms(vk::SourceCollections &programCollection) const
{
    bluePointsPrograms(programCollection);
}

tcu::TestStatus Multibind8BitInstance::iterate(void)
{
    const uint32_t seed =
        (1768315279u & 0xffffff00u) + (static_cast<uint32_t>(m_params.sortSizes) << 7) + m_params.caseIndex;
    de::Random rng(seed);

    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 extent(16, 16, 1);
    const auto floatExtent = extent.asFloat();
    const auto extentVk    = vk::makeExtent3D(extent);
    const auto format      = vk::VK_FORMAT_R8G8B8A8_UNORM;
    const auto usage       = (vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                        vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto imageType   = vk::VK_IMAGE_TYPE_2D;
    const auto pointCount  = extentVk.width * extentVk.height * extentVk.depth;

    DE_ASSERT(pointCount - 1u <= static_cast<uint32_t>(std::numeric_limits<uint8_t>::max()));

    vk::ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, format, usage, imageType);

    // Vertex buffer.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pointCount);
    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const float xCoord = (static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f;
            const float yCoord = (static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0f - 1.0f;
            vertices.emplace_back(xCoord, yCoord, 0.0f, 1.0f);
        }

    const auto vertexBufferSize  = static_cast<vk::VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage = vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertexBufferInfo  = vk::makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    vk::BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, vk::HostIntent::W);
    {
        auto &alloc = vertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
        vk::flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    constexpr auto replicaFactor = 512u; // If repeating indices.
    constexpr auto blockCount    = 8u;   // How many blocks we aim to have.
    constexpr auto pieceSize     = 4u;   // We want to make index buffer sizes multiples of 4-bytes.
    const auto pieceCount        = pointCount / pieceSize;

    struct DrawInfo
    {
        uint32_t firstPiece;
        uint32_t drawPieces;
        bool newCmdBuffer;
        bool repeat; // Repeat each index multiple times or not.

        uint32_t firstIndex() const
        {
            return firstPiece * pieceSize;
        }

        uint32_t indexCount() const
        {
            return drawPieces * pieceSize;
        }

        // How many times to repeat each index in the buffer.
        uint32_t repetitions() const
        {
            return (repeat ? replicaFactor : 1u);
        }

        // How many indices to store in the buffer.
        uint32_t bufferIndexCount() const
        {
            return indexCount() * repetitions();
        }

        // If we need to sort DrawInfos, we need to do so by size of the index buffer.
        bool operator<(const DrawInfo &other) const
        {
            return (drawPieces < other.drawPieces);
        }
    };

    std::vector<DrawInfo> drawInfos;
    drawInfos.reserve(blockCount);

    // Lets pseudorandomly choose how many pieces in each block.
    uint32_t prevPieces      = 0u;
    uint32_t remainingPieces = pieceCount;

    for (uint32_t blockNdx = 0u; blockNdx < blockCount; ++blockNdx)
    {
        const auto remainingBlocks = (blockCount - blockNdx - 1u); // After the current one.
        const auto minPieces       = 1;
        const auto maxPieces =
            static_cast<int>(remainingPieces - remainingBlocks); // Leave at least 1 piece for each remaining block.
        const auto drawPieces =
            ((blockNdx == blockCount - 1u) ? remainingPieces : static_cast<uint32_t>(rng.getInt(minPieces, maxPieces)));
        const bool newCmdBuffer = rng.getBool();
        const bool repeat       = (rng.getInt(1, 4) == 1);
        drawInfos.push_back(DrawInfo{prevPieces, drawPieces, newCmdBuffer, repeat});

        prevPieces += drawPieces;
        remainingPieces -= drawPieces;
    }

    if (m_params.sortSizes)
        std::sort(drawInfos.begin(), drawInfos.end());

    // Index buffers.
    using BufferWithMemoryPtr = std::unique_ptr<vk::BufferWithMemory>;
    std::vector<BufferWithMemoryPtr> indexBuffers;
    indexBuffers.reserve(blockCount);

    for (uint32_t blockNdx = 0u; blockNdx < blockCount; ++blockNdx)
    {
        std::vector<uint8_t> indices;
        const auto &drawInfo = drawInfos.at(blockNdx);
        indices.reserve(drawInfo.bufferIndexCount());
        const auto idxBegin = drawInfo.firstIndex();
        const auto idxEnd   = drawInfo.firstIndex() + drawInfo.indexCount();
        for (uint32_t i = idxBegin; i < idxEnd; ++i)
        {
            for (uint32_t j = 0; j < drawInfo.repetitions(); ++j)
                indices.push_back(static_cast<uint8_t>(i));
        }

        const auto bufferSize  = static_cast<vk::VkDeviceSize>(de::dataSize(indices));
        const auto bufferUsage = vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        const auto createInfo  = vk::makeBufferCreateInfo(bufferSize, bufferUsage);

        indexBuffers.emplace_back(
            new vk::BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, vk::HostIntent::W));
        auto &alloc = indexBuffers.back()->getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(indices), de::dataSize(indices));
        vk::flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Render pass that loads and stores the attachment.
    const auto renderPass =
        vk::makeRenderPass(ctx.vkd, ctx.device, format, vk::VK_FORMAT_UNDEFINED, vk::VK_ATTACHMENT_LOAD_OP_LOAD);
    const auto framebuffer = vk::makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(),
                                                 extentVk.width, extentVk.height);

    const std::vector<vk::VkViewport> viewports(1u, vk::makeViewport(extent));
    const std::vector<vk::VkRect2D> scissors(1u, vk::makeRect2D(extent));

    // Graphics pipeline.
    const auto &binaries      = m_context.getBinaryCollection();
    const auto vertShader     = vk::createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragShader     = vk::createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
    const auto pipelineLayout = vk::makePipelineLayout(ctx.vkd, ctx.device);
    const auto pipeline = vk::makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE,
                                                   VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader, *renderPass, viewports,
                                                   scissors, vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    const auto cmdPool = vk::makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);

    using SemaphorePtr = std::unique_ptr<vk::Move<vk::VkSemaphore>>;
    using CmdBufferPtr = std::unique_ptr<vk::Move<vk::VkCommandBuffer>>;

    std::vector<SemaphorePtr> cmdSemaphores;
    std::vector<CmdBufferPtr> cmdBuffers;
    vk::VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;

    const auto colorSRR = vk::makeDefaultImageSubresourceRange();

    // Lambda that creates a new command buffer, its semaphore, stores the handle in cmdBuffer and begins cmdBuffer.
    const auto makeNewCmdbuffer = [&]()
    {
        cmdSemaphores.emplace_back(new vk::Move<vk::VkSemaphore>(vk::createSemaphore(ctx.vkd, ctx.device)));
        cmdBuffers.emplace_back(new vk::Move<vk::VkCommandBuffer>(
            vk::allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY)));
        cmdBuffer = cmdBuffers.back()->get();
        vk::beginCommandBuffer(ctx.vkd, cmdBuffer);
    };

    // Lambda that starts a new render pass and binds the vertex buffer and the pipeline for it.
    const auto startRenderPass = [&]()
    {
        vk::beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.front());
        ctx.vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        const vk::VkDeviceSize offset = 0ull;
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &offset);
    };

    {
        makeNewCmdbuffer();

        // Clear color buffer to black.
        const auto clearColor = vk::makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        const auto preClearBarrier =
            vk::makeImageMemoryBarrier(0u, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                                       vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.getImage(), colorSRR);
        vk::cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);

        ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &clearColor.color, 1u, &colorSRR);

        const auto postClearBarrier = vk::makeImageMemoryBarrier(
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,
            (vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            colorBuffer.getImage(), colorSRR);
        vk::cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);
    }

    startRenderPass();

    for (size_t drawIdx = 0; drawIdx < drawInfos.size(); ++drawIdx)
    {
        const auto &drawInfo = drawInfos.at(drawIdx);

        if (drawInfo.newCmdBuffer)
        {
            vk::endRenderPass(ctx.vkd, cmdBuffer);
            vk::endCommandBuffer(ctx.vkd, cmdBuffer);

            makeNewCmdbuffer();

            const auto drawBarrier = vk::makeMemoryBarrier(
                vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                (vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
            vk::cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &drawBarrier);
            startRenderPass();
        }

        // Bind the new index buffer and draw.
        ctx.vkd.cmdBindIndexBuffer(cmdBuffer, indexBuffers.at(drawIdx)->get(), 0ull, vk::VK_INDEX_TYPE_UINT8);
        ctx.vkd.cmdDrawIndexed(cmdBuffer, drawInfo.bufferIndexCount(), 1u, 0u, 0,
                               0u); // Note first index is zero because we use separate index buffers for each call.
    }

    vk::endRenderPass(ctx.vkd, cmdBuffer);

    // Copy color buffer to verification buffer before finishing.
    vk::copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1));

    vk::endCommandBuffer(ctx.vkd, cmdBuffer);

    std::vector<vk::VkSubmitInfo> submitInfos;
    submitInfos.reserve(cmdBuffers.size());

    const auto waitStage = static_cast<vk::VkPipelineStageFlags>(vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    for (uint32_t cmdIdx = 0; cmdIdx < cmdBuffers.size(); ++cmdIdx)
    {
        const bool hasPrevious = (cmdIdx > 0u);

        // Each command buffer waits for the previous one to finish at the color attachment output stage.
        submitInfos.push_back(vk::VkSubmitInfo{
            vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            (hasPrevious ? 1u : 0u),
            (hasPrevious ? &cmdSemaphores.at(cmdIdx - 1u)->get() : nullptr),
            (hasPrevious ? &waitStage : nullptr),
            1u,
            &cmdBuffers.at(cmdIdx)->get(),
            1u,
            &cmdSemaphores.at(cmdIdx)->get(),
        });
    }

    const auto fence = vk::createFence(ctx.vkd, ctx.device);
    ctx.vkd.queueSubmit(ctx.queue, de::sizeU32(submitInfos), de::dataOrNull(submitInfos), *fence);
    vk::waitForFence(ctx.vkd, ctx.device, *fence);

    vk::invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());

    // Prepare reference image (all blue color, see frag shader).
    const auto tcuFormat = vk::mapVkFormat(format);
    tcu::TextureLevel refLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();
    tcu::clear(reference, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    tcu::ConstPixelBufferAccess result(tcuFormat, extent, colorBuffer.getBufferAllocation().getHostPtr());

    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f);

    if (!tcu::floatThresholdCompare(log, "ColorBuffer", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected contents in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

// Update before draw: test that updating the index buffer before drawing works.
// * vkCmdBindIndexBuffer
// * Transfer op (could be compute too) that updates the index buffer contents.
// * Barrier.
// * vkCmdDrawIndexed in a render pass
//
// The tests will use different index types, and a 16x16 FB covered with 256 points.
struct UpdateBeforeDrawParams
{
    vk::VkIndexType indexType;
};

class UpdateBeforeDrawInstance : public vkt::TestInstance
{
public:
    UpdateBeforeDrawInstance(Context &context, const UpdateBeforeDrawParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~UpdateBeforeDrawInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const UpdateBeforeDrawParams m_params;
};

class UpdateBeforeDrawCase : public vkt::TestCase
{
public:
    UpdateBeforeDrawCase(tcu::TestContext &testCtx, const std::string &name, const UpdateBeforeDrawParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~UpdateBeforeDrawCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new UpdateBeforeDrawInstance(context, m_params);
    }

protected:
    const UpdateBeforeDrawParams m_params;
};

void UpdateBeforeDrawCase::checkSupport(Context &context) const
{
    if (m_params.indexType == vk::VK_INDEX_TYPE_UINT8)
    {
        const auto &index8Features = context.getIndexTypeUint8Features();
        if (!index8Features.indexTypeUint8)
            TCU_THROW(NotSupportedError, "indexTypeUint8 not supported");
    }
}

void UpdateBeforeDrawCase::initPrograms(vk::SourceCollections &programCollection) const
{
    bluePointsPrograms(programCollection);
}

tcu::TestStatus UpdateBeforeDrawInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 extent(16, 16, 1);
    const auto floatExtent = extent.asFloat();
    const auto extentVk    = vk::makeExtent3D(extent);
    const auto format      = vk::VK_FORMAT_R8G8B8A8_UNORM;
    const auto usage       = (vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                        vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto imageType   = vk::VK_IMAGE_TYPE_2D;
    const auto pointCount  = extentVk.width * extentVk.height * extentVk.depth;

    DE_ASSERT(pointCount - 1u <= static_cast<uint32_t>(std::numeric_limits<uint8_t>::max()));

    vk::ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, format, usage, imageType);

    // Vertex buffer.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pointCount);
    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const float xCoord = (static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f;
            const float yCoord = (static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0f - 1.0f;
            vertices.emplace_back(xCoord, yCoord, 0.0f, 1.0f);
        }

    const auto vertexBufferOffset = static_cast<vk::VkDeviceSize>(0);
    const auto vertexBufferSize   = static_cast<vk::VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage  = vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertexBufferInfo   = vk::makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    vk::BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, vk::HostIntent::W);
    {
        auto &alloc = vertexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
        vk::flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Staging buffer: we will generate the actual indices here.
    std::vector<uint8_t> stagingBufferBytes;
    if (m_params.indexType == vk::VK_INDEX_TYPE_UINT8)
    {
        stagingBufferBytes.resize(pointCount);
        std::iota(stagingBufferBytes.begin(), stagingBufferBytes.end(), uint8_t{0});
    }
    else if (m_params.indexType == vk::VK_INDEX_TYPE_UINT16)
    {
        std::vector<uint16_t> indices(pointCount);
        std::iota(indices.begin(), indices.end(), uint16_t{0});
        stagingBufferBytes.resize(de::dataSize(indices));
        memcpy(stagingBufferBytes.data(), indices.data(), stagingBufferBytes.size());
    }
    else if (m_params.indexType == vk::VK_INDEX_TYPE_UINT32)
    {
        std::vector<uint32_t> indices(pointCount);
        std::iota(indices.begin(), indices.end(), uint32_t{0});
        stagingBufferBytes.resize(de::dataSize(indices));
        memcpy(stagingBufferBytes.data(), indices.data(), stagingBufferBytes.size());
    }
    else
        DE_ASSERT(false);

    const auto stagingBufferSize  = static_cast<vk::VkDeviceSize>(de::dataSize(stagingBufferBytes));
    const auto stagingBufferUsage = static_cast<vk::VkBufferUsageFlags>(vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const auto stagingBufferInfo  = vk::makeBufferCreateInfo(stagingBufferSize, stagingBufferUsage);
    vk::BufferWithMemory stagingBuffer(ctx.vkd, ctx.device, ctx.allocator, stagingBufferInfo, vk::HostIntent::W);
    {
        auto &alloc = stagingBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(stagingBufferBytes), de::dataSize(stagingBufferBytes));
        vk::flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Index buffer: contents will be filled later with a copy from the staging buffer.
    const auto indexBufferUsage = static_cast<vk::VkBufferUsageFlags>(vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                                      vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto indexBufferInfo  = vk::makeBufferCreateInfo(stagingBufferSize, indexBufferUsage);
    vk::BufferWithMemory indexBuffer(ctx.vkd, ctx.device, ctx.allocator, indexBufferInfo, vk::HostIntent::NONE);

    // Render pass that loads and stores the attachment.
    const auto renderPass  = vk::makeRenderPass(ctx.vkd, ctx.device, format);
    const auto framebuffer = vk::makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(),
                                                 extentVk.width, extentVk.height);

    const std::vector<vk::VkViewport> viewports(1u, vk::makeViewport(extent));
    const std::vector<vk::VkRect2D> scissors(1u, vk::makeRect2D(extent));

    // Graphics pipeline.
    const auto &binaries      = m_context.getBinaryCollection();
    const auto vertShader     = vk::createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragShader     = vk::createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));
    const auto pipelineLayout = vk::makePipelineLayout(ctx.vkd, ctx.device);
    const auto pipeline = vk::makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE,
                                                   VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader, *renderPass, viewports,
                                                   scissors, vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    // Command pool with buffer, run stuff.
    vk::CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    vk::beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
    ctx.vkd.cmdBindIndexBuffer(cmdBuffer, indexBuffer.get(), 0ull, m_params.indexType);
    ctx.vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

    // After the index buffer is bound, fill its contents from the staginb buffer.
    {
        const auto copyRegion = vk::makeBufferCopy(0ull, 0ull, stagingBufferSize);
        ctx.vkd.cmdCopyBuffer(cmdBuffer, *stagingBuffer, *indexBuffer, 1u, &copyRegion);
    }

    // Insert a barrier so the transfer happens before reading indices.
    {
        const auto xferToDraw = vk::makeMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_INDEX_READ_BIT);
        vk::cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, &xferToDraw);
    }

    // Render pass and draw.
    vk::beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.front(),
                        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    ctx.vkd.cmdDrawIndexed(cmdBuffer, pointCount, 1u, 0u, 0, 0u);
    vk::endRenderPass(ctx.vkd, cmdBuffer);

    vk::copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1));

    vk::endCommandBuffer(ctx.vkd, cmdBuffer);
    vk::submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto tcuFormat = vk::mapVkFormat(format);
    vk::invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::ConstPixelBufferAccess result(tcuFormat, extent, colorBuffer.getBufferAllocation().getHostPtr());

    // Prepare reference image (all blue color, see frag shader).
    tcu::TextureLevel refLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();
    tcu::clear(reference, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f);

    if (!tcu::floatThresholdCompare(log, "ColorBuffer", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected contents in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

DrawIndexedTests::DrawIndexedTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
    : TestCaseGroup(testCtx, "indexed_draw")
    , m_groupParams(groupParams)
{
    /* Left blank on purpose */
}

DrawIndexedTests::~DrawIndexedTests(void)
{
}

void DrawIndexedTests::init(void)
{
    init(false);
#ifndef CTS_USES_VULKANSC
    init(true);
#endif
}

template <typename Instance, typename Support = NoSupport0>
class maintenance6InstanceFactory : public TestCase
{

public:
    maintenance6InstanceFactory(tcu::TestContext &testCtx, const std::string &name,
                                typename Instance::TestSpec testSpec)
        : TestCase(testCtx, name)
        , m_testSpec(testSpec)
        , m_support()
    {
    }

    maintenance6InstanceFactory(tcu::TestContext &testCtx, const std::string &name,
                                typename Instance::TestSpec testSpec, const Support &support)
        : TestCase(testCtx, name)
        , m_testSpec(testSpec)
        , m_support(support)
    {
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new Instance(context, m_testSpec);
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const override
    {
        for (ShaderMap::const_iterator i = m_testSpec.shaders.begin(); i != m_testSpec.shaders.end(); ++i)
        {
            programCollection.glslSources.add(i->second)
                << glu::ShaderSource(i->first, ShaderSourceProvider::getSource(m_testCtx.getArchive(), i->second));
        }
    }

    virtual void checkSupport(Context &context) const override
    {
        m_support.checkSupport(context);
    }

    // When CTS_USES_VULKANSC, the default feature list is sufficient
#ifndef CTS_USES_VULKANSC
    std::string getRequiredCapabilitiesId() const override
    {
        if (m_testSpec.nullDescriptor)
        {
            return "maintenance6_nulldescriptor";
        }
        return "DEFAULT";
    }

    void initDeviceCapabilities(DevCaps &caps) override
    {
        caps.addExtension("VK_KHR_create_renderpass2");
        caps.addExtension("VK_KHR_dynamic_rendering");
        caps.addFeature(&vk::VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering);

        caps.addExtension(VK_EXT_MULTI_DRAW_EXTENSION_NAME);
        caps.addFeature(&vk::VkPhysicalDeviceMultiDrawFeaturesEXT::multiDraw);

        caps.addExtension(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
        // VUID-vkCmdDrawIndexedIndirectCount-None-04445
        caps.addFeature(&vk::VkPhysicalDeviceVulkan12Features::drawIndirectCount);

        caps.addExtension(VK_KHR_MAINTENANCE_6_EXTENSION_NAME);
        caps.addFeature(&vk::VkPhysicalDeviceMaintenance6FeaturesKHR::maintenance6);

        caps.addFeature(&vk::VkPhysicalDeviceVulkan12Features::drawIndirectCount);
        caps.addFeature(&vk::VkPhysicalDeviceFeatures::fragmentStoresAndAtomics);

        caps.addExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
        caps.addFeature(&vk::VkPhysicalDeviceMaintenance5FeaturesKHR::maintenance5);
        if (m_testSpec.nullDescriptor)
        {
            caps.addExtension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
            caps.addFeature(&vk::VkPhysicalDeviceRobustness2FeaturesEXT::nullDescriptor);
        }
        // VUID-RuntimeSpirv-NonWritable-06340
        if (m_testSpec.testDrawCount)
        {
            caps.addFeature(&vk::VkPhysicalDeviceFeatures::fragmentStoresAndAtomics);
        }
    }
#endif

private:
    const typename Instance::TestSpec m_testSpec;
    const Support m_support;
};

void DrawIndexedTests::init(bool useMaintenance5Ext)
{
    std::string maintenance5ExtNameSuffix = useMaintenance5Ext ? "_maintenance_5" : "";

    const struct
    {
        const vk::VkPrimitiveTopology topology;
        const char *nameSuffix;
    } TopologyCases[] = {
        // triangle list
        {vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, "triangle_list"},
        // triangle strip
        {vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, "triangle_strip"},
    };

    const struct
    {
        const int offset;
        const char *nameSuffix;
    } OffsetCases[] = {
        {VERTEX_OFFSET_DEFAULT, ""},
        //  using -1 as the vertex offset
        {VERTEX_OFFSET_MINUS_ONE, "_offset_minus_one"},
        //  using a large negative number as the vertex offset
        {VERTEX_OFFSET_NEGATIVE, "_offset_negative_large"},
    };

    const struct
    {
        IndexBindOffset bindOffset;
        const char *nameSuffix;
    } IndexBindOffsetCases[] = {
        {IndexBindOffset::DEFAULT, ""},
        //  and applying an index buffer bind offset
        {IndexBindOffset::POSITIVE, "_with_bind_offset"},
    };

    const struct
    {
        MemoryBindOffset memoryBindOffset;
        const char *nameSuffix;
    } MemoryBindOffsetCases[] = {
        {MemoryBindOffset::DEFAULT, ""},
        //  and applying an extra memory allocation offset
        {MemoryBindOffset::POSITIVE, "_with_alloc_offset"},
    };

    for (const auto &offsetCase : OffsetCases)
    {
        for (const auto &indexBindOffsetCase : IndexBindOffsetCases)
        {
            const auto indexBindOffset = static_cast<vk::VkDeviceSize>(indexBindOffsetCase.bindOffset);

            for (const auto &memoryBindOffsetCase : MemoryBindOffsetCases)
            {
                const auto memoryBindOffset = static_cast<vk::VkDeviceSize>(memoryBindOffsetCase.memoryBindOffset);

                for (const auto &topologyCase : TopologyCases)
                {
                    {
                        DrawIndexed::TestSpec testSpec({{glu::SHADERTYPE_VERTEX, "vulkan/draw/VertexFetch.vert"},
                                                        {glu::SHADERTYPE_FRAGMENT, "vulkan/draw/VertexFetch.frag"}},
                                                       topologyCase.topology, m_groupParams, offsetCase.offset,
                                                       indexBindOffset, memoryBindOffset, TEST_TYPE_NON_MAINTENANCE_6,
                                                       useMaintenance5Ext, false, false);

                        const auto testName = std::string("draw_indexed_") + topologyCase.nameSuffix +
                                              offsetCase.nameSuffix + indexBindOffsetCase.nameSuffix +
                                              memoryBindOffsetCase.nameSuffix + maintenance5ExtNameSuffix;

                        addChild(new InstanceFactory<DrawIndexed, FunctionSupport1<DrawIndexed::TestSpec>>(
                            m_testCtx, testName, testSpec,
                            FunctionSupport1<DrawIndexed::TestSpec>::Args(checkSupport, testSpec)));
                    }
                    {
                        DrawInstancedIndexed::TestSpec testSpec(
                            {{glu::SHADERTYPE_VERTEX, "vulkan/draw/VertexFetchInstancedFirstInstance.vert"},
                             {glu::SHADERTYPE_FRAGMENT, "vulkan/draw/VertexFetch.frag"}},
                            topologyCase.topology, m_groupParams, offsetCase.offset, indexBindOffset, memoryBindOffset,
                            TEST_TYPE_NON_MAINTENANCE_6, useMaintenance5Ext, false, false);

                        const auto testName = std::string("draw_instanced_indexed_") + topologyCase.nameSuffix +
                                              offsetCase.nameSuffix + indexBindOffsetCase.nameSuffix +
                                              memoryBindOffsetCase.nameSuffix + maintenance5ExtNameSuffix;

                        addChild(
                            new InstanceFactory<DrawInstancedIndexed, FunctionSupport1<DrawInstancedIndexed::TestSpec>>(
                                m_testCtx, testName, testSpec,
                                FunctionSupport1<DrawInstancedIndexed::TestSpec>::Args(checkSupport, testSpec)));
                    }
                }
            }
        }
    }

    const struct
    {
        TestType testType;
        std::string nameSuffix;
    } Maintenance6Cases[] = {
        {TEST_TYPE_MAINTENANCE6_INDEXED, ""},
        {TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT, "_indirect"},
        {TEST_TYPE_MAINTENANCE6_INDEXED_INDIRECT_COUNT, "_indirect_count"},
#ifndef CTS_USES_VULKANSC
        {TEST_TYPE_MAINTENANCE6_MULTI_INDEXED_EXT, "_multi"},
#endif
    };

    for (const auto &maintenance6Case : Maintenance6Cases)
    {
        for (int m5 = 0; m5 < 2; m5++)
        {
            for (int null = 0; null < 2; null++)
            {
                for (uint32_t testDrawCountIdx = 0; testDrawCountIdx < 2; testDrawCountIdx++)
                {
                    const char *m5Suffix       = m5 == 0 ? "" : "_bindindexbuffer2";
                    const char *nullSuffix     = null == 0 ? "" : "_nulldescriptor";
                    const auto drawCountSuffix = (testDrawCountIdx == 0) ? "" : "_count";

                    const auto testName = std::string("draw_indexed") + drawCountSuffix + maintenance6Case.nameSuffix +
                                          m5Suffix + nullSuffix + maintenance5ExtNameSuffix +
                                          std::string("_maintenance6");

                    const auto vertShader =
                        (testDrawCountIdx == 0) ? "vulkan/draw/VertexFetch.vert" : "vulkan/draw/VertexFetchCount.vert";
                    const auto fragShader =
                        (testDrawCountIdx == 0) ? "vulkan/draw/VertexFetch.frag" : "vulkan/draw/VertexFetchCount.frag";

                    DrawIndexedMaintenance6::TestSpec testSpec(
                        {{glu::SHADERTYPE_VERTEX, vertShader}, {glu::SHADERTYPE_FRAGMENT, fragShader}},
                        vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST, m_groupParams, 0, 0, 0, maintenance6Case.testType,
                        useMaintenance5Ext, null == 1, m5 == 1, testDrawCountIdx == 1);

                    addChild(new maintenance6InstanceFactory<DrawIndexedMaintenance6,
                                                             FunctionSupport1<DrawIndexed::TestSpec>>(
                        m_testCtx, testName, testSpec,
                        FunctionSupport1<DrawIndexedMaintenance6::TestSpec>::Args(checkSupport, testSpec)));
                }
            }
        }
    }

    if (!m_groupParams->useDynamicRendering && !m_groupParams->useSecondaryCmdBuffer && !useMaintenance5Ext)
    {
        for (const bool sortSizes : {false, true})
        {
            const auto caseCount = 20u;
            for (uint32_t i = 0u; i < caseCount; ++i)
            {
                const Multibind8BitParams params{i, sortSizes};
                const auto testName = "multibind_8bit_case_" + std::to_string(i) + (sortSizes ? "_sorted" : "");
                addChild(new Multibind8BitCase(m_testCtx, testName, params));
            }
        }

        const struct
        {
            vk::VkIndexType indexType;
            const char *name;
        } indexTypeCases[] = {
            {vk::VK_INDEX_TYPE_UINT32, "32"},
            {vk::VK_INDEX_TYPE_UINT16, "16"},
            {vk::VK_INDEX_TYPE_UINT8, "8"},
        };

        for (const auto &indexTypeCase : indexTypeCases)
        {
            const auto testName = std::string("update_index_buffer_before_draw_") + indexTypeCase.name;
            const UpdateBeforeDrawParams params{indexTypeCase.indexType};
            addChild(new UpdateBeforeDrawCase(m_testCtx, testName, params));
        }
    }
}

} // namespace Draw
} // namespace vkt
