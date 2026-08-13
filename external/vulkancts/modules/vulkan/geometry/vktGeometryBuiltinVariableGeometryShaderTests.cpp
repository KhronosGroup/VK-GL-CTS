/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 The Android Open Source Project
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
 * \brief Basic Geometry Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktGeometryBuiltinVariableGeometryShaderTests.hpp"
#include "vktGeometryBasicClass.hpp"
#include "vktGeometryTestsUtil.hpp"

#include "gluTextureUtil.hpp"
#include "glwEnums.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace geometry
{
namespace
{
using de::MovePtr;
using std::string;
using std::vector;
using tcu::TestCaseGroup;
using tcu::TestContext;
using tcu::TestStatus;

enum VariableTest
{
    TEST_POINT_SIZE = 0,
    TEST_PRIMITIVE_ID_IN,
    TEST_PRIMITIVE_ID,
    TEST_POSITION,
    TEST_LAST
};

class BuiltinVariableRenderTestInstance : public GeometryExpanderRenderTestInstance
{
public:
    BuiltinVariableRenderTestInstance(Context &context, const char *name, const VariableTest test,
                                      const bool indicesTest);
    void genVertexAttribData(void);
    void createIndicesBuffer(void);

protected:
    void drawCommand(const VkCommandBuffer &cmdBuffer);

private:
    const bool m_indicesTest;
    std::vector<uint16_t> m_indices;
    Move<vk::VkBuffer> m_indicesBuffer;
    MovePtr<Allocation> m_allocation;
};

BuiltinVariableRenderTestInstance::BuiltinVariableRenderTestInstance(Context &context, const char *name,
                                                                     const VariableTest test, const bool indicesTest)
    : GeometryExpanderRenderTestInstance(context,
                                         (test == TEST_PRIMITIVE_ID_IN) ? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP :
                                         (test == TEST_POSITION)        ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP :
                                                                          VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
                                         name)
    , m_indicesTest(indicesTest)
{
    genVertexAttribData();
}

void BuiltinVariableRenderTestInstance::genVertexAttribData(void)
{
    m_numDrawVertices = 5;

    m_vertexPosData.resize(m_numDrawVertices);
    m_vertexPosData[0] = tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f);
    m_vertexPosData[1] = tcu::Vec4(0.0f, 0.5f, 0.0f, 1.0f);
    m_vertexPosData[2] = tcu::Vec4(-0.7f, -0.1f, 0.0f, 1.0f);
    m_vertexPosData[3] = tcu::Vec4(-0.1f, -0.7f, 0.0f, 1.0f);
    m_vertexPosData[4] = tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f);

    m_vertexAttrData.resize(m_numDrawVertices);
    m_vertexAttrData[0] = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    m_vertexAttrData[1] = tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
    m_vertexAttrData[2] = tcu::Vec4(2.0f, 0.0f, 0.0f, 0.0f);
    m_vertexAttrData[3] = tcu::Vec4(3.0f, 0.0f, 0.0f, 0.0f);
    m_vertexAttrData[4] = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);

    if (m_indicesTest)
    {
        // Only used by primitive ID restart test
        m_indices.resize(m_numDrawVertices);
        m_indices[0] = 1;
        m_indices[1] = 4;
        m_indices[2] = 0xFFFF; // restart
        m_indices[3] = 2;
        m_indices[4] = 1;
        createIndicesBuffer();
    }
}

void BuiltinVariableRenderTestInstance::createIndicesBuffer(void)
{
    // Create vertex indices buffer
    const DeviceInterface &vk                  = m_context.getDeviceInterface();
    const VkDevice device                      = m_context.getDevice();
    Allocator &memAlloc                        = m_context.getDefaultAllocator();
    const VkDeviceSize indexBufferSize         = m_indices.size() * sizeof(uint16_t);
    const VkBufferCreateInfo indexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        0u,                                   // VkBufferCreateFlags flags;
        indexBufferSize,                      // VkDeviceSize size;
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,     // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        0u,                                   // uint32_t queueFamilyCount;
        nullptr                               // const uint32_t* pQueueFamilyIndices;
    };

    m_indicesBuffer = createBuffer(vk, device, &indexBufferParams);
    m_allocation =
        memAlloc.allocate(getBufferMemoryRequirements(vk, device, *m_indicesBuffer), MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(device, *m_indicesBuffer, m_allocation->getMemory(), m_allocation->getOffset()));
    // Load indices into buffer
    deMemcpy(m_allocation->getHostPtr(), &m_indices[0], (size_t)indexBufferSize);
    flushAlloc(vk, device, *m_allocation);
}

void BuiltinVariableRenderTestInstance::drawCommand(const VkCommandBuffer &cmdBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    if (m_indicesTest)
    {
        vk.cmdBindIndexBuffer(cmdBuffer, *m_indicesBuffer, 0, VK_INDEX_TYPE_UINT16);
        vk.cmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
    }
    else
        vk.cmdDraw(cmdBuffer, static_cast<uint32_t>(m_numDrawVertices), 1u, 0u, 0u);
}

class BuiltinVariableRenderTest : public TestCase
{
public:
    BuiltinVariableRenderTest(TestContext &testCtx, const char *name, const VariableTest test, const bool flag = false);
    void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    const VariableTest m_test;
    const bool m_flag;
};

BuiltinVariableRenderTest::BuiltinVariableRenderTest(TestContext &testCtx, const char *name, const VariableTest test,
                                                     const bool flag)
    : TestCase(testCtx, name)
    , m_test(test)
    , m_flag(flag)
{
}

void BuiltinVariableRenderTest::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_test == TEST_POINT_SIZE)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE);
}

void BuiltinVariableRenderTest::initPrograms(SourceCollections &sourceCollections) const
{
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "out gl_PerVertex\n"
            << " {\n"
            << "    vec4 gl_Position;\n"
            << "    float gl_PointSize;\n"
            << "};\n"
            << "layout(location = 0) in vec4 a_position;\n";
        switch (m_test)
        {
        case TEST_POINT_SIZE:
            src << "layout(location = 1) in vec4 a_pointSize;\n"
                << "layout(location = 0) out vec4 v_geom_pointSize;\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = a_position;\n"
                << "    gl_PointSize = 1.0;\n"
                << "    v_geom_pointSize = a_pointSize;\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID_IN:
            src << "void main (void)\n"
                << "{\n"
                << "    gl_Position = a_position;\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID:
            src << "layout(location = 1) in vec4 a_primitiveID;\n"
                << "layout(location = 0) out vec4 v_geom_primitiveID;\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = a_position;\n"
                << "    v_geom_primitiveID = a_primitiveID;\n"
                << "}\n";
            break;
        case TEST_POSITION:
            src << "layout(location = 0) out vec4 v_position;\n"
                << "void main (void)\n"
                << "{\n"
                << "    v_position = a_position;\n"
                << "    gl_Position = a_position;\n"
                << "}\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        sourceCollections.glslSources.add("vertex") << glu::VertexSource(src.str());
    }

    {
        std::ostringstream src;
        switch (m_test)
        {
        case TEST_POINT_SIZE:
            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "in gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "    float gl_PointSize;\n"
                << "} gl_in[];\n"
                << "out gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "    float gl_PointSize;\n"
                << "};\n"
                << "#extension GL_EXT_geometry_point_size : require\n"
                << "layout(points) in;\n"
                << "layout(points, max_vertices = 1) out;\n"
                << "layout(location = 0) in vec4 v_geom_pointSize[];\n"
                << "layout(location = 0) out vec4 v_frag_FragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = gl_in[0].gl_Position;\n"
                << "    gl_PointSize = v_geom_pointSize[0].x + 1.0;\n"
                << "    v_frag_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
                << "    EmitVertex();\n"
                << "}\n";
            sourceCollections.glslSources.add("geometry") << glu::GeometrySource(src.str());
            break;
        case TEST_PRIMITIVE_ID_IN:
            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "in gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "    float gl_PointSize;\n"
                << "} gl_in[];\n"
                << "out gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "    float gl_PointSize;\n"
                << "};\n"
                << "layout(lines) in;\n"
                << "layout(triangle_strip, max_vertices = 10) out;\n"
                << "layout(location = 0) out vec4 v_frag_FragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    const vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
                << "    const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
                << "    const vec4 blue = vec4(0.0, 0.0, 1.0, 1.0);\n"
                << "    const vec4 yellow = vec4(1.0, 1.0, 0.0, 1.0);\n"
                << "    const vec4 colors[4] = vec4[4](red, green, blue, yellow);\n"
                << "    for (int counter = 0; counter < 3; ++counter)\n"
                << "    {\n"
                << "        float percent = 0.1 * counter;\n"
                << "        gl_Position = gl_in[0].gl_Position * vec4(1.0 + percent, 1.0 + percent, 1.0, 1.0);\n"
                << "        v_frag_FragColor = colors[gl_PrimitiveIDIn % 4];\n"
                << "        EmitVertex();\n"
                << "        gl_Position = gl_in[1].gl_Position * vec4(1.0 + percent, 1.0 + percent, 1.0, 1.0);\n"
                << "        v_frag_FragColor = colors[gl_PrimitiveIDIn % 4];\n"
                << "        EmitVertex();\n"
                << "    }\n"
                << "}\n";
            sourceCollections.glslSources.add("geometry") << glu::GeometrySource(src.str());
            break;
        case TEST_PRIMITIVE_ID:
            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "in gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "    float gl_PointSize;\n"
                << "} gl_in[];\n"
                << "out gl_PerVertex\n"
                << "{\n"
                << "    vec4 gl_Position;\n"
                << "    float gl_PointSize;\n"
                << "};\n"
                << "layout(points, invocations=1) in;\n"
                << "layout(triangle_strip, max_vertices = 3) out;\n"
                << "layout(location = 0) in vec4 v_geom_primitiveID[];\n"
                << "void main (void)\n"
                << "{\n"
                << "    gl_Position = gl_in[0].gl_Position + vec4(0.05, 0.0, 0.0, 0.0);\n"
                << "    gl_PrimitiveID = int(floor(v_geom_primitiveID[0].x)) + 3;\n"
                << "    EmitVertex();\n"
                << "    gl_Position = gl_in[0].gl_Position - vec4(0.05, 0.0, 0.0, 0.0);\n"
                << "    gl_PrimitiveID = int(floor(v_geom_primitiveID[0].x)) + 3;\n"
                << "    EmitVertex();\n"
                << "    gl_Position = gl_in[0].gl_Position + vec4(0.0, 0.05, 0.0, 0.0);\n"
                << "    gl_PrimitiveID = int(floor(v_geom_primitiveID[0].x)) + 3;\n"
                << "    EmitVertex();\n"
                << "}\n";
            sourceCollections.glslSources.add("geometry") << glu::GeometrySource(src.str());
            break;
        case TEST_POSITION:
            src << "struct VSOut\n"
                << "{\n"
                << "    float4 Position : SV_POSITION;\n"
                << "};\n"
                << "[maxvertexcount(10)]\n"
                << "void main(triangle VSOut input[3], inout TriangleStream<VSOut> TriStream)\n"
                << "{\n"
                << "    VSOut output;\n"
                << "    output.Position = input[0].Position;\n"
                << "    TriStream.Append(output);\n"
                << "    output.Position = input[1].Position;\n"
                << "    TriStream.Append(output);\n"
                << "    output.Position = input[2].Position;\n"
                << "    TriStream.Append(output);\n"
                << "}\n";
            sourceCollections.hlslSources.add("geometry") << glu::GeometrySource(src.str());
            break;
        default:
            DE_ASSERT(0);
            break;
        }
    }

    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
        switch (m_test)
        {
        case TEST_POINT_SIZE:
            src << "layout(location = 0) out vec4 fragColor;\n"
                << "layout(location = 0) in vec4 v_frag_FragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    fragColor = v_frag_FragColor;\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID_IN:
            src << "layout(location = 0) out vec4 fragColor;\n"
                << "layout(location = 0) in vec4 v_frag_FragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    fragColor = v_frag_FragColor;\n"
                << "}\n";
            break;
        case TEST_PRIMITIVE_ID:
            src << "layout(location = 0) out vec4 fragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    const vec4 red = vec4(1.0, 0.0, 0.0, 1.0);\n"
                << "    const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
                << "    const vec4 blue = vec4(0.0, 0.0, 1.0, 1.0);\n"
                << "    const vec4 yellow = vec4(1.0, 1.0, 0.0, 1.0);\n"
                << "    const vec4 colors[4] = vec4[4](yellow, red, green, blue);\n"
                << "    fragColor = colors[gl_PrimitiveID % 4];\n"
                << "}\n";
            break;
        case TEST_POSITION:
            src << "layout(location = 0) out vec4 fragColor;\n"
                << "void main (void)\n"
                << "{\n"
                << "    fragColor = vec4(1.0, 1.0, 0.0, 1.0);;\n"
                << "}\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        sourceCollections.glslSources.add("fragment") << glu::FragmentSource(src.str());
    }
}

TestInstance *BuiltinVariableRenderTest::createInstance(Context &context) const
{
    return new BuiltinVariableRenderTestInstance(context, getName(), m_test, m_flag);
}

class BuiltinVariableMatchingPrimitiveIDTestInstance : public TestInstance
{
public:
    BuiltinVariableMatchingPrimitiveIDTestInstance(Context &context) : TestInstance(context)
    {
    }
    tcu::TestStatus iterate(void);
};

tcu::TestStatus BuiltinVariableMatchingPrimitiveIDTestInstance::iterate(void)
{
    const vk::DeviceInterface &vk   = m_context.getDeviceInterface();
    const vk::VkDevice device       = m_context.getDevice();
    const vk::VkQueue queue         = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    auto &alloc                     = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();

    const tcu::IVec2 resolution = tcu::IVec2(32u, 32u);
    const VkFormat colorFormat  = VK_FORMAT_R8G8B8A8_UNORM;

    const vk::VkDeviceSize colorOutputBufferSize =
        resolution.x() * resolution.y() * tcu::getPixelSize(vk::mapVkFormat(colorFormat));
    vk::BufferWithMemory colorOutputBuffer = vk::BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        vk::MemoryRequirement::HostVisible);

    const vk::Move<vk::VkCommandPool> cmdPool(createCommandPool(vk, device, 0u, queueFamilyIndex));
    const vk::Move<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const ImageWithMemory colorAttachmentImage(
        vk, device, alloc,
        makeImageCreateInfo(resolution, colorFormat,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        MemoryRequirement::Any);
    const VkImageSubresourceRange colorSubRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const Unique<VkImageView> colorAttachmentView(
        makeImageView(vk, device, *colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubRange));
    const Unique<VkRenderPass> renderPass(makeRenderPass(vk, device, colorFormat));
    const Unique<VkFramebuffer> framebuffer(
        makeFramebuffer(vk, device, *renderPass, *colorAttachmentView, resolution.x(), resolution.y(), 1u));

    const auto pipelineLayout = makePipelineLayout(vk, device);

    const Unique<VkPipeline> pipeline(
        GraphicsPipelineBuilder()
            .setRenderSize(resolution)
            .setShader(vk, device, VK_SHADER_STAGE_VERTEX_BIT, m_context.getBinaryCollection().get("vert"), nullptr)
            .setShader(vk, device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                       m_context.getBinaryCollection().get("tese"), nullptr)
            .setShader(vk, device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                       m_context.getBinaryCollection().get("tesc"), nullptr)
            .setShader(vk, device, VK_SHADER_STAGE_GEOMETRY_BIT, m_context.getBinaryCollection().get("geom"), nullptr)
            .setShader(vk, device, VK_SHADER_STAGE_FRAGMENT_BIT, m_context.getBinaryCollection().get("frag"), nullptr)
            .setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
            .setPatchControlPoints(4u)
            .build(vk, device, *pipelineLayout, *renderPass));

    beginCommandBuffer(vk, *cmdBuffer, 0u);
    const VkRect2D renderArea = {
        makeOffset2D(0, 0),
        makeExtent2D(resolution.x(), resolution.y()),
    };
    const tcu::Vec4 clearColor(1.0f, 1.0f, 1.0f, 1.0f);
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);
    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vk.cmdDraw(*cmdBuffer, 16u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    vk::VkImageMemoryBarrier imageBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorAttachmentImage, makeDefaultImageSubresourceRange());
    vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, nullptr, 0u, nullptr, 1u,
                          &imageBarrier);

    const auto copyRegion = makeBufferImageCopy(0u, makeDefaultImageSubresourceLayers(), {0, 0, 0},
                                                {(uint32_t)resolution.x(), (uint32_t)resolution.y(), 1u});
    vk.cmdCopyImageToBuffer(*cmdBuffer, *colorAttachmentImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            *colorOutputBuffer, 1u, &copyRegion);
    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    const vk::Allocation &outputBufferAllocation = colorOutputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputBufferAllocation);

    bool allZero             = true;
    const uint8_t *bufferPtr = static_cast<uint8_t *>(outputBufferAllocation.getHostPtr());
    for (int y = 0; y < resolution.y(); ++y)
    {
        for (int x = 0; x < resolution.x(); ++x)
        {
            const uint32_t pixelIndex = y * resolution.x() + x;
            const uint8_t *pixelPtr   = &bufferPtr[pixelIndex * 4];

            uint32_t tescPrimitiveID = (uint32_t)pixelPtr[0];
            uint32_t tesePrimitiveID = (uint32_t)pixelPtr[1];
            uint32_t geomPrimitiveID = (uint32_t)pixelPtr[2];
            uint32_t alpha           = (uint32_t)pixelPtr[3];

            if (tescPrimitiveID != tesePrimitiveID)
            {
                log << tcu::TestLog::Message << "At (" << x << ", " << y
                    << ") gl_PrimitiveID in tessellation control shader was  " << tescPrimitiveID
                    << ", but gl_PrimitiveID in tessellation evaluation shader was " << tesePrimitiveID
                    << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
            else if (tesePrimitiveID != geomPrimitiveID)
            {
                log << tcu::TestLog::Message << "At (" << x << ", " << y
                    << ") gl_PrimitiveID in tessellation evaluation shader was  " << tesePrimitiveID
                    << ", but gl_PrimitiveID in geometry shader was " << geomPrimitiveID << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
            else if (alpha != 255)
            {
                log << tcu::TestLog::Message << "At (" << x << ", " << y
                    << ") expected alpha value was 255, but actual value was " << alpha << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }

            allZero = allZero && (tescPrimitiveID == 0);
        }
    }
    if (allZero)
    {
        log << tcu::TestLog::Message << "All gl_PrimitiveID values were zero." << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class BuiltinVariableMatchingPrimitiveIDTest : public TestCase
{
public:
    BuiltinVariableMatchingPrimitiveIDTest(TestContext &testCtx, const char *name) : TestCase(testCtx, name)
    {
    }
    void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;
};

void BuiltinVariableMatchingPrimitiveIDTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::stringstream vert;
    vert << "#version 450\n"
         << "void main() {\n"
         << "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
         << "    vec2 offset = vec2(float((gl_VertexIndex / 4) & 1), float(((gl_VertexIndex / 4) >> 1) & 1));\n"
         << "    gl_Position = vec4(pos - 1.0f + offset, 0.0f, 1.0f);\n"
         << "}\n";

    std::stringstream tesc;
    tesc << "#version 450\n"
         << "\n"
         << "layout(vertices = 4) out;\n"
         << "layout(location = 0) flat out uint tesc_primitive_id[];\n"
         << "\n"
         << "void main (void)\n"
         << "{\n"
         << "    if (gl_InvocationID == 0) {\n"
         << "        gl_TessLevelInner[0] = 2.0f;\n"
         << "        gl_TessLevelInner[1] = 2.0f;\n"
         << "        gl_TessLevelOuter[0] = 2.0f;\n"
         << "        gl_TessLevelOuter[1] = 2.0f;\n"
         << "        gl_TessLevelOuter[2] = 2.0f;\n"
         << "        gl_TessLevelOuter[3] = 2.0f;\n"
         << "    }\n"
         << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
         << "    tesc_primitive_id[gl_InvocationID] = gl_PrimitiveID;\n"
         << "}\n";
    std::stringstream tese;
    tese << "#version 450\n"
         << "\n"
         << "layout(quads, equal_spacing) in;\n"
         << "layout(location = 0) flat  in uint tesc_primitive_id[];\n"
         << "layout(location = 0) flat out uint out_tesc_primitive_id;\n"
         << "layout(location = 1) flat out uint tese_primitive_id;\n"
         << "\n"
         << "void main (void)\n"
         << "{\n"
         << "    float u = gl_TessCoord.x;\n"
         << "    float v = gl_TessCoord.y;\n"
         << "    float omu = 1.0f - u;\n"
         << "    float omv = 1.0f - v;\n"
         << "    gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * "
            "gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;\n"
         << "    out_tesc_primitive_id = tesc_primitive_id[0];\n"
         << "    tese_primitive_id = gl_PrimitiveID;\n"
         << "}\n";

    std::stringstream geom;
    geom << "#version 450\n"
         << "layout(triangles) in;\n"
         << "layout(triangle_strip, max_vertices = 4) out;\n"
         << "layout(location = 0) flat in uint tesc_primitive_id[];\n"
         << "layout(location = 1) flat in uint tese_primitive_id[];\n"
         << "layout(location = 0) flat out uint out_tesc_primitive_id;\n"
         << "layout(location = 1) flat out uint out_tese_primitive_id;\n"
         << "layout(location = 2) flat out uint out_geom_primitive_id;\n"
         << "\n"
         << "void main(void)\n"
         << "{\n"
         << "    out_tesc_primitive_id = tesc_primitive_id[0];\n"
         << "    out_tese_primitive_id = tese_primitive_id[0];\n"
         << "    gl_PrimitiveID = gl_PrimitiveIDIn;\n"
         << "    out_geom_primitive_id = gl_PrimitiveID;\n"
         << "    gl_Position = gl_in[0].gl_Position;\n"
         << "    EmitVertex();\n"
         << "    out_tesc_primitive_id = tesc_primitive_id[1];\n"
         << "    out_tese_primitive_id = tese_primitive_id[1];\n"
         << "    gl_Position = gl_in[1].gl_Position;\n"
         << "    EmitVertex();\n"
         << "    out_tesc_primitive_id = tesc_primitive_id[2];\n"
         << "    out_tese_primitive_id = tese_primitive_id[2];\n"
         << "    gl_Position = gl_in[2].gl_Position;\n"
         << "    EmitVertex();\n"
         << "    EndPrimitive();\n"
         << "}\n";
    std::stringstream frag;
    frag << "#version 450\n"
         << "layout(location = 0) flat in uint tesc_primitive_id;\n"
         << "layout(location = 1) flat in uint tese_primitive_id;\n"
         << "layout(location = 2) flat in uint geom_primitive_id;\n"
         << "layout (location = 0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    outColor = vec4(\n"
         << "                    float(tesc_primitive_id) / 255.0f,\n"
         << "                    float(tese_primitive_id) / 255.0f,\n"
         << "                    float(geom_primitive_id) / 255.0f,\n"
         << "                    1.0f\n"
         << "                    );\n"
         << "}\n";

    sourceCollections.glslSources.add("vert") << glu::VertexSource(vert.str());
    sourceCollections.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
    sourceCollections.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    sourceCollections.glslSources.add("geom") << glu::GeometrySource(geom.str());
    sourceCollections.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *BuiltinVariableMatchingPrimitiveIDTest::createInstance(Context &context) const
{
    return new BuiltinVariableMatchingPrimitiveIDTestInstance(context);
}

void BuiltinVariableMatchingPrimitiveIDTest::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
}

} // namespace

TestCaseGroup *createBuiltinVariableGeometryShaderTests(TestContext &testCtx)
{
    MovePtr<TestCaseGroup> basicGroup(new tcu::TestCaseGroup(testCtx, "builtin_variable"));
    MovePtr<TestCaseGroup> in_block(new tcu::TestCaseGroup(testCtx, "in_block"));
    MovePtr<TestCaseGroup> outside_block(new tcu::TestCaseGroup(testCtx, "outside_block"));
    MovePtr<TestCaseGroup> primitive_id(new tcu::TestCaseGroup(testCtx, "primitive_id"));

    // test gl_PointSize
    in_block->addChild(new BuiltinVariableRenderTest(testCtx, "point_size", TEST_POINT_SIZE));
    // test gl_PrimitiveIDIn
    in_block->addChild(new BuiltinVariableRenderTest(testCtx, "primitive_id_in", TEST_PRIMITIVE_ID_IN));
    // test gl_PrimitiveIDIn with primitive restart
    in_block->addChild(new BuiltinVariableRenderTest(testCtx, "primitive_id_in_restarted", TEST_PRIMITIVE_ID_IN, true));
    // test gl_PrimitiveID
    in_block->addChild(new BuiltinVariableRenderTest(testCtx, "primitive_id", TEST_PRIMITIVE_ID));
    // test gl_Position
    outside_block->addChild(new BuiltinVariableRenderTest(testCtx, "position", TEST_POSITION));

    // test gl_PrimitiveID matching between different stages
    primitive_id->addChild(new BuiltinVariableMatchingPrimitiveIDTest(testCtx, "matching"));

    basicGroup->addChild(in_block.release());
    basicGroup->addChild(outside_block.release());
    basicGroup->addChild(primitive_id.release());

    return basicGroup.release();
}

} // namespace geometry
} // namespace vkt
