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
 * \brief Draw tests to verify line drawing parameters do not affect non-line primitives
 *//*--------------------------------------------------------------------*/

#include "vktDrawNonLineTests.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuImageCompare.hpp"

#include "deRandom.hpp"

#include <memory>
#include <vector>

namespace vkt
{
namespace Draw
{

namespace
{

using namespace vk;

enum class VertexTopology
{
    TRIANGLES = 0,
    LINES,
    POINTS,
};

enum class GeometryOutput
{
    NONE = 0, // No geometry shader
    TRIANGLES,
    LINES,
    POINTS,
};

VkPrimitiveTopology toPrimitiveTopology(VertexTopology topology)
{
    VkPrimitiveTopology result = VK_PRIMITIVE_TOPOLOGY_LAST;
    switch (topology)
    {
    case VertexTopology::TRIANGLES:
        result = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    case VertexTopology::LINES:
        result = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;
    case VertexTopology::POINTS:
        result = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        break;
    default:
        DE_ASSERT(false);
        break;
    }
    return result;
}

struct Params
{
    VertexTopology vertexTopology;
    GeometryOutput geometryOutput;
    VkPolygonMode polygonMode;
    VkLineRasterizationModeKHR lineRasterizationMode;

    bool useGeometryShader(void) const
    {
        return (geometryOutput != GeometryOutput::NONE);
    }

    std::string getGeometryInputPrimitive(void) const
    {
        std::string result;
        switch (vertexTopology)
        {
        case VertexTopology::TRIANGLES:
            result = "triangles";
            break;
        case VertexTopology::LINES:
            result = "lines";
            break;
        case VertexTopology::POINTS:
            result = "points";
            break;
        default:
            DE_ASSERT(false);
            break;
        }
        return result;
    }

    uint32_t getVertexOutputLength(void) const
    {
        uint32_t result = 0u;
        switch (vertexTopology)
        {
        case VertexTopology::TRIANGLES:
            result = 3u;
            break;
        case VertexTopology::LINES:
            result = 2u;
            break;
        case VertexTopology::POINTS:
            result = 1u;
            break;
        default:
            DE_ASSERT(false);
            break;
        }
        return result;
    }

    std::string getGeometryOutputPrimitive(void) const
    {
        std::string result;
        switch (geometryOutput)
        {
        case GeometryOutput::TRIANGLES:
            result = "triangle_strip";
            break;
        case GeometryOutput::LINES:
            result = "line_strip";
            break;
        case GeometryOutput::POINTS:
            result = "points";
            break;
        default:
            DE_ASSERT(false);
            break;
        }
        return result;
    }

    uint32_t getGeometryOutputLength(void) const
    {
        uint32_t result = 0u;
        switch (geometryOutput)
        {
        case GeometryOutput::TRIANGLES:
            result = 3u;
            break;
        case GeometryOutput::LINES:
            result = 2u;
            break;
        case GeometryOutput::POINTS:
            result = 1u;
            break;
        default:
            DE_ASSERT(false);
            break;
        }
        return result;
    }

    uint32_t getRandomSeed(void) const
    {
        return ((1u << 24) | (static_cast<uint32_t>(vertexTopology) << 16) |
                (static_cast<uint32_t>(geometryOutput) << 8) | (static_cast<uint32_t>(polygonMode)));
        // We exclude the line rasterization mode on purpose.
    }
};

class NonLineDrawInstance : public vkt::TestInstance
{
public:
    NonLineDrawInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~NonLineDrawInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class NonLineDrawCase : public vkt::TestCase
{
public:
    NonLineDrawCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~NonLineDrawCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const Params m_params;
};

void NonLineDrawCase::checkSupport(Context &context) const
{
    if (m_params.useGeometryShader())
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    const auto &lineRasterFeatures  = context.getLineRasterizationFeatures();
    const VkBool32 *requiredFeature = nullptr;

    switch (m_params.lineRasterizationMode)
    {
    case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_KHR:
        requiredFeature = &lineRasterFeatures.rectangularLines;
        break;
    case VK_LINE_RASTERIZATION_MODE_BRESENHAM_KHR:
        requiredFeature = &lineRasterFeatures.bresenhamLines;
        break;
    case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_KHR:
        requiredFeature = &lineRasterFeatures.smoothLines;
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    if (!(*requiredFeature))
        TCU_THROW(NotSupportedError, "Required line type not supported");
}

void NonLineDrawCase::initPrograms(SourceCollections &dst) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=1) in vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "out gl_PerVertex\n"
         << "{\n"
         << "    vec4 gl_Position;\n"
         << "    float gl_PointSize;\n"
         << "};\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
         << "    outColor = inColor;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (location=0) in vec4 inColor;\n"
         << "void main (void) {\n"
         << "    outColor = inColor;\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());

    if (m_params.useGeometryShader())
    {
        const std::string inputPrimitive  = m_params.getGeometryInputPrimitive();
        const std::string outputPrimitive = m_params.getGeometryOutputPrimitive();
        const auto inputLength            = m_params.getVertexOutputLength();
        const auto outputLength           = m_params.getGeometryOutputLength();

        std::ostringstream geom;
        geom << "#version 460\n"
             << "layout(" << inputPrimitive << ") in;\n"
             << "layout(" << outputPrimitive << ", max_vertices=" << outputLength << ") out;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "} gl_in[" << inputLength << "];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "};\n"
             << "layout (location=0) in vec4 inColor[" << inputLength << "];\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main (void) {\n";

        if (m_params.geometryOutput == GeometryOutput::POINTS)
        {
            geom << "    for (uint i = 0; i < " << inputLength << "; ++i) {\n"
                 << "        gl_Position = gl_in[i].gl_Position;\n"
                 << "        gl_PointSize = gl_in[i].gl_PointSize;\n"
                 << "        outColor = inColor[i];\n"
                 << "        EmitVertex(); EndPrimitive();\n"
                 << "    }\n";
        }
        else if (m_params.geometryOutput == GeometryOutput::TRIANGLES &&
                 m_params.vertexTopology == VertexTopology::TRIANGLES)
        {
            geom << "    for (uint i = 0; i < " << inputLength << "; ++i) {\n"
                 << "        gl_Position = gl_in[i].gl_Position;\n"
                 << "        gl_PointSize = gl_in[i].gl_PointSize;\n"
                 << "        outColor = inColor[i];\n"
                 << "        EmitVertex();\n"
                 << "    }\n"
                 << "    EndPrimitive();\n";
        }
        else if (m_params.geometryOutput == GeometryOutput::TRIANGLES &&
                 m_params.vertexTopology == VertexTopology::LINES)
        {
            geom << "    for (uint i = 0; i < " << inputLength << "; ++i) {\n"
                 << "        gl_Position = gl_in[i].gl_Position;\n"
                 << "        gl_PointSize = gl_in[i].gl_PointSize;\n"
                 << "        outColor = inColor[i];\n"
                 << "        EmitVertex();\n"
                 << "    }\n"
                 // Generate a third vertex by applying an offset to the vertical coordinate of the first vertex.
                 << "    gl_Position = gl_in[0].gl_Position + vec4(0.0, -0.5, 0.0, 0.0);\n"
                 << "    gl_PointSize = gl_in[0].gl_PointSize;\n"
                 << "    outColor = inColor[0];\n"
                 << "    EmitVertex();\n"
                 << "    EndPrimitive();\n";
        }
        else if (m_params.geometryOutput == GeometryOutput::TRIANGLES &&
                 m_params.vertexTopology == VertexTopology::POINTS)
        {
            // Generate a triangle around the point by creating new vertices with offsets around coordinates.
            geom << "    gl_Position = gl_in[0].gl_Position + vec4(0.0, -0.5, 0.0, 0.0);\n"
                 << "    gl_PointSize = gl_in[0].gl_PointSize;\n"
                 << "    outColor = inColor[0];\n"
                 << "    EmitVertex();\n"
                 << "    gl_Position = gl_in[0].gl_Position + vec4(0.0, 0.5, 0.0, 0.0);\n"
                 << "    gl_PointSize = gl_in[0].gl_PointSize;\n"
                 << "    outColor = inColor[0];\n"
                 << "    EmitVertex();\n"
                 << "    gl_Position = gl_in[0].gl_Position + vec4(0.5, 0.0, 0.0, 0.0);\n"
                 << "    gl_PointSize = gl_in[0].gl_PointSize;\n"
                 << "    outColor = inColor[0];\n"
                 << "    EmitVertex();\n"
                 << "    EndPrimitive();\n";
        }
        else
            DE_ASSERT(false);

        geom << "}\n";
        dst.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }
}

TestInstance *NonLineDrawCase::createInstance(Context &context) const
{
    return new NonLineDrawInstance(context, m_params);
}

tcu::TestStatus NonLineDrawInstance::iterate(void)
{
    const auto &ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(32, 32, 1);
    const auto vkExtent  = makeExtent3D(fbExtent);
    const auto fbFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat = mapVkFormat(fbFormat);
    const auto fbUsage   = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f,
                              0.0f); // We expect rasterization to produce the same result in both cases.
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto iterations  = 2u;
    const auto vertOutLen  = m_params.getVertexOutputLength();
    const auto quadrantVtx = 6u;

    // Color buffers with verification buffer.
    using ImageWithBufferPtr = std::unique_ptr<ImageWithBuffer>;
    std::vector<ImageWithBufferPtr> colorBuffers;
    colorBuffers.reserve(iterations);
    for (uint32_t i = 0u; i < iterations; ++i)
    {
        colorBuffers.emplace_back(
            new ImageWithBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D));
    }

    // Generate vertices per quadrant, grouping colors by primitive.
    const std::vector<tcu::Vec4> colorCatalogue{
        tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };

    const std::vector<tcu::Vec4> quadrantOffsets = {
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, -1.0f, 0.0f, 0.0f),
        tcu::Vec4(-1.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
    };

    struct VertexInfo
    {
        VertexInfo(const tcu::Vec4 &position_, const tcu::Vec4 &color_) : position(position_), color(color_)
        {
        }

        tcu::Vec4 position;
        tcu::Vec4 color;
    };

    de::Random rnd(m_params.getRandomSeed());
    std::vector<VertexInfo> vertices;
    vertices.reserve(quadrantOffsets.size() * quadrantVtx);

    tcu::Vec4 currentColor = clearColor;
    for (const auto &qOffset : quadrantOffsets)
    {
        for (uint32_t i = 0u; i < quadrantVtx; ++i)
        {
            // Switch color when starting new pritimive.
            if (i % vertOutLen == 0u)
                currentColor = colorCatalogue.at(rnd.getInt(0, static_cast<int>(colorCatalogue.size() - 1u)));

            const auto x             = rnd.getFloat();
            const auto y             = rnd.getFloat();
            const tcu::Vec4 position = tcu::Vec4(x, y, 0.0f, 1.0f) + qOffset;

            vertices.emplace_back(position, currentColor);
        }
    };

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);
    const auto renderPass     = makeRenderPass(ctx.vkd, ctx.device, fbFormat);

    std::vector<Move<VkFramebuffer>> framebuffers;
    framebuffers.reserve(iterations);
    for (uint32_t i = 0u; i < iterations; ++i)
        framebuffers.emplace_back(makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffers.at(i)->getImageView(),
                                                  vkExtent.width, vkExtent.height));

    // Modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto geomModule =
        (m_params.useGeometryShader() ? createShaderModule(ctx.vkd, ctx.device, binaries.get("geom")) :
                                        Move<VkShaderModule>());
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        makeVertexInputBindingDescription(0u, static_cast<uint32_t>(sizeof(VertexInfo)), VK_VERTEX_INPUT_RATE_VERTEX),
    };

    const std::vector<VkVertexInputAttributeDescription> attributeDescriptions{
        makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(VertexInfo, position))),
        makeVertexInputAttributeDescription(1u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(VertexInfo, color))),
    };

    const VkPipelineVertexInputStateCreateInfo vertexInput = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //  VkStructureType                          sType;
        nullptr,                                                   //  const void*                              pNext;
        0u,                                                        //  VkPipelineVertexInputStateCreateFlags    flags;
        de::sizeU32(vertexBindings),    //  uint32_t                                 vertexBindingDescriptionCount;
        de::dataOrNull(vertexBindings), //  const VkVertexInputBindingDescription*   pVertexBindingDescriptions;
        de::sizeU32(
            attributeDescriptions), //  uint32_t                                 vertexAttributeDescriptionCount;
        de::dataOrNull(
            attributeDescriptions), //  const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const VkPipelineRasterizationStateCreateInfo defaultRasterizationState = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, //  VkStructureType                         sType;
        nullptr,                                                    //  const void*                             pNext;
        0u,                                                         //  VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                        //  VkBool32                                depthClampEnable;
        VK_FALSE,                        //  VkBool32                                rasterizerDiscardEnable;
        m_params.polygonMode,            //  VkPolygonMode                           polygonMode;
        VK_CULL_MODE_NONE,               //  VkCullModeFlags                         cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE, //  VkFrontFace                             frontFace;
        VK_FALSE,                        //  VkBool32                                depthBiasEnable;
        0.0f,                            //  float                                   depthBiasConstantFactor;
        0.0f,                            //  float                                   depthBiasClamp;
        0.0f,                            //  float                                   depthBiasSlopeFactor;
        1.0f,                            //  float                                   lineWidth;
    };

    const VkPipelineRasterizationLineStateCreateInfoKHR lineRasterizationState = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_KHR, //  VkStructureType             sType;
        nullptr,                                                             //  const void*                 pNext;
        m_params.lineRasterizationMode, //  VkLineRasterizationModeKHR  lineRasterizationMode;
        VK_FALSE,                       //  VkBool32                    stippledLineEnable;
        0u,                             //  uint32_t                    lineStippleFactor;
        0u,                             //  uint16_t                    lineStipplePattern;
    };

    std::vector<VkPipelineRasterizationStateCreateInfo> rasterizationStates;
    rasterizationStates.reserve(iterations);

    for (uint32_t i = 0u; i < iterations; ++i)
    {
        // Critical for the test: draw first without line drawing parameters, draw second with line drawing params.
        const bool withLineParams = (i > 0u);
        auto rasterState          = defaultRasterizationState;
        if (withLineParams)
            rasterState.pNext = &lineRasterizationState;
        rasterizationStates.push_back(rasterState);
    }

    const auto primitiveTopology = toPrimitiveTopology(m_params.vertexTopology);

    std::vector<Move<VkPipeline>> pipelines;
    pipelines.reserve(iterations);

    for (uint32_t i = 0u; i < iterations; ++i)
    {
        pipelines.emplace_back(makeGraphicsPipeline(
            ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, *geomModule, *fragModule,
            *renderPass, viewports, scissors, primitiveTopology, 0u, 0u, &vertexInput, &rasterizationStates.at(i)));
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    for (uint32_t i = 0u; i < iterations; ++i)
    {
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffers.at(i), scissors.at(0u), clearColor);
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipelines.at(i));
        ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
        endRenderPass(ctx.vkd, cmdBuffer);
    }
    for (uint32_t i = 0u; i < iterations; ++i)
    {
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffers.at(i)->getImage(), colorBuffers.at(i)->getBuffer(),
                          fbExtent.swizzle(0, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify the result of both draws is identical.
    for (uint32_t i = 0u; i < iterations; ++i)
        invalidateAlloc(ctx.vkd, ctx.device, colorBuffers.at(i)->getBufferAllocation());

    tcu::ConstPixelBufferAccess referenceAccess(tcuFormat, fbExtent,
                                                colorBuffers.at(0u)->getBufferAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent,
                                             colorBuffers.at(1u)->getBufferAllocation().getHostPtr());

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDrawNonLineTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "non_line_with_params"));

    const struct
    {
        VertexTopology vertexTopology;
        const char *name;
    } vertexTopologyCases[] = {
        {VertexTopology::TRIANGLES, "vtx_triangles"},
        {VertexTopology::LINES, "vtx_lines"},
        {VertexTopology::POINTS, "vtx_points"},
    };

    const struct
    {
        GeometryOutput geometryOutput;
        const char *suffix;
    } geometryOutputCases[] = {
        {GeometryOutput::NONE, ""},
        {GeometryOutput::TRIANGLES, "_geom_triangles"},
        {GeometryOutput::LINES, "_geom_lines"},
        {GeometryOutput::POINTS, "_geom_points"},
    };

    const struct
    {
        VkPolygonMode polygonMode;
        const char *suffix;
    } polygonModeCases[] = {
        {VK_POLYGON_MODE_FILL, "_mode_fill"},
        {VK_POLYGON_MODE_LINE, "_mode_line"},
        {VK_POLYGON_MODE_POINT, "_mode_point"},
    };

    const struct
    {
        VkLineRasterizationModeKHR lineRasterMode;
        const char *suffix;
    } lineRasterModeCases[] = {
        {VK_LINE_RASTERIZATION_MODE_RECTANGULAR_KHR, "_line_raster_rect"},
        {VK_LINE_RASTERIZATION_MODE_BRESENHAM_KHR, "_line_raster_bresenham"},
        {VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_KHR, "_line_raster_smooth"},
    };

    for (const auto &vertexTopologyCase : vertexTopologyCases)
        for (const auto geometryOutputCase : geometryOutputCases)
            for (const auto polygonModeCase : polygonModeCases)
                for (const auto lineRasterModeCase : lineRasterModeCases)
                {
                    const auto &geometryOutput = geometryOutputCase.geometryOutput;
                    const auto &vertexTopology = vertexTopologyCase.vertexTopology;
                    const auto &polygonMode    = polygonModeCase.polygonMode;
                    const auto &lineRasterMode = lineRasterModeCase.lineRasterMode;

                    const bool producesLines =
                        (geometryOutput == GeometryOutput::LINES ||
                         (geometryOutput == GeometryOutput::NONE && vertexTopology == VertexTopology::LINES));
                    const bool producesTriangles =
                        (geometryOutput == GeometryOutput::TRIANGLES ||
                         (geometryOutput == GeometryOutput::NONE && vertexTopology == VertexTopology::TRIANGLES));

                    // These cases rasterize lines, so we must skip because rendering may be different depending on the line rasterization parameters.
                    if (producesLines || (producesTriangles && polygonMode == VK_POLYGON_MODE_LINE))
                        break;

                    const Params params{
                        vertexTopology,
                        geometryOutput,
                        polygonMode,
                        lineRasterMode,
                    };

                    const auto testName = std::string(vertexTopologyCase.name) + geometryOutputCase.suffix +
                                          polygonModeCase.suffix + lineRasterModeCase.suffix;

                    mainGroup->addChild(new NonLineDrawCase(testCtx, testName, params));
                }

    return mainGroup.release();
}

} // namespace Draw
} // namespace vkt
