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
 * \brief VK_KHR_shader_draw_parameters tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawShaderDrawParametersTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"
#include "vktDrawBaseClass.hpp"

#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{

enum TestFlagBits
{
    TEST_FLAG_INSTANCED      = 1u << 0,
    TEST_FLAG_INDEXED        = 1u << 1,
    TEST_FLAG_INDIRECT       = 1u << 2,
    TEST_FLAG_MULTIDRAW      = 1u << 3, //!< multiDrawIndirect
    TEST_FLAG_FIRST_INSTANCE = 1u << 4, //!< drawIndirectFirstInstance
};
typedef uint32_t TestFlags;

struct FlagsTestSpec : public TestSpecBase
{
    TestFlags flags;

    FlagsTestSpec(const SharedGroupParams groupParams_)
        : TestSpecBase{{}, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, groupParams_}
    {
    }
};

enum Constants
{
    // \note Data layout in buffers (junk data and good data is intertwined).
    //       Values are largely arbitrary, but we try to avoid "nice" numbers to make sure the test doesn't pass by accident.
    NUM_VERTICES            = 4,  //!< number of consecutive good vertices
    NDX_FIRST_VERTEX        = 2,  //!< index of first good vertex data
    NDX_SECOND_VERTEX       = 9,  //!< index of second good vertex data
    NDX_FIRST_INDEX         = 11, //!< index of a first good index (in index data)
    NDX_SECOND_INDEX        = 17, //!< index of a second good index
    OFFSET_FIRST_INDEX      = 1,  //!< offset added to the first index
    OFFSET_SECOND_INDEX     = 4,  //!< offset added to the second index
    MAX_INSTANCE_COUNT      = 3,  //!< max number of draw instances
    MAX_INDIRECT_DRAW_COUNT = 3,  //!< max drawCount of indirect calls
};

class DrawTest : public DrawTestsBaseClass
{
public:
    typedef FlagsTestSpec TestSpec;
    DrawTest(Context &context, TestSpec testSpec);
    tcu::TestStatus iterate(void);

private:
    template <typename T, std::size_t N>
    void setIndirectCommand(const T (&pCmdData)[N]);

    void drawReferenceImage(const tcu::PixelBufferAccess &refImage) const;

    bool isInstanced(void) const
    {
        return (m_flags & TEST_FLAG_INSTANCED) != 0;
    }
    bool isIndexed(void) const
    {
        return (m_flags & TEST_FLAG_INDEXED) != 0;
    }
    bool isIndirect(void) const
    {
        return (m_flags & TEST_FLAG_INDIRECT) != 0;
    }
    bool isMultiDraw(void) const
    {
        return (m_flags & TEST_FLAG_MULTIDRAW) != 0;
    }
    bool isFirstInstance(void) const
    {
        return (m_flags & TEST_FLAG_FIRST_INSTANCE) != 0;
    }
    void draw(vk::VkCommandBuffer cmdBuffer);

#ifndef CTS_USES_VULKANSC
    void beginSecondaryCmdBuffer(vk::VkRenderingFlagsKHR renderingFlags = 0u);
#endif // CTS_USES_VULKANSC

    const TestFlags m_flags;
    de::SharedPtr<Buffer> m_indexBuffer;
    de::SharedPtr<Buffer> m_indirectBuffer;
};

DrawTest::DrawTest(Context &context, TestSpec testSpec)
    : DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT],
                         testSpec.groupParams, testSpec.topology)
    , m_flags(testSpec.flags)
{
    DE_ASSERT(m_topology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    DE_ASSERT(!isMultiDraw() || isIndirect());
    DE_ASSERT(!isFirstInstance() || (isIndirect() && isInstanced()));

    // Vertex data
    {
        int refIndex = NDX_FIRST_VERTEX - OFFSET_FIRST_INDEX;

        m_data.push_back(VertexElementData(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f), -1));
        m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f), -1));

        if (!isIndexed())
            refIndex = 0;

        m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, -0.3f, 1.0f, 1.0f), tcu::Vec4(1.0f), refIndex++));
        m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, 0.3f, 1.0f, 1.0f), tcu::Vec4(1.0f), refIndex++));
        m_data.push_back(VertexElementData(tcu::Vec4(0.3f, -0.3f, 1.0f, 1.0f), tcu::Vec4(1.0f), refIndex++));
        m_data.push_back(VertexElementData(tcu::Vec4(0.3f, 0.3f, 1.0f, 1.0f), tcu::Vec4(1.0f), refIndex++));

        m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f), -1));
        m_data.push_back(VertexElementData(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f), -1));
        m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f), -1));

        if (!isIndexed())
            refIndex = 0;

        m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, -0.3f, 1.0f, 1.0f), tcu::Vec4(1.0f), refIndex++));
        m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, 0.3f, 1.0f, 1.0f), tcu::Vec4(1.0f), refIndex++));
        m_data.push_back(VertexElementData(tcu::Vec4(0.3f, -0.3f, 1.0f, 1.0f), tcu::Vec4(1.0f), refIndex++));
        m_data.push_back(VertexElementData(tcu::Vec4(0.3f, 0.3f, 1.0f, 1.0f), tcu::Vec4(1.0f), refIndex++));

        m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f), -1));
        m_data.push_back(VertexElementData(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::Vec4(1.0f), -1));

        // Make sure constants are up to date
        DE_ASSERT(m_data.size() == NDX_SECOND_VERTEX + NUM_VERTICES + 2);
        DE_ASSERT(NDX_SECOND_VERTEX - NDX_FIRST_VERTEX - NUM_VERTICES == 3);
    }

    if (isIndirect())
    {
        const std::size_t indirectBufferSize =
            MAX_INDIRECT_DRAW_COUNT * 32; // space for COUNT commands plus some gratuitous padding
        m_indirectBuffer = Buffer::createAndAlloc(
            m_vk, m_context.getDevice(), BufferCreateInfo(indirectBufferSize, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
            m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

        deMemset(m_indirectBuffer->getBoundMemory().getHostPtr(), 0, indirectBufferSize);
        vk::flushMappedMemoryRange(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory().getMemory(),
                                   m_indirectBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
    }

    if (isIndexed())
    {
        DE_ASSERT(NDX_FIRST_INDEX + NUM_VERTICES <= NDX_SECOND_INDEX);
        const std::size_t indexBufferSize = sizeof(uint32_t) * (NDX_SECOND_INDEX + NUM_VERTICES);
        m_indexBuffer                     = Buffer::createAndAlloc(m_vk, m_context.getDevice(),
                                                                   BufferCreateInfo(indexBufferSize, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
                                                                   m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);
        uint32_t *indices                 = static_cast<uint32_t *>(m_indexBuffer->getBoundMemory().getHostPtr());

        deMemset(indices, 0, indexBufferSize);

        for (int i = 0; i < NUM_VERTICES; i++)
        {
            indices[NDX_FIRST_INDEX + i]  = static_cast<uint32_t>(NDX_FIRST_VERTEX + i) - OFFSET_FIRST_INDEX;
            indices[NDX_SECOND_INDEX + i] = static_cast<uint32_t>(NDX_SECOND_VERTEX + i) - OFFSET_SECOND_INDEX;
        }

        vk::flushMappedMemoryRange(m_vk, m_context.getDevice(), m_indexBuffer->getBoundMemory().getMemory(),
                                   m_indexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
    }

    initialize();
}

template <typename T, std::size_t N>
void DrawTest::setIndirectCommand(const T (&pCmdData)[N])
{
    DE_ASSERT(N != 0 && N <= MAX_INDIRECT_DRAW_COUNT);

    const std::size_t dataSize = N * sizeof(T);

    deMemcpy(m_indirectBuffer->getBoundMemory().getHostPtr(), pCmdData, dataSize);
    vk::flushMappedMemoryRange(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory().getMemory(),
                               m_indirectBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
}

//! This function must be kept in sync with the shader.
void DrawTest::drawReferenceImage(const tcu::PixelBufferAccess &refImage) const
{
    using tcu::IVec4;
    using tcu::Vec2;
    using tcu::Vec4;

    const Vec2 perInstanceOffset[] = {Vec2(0.0f, 0.0f), Vec2(-0.3f, 0.0f), Vec2(0.0f, 0.3f)};
    const Vec2 perDrawOffset[]     = {Vec2(0.0f, 0.0f), Vec2(-0.3f, -0.3f), Vec2(0.3f, 0.3f)};
    const Vec4 allColors[]         = {Vec4(1.0f), Vec4(0.0f, 0.0f, 1.0f, 1.0f), Vec4(0.0f, 1.0f, 0.0f, 1.0f)};
    const int numInstances         = isInstanced() ? MAX_INSTANCE_COUNT : 1;
    const int numIndirectDraws     = isMultiDraw() ? MAX_INDIRECT_DRAW_COUNT : 1;
    const int rectWidth            = static_cast<int>(static_cast<float>(WIDTH) * 0.6f / 2.0f);
    const int rectHeight           = static_cast<int>(static_cast<float>(HEIGHT) * 0.6f / 2.0f);

    DE_ASSERT(DE_LENGTH_OF_ARRAY(perInstanceOffset) >= numInstances);
    DE_ASSERT(DE_LENGTH_OF_ARRAY(allColors) >= numInstances && DE_LENGTH_OF_ARRAY(allColors) >= numIndirectDraws);
    DE_ASSERT(DE_LENGTH_OF_ARRAY(perDrawOffset) >= numIndirectDraws);

    tcu::clear(refImage, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    for (int drawNdx = 0; drawNdx < numIndirectDraws; ++drawNdx)
        for (int instanceNdx = 0; instanceNdx < numInstances; ++instanceNdx)
        {
            const Vec2 offset = perInstanceOffset[instanceNdx] + perDrawOffset[drawNdx];
            const Vec4 &color = allColors[isMultiDraw() ? drawNdx : instanceNdx];
            int x             = static_cast<int>(static_cast<float>(WIDTH) * (1.0f - 0.3f + offset.x()) / 2.0f);
            int y             = static_cast<int>(static_cast<float>(HEIGHT) * (1.0f - 0.3f + offset.y()) / 2.0f);

            tcu::clear(tcu::getSubregion(refImage, x, y, rectWidth, rectHeight), color);
        }
}

tcu::TestStatus DrawTest::iterate(void)
{
    // Draw
#ifndef CTS_USES_VULKANSC
    if (m_groupParams->useSecondaryCmdBuffer)
    {
        // record secondary command buffer
        if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        {
            beginSecondaryCmdBuffer(vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
            beginDynamicRender(*m_secCmdBuffer);
        }
        else
            beginSecondaryCmdBuffer();

        draw(*m_secCmdBuffer);

        if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            endDynamicRender(*m_secCmdBuffer);

        endCommandBuffer(m_vk, *m_secCmdBuffer);

        // record primary command buffer
        beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

        preRenderBarriers();

        if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            beginDynamicRender(*m_cmdBuffer, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

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
        draw(*m_cmdBuffer);
        endDynamicRender(*m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);
    }
#endif // CTS_USES_VULKANSC

    if (!m_groupParams->useDynamicRendering)
    {
        beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
        preRenderBarriers();
        beginLegacyRender(*m_cmdBuffer);
        draw(*m_cmdBuffer);
        endLegacyRender(*m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);
    }

    // Submit
    {
        const vk::VkQueue queue   = m_context.getUniversalQueue();
        const vk::VkDevice device = m_context.getDevice();

        submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());
    }

    // Validate
    {
        tcu::TextureLevel referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat),
                                         static_cast<int>(0.5f + static_cast<float>(WIDTH)),
                                         static_cast<int>(0.5f + static_cast<float>(HEIGHT)));

        drawReferenceImage(referenceFrame.getAccess());

        const vk::VkOffset3D zeroOffset                 = {0, 0, 0};
        const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(
            m_context.getUniversalQueue(), m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset,
            WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

        if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result",
                               referenceFrame.getAccess(), renderedFrame, 0.05f, tcu::COMPARE_LOG_RESULT))
            return tcu::TestStatus::fail("Rendered image is incorrect");
        else
            return tcu::TestStatus::pass("OK");
    }
}

#ifndef CTS_USES_VULKANSC
void DrawTest::beginSecondaryCmdBuffer(vk::VkRenderingFlagsKHR renderingFlags)
{
    vk::VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                                             // const void* pNext;
        renderingFlags,                                                      // VkRenderingFlagsKHR flags;
        0u,                                                                  // uint32_t viewMask;
        1u,                                                                  // uint32_t colorAttachmentCount;
        &m_colorAttachmentFormat,                                            // const VkFormat* pColorAttachmentFormats;
        vk::VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        vk::VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        vk::VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits rasterizationSamples;
    };
    const vk::VkCommandBufferInheritanceInfo bufferInheritanceInfo = vk::initVulkanStructure(&inheritanceRenderingInfo);

    vk::VkCommandBufferUsageFlags usageFlags = vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        usageFlags |= vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    const vk::VkCommandBufferBeginInfo commandBufBeginParams{
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        DE_NULL,                                         // const void* pNext;
        usageFlags,                                      // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo};

    VK_CHECK(m_vk.beginCommandBuffer(*m_secCmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

void DrawTest::draw(vk::VkCommandBuffer cmdBuffer)
{
    const vk::VkDeviceSize vertexBufferOffset = 0;
    const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();

    m_vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
    m_vk.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    if (isIndexed())
        m_vk.cmdBindIndexBuffer(cmdBuffer, m_indexBuffer->object(), 0ull, vk::VK_INDEX_TYPE_UINT32);

    const uint32_t numInstances = isInstanced() ? MAX_INSTANCE_COUNT : 1;

    if (isIndirect())
    {
        if (isIndexed())
        {
            const vk::VkDrawIndexedIndirectCommand commands[]{
                // indexCount, instanceCount, firstIndex, vertexOffset, firstInstance
                {NUM_VERTICES, numInstances, NDX_FIRST_INDEX, OFFSET_FIRST_INDEX, (isFirstInstance() ? 2u : 0u)},
                {NUM_VERTICES, numInstances, NDX_SECOND_INDEX, OFFSET_SECOND_INDEX, (isFirstInstance() ? 1u : 0u)},
                {NUM_VERTICES, numInstances, NDX_FIRST_INDEX, OFFSET_FIRST_INDEX, (isFirstInstance() ? 3u : 0u)},
            };
            setIndirectCommand(commands);
        }
        else
        {
            const vk::VkDrawIndirectCommand commands[]{
                // vertexCount, instanceCount, firstVertex, firstInstance
                {NUM_VERTICES, numInstances, NDX_FIRST_VERTEX, (isFirstInstance() ? 2u : 0u)},
                {NUM_VERTICES, numInstances, NDX_SECOND_VERTEX, (isFirstInstance() ? 1u : 0u)},
                {NUM_VERTICES, numInstances, NDX_FIRST_VERTEX, (isFirstInstance() ? 3u : 0u)},
            };
            setIndirectCommand(commands);
        }
    }

    if (isIndirect())
    {
        const uint32_t numIndirectDraws = isMultiDraw() ? MAX_INDIRECT_DRAW_COUNT : 1;

        if (isIndexed())
            m_vk.cmdDrawIndexedIndirect(cmdBuffer, m_indirectBuffer->object(), 0ull, numIndirectDraws,
                                        sizeof(vk::VkDrawIndexedIndirectCommand));
        else
            m_vk.cmdDrawIndirect(cmdBuffer, m_indirectBuffer->object(), 0ull, numIndirectDraws,
                                 sizeof(vk::VkDrawIndirectCommand));
    }
    else
    {
        const uint32_t firstInstance = 2;

        if (isIndexed())
            m_vk.cmdDrawIndexed(cmdBuffer, NUM_VERTICES, numInstances, NDX_FIRST_INDEX, OFFSET_FIRST_INDEX,
                                firstInstance);
        else
            m_vk.cmdDraw(cmdBuffer, NUM_VERTICES, numInstances, NDX_FIRST_VERTEX, firstInstance);
    }
}

void checkSupport(Context &context, DrawTest::TestSpec testSpec)
{
    context.requireDeviceFunctionality("VK_KHR_shader_draw_parameters");

    // Shader draw parameters is part of Vulkan 1.1 but is optional
    if (context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
    {
        // Check if shader draw parameters is supported on the physical device.
        vk::VkPhysicalDeviceShaderDrawParametersFeatures drawParameters = {
            vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES, // sType
            DE_NULL,                                                               // pNext
            VK_FALSE                                                               // shaderDrawParameters
        };

        vk::VkPhysicalDeviceFeatures features;
        deMemset(&features, 0, sizeof(vk::VkPhysicalDeviceFeatures));

        vk::VkPhysicalDeviceFeatures2 featuresExt = {vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, // sType
                                                     &drawParameters,                                  // pNext
                                                     features};

        context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &featuresExt);

        if (drawParameters.shaderDrawParameters == VK_FALSE)
            TCU_THROW(NotSupportedError, "shaderDrawParameters feature not supported by the device");
    }

    if (testSpec.groupParams->useDynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    if (testSpec.flags & TEST_FLAG_MULTIDRAW)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_DRAW_INDIRECT);

    if (testSpec.flags & TEST_FLAG_FIRST_INSTANCE)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DRAW_INDIRECT_FIRST_INSTANCE);
}

void addDrawCase(tcu::TestCaseGroup *group, DrawTest::TestSpec testSpec, const TestFlags flags)
{
    std::ostringstream name;
    name << "draw";

    if (flags & TEST_FLAG_INDEXED)
        name << "_indexed";
    if (flags & TEST_FLAG_INDIRECT)
        name << "_indirect";
    if (flags & TEST_FLAG_INSTANCED)
        name << "_instanced";
    if (flags & TEST_FLAG_FIRST_INSTANCE)
        name << "_first_instance";

    testSpec.flags |= flags;

    group->addChild(new InstanceFactory<DrawTest, FunctionSupport1<DrawTest::TestSpec>>(
        group->getTestContext(), name.str(), testSpec,
        FunctionSupport1<DrawTest::TestSpec>::Args(checkSupport, testSpec)));
}

} // namespace

ShaderDrawParametersTests::ShaderDrawParametersTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
    : TestCaseGroup(testCtx, "shader_draw_parameters")
    , m_groupParams(groupParams)
{
}

void ShaderDrawParametersTests::init(void)
{
    {
        DrawTest::TestSpec testSpec(m_groupParams);
        testSpec.shaders[glu::SHADERTYPE_VERTEX]   = "vulkan/draw/VertexFetchShaderDrawParameters.vert";
        testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
        testSpec.topology                          = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        testSpec.flags                             = 0;

        de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(getTestContext(), "base_vertex"));
        addDrawCase(group.get(), testSpec, 0);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDEXED);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDIRECT);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDEXED | TEST_FLAG_INDIRECT);
        addChild(group.release());
    }
    {
        DrawTest::TestSpec testSpec(m_groupParams);
        testSpec.shaders[glu::SHADERTYPE_VERTEX]   = "vulkan/draw/VertexFetchShaderDrawParameters.vert";
        testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
        testSpec.topology                          = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        testSpec.flags                             = TEST_FLAG_INSTANCED;

        de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(getTestContext(), "base_instance"));
        addDrawCase(group.get(), testSpec, 0);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDEXED);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDIRECT);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDIRECT | TEST_FLAG_FIRST_INSTANCE);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDEXED | TEST_FLAG_INDIRECT);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDEXED | TEST_FLAG_INDIRECT | TEST_FLAG_FIRST_INSTANCE);
        addChild(group.release());
    }
    {
        DrawTest::TestSpec testSpec(m_groupParams);
        testSpec.shaders[glu::SHADERTYPE_VERTEX]   = "vulkan/draw/VertexFetchShaderDrawParametersDrawIndex.vert";
        testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/draw/VertexFetch.frag";
        testSpec.topology                          = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        testSpec.flags                             = TEST_FLAG_INDIRECT | TEST_FLAG_MULTIDRAW;

        de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(getTestContext(), "draw_index"));
        addDrawCase(group.get(), testSpec, 0);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INSTANCED);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDEXED);
        addDrawCase(group.get(), testSpec, TEST_FLAG_INDEXED | TEST_FLAG_INSTANCED);
        addChild(group.release());
    }
}

} // namespace Draw
} // namespace vkt
