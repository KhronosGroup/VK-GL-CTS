/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Tests for empty and missing Fragment Shaders.
 *//*--------------------------------------------------------------------*/

#include "vktPipelineEmptyFSTests.hpp"
#include "tcuImageCompare.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"

#include <iostream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

struct TestParams
{
    PipelineConstructionType constructionType;
    VkShaderStageFlagBits lastVertexStage; // Last vertex shader stage: vertex, tessellation or geometry.
    bool emptyFS;                          // True: empty FS; False: do not include a fragment shader at all.

    TestParams(PipelineConstructionType type_, VkShaderStageFlagBits lastStage, bool emptyFS_)
        : constructionType(type_)
        , lastVertexStage(lastStage)
        , emptyFS(emptyFS_)
    {
        DE_ASSERT(lastIsVertex() || lastIsTessellation() || lastIsGeometry());
    }

    bool lastIsVertex(void) const
    {
        return (lastVertexStage == VK_SHADER_STAGE_VERTEX_BIT);
    }

    bool lastIsTessellation(void) const
    {
        return (lastVertexStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
                lastVertexStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    }

    bool lastIsGeometry(void) const
    {
        return (lastVertexStage == VK_SHADER_STAGE_GEOMETRY_BIT);
    }
};

class EmptyFSInstance : public vkt::TestInstance
{
public:
    EmptyFSInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~EmptyFSInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

class EmptyFSCase : public vkt::TestCase
{
public:
    EmptyFSCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~EmptyFSCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const TestParams m_params;
};

TestInstance *EmptyFSCase::createInstance(Context &context) const
{
    return new EmptyFSInstance(context, m_params);
}

void EmptyFSCase::checkSupport(Context &context) const
{
    if (m_params.lastIsTessellation())
    {
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE);
    }

    if (m_params.lastIsGeometry())
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);
}

void EmptyFSCase::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream userOutputsDecl;
    userOutputsDecl << "layout (location=0) out float added;\n"
                    << "layout (location=1) out float multiplied;\n";

    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << (m_params.lastIsVertex() ? userOutputsDecl.str() : "") << "out gl_PerVertex\n"
         << "{\n"
         << "    vec4  gl_Position;\n"
         << "    float gl_PointSize;\n"
         << "};\n"
         << "void main (void)\n"
         << "{\n"
         << "    gl_Position  = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << (m_params.lastIsVertex() ? "    added        = inPos.x + 1000.0;\n"
                                       "    multiplied   = inPos.y * 1000.0;\n" :
                                       "")
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    if (m_params.lastIsTessellation())
    {
        // Add passthrough tessellation control shader.
        std::ostringstream tesc;
        tesc << "#version 460\n"
             << "layout (vertices=3) out;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_out[];\n"
             << "void main (void)\n"
             << "{\n"
             << "    gl_TessLevelInner[0] = 1.0;\n"
             << "    gl_TessLevelInner[1] = 1.0;\n"
             << "    gl_TessLevelOuter[0] = 1.0;\n"
             << "    gl_TessLevelOuter[1] = 1.0;\n"
             << "    gl_TessLevelOuter[2] = 1.0;\n"
             << "    gl_TessLevelOuter[3] = 1.0;\n"
             << "    gl_out[gl_InvocationID].gl_Position  = gl_in[gl_InvocationID].gl_Position;\n"
             << "    gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
             << "}\n";

        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());

        std::ostringstream tese;
        tese << "#version 460\n"
             << "layout (triangles, fractional_odd_spacing, cw) in;\n"
             << userOutputsDecl.str() << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "};\n"
             << "void main (void)\n"
             << "{\n"
             << "    vec4 pos     = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
             << "                   (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
             << "                   (gl_TessCoord.z * gl_in[2].gl_Position);\n"
             << "    gl_Position  = pos;\n"
             << "    gl_PointSize = gl_in[0].gl_PointSize;\n"
             << "    added        = pos.x + 1000.0;\n"
             << "    multiplied   = pos.y * 1000.0;\n"
             << "}\n";

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }

    if (m_params.lastIsGeometry())
    {
        const auto vertexCount = 3u;

        std::ostringstream geom;
        geom << "#version 450\n"
             << "layout (triangles) in;\n"
             << "layout (triangle_strip, max_vertices=" << vertexCount << ") out;\n"
             << userOutputsDecl.str() << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[" << vertexCount << "];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "};\n"
             << "void main() {\n";

        for (uint32_t i = 0u; i < vertexCount; ++i)
        {
            geom << "    gl_Position  = gl_in[" << i << "].gl_Position;\n"
                 << "    gl_PointSize = gl_in[" << i << "].gl_PointSize;\n"
                 << "    added        = gl_in[" << i << "].gl_Position.x + 1000.0;\n"
                 << "    multiplied   = gl_in[" << i << "].gl_Position.y * 1000.0;\n"
                 << "    EmitVertex();\n";
        }

        geom << "}\n";
        programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }

    if (m_params.emptyFS)
    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) in float added;\n"
             << "layout (location=1) in float multiplied;\n"
             << "void main (void) {}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
}

tcu::TestStatus EmptyFSInstance::iterate(void)
{
    const auto &ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto pixelCount  = fbExtent.x() * fbExtent.y() * fbExtent.z();
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto fbFormat    = VK_FORMAT_R8G8B8A8_UNORM;
    const auto dsFormat    = VK_FORMAT_D16_UNORM;
    const auto tcuFormat   = mapVkFormat(dsFormat);
    const float depthThres = 0.000025f; // 1/65535 < depthThres < 2/65535
    const auto fbUsage     = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto dsUsage     = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    // Depth/stencil buffer.
    ImageWithBuffer dsBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, dsFormat, dsUsage, VK_IMAGE_TYPE_2D,
                             makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u));

    // Vertices.
    const float pixelWidth  = 2.0f / static_cast<float>(vkExtent.width);
    const float pixelHeight = 2.0f / static_cast<float>(vkExtent.height);
    const float horMargin   = pixelWidth / 4.0f;
    const float vertMargin  = pixelHeight / 4.0f;

    const auto calcCenter = [](int i, int size)
    { return (static_cast<float>(i) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f; };

    // One triangle per pixel with varying depth.
    std::vector<tcu::Vec4> vertices;
    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float xCenter = calcCenter(x, fbExtent.x());
            const float yCenter = calcCenter(y, fbExtent.y());
            const int pixelId   = y * fbExtent.x() + x;
            const float depth   = static_cast<float>(pixelId) / static_cast<float>(pixelCount);

            // Triangle around the center.
            vertices.emplace_back(xCenter, yCenter - vertMargin, depth, 1.0f);
            vertices.emplace_back(xCenter - horMargin, yCenter + vertMargin, depth, 1.0f);
            vertices.emplace_back(xCenter + horMargin, yCenter + vertMargin, depth, 1.0f);
        }

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vbAlloc); // strictly speaking, not needed.

    // Pipeline layout
    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device);
    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, fbFormat, dsFormat);
    std::vector<VkImage> images{colorBuffer.getImage(), dsBuffer.getImage()};
    std::vector<VkImageView> imageViews{colorBuffer.getImageView(), dsBuffer.getImageView()};

    DE_ASSERT(images.size() == imageViews.size());
    renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(images), de::dataOrNull(images),
                                 de::dataOrNull(imageViews), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto tescModule =
        (m_params.lastIsTessellation() ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("tesc")) : ShaderWrapper());
    const auto teseModule =
        (m_params.lastIsTessellation() ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("tese")) : ShaderWrapper());
    const auto geomModule =
        (m_params.lastIsGeometry() ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("geom")) : ShaderWrapper());
    const auto fragModule =
        (m_params.emptyFS ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag")) : ShaderWrapper());

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = initVulkanStructure();
    rasterizationStateCreateInfo.lineWidth                              = 1.0f;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = initVulkanStructure();
    depthStencilStateCreateInfo.depthTestEnable                       = VK_TRUE;
    depthStencilStateCreateInfo.depthWriteEnable                      = VK_TRUE;
    depthStencilStateCreateInfo.depthCompareOp                        = VK_COMPARE_OP_ALWAYS;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = initVulkanStructure();
    inputAssemblyStateCreateInfo.topology =
        (m_params.lastIsTessellation() ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    GraphicsPipelineWrapper pipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                            m_context.getDeviceExtensions(), m_params.constructionType);
    pipelineWrapper.setMonolithicPipelineLayout(pipelineLayout);
    pipelineWrapper.setDefaultVertexInputState(true);
    pipelineWrapper.setDefaultColorBlendState();
    pipelineWrapper.setDefaultMultisampleState();
    pipelineWrapper.setDefaultPatchControlPoints(3u);
    pipelineWrapper.setupVertexInputState(nullptr, &inputAssemblyStateCreateInfo);
    pipelineWrapper.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule,
                                                     &rasterizationStateCreateInfo, tescModule, teseModule, geomModule);
    pipelineWrapper.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule, &depthStencilStateCreateInfo);
    pipelineWrapper.setupFragmentOutputState(*renderPass);
    pipelineWrapper.buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const std::vector<VkClearValue> clearValues{makeClearValueColor(clearColor), makeClearValueDepthStencil(0.0f, 0u)};
    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), de::sizeU32(clearValues), de::dataOrNull(clearValues));
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    pipelineWrapper.bind(cmdBuffer);
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, dsBuffer.getImage(), dsBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                      1u, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                      (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify depth output.
    invalidateAlloc(ctx.vkd, ctx.device, dsBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, dsBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const int pixelId = y * fbExtent.x() + x;
            const float depth = static_cast<float>(pixelId) / static_cast<float>(pixelCount);

            referenceAccess.setPixDepth(depth, x, y);
        }

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::dsThresholdCompare(log, "DepthResult", "", referenceAccess, resultAccess, depthThres,
                                 tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createEmptyFSTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineType)
{
    de::MovePtr<tcu::TestCaseGroup> emptyFSTests(new tcu::TestCaseGroup(testCtx, "empty_fs"));

    const struct
    {
        VkShaderStageFlagBits shaderStage;
        const char *name;
    } vertexStages[] = {
        {VK_SHADER_STAGE_VERTEX_BIT, "vert"},
        {VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tess"},
        {VK_SHADER_STAGE_VERTEX_BIT, "geom"},
    };

    for (const auto &stage : vertexStages)
        for (const bool emptyFS : {false, true})
        {
            const std::string suffix(emptyFS ? "_empty_fs" : "_no_fs");
            const std::string testName = stage.name + suffix;
            const TestParams params{pipelineType, stage.shaderStage, emptyFS};

            emptyFSTests->addChild(new EmptyFSCase(testCtx, testName, params));
        }

    return emptyFSTests.release();
}

} // namespace pipeline
} // namespace vkt
