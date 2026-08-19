/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief Large/Off-center view port tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawLargeViewportTests.hpp"

#include "vktDrawCreateInfoUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{

using namespace vk;
using namespace tcu;

enum
{
    WIDTH  = 64,
    HEIGHT = 64
};

enum class ViewportCase
{
    VP_NEGATIVE_Y_MAX_HEIGHT = 0,
    VP_NEGATIVE_X_MAX_WIDTH,
    VP_NEGATIVE_Y_MAX_HEIGHT_OVERLAP,
    VP_NEGATIVE_X_MAX_WIDTH_OVERLAP,
    VP_CASE_NONE
};

struct TestParams
{
    TestParams(const ViewportCase vpc, const bool dynVp, const SharedGroupParams gp)
        : viewportCase(vpc)
        , dynamicViewport(dynVp)
        , groupParams(gp)
    {
    }

    const ViewportCase viewportCase;
    const bool dynamicViewport;
    const SharedGroupParams groupParams;
};

class LargeViewportTestInstance : public TestInstance
{
public:
    LargeViewportTestInstance(Context &ctx, const TestParams &testParams);
    virtual ~LargeViewportTestInstance(void);
    TestStatus iterate(void);

#ifndef CTS_USES_VULKANSC
    void beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkFormat colorAttachmentFormat,
                                 VkRenderingFlagsKHR renderingFlags = 0u) const;
#endif // CTS_USES_VULKANSC

private:
    const TestParams m_testParams;
};

LargeViewportTestInstance::LargeViewportTestInstance(Context &ctx, const TestParams &testParams)
    : TestInstance(ctx)
    , m_testParams(testParams)
{
}

LargeViewportTestInstance::~LargeViewportTestInstance(void)
{
}

VkViewport getViewport(Context &ctx, const ViewportCase &vpcase, const UVec2 &imageSize)
{
    const auto &phyDevProps          = ctx.getDeviceProperties();
    const auto limits                = phyDevProps.limits;
    const auto maxViewportDimensionX = limits.maxViewportDimensions[0];
    const auto maxViewportDimensionY = limits.maxViewportDimensions[1];
    const auto boundsMax             = limits.viewportBoundsRange[1];

    const auto defaultViewport = makeViewport(0.0f, 0.0f, (float)imageSize.x(), (float)imageSize.y(), 0.0f, 1.0f);

    if (vpcase == ViewportCase::VP_NEGATIVE_Y_MAX_HEIGHT)
        return makeViewport(0.0f, -boundsMax, (float)imageSize.x(), (float)maxViewportDimensionY, 0.0f, 1.0f);

    if (vpcase == ViewportCase::VP_NEGATIVE_X_MAX_WIDTH)
        return makeViewport(-boundsMax, 0.0f, (float)maxViewportDimensionX, (float)imageSize.y(), 0.0f, 1.0f);

    if (vpcase == ViewportCase::VP_NEGATIVE_Y_MAX_HEIGHT_OVERLAP)
        return makeViewport(0.0f, -static_cast<float>(maxViewportDimensionY) * 0.75f, (float)imageSize.x(),
                            (float)maxViewportDimensionY, 0.0f, 1.0f);

    if (vpcase == ViewportCase::VP_NEGATIVE_X_MAX_WIDTH_OVERLAP)
        return makeViewport(-static_cast<float>(maxViewportDimensionX) * 0.75f, 0.0f, (float)maxViewportDimensionX,
                            (float)imageSize.y(), 0.0f, 1.0f);

    return defaultViewport;
}

#ifndef CTS_USES_VULKANSC
void LargeViewportTestInstance::beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkFormat colorAttachmentFormat,
                                                        VkRenderingFlagsKHR renderingFlags) const
{
    VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        nullptr,                                                         // const void* pNext;
        renderingFlags,                                                  // VkRenderingFlagsKHR flags;
        0u,                                                              // uint32_t viewMask;
        1u,                                                              // uint32_t colorAttachmentCount;
        &colorAttachmentFormat,                                          // const VkFormat* pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        VK_SAMPLE_COUNT_1_BIT,                                           // VkSampleCountFlagBits rasterizationSamples;
    };
    const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);

    VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    const VkCommandBufferBeginInfo commandBufBeginParams{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo};

    const DeviceInterface &vk = m_context.getDeviceInterface();
    VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

TestStatus LargeViewportTestInstance::iterate(void)
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &allocator       = m_context.getDefaultAllocator();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();

    const auto imageSize   = UVec2(WIDTH, HEIGHT);
    const auto imageExtent = makeExtent3D(imageSize.x(), imageSize.y(), 1u);
    const auto renderArea  = makeRect2D(imageExtent.width, imageExtent.height);
    const auto imageUsage =
        (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat   = mapVkFormat(imageFormat);
    const auto clearColor  = RGBA::black().toVec();

    // Large view port
    const auto customViewport = getViewport(m_context, m_testParams.viewportCase, imageSize);
    const auto numViewports   = 1u;

    // Output image
    ImageWithBuffer colorBuffer(vkd, device, allocator, imageExtent, imageFormat, imageUsage, VK_IMAGE_TYPE_2D);

    // Render pass
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> frameBuffer;

    if (!m_testParams.groupParams->useDynamicRendering)
    {
        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(AttachmentDescription(
            imageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

        const VkAttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, nullptr, 1,
                                                           &colorAttachmentRef, nullptr, AttachmentReference(), 0,
                                                           nullptr));

        renderPass = createRenderPass(vkd, device, &renderPassCreateInfo);

        // Framebuffer
        std::vector<VkImageView> colorAttachment{colorBuffer.getImageView()};
        const FramebufferCreateInfo framebufferCreateInfo(*renderPass, colorAttachment, imageSize.x(), imageSize.y(),
                                                          1u);
        frameBuffer = createFramebuffer(vkd, device, &framebufferCreateInfo);
    }

    // Shader modules
    const Unique<VkShaderModule> vertexModule(
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert")));
    const Unique<VkShaderModule> fragModule(
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag")));

    // Pipeline
    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);
    Move<VkPipeline> pipeline;

    {
        const PipelineCreateInfo::ColorBlendState::Attachment colorBlendState;
        const std::vector<VkRect2D> scissors{renderArea};

        PipelineCreateInfo pipelineCreateInfo(*pipelineLayout, *renderPass, 0, 0);
        pipelineCreateInfo.addShader(
            PipelineCreateInfo::PipelineShaderStage(*vertexModule, "main", VK_SHADER_STAGE_VERTEX_BIT));
        pipelineCreateInfo.addShader(
            PipelineCreateInfo::PipelineShaderStage(*fragModule, "main", VK_SHADER_STAGE_FRAGMENT_BIT));

        pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState());
        pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP));
        pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &colorBlendState));
        pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
        pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
        pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

        if (m_testParams.dynamicViewport)
        {
            pipelineCreateInfo.addState(
                PipelineCreateInfo::DynamicState(std::vector<VkDynamicState>(1, VK_DYNAMIC_STATE_VIEWPORT)));
            pipelineCreateInfo.addState(
                PipelineCreateInfo::ViewportState(numViewports, std::vector<vk::VkViewport>(0), scissors));
        }
        else
        {
            pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(
                numViewports, std::vector<VkViewport>(numViewports, customViewport), scissors));
        }

#ifndef CTS_USES_VULKANSC
        VkPipelineRenderingCreateInfoKHR renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                             nullptr,
                                                             0u,
                                                             1u,
                                                             &imageFormat,
                                                             VK_FORMAT_UNDEFINED,
                                                             VK_FORMAT_UNDEFINED};

        if (m_testParams.groupParams->useDynamicRendering)
            pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

        pipeline = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    // Command pool and command buffer
    Move<VkCommandPool> cmdPool =
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    Move<VkCommandBuffer> cmdBufferPtr = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer               = *cmdBufferPtr;

    // Clear
    clearColorImage(vkd, device, queue, queueIndex, colorBuffer.getImage(), clearColor, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u);

    // Execute draw
    Move<VkCommandBuffer> secCmdBufferPtr;
#ifndef CTS_USES_VULKANSC
    const auto clearValue = makeClearValueColor(clearColor);

    if (m_testParams.groupParams->useSecondaryCmdBuffer)
    {
        secCmdBufferPtr = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

        const auto secCmdBuffer = *secCmdBufferPtr;

        // record secondary command buffer
        if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        {
            beginSecondaryCmdBuffer(secCmdBuffer, imageFormat, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
            beginRendering(vkd, secCmdBuffer, colorBuffer.getImageView(), renderArea, clearValue,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR);
        }
        else
            beginSecondaryCmdBuffer(secCmdBuffer, imageFormat);

        vkd.cmdBindPipeline(secCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

        if (m_testParams.dynamicViewport)
            vkd.cmdSetViewport(secCmdBuffer, 0u, numViewports, &customViewport);

        vkd.cmdDraw(secCmdBuffer, 4u, 1u, 0u, 0u);

        if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            endRendering(vkd, secCmdBuffer);

        endCommandBuffer(vkd, secCmdBuffer);

        // record primary command buffer
        beginCommandBuffer(vkd, cmdBuffer, 0u);

        if (!m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            beginRendering(vkd, cmdBuffer, colorBuffer.getImageView(), renderArea, clearValue,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR,
                           VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

        vkd.cmdExecuteCommands(cmdBuffer, 1u, &secCmdBuffer);

        if (!m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            endRendering(vkd, cmdBuffer);

        copyImageToBuffer(vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(),
                          IVec2(imageExtent.width, imageExtent.height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        endCommandBuffer(vkd, cmdBuffer);
    }
    else if (m_testParams.groupParams->useDynamicRendering)
    {
        beginCommandBuffer(vkd, cmdBuffer);

        beginRendering(vkd, cmdBuffer, colorBuffer.getImageView(), renderArea, clearValue,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR);

        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

        if (m_testParams.dynamicViewport)
            vkd.cmdSetViewport(cmdBuffer, 0u, numViewports, &customViewport);

        vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

        endRendering(vkd, cmdBuffer);

        copyImageToBuffer(vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(),
                          IVec2(imageExtent.width, imageExtent.height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        endCommandBuffer(vkd, cmdBuffer);
    }
#endif // CTS_USES_VULKANSC

    if (!m_testParams.groupParams->useDynamicRendering)
    {
        beginCommandBuffer(vkd, cmdBuffer);

        beginRenderPass(vkd, cmdBuffer, *renderPass, *frameBuffer, renderArea, clearColor);

        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

        if (m_testParams.dynamicViewport)
            vkd.cmdSetViewport(cmdBuffer, 0u, numViewports, &customViewport);

        vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

        endRenderPass(vkd, cmdBuffer);

        copyImageToBuffer(vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(),
                          IVec2(imageExtent.width, imageExtent.height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        endCommandBuffer(vkd, cmdBuffer);
    }

    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Get results
    const auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(vkd, device, colorBufferAlloc);

    TextureLevel reference(tcuFormat, imageExtent.width, imageExtent.height);
    const auto channelThreshold = 0.005f; // 1/255 < 0.005 < 2/255
    const Vec4 threshold(channelThreshold, channelThreshold, channelThreshold, 0.0f);

    clear(reference.getAccess(), clearColor);

    if ((m_testParams.viewportCase == ViewportCase::VP_NEGATIVE_Y_MAX_HEIGHT_OVERLAP) ||
        (m_testParams.viewportCase == ViewportCase::VP_NEGATIVE_X_MAX_WIDTH_OVERLAP))
    {
        PixelBufferAccess refAccess = reference.getAccess();
        const auto expectedColor    = tcu::RGBA::red().toVec();

        for (uint32_t y = 0; y < imageExtent.height; ++y)
        {
            for (uint32_t x = 0; x < imageExtent.width; ++x)
            {
                const float ndcX = (2.0f * ((float)x + 0.5f - customViewport.x) / customViewport.width) - 1.0f;
                const float ndcY = (2.0f * ((float)y + 0.5f - customViewport.y) / customViewport.height) - 1.0f;

                if ((ndcY >= -1.0f && ndcY <= 1.0f) && (ndcX >= -1.0f && ndcX <= 1.0f))
                    refAccess.setPixel(expectedColor, x, y);
                else
                    refAccess.setPixel(clearColor, x, y);
            }
        }
    }

    const ConstPixelBufferAccess result(tcuFormat, imageExtent.width, imageExtent.height, 1u,
                                        colorBufferAlloc.getHostPtr());

    auto &log = m_context.getTestContext().getLog();

    if (!floatThresholdCompare(log, "Result", "Result", reference.getAccess(), result, threshold, COMPARE_LOG_ON_ERROR))
        return TestStatus::fail("Fail");

    return TestStatus::pass("Pass");
}

class LargeViewportTestCase : public TestCase
{
public:
    LargeViewportTestCase(TestContext &ctx, const char *name, const TestParams &testParams);
    virtual ~LargeViewportTestCase(void);
    virtual void checkSupport(Context &ctx) const;
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &ctx) const;

private:
    const TestParams m_testParams;
};

LargeViewportTestCase::LargeViewportTestCase(TestContext &ctx, const char *name, const TestParams &testParams)
    : TestCase(ctx, name)
    , m_testParams(testParams)
{
}

LargeViewportTestCase::~LargeViewportTestCase(void)
{
}

void LargeViewportTestCase::checkSupport(Context &ctx) const
{
    if (m_testParams.groupParams->useDynamicRendering)
        ctx.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

void LargeViewportTestCase::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream vert;
    {
        vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "void main (void)\n"
             << "{\n"
             << "  const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
             << "  const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
             << "  gl_Position = vec4(x, y, 1.0, 1.0);\n"
             << "}\n";
    }
    sourceCollections.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    {
        frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout(location = 0) out vec4 fragColor;\n"
             << "\n"
             << "void main() {\n"
             << "    fragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
             << "}\n";
    }
    sourceCollections.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *LargeViewportTestCase::createInstance(Context &ctx) const
{
    return new LargeViewportTestInstance(ctx, m_testParams);
}

void createTests(TestCaseGroup *testGroup, const SharedGroupParams groupParams)
{
    TestContext &testCtx = testGroup->getTestContext();

    struct
    {
        ViewportCase vpCase;
        const std::string vpCaseName;
    } vpCases[] = {
        {ViewportCase::VP_NEGATIVE_Y_MAX_HEIGHT, "large_negative_y_max_height"},
        {ViewportCase::VP_NEGATIVE_X_MAX_WIDTH, "large_negative_x_max_width"},
        {ViewportCase::VP_NEGATIVE_Y_MAX_HEIGHT_OVERLAP, "large_negative_y_max_height_overlap"},
        {ViewportCase::VP_NEGATIVE_X_MAX_WIDTH_OVERLAP, "large_negative_x_max_width_overlap"},
    };

    for (uint32_t caseIdx = 0u; caseIdx < (uint32_t)ViewportCase::VP_CASE_NONE; caseIdx++)
    {
        for (const auto dynamicViewport : {true, false})
        {
            TestParams params(vpCases[caseIdx].vpCase, dynamicViewport, groupParams);

            const std::string testName = vpCases[caseIdx].vpCaseName + (dynamicViewport ? "_dynamic" : "");
            testGroup->addChild(new LargeViewportTestCase(testCtx, testName.c_str(), params));
        }
    }
}

} // namespace

TestCaseGroup *createLargeViewportTests(TestContext &testCtx, const SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "large_viewport", createTests, groupParams);
}

} // namespace Draw
} // namespace vkt
