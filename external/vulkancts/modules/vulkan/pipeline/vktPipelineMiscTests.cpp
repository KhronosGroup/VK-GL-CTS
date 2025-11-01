/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Miscellaneous pipeline tests.
 *//*--------------------------------------------------------------------*/

#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <array>
#include <numeric>
#include <memory>

#include "vkPipelineConstructionUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktPipelineMiscTests.hpp"

#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBuilderUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

enum AmberFeatureBits
{
    AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS = (1 << 0),
    AMBER_FEATURE_TESSELATION_SHADER                 = (1 << 1),
    AMBER_FEATURE_GEOMETRY_SHADER                    = (1 << 2),
};

using AmberFeatureFlags = uint32_t;

#ifndef CTS_USES_VULKANSC
std::vector<std::string> getFeatureList(AmberFeatureFlags flags)
{
    std::vector<std::string> requirements;

    if (flags & AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS)
        requirements.push_back("Features.vertexPipelineStoresAndAtomics");

    if (flags & AMBER_FEATURE_TESSELATION_SHADER)
        requirements.push_back("Features.tessellationShader");

    if (flags & AMBER_FEATURE_GEOMETRY_SHADER)
        requirements.push_back("Features.geometryShader");

    return requirements;
}
#endif // CTS_USES_VULKANSC

void addMonolithicAmberTests(tcu::TestCaseGroup *tests)
{
#ifndef CTS_USES_VULKANSC
    tcu::TestContext &testCtx = tests->getTestContext();

    // Shader test files are saved in <path>/external/vulkancts/data/vulkan/amber/pipeline/<basename>.amber
    struct Case
    {
        const char *basename;
        AmberFeatureFlags flags;
    };

    const Case cases[] = {
        {
            "position_to_ssbo",
            (AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS),
        },
        {
            "primitive_id_from_tess",
            (AMBER_FEATURE_TESSELATION_SHADER | AMBER_FEATURE_GEOMETRY_SHADER),
        },
        // Read gl_layer from fragment shaders without previous writes
        {
            "layer_read_from_frag",
            (AMBER_FEATURE_GEOMETRY_SHADER),
        },
    };
    for (unsigned i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
    {
        std::string file                      = std::string(cases[i].basename) + ".amber";
        std::vector<std::string> requirements = getFeatureList(cases[i].flags);
        cts_amber::AmberTestCase *testCase =
            cts_amber::createAmberTestCase(testCtx, cases[i].basename, "pipeline", file, requirements);

        tests->addChild(testCase);
    }
#else
    DE_UNREF(tests);
#endif
}

class ImplicitPrimitiveIDPassthroughCase : public vkt::TestCase
{
public:
    ImplicitPrimitiveIDPassthroughCase(tcu::TestContext &testCtx, const std::string &name,
                                       const PipelineConstructionType pipelineConstructionType, bool withTessellation)
        : vkt::TestCase(testCtx, name)
        , m_pipelineConstructionType(pipelineConstructionType)
        , m_withTessellationPassthrough(withTessellation)
    {
    }
    ~ImplicitPrimitiveIDPassthroughCase(void)
    {
    }
    void initPrograms(SourceCollections &programCollection) const override;
    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override;

    const PipelineConstructionType m_pipelineConstructionType;

private:
    bool m_withTessellationPassthrough;
};

class ImplicitPrimitiveIDPassthroughInstance : public vkt::TestInstance
{
public:
    ImplicitPrimitiveIDPassthroughInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                           bool withTessellation)
        : vkt::TestInstance(context)
        , m_pipelineConstructionType(pipelineConstructionType)
        , m_renderSize(2, 2)
        , m_extent(makeExtent3D(m_renderSize.x(), m_renderSize.y(), 1u))
        , m_graphicsPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                             context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType)
        , m_withTessellationPassthrough(withTessellation)
    {
    }
    ~ImplicitPrimitiveIDPassthroughInstance(void)
    {
    }
    tcu::TestStatus iterate(void) override;

private:
    PipelineConstructionType m_pipelineConstructionType;
    const tcu::UVec2 m_renderSize;
    const VkExtent3D m_extent;
    const VkFormat m_format = VK_FORMAT_R8G8B8A8_UNORM;
    GraphicsPipelineWrapper m_graphicsPipeline;
    bool m_withTessellationPassthrough;
};

TestInstance *ImplicitPrimitiveIDPassthroughCase::createInstance(Context &context) const
{
    return new ImplicitPrimitiveIDPassthroughInstance(context, m_pipelineConstructionType,
                                                      m_withTessellationPassthrough);
}

void ImplicitPrimitiveIDPassthroughCase::checkSupport(Context &context) const
{
    if (m_withTessellationPassthrough)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
}

void ImplicitPrimitiveIDPassthroughCase::initPrograms(SourceCollections &sources) const
{
    std::ostringstream vert;
    // Generate a vertically split framebuffer, filled with red on the
    // left, and a green on the right.
    vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
         << "void main ()\n"
         << "{\n"
         << "    switch (gl_VertexIndex) {\n"
         << "        case 0:\n"
         << "            gl_Position = vec4(-3.0, -1.0, 0.0, 1.0);\n"
         << "            break;\n"
         << "        case 1:\n"
         << "            gl_Position = vec4(0.0, 3.0, 0.0, 1.0);\n"
         << "            break;\n"
         << "        case 2:\n"
         << "            gl_Position = vec4(0.0, -1.0, 0.0, 1.0);\n"
         << "            break;\n"
         << "        case 3:\n"
         << "            gl_Position = vec4(0.0, -1.0, 0.0, 1.0);\n"
         << "            break;\n"
         << "        case 4:\n"
         << "            gl_Position = vec4(3.0, -1.0, 0.0, 1.0);\n"
         << "            break;\n"
         << "        case 5:\n"
         << "            gl_Position = vec4(0.0, 3.0, 0.0, 1.0);\n"
         << "            break;\n"
         << "    }\n"
         << "}\n";
    sources.glslSources.add("vert") << glu::VertexSource(vert.str());

    if (m_withTessellationPassthrough)
    {
        std::ostringstream tsc;
        tsc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "layout (vertices = 3) out;\n"
            << "\n"
            << "void main ()\n"
            << "{\n"
            << "    if (gl_InvocationID == 0) {\n"
            << "        gl_TessLevelInner[0] = 1.0;\n"
            << "        gl_TessLevelInner[1] = 1.0;\n"
            << "        gl_TessLevelOuter[0] = 1.0;\n"
            << "        gl_TessLevelOuter[1] = 1.0;\n"
            << "        gl_TessLevelOuter[2] = 1.0;\n"
            << "        gl_TessLevelOuter[3] = 1.0;\n"
            << "    }\n"
            << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
            << "}\n";
        sources.glslSources.add("tsc") << glu::TessellationControlSource(tsc.str());

        std::ostringstream tse;
        tse << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "layout (triangles, equal_spacing, cw) in;\n"
            << "\n"
            << "void main ()\n"
            << "{\n"
            << "    gl_Position = gl_in[0].gl_Position * gl_TessCoord.x +\n"
            << "                  gl_in[1].gl_Position * gl_TessCoord.y +\n"
            << "                  gl_in[2].gl_Position * gl_TessCoord.z;\n"
            << "}\n";
        sources.glslSources.add("tse") << glu::TessellationEvaluationSource(tse.str());
    }

    std::ostringstream frag;
    frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    const vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
         << "    const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
         << "    outColor = (gl_PrimitiveID % 2 == 0) ? red : green;\n"
         << "}\n";
    sources.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus ImplicitPrimitiveIDPassthroughInstance::iterate()
{
    const auto &vkd             = m_context.getDeviceInterface();
    const auto device           = m_context.getDevice();
    auto &alloc                 = m_context.getDefaultAllocator();
    const auto qIndex           = m_context.getUniversalQueueFamilyIndex();
    const auto queue            = m_context.getUniversalQueue();
    const auto tcuFormat        = mapVkFormat(m_format);
    const auto colorUsage       = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto verifBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Color attachment.
    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        m_format,                            // VkFormat format;
        m_extent,                            // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        colorUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory colorBuffer(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto colorBufferView =
        makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, m_format, colorSRR);

    // Verification buffer.
    const auto verifBufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat)) * m_extent.width * m_extent.height;
    const auto verifBufferInfo = makeBufferCreateInfo(verifBufferSize, verifBufferUsage);
    BufferWithMemory verifBuffer(vkd, device, alloc, verifBufferInfo, MemoryRequirement::HostVisible);
    auto &verifBufferAlloc = verifBuffer.getAllocation();

    // Render pass and framebuffer.
    RenderPassWrapper renderPass(m_pipelineConstructionType, vkd, device, m_format);
    renderPass.createFramebuffer(vkd, device, colorBuffer.get(), colorBufferView.get(), m_extent.width,
                                 m_extent.height);

    // Shader modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(vkd, device, binaries.get("vert"));
    const auto fragModule = ShaderWrapper(vkd, device, binaries.get("frag"));
    ShaderWrapper tscModule;
    ShaderWrapper tseModule;

    if (m_withTessellationPassthrough)
    {
        tscModule = ShaderWrapper(vkd, device, binaries.get("tsc"));
        tseModule = ShaderWrapper(vkd, device, binaries.get("tse"));
    }

    // Viewports and scissors.
    const std::vector<VkViewport> viewports(1u, makeViewport(m_extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(m_extent));

    const VkPipelineVertexInputStateCreateInfo vertexInputState     = initVulkanStructure();
    const VkPipelineRasterizationStateCreateInfo rasterizationState = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                          sType;
        nullptr,                                                    // const void*                              pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags  flags;
        VK_FALSE,                // VkBool32                                 depthClampEnable;
        VK_FALSE,                // VkBool32                                 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,    // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,       // VkCullModeFlags cullMode;
        VK_FRONT_FACE_CLOCKWISE, // VkFrontFace frontFace;
        VK_FALSE,                // VkBool32 depthBiasEnable;
        0.0f,                    // float depthBiasConstantFactor;
        0.0f,                    // float depthBiasClamp;
        0.0f,                    // float depthBiasSlopeFactor;
        1.0f,                    // float lineWidth;
    };

    // Pipeline layout and graphics pipeline.
    const PipelineLayoutWrapper pipelineLayout(m_pipelineConstructionType, vkd, device);

    const auto topology =
        m_withTessellationPassthrough ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_graphicsPipeline.setDefaultRasterizationState()
        .setDefaultTopology(topology)
        .setupVertexInputState(&vertexInputState)
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule,
                                          &rasterizationState, tscModule, tseModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
        .setupFragmentOutputState(*renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Draw.
    renderPass.begin(vkd, cmdBuffer, scissors.at(0u), clearColor);
    m_graphicsPipeline.bind(cmdBuffer);
    vkd.cmdDraw(cmdBuffer, 6, 1u, 0u, 0u);
    renderPass.end(vkd, cmdBuffer);

    // Copy to verification buffer.
    const auto copyRegion     = makeBufferImageCopy(m_extent, colorSRL);
    const auto transfer2Host  = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    const auto color2Transfer = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.get(), colorSRR);

    cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, &color2Transfer);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer.get(), 1u,
                             &copyRegion);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &transfer2Host);

    endCommandBuffer(vkd, cmdBuffer);

    // Submit and validate result.
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    auto &log = m_context.getTestContext().getLog();
    const tcu::IVec3 iExtent(static_cast<int>(m_extent.width), static_cast<int>(m_extent.height),
                             static_cast<int>(m_extent.depth));
    void *verifBufferData = verifBufferAlloc.getHostPtr();
    const tcu::ConstPixelBufferAccess verifAccess(tcuFormat, iExtent, verifBufferData);
    invalidateAlloc(vkd, device, verifBufferAlloc);

    const auto red   = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    const auto green = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);

    for (int x = 0; x < iExtent.x(); ++x)
        for (int y = 0; y < iExtent.y(); ++y)
        {
            const auto resultColor   = verifAccess.getPixel(x, y);
            const auto expectedColor = (x < iExtent.x() / 2) ? red : green;
            if (resultColor != expectedColor)
            {
                log << tcu::TestLog::ImageSet("Result image",
                                              "Expect left side of framebuffer red, and right side green")
                    << tcu::TestLog::Image("Result", "Verification buffer", verifAccess) << tcu::TestLog::EndImageSet;
                TCU_FAIL("Expected a vertically split framebuffer, filled with red on the left and green the right; "
                         "see the log for the unexpected result");
            }
        }

    return tcu::TestStatus::pass("Pass");
}

#ifndef CTS_USES_VULKANSC
class PipelineLibraryInterpolateAtSampleTestCase : public vkt::TestCase
{
public:
    PipelineLibraryInterpolateAtSampleTestCase(tcu::TestContext &context, const std::string &name);
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;
    //there are 4 sample points, which may have a shader invocation each, each of them writes 5 values
    //and we render a 2x2 grid.
    static constexpr uint32_t width                    = 2;
    static constexpr uint32_t height                   = 2;
    static constexpr VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_4_BIT;
    static constexpr uint32_t ResultCount              = (sampleCount + 1) * sampleCount * width * height;
};

class PipelineLibraryInterpolateAtSampleTestInstance : public vkt::TestInstance
{
public:
    PipelineLibraryInterpolateAtSampleTestInstance(Context &context);
    void runTest(BufferWithMemory &index, BufferWithMemory &values, size_t bufferSize, PipelineConstructionType type);
    virtual tcu::TestStatus iterate(void);
};

PipelineLibraryInterpolateAtSampleTestCase::PipelineLibraryInterpolateAtSampleTestCase(tcu::TestContext &context,
                                                                                       const std::string &name)
    : vkt::TestCase(context, name)
{
}

void PipelineLibraryInterpolateAtSampleTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY);
}

void PipelineLibraryInterpolateAtSampleTestCase::initPrograms(vk::SourceCollections &collection) const
{
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "vec2 positions[6] = vec2[](\n"
            << "        vec2(1.0, 1.0),"
            << "        vec2(-1.0, 1.0),"
            << "        vec2(-1.0, -1.0),"
            << "        vec2(-1.0, -1.0),"
            << "        vec2(1.0, -1.0),"
            << "        vec2(1.0, 1.0)"
            << ");\n"
            << "float values[6] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6};\n"
            << "layout (location=0) out float verify;"
            << "void main() {\n"
            << "        gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
            << "        verify = values[gl_VertexIndex];\n"
            << "}";
        collection.glslSources.add("vert") << glu::VertexSource(src.str());
    }

    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "layout(location = 0) out vec4 outColor;\n"
            << "layout (location=0) in float verify;"
            << "layout(std430, binding = 0) buffer Index {"
            << "    uint writeIndex;"
            << "} index;\n"
            << "layout(std430, binding = 1) buffer Values {"
            << "    float num[" << PipelineLibraryInterpolateAtSampleTestCase::ResultCount << "];"
            << "} values;\n"
            << "void main() {\n"
            << "    uint index = atomicAdd(index.writeIndex, 5);"
            << "    float iSample1 = interpolateAtSample(verify, 0);\n"
            << "    float iSample2 = interpolateAtSample(verify, 1);\n"
            << "    float iSample3 = interpolateAtSample(verify, 2);\n"
            << "    float iSample4 = interpolateAtSample(verify, 3);\n"
            << "    values.num[index] = verify;"
            << "    values.num[index + 1] = iSample1;"
            << "    values.num[index + 2] = iSample2;"
            << "    values.num[index + 3] = iSample3;"
            << "    values.num[index + 4] = iSample4;"
            << "    outColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
            << "}";
        collection.glslSources.add("frag") << glu::FragmentSource(src.str());
    }
}

TestInstance *PipelineLibraryInterpolateAtSampleTestCase::createInstance(Context &context) const
{
    return new PipelineLibraryInterpolateAtSampleTestInstance(context);
}

PipelineLibraryInterpolateAtSampleTestInstance::PipelineLibraryInterpolateAtSampleTestInstance(Context &context)
    : vkt::TestInstance(context)
{
}

void PipelineLibraryInterpolateAtSampleTestInstance::runTest(BufferWithMemory &index, BufferWithMemory &values,
                                                             size_t bufferSize, PipelineConstructionType type)
{
    const auto &vki       = m_context.getInstanceInterface();
    const auto &vkd       = m_context.getDeviceInterface();
    const auto physDevice = m_context.getPhysicalDevice();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    auto imageFormat      = vk::VK_FORMAT_R8G8B8A8_UNORM;
    auto imageExtent      = vk::makeExtent3D(2, 2, 1u);

    const std::vector<vk::VkViewport> viewports{makeViewport(imageExtent)};
    const std::vector<vk::VkRect2D> scissors{makeRect2D(imageExtent)};

    de::MovePtr<vk::ImageWithMemory> colorAttachment;

    vk::GraphicsPipelineWrapper pipeline1(vki, vkd, physDevice, device, m_context.getDeviceExtensions(), type);
    const auto qIndex = m_context.getUniversalQueueFamilyIndex();

    const auto subresourceRange = vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto imageUsage       = static_cast<vk::VkImageUsageFlags>(vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                               vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const vk::VkImageCreateInfo imageCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        imageFormat,                             // VkFormat format;
        imageExtent,                             // VkExtent3D extent;
        1u,                                      // uint32_t mipLevels;
        1u,                                      // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_4_BIT,               // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                              // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                      // uint32_t queueFamilyIndexCount;
        &qIndex,                                 // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    colorAttachment = de::MovePtr<vk::ImageWithMemory>(
        new vk::ImageWithMemory(vkd, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any));
    auto colorAttachmentView = vk::makeImageView(vkd, device, colorAttachment->get(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                 imageFormat, subresourceRange);

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

    auto descriptorSetLayout = layoutBuilder.build(vkd, device);
    vk::PipelineLayoutWrapper graphicsPipelineLayout(type, vkd, device, descriptorSetLayout.get());

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    const auto descriptorSetBuffer = makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

    // Update descriptor sets.
    DescriptorSetUpdateBuilder updater;

    const auto indexBufferInfo = makeDescriptorBufferInfo(index.get(), 0ull, sizeof(uint32_t));
    const auto valueBufferInfo = makeDescriptorBufferInfo(values.get(), 0ull, bufferSize);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indexBufferInfo);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &valueBufferInfo);

    updater.update(vkd, device);

    auto vtxshader = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"));
    auto frgshader = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"));

    const vk::VkPipelineVertexInputStateCreateInfo vertexInputState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType
        nullptr, // const void*                                 pNext
        0u,      // VkPipelineVertexInputStateCreateFlags       flags
        0u,      // uint32_t                                    vertexBindingDescriptionCount
        nullptr, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        0u,      // uint32_t                                    vertexAttributeDescriptionCount
        nullptr, // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    VkPipelineMultisampleStateCreateInfo multisampling = initVulkanStructure();
    multisampling.sampleShadingEnable                  = VK_FALSE;
    multisampling.rasterizationSamples                 = VK_SAMPLE_COUNT_4_BIT;
    multisampling.minSampleShading                     = 1.0f;     // Optional
    multisampling.pSampleMask                          = NULL;     // Optional
    multisampling.alphaToCoverageEnable                = VK_FALSE; // Optional
    multisampling.alphaToOneEnable                     = VK_FALSE; // Optional

    static const VkPipelineColorBlendStateCreateInfo colorBlendState{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
        nullptr,                 // const void*                                    pNext
        0u,                      // VkPipelineColorBlendStateCreateFlags            flags
        VK_FALSE,                // VkBool32                                        logicOpEnable
        VK_LOGIC_OP_CLEAR,       // VkLogicOp                                    logicOp
        0u,                      // uint32_t                                        attachmentCount
        nullptr,                 // const VkPipelineColorBlendAttachmentState*    pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f} // float                                        blendConstants[4]
    };

    pipeline1.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewports, scissors, graphicsPipelineLayout, VK_NULL_HANDLE, 0u, vtxshader)
        .setupFragmentShaderState(graphicsPipelineLayout, VK_NULL_HANDLE, 0u, frgshader)
        .setupFragmentOutputState(VK_NULL_HANDLE, 0u, &colorBlendState, &multisampling)
        .setMonolithicPipelineLayout(graphicsPipelineLayout)
        .buildPipeline();

    auto commandPool   = createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, qIndex);
    auto commandBuffer = vk::allocateCommandBuffer(vkd, device, commandPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const auto clearValueColor = vk::makeClearValueColor(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    const vk::VkRect2D renderArea = {{0u, 0u}, {imageExtent.width, imageExtent.height}};

    const vk::VkRenderingAttachmentInfoKHR colorAttachments = {
        vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        colorAttachmentView.get(),                           // VkImageView imageView;
        vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,          // VkImageLayout imageLayout;
        vk::VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                      // VkImageView resolveImageView;
        vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,          // VkImageLayout resolveImageLayout;
        vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp loadOp;
        vk::VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
        clearValueColor                                      // VkClearValue clearValue;
    };
    const VkRenderingInfoKHR render_info = {
        VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, 0, 0, renderArea, 1, 0, 1, &colorAttachments, nullptr, nullptr};

    vk::beginCommandBuffer(vkd, commandBuffer.get());
    vk::VkImageMemoryBarrier initialBarrier =
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, (*colorAttachment).get(), subresourceRange);
    vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0, nullptr, 0, nullptr, 1,
                           &initialBarrier);
    vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0u, 1,
                              &descriptorSetBuffer.get(), 0u, nullptr);

    vkd.cmdBeginRendering(*commandBuffer, &render_info);
    pipeline1.bind(commandBuffer.get());
    vkd.cmdDraw(commandBuffer.get(), 6, 1, 0, 0);
    vkd.cmdEndRendering(*commandBuffer);

    const VkBufferMemoryBarrier indexBufferBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, index.get(), 0ull, sizeof(uint32_t));
    vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0,
                           nullptr, 1, &indexBufferBarrier, 0, nullptr);

    const VkBufferMemoryBarrier valueBufferBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, values.get(), 0ull, bufferSize);
    vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0,
                           nullptr, 1, &valueBufferBarrier, 0, nullptr);

    vk::endCommandBuffer(vkd, commandBuffer.get());
    vk::submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), commandBuffer.get());
}

tcu::TestStatus PipelineLibraryInterpolateAtSampleTestInstance::iterate(void)
{
    const auto &vkd   = m_context.getDeviceInterface();
    const auto device = m_context.getDevice();
    auto &alloc       = m_context.getDefaultAllocator();

    struct ValueBuffer
    {
        float values[PipelineLibraryInterpolateAtSampleTestCase::ResultCount];
    };

    size_t resultSize = PipelineLibraryInterpolateAtSampleTestCase::ResultCount;

    const auto indexBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto valueBufferSize = static_cast<VkDeviceSize>(sizeof(ValueBuffer));

    auto indexCreateInfo  = makeBufferCreateInfo(indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto valuesCreateInfo = makeBufferCreateInfo(valueBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    BufferWithMemory indexBufferMonolithic(vkd, device, alloc, indexCreateInfo, MemoryRequirement::HostVisible);
    BufferWithMemory valuesBufferMonolithic(vkd, device, alloc, valuesCreateInfo, MemoryRequirement::HostVisible);
    BufferWithMemory indexBufferGPL(vkd, device, alloc, indexCreateInfo, MemoryRequirement::HostVisible);
    BufferWithMemory valuesBufferGPL(vkd, device, alloc, valuesCreateInfo, MemoryRequirement::HostVisible);

    auto &indexBufferMonolithicAlloc  = indexBufferMonolithic.getAllocation();
    auto &valuesBufferMonolithicAlloc = valuesBufferMonolithic.getAllocation();
    auto &indexBufferGPLAlloc         = indexBufferGPL.getAllocation();
    auto &valuesBufferGPLAlloc        = valuesBufferGPL.getAllocation();

    void *indexBufferMonolithicData  = indexBufferMonolithicAlloc.getHostPtr();
    void *valuesBufferMonolithicData = valuesBufferMonolithicAlloc.getHostPtr();
    void *indexBufferGPLData         = indexBufferGPLAlloc.getHostPtr();
    void *valuesBufferGPLData        = valuesBufferGPLAlloc.getHostPtr();

    deMemset(indexBufferMonolithicData, 0, sizeof(uint32_t));
    deMemset(valuesBufferMonolithicData, 0, sizeof(ValueBuffer));
    deMemset(indexBufferGPLData, 0, sizeof(uint32_t));
    deMemset(valuesBufferGPLData, 0, sizeof(ValueBuffer));

    flushAlloc(vkd, device, indexBufferMonolithicAlloc);
    flushAlloc(vkd, device, valuesBufferMonolithicAlloc);
    flushAlloc(vkd, device, indexBufferGPLAlloc);
    flushAlloc(vkd, device, valuesBufferGPLAlloc);

    runTest(indexBufferMonolithic, valuesBufferMonolithic, valueBufferSize, vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
    runTest(indexBufferGPL, valuesBufferGPL, valueBufferSize, vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY);

    invalidateAlloc(vkd, device, indexBufferMonolithicAlloc);
    invalidateAlloc(vkd, device, valuesBufferMonolithicAlloc);
    invalidateAlloc(vkd, device, indexBufferGPLAlloc);
    invalidateAlloc(vkd, device, valuesBufferGPLAlloc);

    uint32_t monolithicIndex;
    uint32_t GPLIndex;
    struct ValueBuffer monolithicResult = ValueBuffer();
    struct ValueBuffer GPLResult        = ValueBuffer();
    memcpy((void *)&monolithicIndex, indexBufferMonolithicData, sizeof(uint32_t));
    memcpy((void *)&GPLIndex, indexBufferGPLData, sizeof(uint32_t));
    memcpy((void *)&monolithicResult, valuesBufferMonolithicData, sizeof(ValueBuffer));
    memcpy((void *)&GPLResult, valuesBufferGPLData, sizeof(ValueBuffer));

    //we can't know which order the shaders will run in
    std::sort(monolithicResult.values, monolithicResult.values + resultSize);
    std::sort(GPLResult.values, GPLResult.values + resultSize);

    //check that the atomic counters are at enough for the number of invocations
    constexpr int expected = (PipelineLibraryInterpolateAtSampleTestCase::sampleCount + 1) *
                             PipelineLibraryInterpolateAtSampleTestCase::width *
                             PipelineLibraryInterpolateAtSampleTestCase::height;

    if (monolithicIndex < expected && GPLIndex < expected)
    {
        return tcu::TestStatus::fail("Atomic counter value lower than expected");
    }

    for (uint32_t i = 1; i < PipelineLibraryInterpolateAtSampleTestCase::ResultCount; i++)
    {
        if (monolithicResult.values[i] != monolithicResult.values[i])
        {
            return tcu::TestStatus::fail("Comparison failed");
        }
    }

    return tcu::TestStatus::pass("Pass");
}
#endif

struct BindingTestConfig
{
    PipelineConstructionType construction;
    bool backwardsBinding;
    bool holes;
};

/*
 * Test the following behaviours:
 * Descriptor sets updated/bound in backwards order
 * Descriptor sets with index holes updated/bound/used
 */
class PipelineLayoutBindingTestCases : public vkt::TestCase
{
public:
    PipelineLayoutBindingTestCases(tcu::TestContext &testCtx, const std::string &name, const BindingTestConfig &config)
        : vkt::TestCase(testCtx, name)
        , m_config(config)
    {
    }
    ~PipelineLayoutBindingTestCases(void)
    {
    }
    void initPrograms(SourceCollections &programCollection) const override;
    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override;

    const BindingTestConfig m_config;
};

class PipelineLayoutBindingTestInstance : public vkt::TestInstance
{
public:
    PipelineLayoutBindingTestInstance(Context &context, const BindingTestConfig &config)
        : vkt::TestInstance(context)
        , m_renderSize(2, 2)
        , m_extent(makeExtent3D(m_renderSize.x(), m_renderSize.y(), 1u))
        , m_format(VK_FORMAT_R8G8B8A8_UNORM)
        , m_graphicsPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                             context.getDevice(), context.getDeviceExtensions(), config.construction)
        , m_config(config)
    {
    }
    ~PipelineLayoutBindingTestInstance(void)
    {
    }
    tcu::TestStatus iterate(void) override;

private:
    const tcu::UVec2 m_renderSize;
    const VkExtent3D m_extent;
    const VkFormat m_format;
    GraphicsPipelineWrapper m_graphicsPipeline;
    const BindingTestConfig m_config;
};

TestInstance *PipelineLayoutBindingTestCases::createInstance(Context &context) const
{
    return new PipelineLayoutBindingTestInstance(context, m_config);
}

void PipelineLayoutBindingTestCases::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_config.construction);
}

void PipelineLayoutBindingTestCases::initPrograms(SourceCollections &sources) const
{
    std::ostringstream src;
    src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
        << "vec2 positions[3] = vec2[](\n"
        << "        vec2(-1.0, -1.0),"
        << "        vec2(3.0, -1.0),"
        << "        vec2(-1.0, 3.0)"
        << ");\n"
        << "void main() {\n"
        << "        gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
        << "}";
    sources.glslSources.add("vert") << glu::VertexSource(src.str());

    std::ostringstream frag;
    frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout(set = 0, binding = 0) uniform Output0 {"
         << "    uint data;"
         << "} buf0;\n";
    if (!m_config.holes)
    {
        frag << "layout(set = 1, binding = 0) uniform Output1 {"
             << "    uint data;"
             << "} buf1;\n"
             << "layout(set = 2, binding = 0) uniform Output2 {"
             << "    uint data;"
             << "} buf2;\n"
             << "\n";
    }
    frag << "layout(set = 3, binding = 0) uniform Output3 {"
         << "    uint data;"
         << "} buf3;\n"
         << "void main ()\n"
         << "{\n"
         << "    const vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
         << "    const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n";
    if (!m_config.holes)
    {
        frag << "    outColor = ((buf0.data == 0) && (buf1.data == 1) && (buf2.data == 2) && (buf3.data == 3)) ? green "
                ": red;\n";
    }
    else
    {
        frag << "    outColor = ((buf0.data == 0) && (buf3.data == 3)) ? green : red;\n";
    }
    frag << "}\n";
    sources.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus PipelineLayoutBindingTestInstance::iterate()
{
    const auto &vkd             = m_context.getDeviceInterface();
    const auto device           = m_context.getDevice();
    auto &alloc                 = m_context.getDefaultAllocator();
    const auto qIndex           = m_context.getUniversalQueueFamilyIndex();
    const auto queue            = m_context.getUniversalQueue();
    const auto tcuFormat        = mapVkFormat(m_format);
    const auto colorUsage       = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto verifBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Color attachment.
    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        m_format,                            // VkFormat format;
        m_extent,                            // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        colorUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory colorBuffer(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto colorBufferView =
        makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, m_format, colorSRR);

    // Verification buffer.
    const auto verifBufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat)) * m_extent.width * m_extent.height;
    const auto verifBufferInfo = makeBufferCreateInfo(verifBufferSize, verifBufferUsage);
    BufferWithMemory verifBuffer(vkd, device, alloc, verifBufferInfo, MemoryRequirement::HostVisible);
    auto &verifBufferAlloc = verifBuffer.getAllocation();

    // Render pass and framebuffer.
    RenderPassWrapper renderPass(m_config.construction, vkd, device, m_format);
    renderPass.createFramebuffer(vkd, device, colorBuffer.get(), colorBufferView.get(), m_extent.width,
                                 m_extent.height);

    // Shader modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(vkd, device, binaries.get("vert"));
    const auto fragModule = ShaderWrapper(vkd, device, binaries.get("frag"));

    // Viewports and scissors.
    const std::vector<VkViewport> viewports(1u, makeViewport(m_extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(m_extent));

    const VkPipelineVertexInputStateCreateInfo vertexInputState     = initVulkanStructure();
    const VkPipelineRasterizationStateCreateInfo rasterizationState = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                          sType;
        nullptr,                                                    // const void*                              pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags  flags;
        VK_FALSE,                // VkBool32                                 depthClampEnable;
        VK_FALSE,                // VkBool32                                 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,    // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,       // VkCullModeFlags cullMode;
        VK_FRONT_FACE_CLOCKWISE, // VkFrontFace frontFace;
        VK_FALSE,                // VkBool32 depthBiasEnable;
        0.0f,                    // float depthBiasConstantFactor;
        0.0f,                    // float depthBiasClamp;
        0.0f,                    // float depthBiasSlopeFactor;
        1.0f,                    // float lineWidth;
    };

    std::array<int, 4> tmpIndices = {};
    std::array<int, 4> indices    = {};
    std::iota(tmpIndices.begin(), tmpIndices.end(), 0);
    if (m_config.backwardsBinding)
    {
        std::copy(tmpIndices.rbegin(), tmpIndices.rend(), indices.begin());
    }
    else
    {
        std::copy(tmpIndices.begin(), tmpIndices.end(), indices.begin());
    }

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

    std::vector<vk::Move<VkDescriptorSetLayout>> descriptorSetLayouts = {};

    for (size_t i = 0; i < indices.size(); i++)
    {
        descriptorSetLayouts.emplace_back(layoutBuilder.build(vkd, device));
    }

    // Pipeline layout and graphics pipeline.
    uint32_t setAndDescriptorCount = de::sizeU32(indices);
    const vk::PipelineLayoutWrapper pipelineLayout(m_config.construction, vkd, device, descriptorSetLayouts);
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setAndDescriptorCount);
    const auto descriptorPool =
        poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, setAndDescriptorCount);
    std::vector<vk::Move<VkDescriptorSet>> descriptorSetsWrap = {};
    std::vector<VkDescriptorSet> descriptorSets               = {};

    for (const auto &setLayout : descriptorSetLayouts)
    {
        descriptorSetsWrap.emplace_back(makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get()));
    }

    for (size_t i = 0; i < indices.size(); i++)
    {
        descriptorSets.emplace_back(descriptorSetsWrap[i].get());
    }

    const auto bufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    std::vector<std::unique_ptr<BufferWithMemory>> buffers;
    //create uniform buffers
    for (size_t i = 0; i < indices.size(); i++)
    {
        auto outBufferInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        auto buffer        = std::unique_ptr<vk::BufferWithMemory>(
            new vk::BufferWithMemory(vkd, device, alloc, outBufferInfo, vk::MemoryRequirement::HostVisible));
        auto &bufferAlloc    = buffer->getAllocation();
        uint32_t *bufferData = (uint32_t *)bufferAlloc.getHostPtr();
        *bufferData          = (uint32_t)i;
        flushAlloc(vkd, device, bufferAlloc);
        buffers.push_back(std::move(buffer));
    }

    DescriptorSetUpdateBuilder updater;

    for (auto i : indices)
    {
        const auto bufferInfo = makeDescriptorBufferInfo(buffers[i]->get(), 0ull, bufferSize);
        updater.writeSingle(descriptorSets[i], DescriptorSetUpdateBuilder::Location::binding(0),
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo);
        updater.update(vkd, device);
    }

    const auto topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_graphicsPipeline.setDefaultRasterizationState()
        .setDefaultTopology(topology)
        .setupVertexInputState(&vertexInputState)
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule,
                                          &rasterizationState)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
        .setupFragmentOutputState(*renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Draw.
    renderPass.begin(vkd, cmdBuffer, scissors.at(0u), clearColor);
    for (auto i : indices)
    {
        if (m_config.holes && ((i == 1) || (i == 2)))
        {
            continue;
        }
        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), i, 1,
                                  &descriptorSets[i], 0u, nullptr);
    }
    m_graphicsPipeline.bind(cmdBuffer);
    vkd.cmdDraw(cmdBuffer, 3, 1u, 0u, 0u);
    renderPass.end(vkd, cmdBuffer);

    // Copy to verification buffer.
    const auto copyRegion     = makeBufferImageCopy(m_extent, colorSRL);
    const auto transfer2Host  = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    const auto color2Transfer = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.get(), colorSRR);

    cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, &color2Transfer);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer.get(), 1u,
                             &copyRegion);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &transfer2Host);

    endCommandBuffer(vkd, cmdBuffer);

    // Submit and validate result.
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    const tcu::IVec3 iExtent(static_cast<int>(m_extent.width), static_cast<int>(m_extent.height),
                             static_cast<int>(m_extent.depth));
    void *verifBufferData = verifBufferAlloc.getHostPtr();
    const tcu::ConstPixelBufferAccess verifAccess(tcuFormat, iExtent, verifBufferData);
    invalidateAlloc(vkd, device, verifBufferAlloc);

    const auto green = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    tcu::TextureLevel referenceLevel(mapVkFormat(m_format), m_extent.height, m_extent.height);
    tcu::PixelBufferAccess reference = referenceLevel.getAccess();
    tcu::clear(reference, green);

    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", reference,
                                    verifAccess, tcu::Vec4(0.0), tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Image comparison failed");

    return tcu::TestStatus::pass("Pass");
}

void initCompatibleRenderPassPrograms(SourceCollections &dst, PipelineConstructionType)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "vec2 positions[] = vec2[](\n"
         << "    vec2(-1.0, -1.0),\n"
         << "    vec2( 3.0, -1.0),\n"
         << "    vec2(-1.0,  3.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void checkCompatibleRenderPassSupport(Context &context, PipelineConstructionType pipelineConstructionType)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    checkPipelineConstructionRequirements(vki, physicalDevice, pipelineConstructionType);
}

tcu::TestStatus compatibleRenderPassTest(Context &context, PipelineConstructionType pipelineConstructionType)
{
    const auto &ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(1, 1, 1);
    const auto vkExtent  = makeExtent3D(fbExtent);
    const auto fbFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat = mapVkFormat(fbFormat);
    const auto fbUsage   = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader.
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.

    // Color buffer with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    const PipelineLayoutWrapper pipelineLayout(pipelineConstructionType, ctx.vkd, ctx.device);
    auto renderPass         = makeRenderPass(ctx.vkd, ctx.device, fbFormat);
    const auto compatibleRP = makeRenderPass(ctx.vkd, ctx.device, fbFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries = context.getBinaryCollection();
    const ShaderWrapper vertModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const ShaderWrapper fragModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    // Empty vertex input state.
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    GraphicsPipelineWrapper pipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                            context.getDeviceExtensions(), pipelineConstructionType);

    pipelineWrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setDefaultDepthStencilState()
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
        .setupFragmentOutputState(*renderPass);

    // Important: at this point, the 4 libraries should have been created. Now we destroy the original render pass to make sure it's
    // no longer used, and use the compatible one for the remainder of the test.
    renderPass = Move<VkRenderPass>();

    // Finally, we link the complete pipeline and use the compatible render pass in the command buffer.
    DE_ASSERT(isConstructionTypeLibrary(pipelineConstructionType));
    pipelineWrapper.setMonolithicPipelineLayout(pipelineLayout).buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    beginRenderPass(ctx.vkd, cmdBuffer, *compatibleRP, *framebuffer, scissors.at(0u), clearColor);
    pipelineWrapper.bind(cmdBuffer);
    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color output.
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, geomColor);

    auto &log = context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

void initArrayOfStructsInterfacePrograms(SourceCollections &dst, PipelineConstructionType)
{
    // the purpose of tests is to check that sending struct between shaders stages does not crash the driver
    dst.glslSources.add("vert") << glu::VertexSource("#version 450\n"
                                                     "struct R { vec4 rgba; };\n"
                                                     "layout(location = 0) out R outColor[3];\n"
                                                     "void main (void)\n"
                                                     "{\n"
                                                     "  outColor[0].rgba = vec4(0.0, 0.9, 0.0, 1.0);\n"
                                                     "  outColor[1].rgba = vec4(0.3, 0.0, 0.0, 1.0);\n"
                                                     "  outColor[2].rgba = vec4(0.0, 0.0, 0.6, 1.0);\n"
                                                     "  const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
                                                     "  const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
                                                     "  gl_Position = vec4(x, y, 0.6, 1.0);\n"
                                                     "}\n");

    dst.glslSources.add("frag") << glu::FragmentSource(
        "#version 450\n"
        "struct R { vec4 rgba; };\n"
        "layout(location = 0) in R inColor[3];\n"
        "layout(location = 0) out vec4 color;\n"
        "void main() {\n"
        "    color = inColor[2].rgba + inColor[1].rgba + inColor[0].rgba;\n"
        "}\n");
}

void checkArrayOfStructsInterfaceSupport(Context &context, PipelineConstructionType pipelineConstructionType)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    checkPipelineConstructionRequirements(vki, physicalDevice, pipelineConstructionType);
}

tcu::TestStatus arrayOfStructsInterfaceTest(Context &context, PipelineConstructionType pipelineConstructionType)
{
    const DeviceInterface &vk             = context.getDeviceInterface();
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkDevice device                 = context.getDevice();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    Allocator &memAlloc                   = context.getDefaultAllocator();
    const uint32_t queueFamilyIndex       = context.getUniversalQueueFamilyIndex();
    const tcu::IVec3 fbExtent(4, 4, 1);
    const auto vkExtent  = makeExtent3D(fbExtent);
    const auto fbFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat = mapVkFormat(fbFormat);
    const auto fbUsage   = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.00f, 0.00f, 0.00f, 1.0f);
    const tcu::Vec4 expecColor(0.30f, 0.90f, 0.60f, 1.0f);
    const tcu::Vec4 threshold(0.02f, 0.02f, 0.02f, 0.0f);

    // Color buffer with verification buffer
    ImageWithBuffer colorBuffer(vk, device, memAlloc, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);
    RenderPassWrapper renderPass(pipelineConstructionType, vk, device, fbFormat);
    renderPass.createFramebuffer(vk, device, colorBuffer.getImage(), colorBuffer.getImageView(), fbExtent.x(),
                                 fbExtent.y());

    const auto &binaries(context.getBinaryCollection());
    const ShaderWrapper vertModule(vk, device, binaries.get("vert"));
    const ShaderWrapper fragModule(vk, device, binaries.get("frag"));
    const PipelineLayoutWrapper pipelineLayout(pipelineConstructionType, vk, device);

    const std::vector<VkViewport> viewports{makeViewport(vkExtent)};
    const std::vector<VkRect2D> scissors{makeRect2D(vkExtent)};

    VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    GraphicsPipelineWrapper pipelineWrapper(vki, vk, physicalDevice, device, context.getDeviceExtensions(),
                                            pipelineConstructionType);
    pipelineWrapper.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setDefaultDepthStencilState()
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
        .setupFragmentOutputState(*renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    CommandPoolWithBuffer cmd(vk, device, queueFamilyIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(vk, cmdBuffer);

    renderPass.begin(vk, cmdBuffer, scissors.at(0u), clearColor);
    pipelineWrapper.bind(cmdBuffer);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
    renderPass.end(vk, cmdBuffer);

    copyImageToBuffer(vk, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    endCommandBuffer(vk, cmdBuffer);
    submitCommandsAndWait(vk, device, context.getUniversalQueue(), cmdBuffer);

    invalidateAlloc(vk, device, colorBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());
    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, expecColor);

    auto &log = context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

#ifndef CTS_USES_VULKANSC
struct VaryingSamplesFragParams
{
    const PipelineConstructionType constructionType;
    const VkSampleCountFlagBits multiSampleCount;
};

void initVaryingSamplesFragPrograms(SourceCollections &dst, VaryingSamplesFragParams)
{
    // The framebuffer will contain a single pixel and we will draw a quad using the 4 pixel corners. inSamplePos will contain 0s
    // and 1s in the X and Y values so that the value at each corner will match its corresponding sample location. The result is
    // that interpolating outSamplePos for a sample will give you the corresponding standard sample location.
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=1) in vec4 inSamplePos;\n"
         << "layout (location=0) out vec2 outSamplePos;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    outSamplePos = inSamplePos.xy;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    // Each frag shader invocation will interpolate the sample position for every sample, and will store the results of every
    // interpolation in the positions buffer. So if we work with 4 samples but get 2 actual invocations (e.g.):
    // - sampleCount from the push constants will be 4.
    // - mySampleId will end up containing 2.
    // - samplePositions will have 2 blocks of 4 results each, with the 4 interpolations for the first and second invocations.
    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) in vec2 inSamplePos;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { int sampleCount; } pc;\n"
         << "layout (set=0, binding=0, std430) buffer MySampleIdBlock { int mySampleId; } atomicBuffer;\n"
         << "layout (set=0, binding=1, std430) buffer SamplePositionsBlock { vec2 samplePositions[]; } "
            "positionsBuffer;\n"
         << "void main (void) {\n"
         << "    const int sampleId = atomicAdd(atomicBuffer.mySampleId, 1);\n"
         << "    memoryBarrier();\n"
         << "    const int bufferOffset = pc.sampleCount * sampleId;\n"
         << "    for (int idx = 0; idx < pc.sampleCount; ++idx) {\n"
         << "        positionsBuffer.samplePositions[bufferOffset + idx] = interpolateAtSample(inSamplePos, idx);\n"
         << "    }\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void checkVaryingSamplesFragSupport(Context &context, VaryingSamplesFragParams params)
{
    const auto ctx = context.getContextCommonData();

    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, params.constructionType);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
    context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    // Check sample count support.
    const auto allowedSampleCounts = context.getDeviceProperties().limits.framebufferNoAttachmentsSampleCounts;
    if ((allowedSampleCounts & params.multiSampleCount) == 0u)
        TCU_THROW(NotSupportedError, "Requested sample count not supported");

    // Check for standard sample locations.
    if (!context.getDeviceProperties().limits.standardSampleLocations)
        TCU_THROW(NotSupportedError, "Standard sample locations required");
}

// This test creates a fragment shader pipeline library using a fragment shader that doesn't have sample shading enabled. In
// addition, thanks to using dynamic rendering, no multisample information is included when creating such library. Then, the library
// is included in two final pipelines: in one of them the multisample information indicates single-sample and, in the other one, it
// indicates multisample.
//
// Then, the test runs two render loops: one for the single-sample pipeline and one for the multisample one. We expect that the
// fragment shader produces the right results in both cases, even if the amount of samples was not available when the fragment
// shader pipeline library was created.
//
// The fragment shader has been written in a way such that, when used with a single-pixel framebuffer, each invocation writes the
// pixel locations of all available samples to an output buffer (note: so if 4 samples result in 4 invocations, we end up with a
// maximum of 16 sample locations in the buffer). See the frag shader above.
tcu::TestStatus varyingSamplesFragTest(Context &context, VaryingSamplesFragParams params)
{
    const auto &ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(1, 1, 1);
    const auto &vkExtent    = makeExtent3D(fbExtent);
    const auto descType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto bindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto dataStages   = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto kBufferCount = 2u; // Matches frag shader: atomic buffer and positions buffer.
    const bool isOptimized  = (params.constructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY);

    struct PositionSampleCoords
    {
        const tcu::Vec4 position;
        const tcu::Vec4 sampleCoords;
    };

    // Vertices.
    const std::vector<PositionSampleCoords> vertices{
        PositionSampleCoords{tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f)},
        PositionSampleCoords{tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 0.0f)},
        PositionSampleCoords{tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f)},
        PositionSampleCoords{tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f)},
    };

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vbAlloc); // strictly speaking, not needed.

    // Storage buffers used in the fragment shader: atomic buffer and positions buffer.
    int32_t invocationCount = 0;
    const auto abSize       = static_cast<VkDeviceSize>(sizeof(invocationCount));
    const auto abInfo       = makeBufferCreateInfo(abSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory atomicBuffer(ctx.vkd, ctx.device, ctx.allocator, abInfo, MemoryRequirement::HostVisible);
    const auto abAlloc  = atomicBuffer.getAllocation();
    void *abData        = abAlloc.getHostPtr();
    const auto abOffset = static_cast<VkDeviceSize>(0);

    const auto maxPositions = params.multiSampleCount * params.multiSampleCount;
    std::vector<tcu::Vec2> samplePositions(maxPositions, tcu::Vec2(-1.0f, -1.0f));
    const auto pbSize = static_cast<VkDeviceSize>(de::dataSize(samplePositions));
    const auto pbInfo = makeBufferCreateInfo(pbSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory positionsBuffer(ctx.vkd, ctx.device, ctx.allocator, pbInfo, MemoryRequirement::HostVisible);
    const auto pbAlloc  = positionsBuffer.getAllocation();
    void *pbData        = pbAlloc.getHostPtr();
    const auto pbOffset = static_cast<VkDeviceSize>(0);

    // Descriptor pool, set, layout, etc.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, kBufferCount);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder layoutBuilder;
    for (uint32_t i = 0u; i < kBufferCount; ++i)
        layoutBuilder.addSingleBinding(descType, dataStages);
    const auto setLayout     = layoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder updateBuilder;
    const auto abDescInfo = makeDescriptorBufferInfo(atomicBuffer.get(), abOffset, abSize);
    const auto pbDescInfo = makeDescriptorBufferInfo(positionsBuffer.get(), pbOffset, pbSize);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType, &abDescInfo);
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), descType, &pbDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    // Push constants.
    const auto pcSize  = static_cast<uint32_t>(sizeof(int32_t));
    const auto pcRange = makePushConstantRange(dataStages, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Modules.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    // Pipeline state.

    const auto bindingDesc = makeVertexInputBindingDescription(0u, static_cast<uint32_t>(sizeof(PositionSampleCoords)),
                                                               VK_VERTEX_INPUT_RATE_VERTEX);

    const std::vector<VkVertexInputAttributeDescription> inputAttributes{
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(PositionSampleCoords, position))),
        makeVertexInputAttributeDescription(1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(PositionSampleCoords, sampleCoords))),
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &bindingDesc,                    // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        de::sizeU32(inputAttributes),    // uint32_t vertexAttributeDescriptionCount;
        de::dataOrNull(inputAttributes), // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                     // const void* pNext;
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags flags;
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,                        // VkPrimitiveTopology topology;
        VK_FALSE,                                                    // VkBool32 primitiveRestartEnable;
    };

    const VkPipelineViewportStateCreateInfo viewportStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        0u,                                                    // VkPipelineViewportStateCreateFlags flags;
        de::sizeU32(viewports),                                // uint32_t viewportCount;
        de::dataOrNull(viewports),                             // const VkViewport* pViewports;
        de::sizeU32(scissors),                                 // uint32_t scissorCount;
        de::dataOrNull(scissors),                              // const VkRect2D* pScissors;
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,                                       // VkPolygonMode polygonMode;
        VK_CULL_MODE_BACK_BIT,                                      // VkCullModeFlags cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f,                                                       // float lineWidth;
    };

    // We will use two pipelines: one will be single-sample and the other one will be multisample.
    VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        params.multiSampleCount,                                  // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = initVulkanStructure();

    const VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = initVulkanStructure();

    const VkPipelineRenderingCreateInfo renderingCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, // VkStructureType sType;
        nullptr,                                          // const void* pNext;
        0u,                                               // uint32_t viewMask;
        0u,                                               // uint32_t colorAttachmentCount;
        nullptr,                                          // const VkFormat* pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                              // VkFormat depthAttachmentFormat;
        VK_FORMAT_UNDEFINED,                              // VkFormat stencilAttachmentFormat;
    };

    // Create a library with the vertex input state and the pre-rasterization shader state.
    Move<VkPipeline> preFragLib;
    Move<VkPipeline> fragShaderLib;
    Move<VkPipeline> fragOutputLibMulti;
    Move<VkPipeline> fragOutputLibSingle;

    VkPipelineCreateFlags libCreationFlags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    VkPipelineCreateFlags linkFlags        = 0u;

    if (isOptimized)
    {
        libCreationFlags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
        linkFlags |= VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
    }

    // Vertex input state and pre-rasterization shader state library.
    {
        VkGraphicsPipelineLibraryCreateInfoEXT vertexInputLibInfo = initVulkanStructureConst(&renderingCreateInfo);
        vertexInputLibInfo.flags |= (VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
                                     VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);

        VkGraphicsPipelineCreateInfo preFragPipelineInfo = initVulkanStructure(&vertexInputLibInfo);
        preFragPipelineInfo.flags                        = libCreationFlags;
        preFragPipelineInfo.pVertexInputState            = &vertexInputStateInfo;
        preFragPipelineInfo.pInputAssemblyState          = &inputAssemblyStateInfo;

        preFragPipelineInfo.layout              = pipelineLayout.get();
        preFragPipelineInfo.pViewportState      = &viewportStateInfo;
        preFragPipelineInfo.pRasterizationState = &rasterizationStateInfo;

        const auto vertexStageInfo = makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertModule.get());

        preFragPipelineInfo.stageCount = 1u;
        preFragPipelineInfo.pStages    = &vertexStageInfo;

        preFragLib = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &preFragPipelineInfo);
    }

    // Fragment shader stage library. Note we skip including multisample information here.
    {
        VkGraphicsPipelineLibraryCreateInfoEXT fragShaderLibInfo = initVulkanStructureConst(&renderingCreateInfo);
        fragShaderLibInfo.flags |= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

        VkGraphicsPipelineCreateInfo fragShaderPipelineInfo = initVulkanStructure(&fragShaderLibInfo);
        fragShaderPipelineInfo.flags                        = libCreationFlags;
        fragShaderPipelineInfo.layout                       = pipelineLayout.get();
        fragShaderPipelineInfo.pDepthStencilState           = &depthStencilStateInfo;

        const auto fragStageInfo = makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragModule.get());

        fragShaderPipelineInfo.stageCount = 1u;
        fragShaderPipelineInfo.pStages    = &fragStageInfo;

        fragShaderLib = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &fragShaderPipelineInfo);
    }

    // Fragment output libraries.
    {
        VkGraphicsPipelineLibraryCreateInfoEXT fragOutputLibInfo = initVulkanStructureConst(&renderingCreateInfo);
        fragOutputLibInfo.flags |= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

        VkGraphicsPipelineCreateInfo fragOutputPipelineInfo = initVulkanStructure(&fragOutputLibInfo);
        fragOutputPipelineInfo.flags                        = libCreationFlags;
        fragOutputPipelineInfo.pColorBlendState             = &colorBlendStateInfo;
        fragOutputPipelineInfo.pMultisampleState            = &multisampleStateInfo;

        fragOutputLibMulti = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &fragOutputPipelineInfo);

        multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        fragOutputLibSingle = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &fragOutputPipelineInfo);
    }

    // Linked pipelines.
    Move<VkPipeline> pipelineMulti;
    Move<VkPipeline> pipelineSingle;

    {
        const std::vector<VkPipeline> libraryHandlesMulti{
            preFragLib.get(),
            fragShaderLib.get(),
            fragOutputLibMulti.get(),
        };

        VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo = initVulkanStructure();
        linkedPipelineLibraryInfo.libraryCount                   = de::sizeU32(libraryHandlesMulti);
        linkedPipelineLibraryInfo.pLibraries                     = de::dataOrNull(libraryHandlesMulti);

        VkGraphicsPipelineCreateInfo linkedPipelineInfo = initVulkanStructure(&linkedPipelineLibraryInfo);
        linkedPipelineInfo.flags                        = linkFlags;
        linkedPipelineInfo.layout                       = pipelineLayout.get();

        pipelineMulti = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &linkedPipelineInfo);
    }
    {
        const std::vector<VkPipeline> libraryHandlesSingle{
            preFragLib.get(),
            fragShaderLib.get(),
            fragOutputLibSingle.get(),
        };

        VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo = initVulkanStructure();
        linkedPipelineLibraryInfo.libraryCount                   = de::sizeU32(libraryHandlesSingle);
        linkedPipelineLibraryInfo.pLibraries                     = de::dataOrNull(libraryHandlesSingle);

        VkGraphicsPipelineCreateInfo linkedPipelineInfo = initVulkanStructure(&linkedPipelineLibraryInfo);
        linkedPipelineInfo.flags                        = linkFlags;
        linkedPipelineInfo.layout                       = pipelineLayout.get();

        pipelineSingle = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &linkedPipelineInfo);
    }

    // Standard sample locations
    using LocationsVec = std::vector<tcu::Vec2>;

    const LocationsVec locationSamples1{
        tcu::Vec2(0.5f, 0.5f),
    };

    const LocationsVec locationSamples2{
        tcu::Vec2(0.75f, 0.75f),
        tcu::Vec2(0.25f, 0.25f),
    };

    const LocationsVec locationSamples4{
        tcu::Vec2(0.375f, 0.125f),
        tcu::Vec2(0.875f, 0.375f),
        tcu::Vec2(0.125f, 0.625f),
        tcu::Vec2(0.625f, 0.875f),
    };

    const LocationsVec locationSamples8{
        tcu::Vec2(0.5625f, 0.3125f), tcu::Vec2(0.4375f, 0.6875f), tcu::Vec2(0.8125f, 0.5625f),
        tcu::Vec2(0.3125f, 0.1875f), tcu::Vec2(0.1875f, 0.8125f), tcu::Vec2(0.0625f, 0.4375f),
        tcu::Vec2(0.6875f, 0.9375f), tcu::Vec2(0.9375f, 0.0625f),
    };

    const LocationsVec locationSamples16{
        tcu::Vec2(0.5625f, 0.5625f), tcu::Vec2(0.4375f, 0.3125f), tcu::Vec2(0.3125f, 0.625f),
        tcu::Vec2(0.75f, 0.4375f),   tcu::Vec2(0.1875f, 0.375f),  tcu::Vec2(0.625f, 0.8125f),
        tcu::Vec2(0.8125f, 0.6875f), tcu::Vec2(0.6875f, 0.1875f), tcu::Vec2(0.375f, 0.875f),
        tcu::Vec2(0.5f, 0.0625f),    tcu::Vec2(0.25f, 0.125f),    tcu::Vec2(0.125f, 0.75f),
        tcu::Vec2(0.0f, 0.5f),       tcu::Vec2(0.9375f, 0.25f),   tcu::Vec2(0.875f, 0.9375f),
        tcu::Vec2(0.0625f, 0.0f),
    };

    const auto locationThreshold = 0.00001f;

    const std::map<VkSampleCountFlagBits, const LocationsVec *> locationsByCount{
        std::make_pair(VK_SAMPLE_COUNT_1_BIT, &locationSamples1),
        std::make_pair(VK_SAMPLE_COUNT_2_BIT, &locationSamples2),
        std::make_pair(VK_SAMPLE_COUNT_4_BIT, &locationSamples4),
        std::make_pair(VK_SAMPLE_COUNT_8_BIT, &locationSamples8),
        std::make_pair(VK_SAMPLE_COUNT_16_BIT, &locationSamples16),
    };

    const VkRenderingInfo renderingInfo = {
        VK_STRUCTURE_TYPE_RENDERING_INFO, // VkStructureType sType;
        nullptr,                          // const void* pNext;
        0u,                               // VkRenderingFlags flags;
        scissors.at(0u),                  // VkRect2D renderArea;
        1u,                               // uint32_t layerCount;
        0u,                               // uint32_t viewMask;
        0u,                               // uint32_t colorAttachmentCount;
        nullptr,                          // const VkRenderingAttachmentInfo* pColorAttachments;
        nullptr,                          // const VkRenderingAttachmentInfo* pDepthAttachment;
        nullptr,                          // const VkRenderingAttachmentInfo* pStencilAttachment;
    };

    const auto hostToFragBarrier =
        makeMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));
    const auto fragToHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

    for (const auto multiSample : {false, true})
    {
        // Reset data in buffers.
        invocationCount = 0;
        deMemset(de::dataOrNull(samplePositions), 0, de::dataSize(samplePositions));

        deMemcpy(abData, &invocationCount, sizeof(invocationCount));
        flushAlloc(ctx.vkd, ctx.device, abAlloc);

        deMemcpy(pbData, de::dataOrNull(samplePositions), de::dataSize(samplePositions));
        flushAlloc(ctx.vkd, ctx.device, pbAlloc);

        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
        const auto cmdBuffer = *cmd.cmdBuffer;

        const auto vkSampleCount = (multiSample ? params.multiSampleCount : VK_SAMPLE_COUNT_1_BIT);
        const auto sampleCount   = static_cast<int32_t>(vkSampleCount);

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 &hostToFragBarrier);
        ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, (multiSample ? *pipelineMulti : *pipelineSingle));
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, dataStages, 0u, pcSize, &sampleCount);
        ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
        ctx.vkd.cmdEndRendering(cmdBuffer);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &fragToHostBarrier);
        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

        // Verify buffer contents.
        invalidateAlloc(ctx.vkd, ctx.device, abAlloc);
        invalidateAlloc(ctx.vkd, ctx.device, pbAlloc);

        deMemcpy(&invocationCount, abData, sizeof(invocationCount));
        if (invocationCount <= 0 || invocationCount > sampleCount)
        {
            const auto prefix = (multiSample ? "[MultiSample]" : "[Single-Sample]");
            std::ostringstream msg;
            msg << prefix << " Invalid invocation count found in atomic buffer: expected value in range [1, "
                << sampleCount << "] but found " << invocationCount;
            TCU_FAIL(msg.str());
        }

        const auto itr = locationsByCount.find(vkSampleCount);
        DE_ASSERT(itr != locationsByCount.end());
        const auto expectedLocations = itr->second;

        deMemcpy(de::dataOrNull(samplePositions), pbData, de::dataSize(samplePositions));
        for (int32_t invocationIdx = 0; invocationIdx < invocationCount; ++invocationIdx)
        {
            DE_ASSERT(expectedLocations->size() == static_cast<size_t>(vkSampleCount));
            const auto bufferOffset = invocationIdx * sampleCount;
            for (int32_t sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
            {
                const auto &result   = samplePositions[bufferOffset + sampleIdx];
                const auto &expected = expectedLocations->at(sampleIdx);

                if (!tcu::boolAll(tcu::lessThanEqual(tcu::absDiff(result, expected),
                                                     tcu::Vec2(locationThreshold, locationThreshold))))
                {
                    const auto prefix = (multiSample ? "[MultiSample]" : "[Single-Sample]");
                    std::ostringstream msg;
                    msg << prefix << " Unexpected position found for invocation " << invocationIdx << " sample "
                        << sampleIdx << ": expected " << expected << " but found " << result;
                    TCU_FAIL(msg.str());
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class PipelineNoRenderingTestInstance : public vkt::TestInstance
{
public:
    PipelineNoRenderingTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                    const bool unusedAttachment)
        : vkt::TestInstance(context)
        , m_pipelineConstructionType(pipelineConstructionType)
        , m_unusedAttachment(unusedAttachment)
    {
    }
    ~PipelineNoRenderingTestInstance(void)
    {
    }
    tcu::TestStatus iterate(void) override;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const bool m_unusedAttachment;
};

tcu::TestStatus PipelineNoRenderingTestInstance::iterate()
{
    const auto &vki       = m_context.getInstanceInterface();
    const auto &vkd       = m_context.getDeviceInterface();
    const auto physDevice = m_context.getPhysicalDevice();
    const auto device     = m_context.getDevice();
    const auto qIndex     = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();
    Allocator &alloc      = m_context.getDefaultAllocator();

    const auto cmdPool   = makeCommandPool(vkd, device, qIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const std::vector<VkViewport> viewports(1u, makeViewport(32u, 32u));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(32u, 32u));

    const VkPipelineVertexInputStateCreateInfo vertexInputState     = initVulkanStructure();
    const VkPipelineRasterizationStateCreateInfo rasterizationState = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                          sType;
        nullptr,                                                    // const void*                              pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags  flags;
        VK_FALSE,                // VkBool32                                 depthClampEnable;
        VK_FALSE,                // VkBool32                                 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,    // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,       // VkCullModeFlags cullMode;
        VK_FRONT_FACE_CLOCKWISE, // VkFrontFace frontFace;
        VK_FALSE,                // VkBool32 depthBiasEnable;
        0.0f,                    // float depthBiasConstantFactor;
        0.0f,                    // float depthBiasClamp;
        0.0f,                    // float depthBiasSlopeFactor;
        1.0f,                    // float lineWidth;
    };

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = ShaderWrapper(vkd, device, binaries.get("vert"));
    const auto fragModule = ShaderWrapper(vkd, device, binaries.get("frag"));
    const PipelineLayoutWrapper pipelineLayout(m_pipelineConstructionType, vkd, device);

    vk::GraphicsPipelineWrapper pipeline(vki, vkd, physDevice, device, m_context.getDeviceExtensions(),
                                         m_pipelineConstructionType);
    pipeline.setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputState)
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, VK_NULL_HANDLE, 0u, vertModule,
                                          &rasterizationState)
        .setupFragmentShaderState(pipelineLayout, VK_NULL_HANDLE, 0u, fragModule)
        .setupFragmentOutputState(VK_NULL_HANDLE)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    auto imageExtent            = vk::makeExtent3D(32u, 32u, 1u);
    const auto subresourceRange = vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto imageUsage       = static_cast<vk::VkImageUsageFlags>(vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                               vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const vk::VkImageCreateInfo imageCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        0u,                                      // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
        imageExtent,                             // VkExtent3D extent;
        1u,                                      // uint32_t mipLevels;
        1u,                                      // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                              // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                      // uint32_t queueFamilyIndexCount;
        &qIndex,                                 // const uint32_t* pQueueFamilyIndices;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    auto colorAttachment = de::MovePtr<vk::ImageWithMemory>(
        new vk::ImageWithMemory(vkd, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any));
    auto colorAttachmentView = vk::makeImageView(vkd, device, colorAttachment->get(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                 imageCreateInfo.format, subresourceRange);

    const auto clearValueColor = vk::makeClearValueColor(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    const vk::VkRect2D renderArea = {{0u, 0u}, {imageExtent.width, imageExtent.height}};

    const vk::VkRenderingAttachmentInfoKHR colorAttachments = {
        vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        colorAttachmentView.get(),                           // VkImageView imageView;
        vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,          // VkImageLayout imageLayout;
        vk::VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                      // VkImageView resolveImageView;
        vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,          // VkImageLayout resolveImageLayout;
        vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp loadOp;
        vk::VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
        clearValueColor                                      // VkClearValue clearValue;
    };
    const uint32_t attachmentCount       = m_unusedAttachment ? 1u : 0u;
    const VkRenderingInfoKHR render_info = {VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
                                            0,
                                            0,
                                            renderArea,
                                            1,
                                            0,
                                            attachmentCount,
                                            &colorAttachments,
                                            nullptr,
                                            nullptr};

    beginCommandBuffer(vkd, *cmdBuffer);
    vkd.cmdBeginRendering(*cmdBuffer, &render_info);
    pipeline.bind(*cmdBuffer);
    vkd.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
    vkd.cmdEndRendering(*cmdBuffer);
    endCommandBuffer(vkd, *cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

    return tcu::TestStatus::pass("Pass");
}

class PipelineNoRenderingTestCase : public vkt::TestCase
{
public:
    PipelineNoRenderingTestCase(tcu::TestContext &testCtx, const std::string &name,
                                const PipelineConstructionType pipelineConstructionType, const bool unusedAttachment)
        : vkt::TestCase(testCtx, name)
        , m_pipelineConstructionType(pipelineConstructionType)
        , m_unusedAttachment(unusedAttachment)
    {
    }
    ~PipelineNoRenderingTestCase(void)
    {
    }
    void initPrograms(SourceCollections &programCollection) const override;
    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const bool m_unusedAttachment;
};

TestInstance *PipelineNoRenderingTestCase::createInstance(Context &context) const
{
    return new PipelineNoRenderingTestInstance(context, m_pipelineConstructionType, m_unusedAttachment);
}

void PipelineNoRenderingTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);

    if (m_unusedAttachment &&
        !context.getDynamicRenderingUnusedAttachmentsFeaturesEXT().dynamicRenderingUnusedAttachments)
        TCU_THROW(NotSupportedError, "dynamicRenderingUnusedAttachments");
}

void PipelineNoRenderingTestCase::initPrograms(SourceCollections &sources) const
{
    std::stringstream vert;
    std::stringstream frag;

    vert << "#version 450\n"
         << "void main() {\n"
         << "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
         << "    gl_Position = vec4(pos - 0.5f, 0.0f, 1.0f);\n"
         << "}\n";

    frag << "#version 450\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    outColor = vec4(1.0f);\n"
         << "}\n";

    sources.glslSources.add("vert") << glu::VertexSource(vert.str());
    sources.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

#endif // CTS_USES_VULKANSC

class IdenticallyDefinedLayoutTestInstance : public vkt::TestInstance
{
public:
    IdenticallyDefinedLayoutTestInstance(Context &context) : vkt::TestInstance(context)
    {
    }
    ~IdenticallyDefinedLayoutTestInstance(void)
    {
    }
    tcu::TestStatus iterate(void) override;
};

tcu::TestStatus IdenticallyDefinedLayoutTestInstance::iterate(void)
{
    const auto &vk                  = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = m_context.getUniversalQueue();
    auto &alloc                     = m_context.getDefaultAllocator();

    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageSubresourceLayers subresourceLayers =
        makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

    const uint32_t width  = 4u;
    const uint32_t height = 4u;

    VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                          // VkStructureType sType;
        nullptr,                                                      // const void* pNext;
        0u,                                                           // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                             // VkImageType imageType;
        VK_FORMAT_R8G8B8A8_UNORM,                                     // VkFormat format;
        {width, height, 1u},                                          // VkExtent3D extent;
        1u,                                                           // uint32_t mipLevels;
        1u,                                                           // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                        // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                      // VkImageTiling tiling;
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                    // VkSharingMode sharingMode;
        0u,                                                           // uint32_t queueFamilyIndexCount;
        nullptr,                                                      // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                    // VkImageLayout initialLayout;
    };
    ImageWithMemory sampledImage(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any);
    Move<VkImageView> sampledImageView =
        makeImageView(vk, device, *sampledImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, subresourceRange);
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageWithMemory colorImage(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any);
    Move<VkImageView> colorImageView =
        makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, subresourceRange);

    const uint32_t bufferSize               = width * height * 4u;
    de::MovePtr<BufferWithMemory> srcBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, alloc, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                             MemoryRequirement::HostVisible));
    de::MovePtr<BufferWithMemory> dstBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, alloc, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                             MemoryRequirement::HostVisible));

    uint8_t *srcData = reinterpret_cast<uint8_t *>(srcBuffer->getAllocation().getHostPtr());
    for (uint32_t i = 0; i < bufferSize; ++i)
        srcData[i] = (uint8_t)(i % 256);
    flushAlloc(vk, device, srcBuffer->getAllocation());

    Move<VkSampler> sampler2;
    Move<VkDescriptorSetLayout> descriptorSetLayout2;
    Move<VkPipeline> pipeline;

    RenderPassWrapper renderPass(vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device, VK_FORMAT_R8G8B8A8_UNORM);
    renderPass.createFramebuffer(vk, device, *colorImage, *colorImageView, width, height);

    {
        VkSamplerCreateInfo samplerParams = vk::initVulkanStructure();
        Move<VkSampler> sampler1          = createSampler(vk, device, &samplerParams);
        sampler2                          = createSampler(vk, device, &samplerParams);

        Move<VkDescriptorSetLayout> descriptorSetLayout1 =
            DescriptorSetLayoutBuilder()
                .addBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, vk::VK_SHADER_STAGE_FRAGMENT_BIT,
                            &*sampler1)
                .build(vk, device);
        descriptorSetLayout2 = DescriptorSetLayoutBuilder()
                                   .addBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u,
                                               vk::VK_SHADER_STAGE_FRAGMENT_BIT, &*sampler2)
                                   .build(vk, device);

        PipelineLayoutWrapper pipelineLayout1(vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device,
                                              *descriptorSetLayout1);

        const auto &binaries  = m_context.getBinaryCollection();
        const auto vertModule = ShaderWrapper(vk, device, binaries.get("vert"));
        const auto fragModule = ShaderWrapper(vk, device, binaries.get("frag"));

        const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
        const std::vector<VkViewport> viewports(1u, makeViewport(width, height));
        const std::vector<VkRect2D> scissors(1u, makeRect2D(width, height));

        VkPipelineCreateFlags createFlags = VK_PIPELINE_CREATE_FLAG_BITS_MAX_ENUM;
        void *pNext                       = nullptr;
#ifndef CTS_USES_VULKANSC
        VkPipelineCreateFlags2CreateInfo createFlags2 = vk::initVulkanStructure();
        pNext                                         = &createFlags2;
#endif
        pipeline = makeGraphicsPipeline(vk, device, *pipelineLayout1, vertModule.getModule(), VK_NULL_HANDLE,
                                        VK_NULL_HANDLE, VK_NULL_HANDLE, fragModule.getModule(), *renderPass, viewports,
                                        scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInputState,
                                        nullptr, nullptr, nullptr, nullptr, nullptr, pNext, createFlags);
    }
    PipelineLayoutWrapper pipelineLayout2(vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device, *descriptorSetLayout2);

    Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3u)
            .build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout2);

    const VkDescriptorImageInfo descriptorInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *sampledImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorInfo)
        .update(vk, device);

    const Move<VkCommandPool> cmdPool(
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Move<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    vk::beginCommandBuffer(vk, *cmdBuffer);
    {
        const auto preBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *sampledImage, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1u, &preBarrier);

        VkBufferImageCopy region = makeBufferImageCopy(imageCreateInfo.extent, subresourceLayers);
        vk.cmdCopyBufferToImage(*cmdBuffer, **srcBuffer, *sampledImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                                &region);

        const auto postBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *sampledImage, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1u, &postBarrier);
    }
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout2, 0u, 1u, &*descriptorSet, 0u,
                             nullptr);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    renderPass.begin(vk, *cmdBuffer, makeRect2D(width, height), tcu::Vec4(0.0f));
    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
    renderPass.end(vk, *cmdBuffer);
    {
        const auto preBarrier = makeImageMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorImage, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1u, &preBarrier);

        VkBufferImageCopy region = makeBufferImageCopy(imageCreateInfo.extent, subresourceLayers);
        vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **dstBuffer, 1u,
                                &region);
    }
    vk::endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, dstBuffer->getAllocation());

    uint8_t *dstData = reinterpret_cast<uint8_t *>(dstBuffer->getAllocation().getHostPtr());
    if (memcmp(srcData, dstData, bufferSize) != 0)
    {
        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class IdenticallyDefinedLayoutTestCases : public vkt::TestCase
{
public:
    IdenticallyDefinedLayoutTestCases(tcu::TestContext &testCtx, const std::string &name) : vkt::TestCase(testCtx, name)
    {
    }
    ~IdenticallyDefinedLayoutTestCases(void)
    {
    }
    void initPrograms(SourceCollections &programCollection) const override;
    void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new IdenticallyDefinedLayoutTestInstance(context);
    }
};

void IdenticallyDefinedLayoutTestCases::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream vert;
    std::stringstream frag;

    vert << "#version 450\n"
         << "layout(location = 0) out vec2 uv;\n"
         << "void main() {\n"
         << "    uv = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
         << "    gl_Position = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    frag << "#version 450\n"
         << "layout(location = 0) in vec2 uv;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (set=0, binding=0) uniform sampler2D tex;\n"
         << "void main() {\n"
         << "    outColor = texture(tex, uv);\n"
         << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}
void IdenticallyDefinedLayoutTestCases::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_maintenance4");
    context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

} // namespace

tcu::TestCaseGroup *createMiscTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "misc"));

    // Location of the Amber script files under the data/vulkan/amber source tree.
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        addMonolithicAmberTests(miscTests.get());

    // Verify implicit access to gl_PrimtiveID works
    miscTests->addChild(
        new ImplicitPrimitiveIDPassthroughCase(testCtx, "implicit_primitive_id", pipelineConstructionType, false));
    // Verify implicit access to gl_PrimtiveID works with a tessellation shader
    miscTests->addChild(new ImplicitPrimitiveIDPassthroughCase(testCtx, "implicit_primitive_id_with_tessellation",
                                                               pipelineConstructionType, true));

    if (isConstructionTypeLibrary(pipelineConstructionType))
    {
        addFunctionCaseWithPrograms(miscTests.get(), "compatible_render_pass", checkCompatibleRenderPassSupport,
                                    initCompatibleRenderPassPrograms, compatibleRenderPassTest,
                                    pipelineConstructionType);
    }
    addFunctionCaseWithPrograms(miscTests.get(), "array_of_structs_interface", checkArrayOfStructsInterfaceSupport,
                                initArrayOfStructsInterfacePrograms, arrayOfStructsInterfaceTest,
                                pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
    if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY)
    {
        // Check if interpolateAtSample works as expected when using a pipeline library and null MSAA state in the fragment shader"
        miscTests->addChild(
            new PipelineLibraryInterpolateAtSampleTestCase(testCtx, "interpolate_at_sample_no_sample_shading"));
    }

    if (isConstructionTypeLibrary(pipelineConstructionType))
    {
        const VkSampleCountFlagBits sampleCounts[] = {
            VK_SAMPLE_COUNT_2_BIT,
            VK_SAMPLE_COUNT_4_BIT,
            VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_16_BIT,
        };
        for (const auto sampleCount : sampleCounts)
        {
            const auto testName = "frag_lib_varying_samples_" + std::to_string(static_cast<int>(sampleCount));
            const VaryingSamplesFragParams params{pipelineConstructionType, sampleCount};

            addFunctionCaseWithPrograms(miscTests.get(), testName, checkVaryingSamplesFragSupport,
                                        initVaryingSamplesFragPrograms, varyingSamplesFragTest, params);
        }
    }
#endif // CTS_USES_VULKANSC

    BindingTestConfig config0 = {pipelineConstructionType, true, false};
    BindingTestConfig config1 = {pipelineConstructionType, false, true};
    BindingTestConfig config2 = {pipelineConstructionType, true, true};

    // Verify implicit access to gl_PrimtiveID works with a tessellation shader
    miscTests->addChild(new PipelineLayoutBindingTestCases(testCtx, "descriptor_bind_test_backwards", config0));
    // Verify implicit access to gl_PrimtiveID works with a tessellation shader
    miscTests->addChild(new PipelineLayoutBindingTestCases(testCtx, "descriptor_bind_test_holes", config1));
    // Verify implicit access to gl_PrimtiveID works with a tessellation shader
    miscTests->addChild(new PipelineLayoutBindingTestCases(testCtx, "descriptor_bind_test_backwards_holes", config2));

    // Verify maintenance4 identically defined pipeline layout
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        miscTests->addChild(new IdenticallyDefinedLayoutTestCases(testCtx, "identically_defined_layout"));

#ifndef CTS_USES_VULKANSC
    if (!isConstructionTypeShaderObject(pipelineConstructionType))
    {
        miscTests->addChild(new PipelineNoRenderingTestCase(testCtx, "no_rendering", pipelineConstructionType, false));
        miscTests->addChild(
            new PipelineNoRenderingTestCase(testCtx, "no_rendering_unused_attachment", pipelineConstructionType, true));
    }
#endif

    return miscTests.release();
}

} // namespace pipeline
} // namespace vkt
