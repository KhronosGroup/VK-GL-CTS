/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Input Assembly Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineInputAssemblyTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deMath.h"
#include "deMemory.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <algorithm>
#include <sstream>
#include <vector>
#include <limits>

namespace vkt
{
namespace pipeline
{

using namespace vk;

enum class RestartType
{
    NORMAL,
    NONE,
    ALL,
    DIVIDE,
    SECOND_PASS,
};

namespace
{

class InputAssemblyTest : public vkt::TestCase
{
public:
    const static VkPrimitiveTopology s_primitiveTopologies[];
    const static uint32_t s_restartIndex32;
    const static uint16_t s_restartIndex16;
    const static uint8_t s_restartIndex8;

    InputAssemblyTest(tcu::TestContext &testContext, const std::string &name,
                      const PipelineConstructionType pipelineConstructionType, VkPrimitiveTopology primitiveTopology,
                      int primitiveCount, bool testPrimitiveRestart, bool divideDraw, bool secondPass,
                      VkIndexType indexType);
    virtual ~InputAssemblyTest(void)
    {
    }
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;
    static bool isRestartIndex(VkIndexType indexType, uint32_t indexValue);
#ifndef CTS_USES_VULKANSC
    static uint32_t getRestartIndex(VkIndexType indexType);
#endif // CTS_USES_VULKANSC

protected:
    virtual void createBufferData(VkPrimitiveTopology topology, int primitiveCount, VkIndexType indexType,
                                  std::vector<uint32_t> &indexData, std::vector<Vertex4RGBA> &vertexData) const = 0;
    VkPrimitiveTopology m_primitiveTopology;
    const int m_primitiveCount;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    bool m_testPrimitiveRestart;
    bool m_testDivideDraw;
    bool m_testSecondPass;
    VkIndexType m_indexType;
};

class PrimitiveTopologyTest : public InputAssemblyTest
{
public:
    PrimitiveTopologyTest(tcu::TestContext &testContext, const std::string &name,
                          PipelineConstructionType pipelineConstructionType, VkPrimitiveTopology primitiveTopology,
                          VkIndexType indexType);
    virtual ~PrimitiveTopologyTest(void)
    {
    }

protected:
    virtual void createBufferData(VkPrimitiveTopology topology, int primitiveCount, VkIndexType indexType,
                                  std::vector<uint32_t> &indexData, std::vector<Vertex4RGBA> &vertexData) const;

private:
};

#ifndef CTS_USES_VULKANSC
class PrimitiveRestartTest : public InputAssemblyTest
{
public:
    PrimitiveRestartTest(tcu::TestContext &testContext, const std::string &name,
                         const PipelineConstructionType pipelineConstructionType, VkPrimitiveTopology primitiveTopology,
                         VkIndexType indexType, RestartType restartType);
    virtual ~PrimitiveRestartTest(void)
    {
    }
    virtual void checkSupport(Context &context) const;

protected:
    virtual void createBufferData(VkPrimitiveTopology topology, int primitiveCount, VkIndexType indexType,
                                  std::vector<uint32_t> &indexData, std::vector<Vertex4RGBA> &vertexData) const;

private:
    bool isRestartPrimitive(int primitiveIndex) const;
    void createListPrimitives(int primitiveCount, float originX, float originY, float primitiveSizeX,
                              float primitiveSizeY, int verticesPerPrimitive, VkIndexType indexType,
                              std::vector<uint32_t> &indexData, std::vector<Vertex4RGBA> &vertexData,
                              std::vector<uint32_t> adjacencies) const;

    std::vector<uint32_t> m_restartPrimitives;
    RestartType m_restartType;
};
#endif // CTS_USES_VULKANSC

class InputAssemblyInstance : public vkt::TestInstance
{
public:
    InputAssemblyInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                          const VkPrimitiveTopology primitiveTopology, bool testPrimitiveRestart, bool divideDraw,
                          bool secondPass, VkIndexType indexType, const std::vector<uint32_t> &indexBufferData,
                          const std::vector<Vertex4RGBA> &vertexBufferData);
    virtual ~InputAssemblyInstance(void);
    virtual tcu::TestStatus iterate(void);

private:
    tcu::TestStatus verifyImage(void);
    void uploadIndexBufferData16(uint16_t *destPtr, const std::vector<uint32_t> &indexBufferData);
    void uploadIndexBufferData8(uint8_t *destPtr, const std::vector<uint32_t> &indexBufferData);
    uint32_t getVerticesPerPrimitive(VkPrimitiveTopology topology);

    VkPrimitiveTopology m_primitiveTopology;
    bool m_primitiveRestartEnable;
    bool m_divideDrawEnable;
    bool m_multiPassEnable;
    VkIndexType m_indexType;

    Move<VkBuffer> m_vertexBuffer;
    std::vector<Vertex4RGBA> m_vertices;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    Move<VkBuffer> m_indexBuffer;
    std::vector<uint32_t> m_indices;
    de::MovePtr<Allocation> m_indexBufferAlloc;

    const tcu::UVec2 m_renderSize;

    const VkFormat m_colorFormat;
    VkImageCreateInfo m_colorImageCreateInfo;
    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorAttachmentView;
    std::vector<RenderPassWrapper> m_renderPasses;
    Move<VkFramebuffer> m_framebuffer;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModule;
    ShaderWrapper m_tcsShaderModule;
    ShaderWrapper m_tesShaderModule;

    PipelineLayoutWrapper m_pipelineLayout;
    GraphicsPipelineWrapper m_graphicsPipeline;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

// InputAssemblyTest

const VkPrimitiveTopology InputAssemblyTest::s_primitiveTopologies[] = {
    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY};

const uint32_t InputAssemblyTest::s_restartIndex32 = ~((uint32_t)0u);
const uint16_t InputAssemblyTest::s_restartIndex16 = ~((uint16_t)0u);
const uint8_t InputAssemblyTest::s_restartIndex8   = ~((uint8_t)0u);

InputAssemblyTest::InputAssemblyTest(tcu::TestContext &testContext, const std::string &name,
                                     const PipelineConstructionType pipelineConstructionType,
                                     VkPrimitiveTopology primitiveTopology, int primitiveCount,
                                     bool testPrimitiveRestart, bool testDivideDraw, bool testSecondPass,
                                     VkIndexType indexType)
    : vkt::TestCase(testContext, name)
    , m_primitiveTopology(primitiveTopology)
    , m_primitiveCount(primitiveCount)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_testPrimitiveRestart(testPrimitiveRestart)
    , m_testDivideDraw(testDivideDraw)
    , m_testSecondPass(testSecondPass)
    , m_indexType(indexType)
{
}

void InputAssemblyTest::checkSupport(Context &context) const
{
    if (m_indexType == VK_INDEX_TYPE_UINT8_EXT)
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_index_type_uint8") &&
            !context.isDeviceFunctionalitySupported("VK_EXT_index_type_uint8"))
        {
            TCU_THROW(NotSupportedError, "VK_KHR_index_type_uint8 and VK_EXT_index_type_uint8 is not supported");
        }
    }

    switch (m_primitiveTopology)
    {
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
        break;

    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
        break;

    default:
        break;
    }

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);

#ifndef CTS_USES_VULKANSC
    if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
        context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().triangleFans)
    {
        TCU_THROW(NotSupportedError,
                  "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
    }
#endif // CTS_USES_VULKANSC
}

TestInstance *InputAssemblyTest::createInstance(Context &context) const
{
    std::vector<uint32_t> indexBufferData;
    std::vector<Vertex4RGBA> vertexBufferData;

    createBufferData(m_primitiveTopology, m_primitiveCount, m_indexType, indexBufferData, vertexBufferData);

    return new InputAssemblyInstance(context, m_pipelineConstructionType, m_primitiveTopology, m_testPrimitiveRestart,
                                     m_testDivideDraw, m_testSecondPass, m_indexType, indexBufferData,
                                     vertexBufferData);
}

void InputAssemblyTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream vertexSource;

    vertexSource << "#version 310 es\n"
                    "layout(location = 0) in vec4 position;\n"
                    "layout(location = 1) in vec4 color;\n"
                    "layout(location = 0) out highp vec4 vtxColor;\n"
                    "void main (void)\n"
                    "{\n"
                    "    gl_Position = position;\n"
                 << (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ? "    gl_PointSize = 3.0;\n" : "")
                 << "    vtxColor = color;\n"
                    "}\n";

    sourceCollections.glslSources.add("color_vert") << glu::VertexSource(vertexSource.str());

    sourceCollections.glslSources.add("color_frag")
        << glu::FragmentSource("#version 310 es\n"
                               "layout(location = 0) in highp vec4 vtxColor;\n"
                               "layout(location = 0) out highp vec4 fragColor;\n"
                               "void main (void)\n"
                               "{\n"
                               "    fragColor = vtxColor;\n"
                               "}\n");

    sourceCollections.glslSources.add("color_tcs")
        << glu::TessellationControlSource("#version 310 es\n"
                                          "#extension GL_EXT_tessellation_shader : require\n"
                                          "layout(vertices = 3) out;\n"
                                          "layout(location = 0) in highp vec4 vtxColorIn[];\n"
                                          "layout(location = 0) out highp vec4 vtxColorOut[];\n"
                                          "#define ID gl_InvocationID\n"
                                          "void main (void)\n"
                                          "{\n"
                                          "    vtxColorOut[ID] = vtxColorIn[ID];\n"
                                          "    gl_out[ID].gl_Position = gl_in[ID].gl_Position;\n"
                                          "    if (ID == 0)\n"
                                          "    {\n"
                                          "        gl_TessLevelInner[0] = 5.0;\n"
                                          "        gl_TessLevelOuter[0] = 4.0;\n"
                                          "        gl_TessLevelOuter[1] = 5.0;\n"
                                          "        gl_TessLevelOuter[2] = 6.0;\n"
                                          "    }\n"
                                          "}\n");

    sourceCollections.glslSources.add("color_tes")
        << glu::TessellationEvaluationSource("#version 310 es\n"
                                             "#extension GL_EXT_tessellation_shader : require\n"
                                             "layout(triangles) in;\n"
                                             "layout(location = 0) in vec4 vtxColorIn[];\n"
                                             "layout(location = 0) out vec4 vtxColorOut;\n"
                                             "void main (void)\n"
                                             "{\n"
                                             "    vec4 p0 = gl_TessCoord.x * gl_in[0].gl_Position;\n"
                                             "    vec4 p1 = gl_TessCoord.y * gl_in[1].gl_Position;\n"
                                             "    vec4 p2 = gl_TessCoord.z * gl_in[2].gl_Position;\n"
                                             "    gl_Position = p0 + p1 + p2;\n"
                                             "    vec4 q0 = gl_TessCoord.x * vtxColorIn[0];\n"
                                             "    vec4 q1 = gl_TessCoord.y * vtxColorIn[1];\n"
                                             "    vec4 q2 = gl_TessCoord.z * vtxColorIn[2];\n"
                                             "    vtxColorOut = q0 + q1 + q2;\n"
                                             "}\n");
}

bool InputAssemblyTest::isRestartIndex(VkIndexType indexType, uint32_t indexValue)
{
    if (indexType == VK_INDEX_TYPE_UINT16)
        return indexValue == s_restartIndex16;
    else if (indexType == VK_INDEX_TYPE_UINT8_EXT)
        return indexValue == s_restartIndex8;
    else
        return indexValue == s_restartIndex32;
}

#ifndef CTS_USES_VULKANSC
uint32_t InputAssemblyTest::getRestartIndex(VkIndexType indexType)
{
    if (indexType == VK_INDEX_TYPE_UINT16)
        return InputAssemblyTest::s_restartIndex16;
    else if (indexType == VK_INDEX_TYPE_UINT8_EXT)
        return InputAssemblyTest::s_restartIndex8;
    else
        return InputAssemblyTest::s_restartIndex32;
}
#endif // CTS_USES_VULKANSC

// PrimitiveTopologyTest

PrimitiveTopologyTest::PrimitiveTopologyTest(tcu::TestContext &testContext, const std::string &name,
                                             PipelineConstructionType pipelineConstructionType,
                                             VkPrimitiveTopology primitiveTopology, VkIndexType indexType)
    : InputAssemblyTest(testContext, name, pipelineConstructionType, primitiveTopology, 10, false, false, false,
                        indexType)
{
}

void PrimitiveTopologyTest::createBufferData(VkPrimitiveTopology topology, int primitiveCount, VkIndexType indexType,
                                             std::vector<uint32_t> &indexData,
                                             std::vector<Vertex4RGBA> &vertexData) const
{
    DE_ASSERT(primitiveCount > 0);
    DE_UNREF(indexType);

    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
    const float border              = 0.2f;
    const float originX             = -1.0f + border;
    const float originY             = -1.0f + border;
    const Vertex4RGBA defaultVertex = {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), green};
    float primitiveSizeY            = (2.0f - 2.0f * border);
    float primitiveSizeX;
    std::vector<uint32_t> indices;
    std::vector<Vertex4RGBA> vertices;

    // Calculate primitive size
    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2 + primitiveCount % 2 - 1);
        break;

    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount - 1);
        break;

    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2);
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount + primitiveCount / 2 + primitiveCount % 2 - 1);
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2 + primitiveCount % 2);
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        primitiveSizeX = 1.0f - border;
        primitiveSizeY = 1.0f - border;
        break;

    default:
        primitiveSizeX = 0.0f; // Garbage
        DE_ASSERT(false);
    }

    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            const Vertex4RGBA vertex = {tcu::Vec4(originX + float(primitiveNdx / 2) * primitiveSizeX,
                                                  originY + float(primitiveNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
                                        red};

            vertices.push_back(vertex);
            indices.push_back(primitiveNdx);
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            for (int vertexNdx = 0; vertexNdx < 2; vertexNdx++)
            {
                const Vertex4RGBA vertex = {
                    tcu::Vec4(originX + float((primitiveNdx * 2 + vertexNdx) / 2) * primitiveSizeX,
                              originY + float(vertexNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
                    red};

                vertices.push_back(vertex);
                indices.push_back((primitiveNdx * 2 + vertexNdx));
            }
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (primitiveNdx == 0)
            {
                Vertex4RGBA vertex = {tcu::Vec4(originX, originY, 0.0f, 1.0f), red};

                vertices.push_back(vertex);
                indices.push_back(0);

                vertex.position = tcu::Vec4(originX, originY + primitiveSizeY, 0.0f, 1.0f);
                vertices.push_back(vertex);
                indices.push_back(1);
            }
            else
            {
                const Vertex4RGBA vertex = {tcu::Vec4(originX + float((primitiveNdx + 1) / 2) * primitiveSizeX,
                                                      originY + float((primitiveNdx + 1) % 2) * primitiveSizeY, 0.0f,
                                                      1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back(primitiveNdx + 1);
            }
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
            {
                const Vertex4RGBA vertex = {
                    tcu::Vec4(originX + float((primitiveNdx * 3 + vertexNdx) / 2) * primitiveSizeX,
                              originY + float((primitiveNdx * 3 + vertexNdx) % 2) * primitiveSizeY, 0.0f, 1.0f),
                    red};

                vertices.push_back(vertex);
                indices.push_back(primitiveNdx * 3 + vertexNdx);
            }
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (primitiveNdx == 0)
            {
                for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
                {
                    const Vertex4RGBA vertex = {tcu::Vec4(originX + float(vertexNdx / 2) * primitiveSizeX,
                                                          originY + float(vertexNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
                                                red};

                    vertices.push_back(vertex);
                    indices.push_back(vertexNdx);
                }
            }
            else
            {
                const Vertex4RGBA vertex = {tcu::Vec4(originX + float((primitiveNdx + 2) / 2) * primitiveSizeX,
                                                      originY + float((primitiveNdx + 2) % 2) * primitiveSizeY, 0.0f,
                                                      1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back(primitiveNdx + 2);
            }
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    {
        const float stepAngle = de::min(DE_PI * 0.5f, (2 * DE_PI) / float(primitiveCount));

        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (primitiveNdx == 0)
            {
                Vertex4RGBA vertex = {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), red};

                vertices.push_back(vertex);
                indices.push_back(0);

                vertex.position = tcu::Vec4(primitiveSizeX, 0.0f, 0.0f, 1.0f);
                vertices.push_back(vertex);
                indices.push_back(1);

                vertex.position = tcu::Vec4(primitiveSizeX * deFloatCos(stepAngle),
                                            primitiveSizeY * deFloatSin(stepAngle), 0.0f, 1.0f);
                vertices.push_back(vertex);
                indices.push_back(2);
            }
            else
            {
                const Vertex4RGBA vertex = {tcu::Vec4(primitiveSizeX * deFloatCos(stepAngle * float(primitiveNdx + 1)),
                                                      primitiveSizeY * deFloatSin(stepAngle * float(primitiveNdx + 1)),
                                                      0.0f, 1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back(primitiveNdx + 2);
            }
        }
        break;
    }

    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        vertices.push_back(defaultVertex);

        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            indices.push_back(0);

            for (int vertexNdx = 0; vertexNdx < 2; vertexNdx++)
            {
                const Vertex4RGBA vertex = {
                    tcu::Vec4(originX + float((primitiveNdx * 2 + vertexNdx) / 2) * primitiveSizeX,
                              originY + float(vertexNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
                    red};

                vertices.push_back(vertex);
                indices.push_back(primitiveNdx * 2 + vertexNdx + 1);
            }

            indices.push_back(0);
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        vertices.push_back(defaultVertex);
        indices.push_back(0);

        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (primitiveNdx == 0)
            {
                Vertex4RGBA vertex = {tcu::Vec4(originX, originY, 0.0f, 1.0f), red};

                vertices.push_back(vertex);
                indices.push_back(1);

                vertex.position = tcu::Vec4(originX, originY + primitiveSizeY, 0.0f, 1.0f);
                vertices.push_back(vertex);
                indices.push_back(2);
            }
            else
            {
                const Vertex4RGBA vertex = {tcu::Vec4(originX + float((primitiveNdx + 1) / 2) * primitiveSizeX,
                                                      originY + float((primitiveNdx + 1) % 2) * primitiveSizeY, 0.0f,
                                                      1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back(primitiveNdx + 2);
            }
        }

        indices.push_back(0);
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        vertices.push_back(defaultVertex);

        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
            {
                const Vertex4RGBA vertex = {
                    tcu::Vec4(originX + float((primitiveNdx * 3 + vertexNdx) / 2) * primitiveSizeX,
                              originY + float((primitiveNdx * 3 + vertexNdx) % 2) * primitiveSizeY, 0.0f, 1.0f),
                    red};

                vertices.push_back(vertex);
                indices.push_back(primitiveNdx * 3 + vertexNdx + 1);
                indices.push_back(0);
            }
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        vertices.push_back(defaultVertex);

        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (primitiveNdx == 0)
            {
                for (int vertexNdx = 0; vertexNdx < 3; vertexNdx++)
                {
                    const Vertex4RGBA vertex = {tcu::Vec4(originX + float(vertexNdx / 2) * primitiveSizeX,
                                                          originY + float(vertexNdx % 2) * primitiveSizeY, 0.0f, 1.0f),
                                                red};

                    vertices.push_back(vertex);
                    indices.push_back(vertexNdx + 1);
                    indices.push_back(0);
                }
            }
            else
            {
                const Vertex4RGBA vertex = {tcu::Vec4(originX + float((primitiveNdx + 2) / 2) * primitiveSizeX,
                                                      originY + float((primitiveNdx + 2) % 2) * primitiveSizeY, 0.0f,
                                                      1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back(primitiveNdx + 2 + 1);
                indices.push_back(0);
            }
        }
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    vertexData = vertices;
    indexData  = indices;
}

#ifndef CTS_USES_VULKANSC
// PrimitiveRestartTest

PrimitiveRestartTest::PrimitiveRestartTest(tcu::TestContext &testContext, const std::string &name,
                                           PipelineConstructionType pipelineConstructionType,
                                           VkPrimitiveTopology primitiveTopology, VkIndexType indexType,
                                           RestartType restartType)

    : InputAssemblyTest(testContext, name, pipelineConstructionType, primitiveTopology, 10, true,
                        restartType == RestartType::DIVIDE, restartType == RestartType::SECOND_PASS, indexType)
    , m_restartType(restartType)
{
    uint32_t restartPrimitives[] = {1, 5};

    if (restartType == RestartType::NORMAL)
    {
        m_restartPrimitives =
            std::vector<uint32_t>(restartPrimitives, restartPrimitives + sizeof(restartPrimitives) / sizeof(uint32_t));
    }
    else if (restartType == RestartType::NONE)
    {
        m_restartPrimitives = std::vector<uint32_t>{};
    }
    else if (restartType == RestartType::DIVIDE || restartType == RestartType::SECOND_PASS)
    {
        // Single restart on the last primitive in the list
        m_restartPrimitives.push_back(m_primitiveCount - 1);
    }
    else
    {
        uint32_t count = 1;
        switch (primitiveTopology)
        {
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
            count = 2;
            break;
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
            count = 3;
            break;
        default:
            break;
        }
        for (uint32_t i = 0; i < (uint32_t)m_primitiveCount; ++i)
        {
            if (i % count == count - 1)
            {
                m_restartPrimitives.push_back(i);
            }
        }
    }
}

void PrimitiveRestartTest::checkSupport(Context &context) const
{
    switch (m_primitiveTopology)
    {
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    {
        context.requireDeviceFunctionality("VK_EXT_primitive_topology_list_restart");

        const auto &features = context.getPrimitiveTopologyListRestartFeaturesEXT();
        if (!features.primitiveTopologyListRestart)
            TCU_THROW(NotSupportedError, "Primitive topology list restart feature not supported");
        if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST && !features.primitiveTopologyPatchListRestart)
            TCU_THROW(NotSupportedError, "Primitive topology patch list restart feature not supported");
    }
    break;

    default:
        break;
    }

    InputAssemblyTest::checkSupport(context);
}

void PrimitiveRestartTest::createBufferData(VkPrimitiveTopology topology, int primitiveCount, VkIndexType indexType,
                                            std::vector<uint32_t> &indexData,
                                            std::vector<Vertex4RGBA> &vertexData) const
{
    DE_ASSERT(primitiveCount > 0);
    DE_UNREF(indexType);

    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
    const float border              = 0.2f;
    const float originX             = -1.0f + border;
    const float originY             = -1.0f + border;
    const Vertex4RGBA defaultVertex = {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), green};
    float primitiveSizeY            = (2.0f - 2.0f * border);
    float primitiveSizeX;
    bool primitiveStart = true;
    std::vector<uint32_t> indices;
    std::vector<Vertex4RGBA> vertices;

    // Calculate primitive size
    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2 + primitiveCount % 2 - 1);
        break;

    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2);
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        primitiveSizeX = (2.0f - 2.0f * border) / float(primitiveCount / 2 + primitiveCount % 2);
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        primitiveSizeX = 1.0f - border;
        primitiveSizeY = 1.0f - border;
        break;

    default:
        primitiveSizeX = 0.0f; // Garbage
        DE_ASSERT(false);
    }

    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (isRestartPrimitive(primitiveNdx))
            {
                indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
                primitiveStart = true;
            }
            else
            {
                if (primitiveStart && m_restartType != RestartType::ALL)
                {
                    const Vertex4RGBA vertex = {tcu::Vec4(originX + float(primitiveNdx / 2) * primitiveSizeX,
                                                          originY + float(primitiveNdx % 2) * primitiveSizeY, 0.0f,
                                                          1.0f),
                                                red};

                    vertices.push_back(vertex);
                    indices.push_back((uint32_t)vertices.size() - 1);

                    primitiveStart = false;
                }

                const Vertex4RGBA vertex = {tcu::Vec4(originX + float((primitiveNdx + 1) / 2) * primitiveSizeX,
                                                      originY + float((primitiveNdx + 1) % 2) * primitiveSizeY, 0.0f,
                                                      1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back((uint32_t)vertices.size() - 1);
            }
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    {
        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (isRestartPrimitive(primitiveNdx))
            {
                indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
                primitiveStart = true;
            }
            else
            {
                if (primitiveStart && m_restartType != RestartType::ALL)
                {
                    for (int vertexNdx = 0; vertexNdx < 2; vertexNdx++)
                    {
                        const Vertex4RGBA vertex = {
                            tcu::Vec4(originX + float((primitiveNdx + vertexNdx) / 2) * primitiveSizeX,
                                      originY + float((primitiveNdx + vertexNdx) % 2) * primitiveSizeY, 0.0f, 1.0f),
                            red};

                        vertices.push_back(vertex);
                        indices.push_back((uint32_t)vertices.size() - 1);
                    }

                    primitiveStart = false;
                }
                const Vertex4RGBA vertex = {tcu::Vec4(originX + float((primitiveNdx + 2) / 2) * primitiveSizeX,
                                                      originY + float((primitiveNdx + 2) % 2) * primitiveSizeY, 0.0f,
                                                      1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back((uint32_t)vertices.size() - 1);
            }
        }
        break;
    }

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    {
        const float stepAngle = de::min(DE_PI * 0.5f, (2 * DE_PI) / float(primitiveCount));

        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (isRestartPrimitive(primitiveNdx))
            {
                indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
                primitiveStart = true;
            }
            else
            {
                if (primitiveStart && m_restartType != RestartType::ALL)
                {
                    Vertex4RGBA vertex = {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), red};

                    vertices.push_back(vertex);
                    indices.push_back((uint32_t)vertices.size() - 1);

                    vertex.position =
                        tcu::Vec4(primitiveSizeX * deFloatCos(stepAngle * float(primitiveNdx)),
                                  primitiveSizeY * deFloatSin(stepAngle * float(primitiveNdx)), 0.0f, 1.0f);
                    vertices.push_back(vertex);
                    indices.push_back((uint32_t)vertices.size() - 1);

                    primitiveStart = false;
                }

                const Vertex4RGBA vertex = {tcu::Vec4(primitiveSizeX * deFloatCos(stepAngle * float(primitiveNdx + 1)),
                                                      primitiveSizeY * deFloatSin(stepAngle * float(primitiveNdx + 1)),
                                                      0.0f, 1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back((uint32_t)vertices.size() - 1);
            }
        }
        break;
    }

    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        vertices.push_back(defaultVertex);

        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (isRestartPrimitive(primitiveNdx))
            {
                indices.push_back(0);
                indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
                primitiveStart = true;
            }
            else
            {
                if (primitiveStart && m_restartType != RestartType::ALL)
                {
                    indices.push_back(0);

                    const Vertex4RGBA vertex = {tcu::Vec4(originX + float(primitiveNdx / 2) * primitiveSizeX,
                                                          originY + float(primitiveNdx % 2) * primitiveSizeY, 0.0f,
                                                          1.0f),
                                                red};

                    vertices.push_back(vertex);
                    indices.push_back((uint32_t)vertices.size() - 1);

                    primitiveStart = false;
                }

                const Vertex4RGBA vertex = {tcu::Vec4(originX + float((primitiveNdx + 1) / 2) * primitiveSizeX,
                                                      originY + float((primitiveNdx + 1) % 2) * primitiveSizeY, 0.0f,
                                                      1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back((uint32_t)vertices.size() - 1);
            }
        }

        indices.push_back(0);
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        vertices.push_back(defaultVertex);

        for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
        {
            if (isRestartPrimitive(primitiveNdx))
            {
                indices.push_back(InputAssemblyTest::getRestartIndex(indexType));
                primitiveStart = true;
            }
            else
            {
                if (primitiveStart && m_restartType != RestartType::ALL)
                {
                    for (int vertexNdx = 0; vertexNdx < 2; vertexNdx++)
                    {
                        const Vertex4RGBA vertex = {
                            tcu::Vec4(originX + float((primitiveNdx + vertexNdx) / 2) * primitiveSizeX,
                                      originY + float((primitiveNdx + vertexNdx) % 2) * primitiveSizeY, 0.0f, 1.0f),
                            red};

                        vertices.push_back(vertex);
                        indices.push_back((uint32_t)vertices.size() - 1);
                        indices.push_back(0);
                    }

                    primitiveStart = false;
                }

                const Vertex4RGBA vertex = {tcu::Vec4(originX + float((primitiveNdx + 2) / 2) * primitiveSizeX,
                                                      originY + float((primitiveNdx + 2) % 2) * primitiveSizeY, 0.0f,
                                                      1.0f),
                                            red};

                vertices.push_back(vertex);
                indices.push_back((uint32_t)vertices.size() - 1);
                indices.push_back(0);
            }
        }
        break;

    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY, 1, indexType, indices,
                             vertices, std::vector<uint32_t>());
        break;

    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY, 2, indexType, indices,
                             vertices, std::vector<uint32_t>());
        break;

    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    {
        std::vector<uint32_t> adjacencies = {0, 3};

        createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY, 4, indexType, indices,
                             vertices, adjacencies);
    }
    break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY, 3, indexType, indices,
                             vertices, std::vector<uint32_t>());
        break;

    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    {
        std::vector<uint32_t> adjacencies = {1, 3, 5};

        createListPrimitives(primitiveCount, originX, originY, primitiveSizeX, primitiveSizeY, 6, indexType, indices,
                             vertices, adjacencies);
    }
    break;

    default:
        DE_ASSERT(false);
        break;
    }

    vertexData = vertices;
    indexData  = indices;
}

void PrimitiveRestartTest::createListPrimitives(int primitiveCount, float originX, float originY, float primitiveSizeX,
                                                float primitiveSizeY, int verticesPerPrimitive, VkIndexType indexType,
                                                std::vector<uint32_t> &indexData, std::vector<Vertex4RGBA> &vertexData,
                                                std::vector<uint32_t> adjacencies) const
{
    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
    // Tells which vertex of a primitive is used as a restart index.
    // This is decreased each time a restart primitive is used.
    int restartVertexIndex = verticesPerPrimitive - 1;

    for (int primitiveNdx = 0; primitiveNdx < primitiveCount; primitiveNdx++)
    {
        uint32_t nonAdjacentVertexNdx = 0;

        for (int vertexNdx = 0; vertexNdx < verticesPerPrimitive; vertexNdx++)
        {
            if (isRestartPrimitive(primitiveNdx) && vertexNdx == restartVertexIndex)
            {
                indexData.push_back(InputAssemblyTest::getRestartIndex(indexType));

                restartVertexIndex--;
                if (restartVertexIndex < 0)
                    restartVertexIndex = verticesPerPrimitive - 1;

                break;
            }

            if (std::find(adjacencies.begin(), adjacencies.end(), vertexNdx) != adjacencies.end())
            {
                // This is an adjacency vertex index. Add a green vertex that should never end up to the framebuffer.
                const Vertex4RGBA vertex = {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), green};
                vertexData.push_back(vertex);
                indexData.push_back((uint32_t)vertexData.size() - 1);
                continue;
            }

            const Vertex4RGBA vertex = {
                tcu::Vec4(originX + float((primitiveNdx + nonAdjacentVertexNdx) / 2) * primitiveSizeX,
                          originY + float((primitiveNdx + nonAdjacentVertexNdx) % 2) * primitiveSizeY, 0.0f, 1.0f),
                red};

            vertexData.push_back(vertex);
            indexData.push_back((uint32_t)vertexData.size() - 1);
            nonAdjacentVertexNdx++;
        }
    }
}

bool PrimitiveRestartTest::isRestartPrimitive(int primitiveIndex) const
{
    return std::find(m_restartPrimitives.begin(), m_restartPrimitives.end(), primitiveIndex) !=
           m_restartPrimitives.end();
}
#endif // CTS_USES_VULKANSC

// InputAssemblyInstance

InputAssemblyInstance::InputAssemblyInstance(Context &context, PipelineConstructionType pipelineConstructionType,
                                             VkPrimitiveTopology primitiveTopology, bool testPrimitiveRestart,
                                             bool divideDraw, bool secondPass, VkIndexType indexType,
                                             const std::vector<uint32_t> &indexBufferData,
                                             const std::vector<Vertex4RGBA> &vertexBufferData)

    : vkt::TestInstance(context)
    , m_primitiveTopology(primitiveTopology)
    , m_primitiveRestartEnable(testPrimitiveRestart)
    , m_divideDrawEnable(divideDraw)
    , m_multiPassEnable(secondPass)
    , m_indexType(indexType)
    , m_vertices(vertexBufferData)
    , m_indices(indexBufferData)
    , m_renderSize((primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) ? tcu::UVec2(32, 32) : tcu::UVec2(64, 16))
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_graphicsPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                         context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    SimpleAllocator memAlloc(
        vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    const bool patchList                          = m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

    // Create color image
    {
        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
            nullptr,                                                               // const void* pNext;
            0u,                                                                    // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
            m_colorFormat,                                                         // VkFormat format;
            {m_renderSize.x(), m_renderSize.y(), 1u},                              // VkExtent3D extent;
            1u,                                                                    // uint32_t mipLevels;
            1u,                                                                    // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            1u,                                                                    // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout initialLayout;
        };

        m_colorImageCreateInfo = colorImageParams;
        m_colorImage           = createImage(vk, vkDevice, &m_colorImageCreateInfo);

        // Allocate and bind color image memory
        m_colorImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                    m_colorImageAlloc->getOffset()));
    }

    // Create color attachment view
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,    // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            0u,                                          // VkImageViewCreateFlags flags;
            *m_colorImage,                               // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                       // VkImageViewType viewType;
            m_colorFormat,                               // VkFormat format;
            componentMappingRGBA,                        // VkComponentMapping components;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    // Create render passes
    if (m_multiPassEnable)
    {
        m_renderPasses.emplace_back(
            RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat, VK_FORMAT_UNDEFINED,
                              VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
        m_renderPasses.emplace_back(
            RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat, VK_FORMAT_UNDEFINED,
                              VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
        m_renderPasses.emplace_back(
            RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat, VK_FORMAT_UNDEFINED,
                              VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }
    else
    {
        m_renderPasses.emplace_back(RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat));
    }

    // Create framebuffer
    for (auto &rp : m_renderPasses)
    {
        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *rp,                                       // VkRenderPass renderPass;
            1u,                                        // uint32_t attachmentCount;
            &m_colorAttachmentView.get(),              // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        rp.createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
    }

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = PipelineLayoutWrapper(pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
    }

    m_vertexShaderModule   = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    m_fragmentShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

    if (patchList)
    {
        m_tcsShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_tcs"), 0);
        m_tesShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_tes"), 0);
    }

    // Create pipeline
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBA),        // uint32_t stride;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate inputRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
            {
                0u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                0u                             // uint32_t offset;
            },
            {
                1u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                offsetof(Vertex4RGBA, color),  // uint32_t offset;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t vertexAttributeDescriptionCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            0u,                      // VkPipelineInputAssemblyStateCreateFlags flags;
            m_primitiveTopology,     // VkPrimitiveTopology topology;
            m_primitiveRestartEnable // VkBool32 primitiveRestartEnable;
        };

        const std::vector<VkViewport> viewport{makeViewport(m_renderSize)};
        const std::vector<VkRect2D> scissor{makeRect2D(m_renderSize)};

        const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
            false,                                                // VkBool32 blendEnable;
            VK_BLEND_FACTOR_ONE,                                  // VkBlendFactor srcColorBlendFactor;
            VK_BLEND_FACTOR_ZERO,                                 // VkBlendFactor dstColorBlendFactor;
            VK_BLEND_OP_ADD,                                      // VkBlendOp colorBlendOp;
            VK_BLEND_FACTOR_ONE,                                  // VkBlendFactor srcAlphaBlendFactor;
            VK_BLEND_FACTOR_ZERO,                                 // VkBlendFactor dstAlphaBlendFactor;
            VK_BLEND_OP_ADD,                                      // VkBlendOp alphaBlendOp;
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | // VkColorComponentFlags colorWriteMask;
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

        const VkPipelineColorBlendStateCreateInfo colorBlendStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
            false,                                                    // VkBool32 logicOpEnable;
            VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
            1u,                                                       // uint32_t attachmentCount;
            &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
            {0.0f, 0.0f, 0.0f, 0.0f}    // float blendConstants[4];
        };

        VkPipelineDepthStencilStateCreateInfo depthStencilStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
            false,                                                      // VkBool32 depthTestEnable;
            false,                                                      // VkBool32 depthWriteEnable;
            VK_COMPARE_OP_LESS,                                         // VkCompareOp depthCompareOp;
            false,                                                      // VkBool32 depthBoundsTestEnable;
            false,                                                      // VkBool32 stencilTestEnable;
            // VkStencilOpState front;
            {
                VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
                VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
                VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
                VK_COMPARE_OP_NEVER, // VkCompareOp compareOp;
                0u,                  // uint32_t compareMask;
                0u,                  // uint32_t writeMask;
                0u,                  // uint32_t reference;
            },
            // VkStencilOpState back;
            {
                VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
                VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
                VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
                VK_COMPARE_OP_NEVER, // VkCompareOp compareOp;
                0u,                  // uint32_t compareMask;
                0u,                  // uint32_t writeMask;
                0u,                  // uint32_t reference;
            },
            0.0f, // float minDepthBounds;
            1.0f  // float maxDepthBounds;
        };

        m_graphicsPipeline.setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setupVertexInputState(&vertexInputStateParams, &inputAssemblyStateParams)
            .setupPreRasterizationShaderState(viewport, scissor, m_pipelineLayout, *m_renderPasses[0], 0u,
                                              m_vertexShaderModule, nullptr, m_tcsShaderModule, m_tesShaderModule)
            .setupFragmentShaderState(m_pipelineLayout, *m_renderPasses[0], 0u, m_fragmentShaderModule,
                                      &depthStencilStateParams)
            .setupFragmentOutputState(*m_renderPasses[0], 0u, &colorBlendStateParams)
            .setMonolithicPipelineLayout(m_pipelineLayout)
            .buildPipeline();
    }

    // Create vertex and index buffer
    {
        const VkBufferCreateInfo indexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_indices.size() * sizeof(uint32_t),  // VkDeviceSize size;
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,    // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0u,                                      // VkBufferCreateFlags flags;
            m_vertices.size() * sizeof(Vertex4RGBA), // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,       // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,               // VkSharingMode sharingMode;
            1u,                                      // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                        // const uint32_t* pQueueFamilyIndices;
        };

        m_indexBuffer      = createBuffer(vk, vkDevice, &indexBufferParams);
        m_indexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_indexBuffer),
                                               MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_indexBuffer, m_indexBufferAlloc->getMemory(),
                                     m_indexBufferAlloc->getOffset()));

        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        // Load vertices into index buffer
        if (m_indexType == VK_INDEX_TYPE_UINT32)
        {
            deMemcpy(m_indexBufferAlloc->getHostPtr(), m_indices.data(), m_indices.size() * sizeof(uint32_t));
        }
        else if (m_indexType == VK_INDEX_TYPE_UINT8_EXT)
        {
            uploadIndexBufferData8((uint8_t *)m_indexBufferAlloc->getHostPtr(), m_indices);
        }
        else // m_indexType == VK_INDEX_TYPE_UINT16
        {
            uploadIndexBufferData16((uint16_t *)m_indexBufferAlloc->getHostPtr(), m_indices);
        }

        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));

        flushAlloc(vk, vkDevice, *m_indexBufferAlloc);
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

    {
        const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

        const VkImageMemoryBarrier initialBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                     nullptr,
                                                     0u,                                       // srcAccessMask
                                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // dstAccessMask
                                                     VK_IMAGE_LAYOUT_UNDEFINED,                // oldLayout
                                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // newLayout
                                                     VK_QUEUE_FAMILY_IGNORED,
                                                     VK_QUEUE_FAMILY_IGNORED,
                                                     *m_colorImage,
                                                     {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

        // Barrier between passes
        const VkImageMemoryBarrier passBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                  nullptr,
                                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
                                                  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // dstAccessMask
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // oldLayout
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // newLayout
                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  VK_QUEUE_FAMILY_IGNORED,
                                                  *m_colorImage,
                                                  {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

        const VkDeviceSize vertexBufferOffset = 0;
        const VkRect2D fullScreen             = makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y());
        const VkRect2D leftHalf               = makeRect2D(0, 0, m_renderSize.x() / 2, m_renderSize.y());
        const VkRect2D rightHalf = makeRect2D(m_renderSize.x() / 2, 0, m_renderSize.x() / 2, m_renderSize.y());

        const uint32_t totalIndices         = (uint32_t)m_indices.size();
        const uint32_t verticesPerPrimitive = getVerticesPerPrimitive(m_primitiveTopology);

        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0u, nullptr, 0u,
                              nullptr, 1u, &initialBarrier);

        if (m_divideDrawEnable)
        {
            DE_ASSERT(verticesPerPrimitive > 0);

            const uint32_t maxPrimitivesFirst = (totalIndices / verticesPerPrimitive) / 2;
            const uint32_t firstHalfCount     = maxPrimitivesFirst * verticesPerPrimitive;
            const uint32_t secondHalfCount    = totalIndices - firstHalfCount;

            m_renderPasses[0].begin(vk, *m_cmdBuffer, fullScreen, attachmentClearValue);

            m_graphicsPipeline.bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdBindIndexBuffer(*m_cmdBuffer, *m_indexBuffer, 0, m_indexType);

            vk.cmdDrawIndexed(*m_cmdBuffer, firstHalfCount, 1, 0, 0, 0);
            vk.cmdDrawIndexed(*m_cmdBuffer, secondHalfCount, 1, firstHalfCount, 0, 0);

            m_renderPasses[0].end(vk, *m_cmdBuffer);
        }
        else if (m_multiPassEnable)
        {
            DE_ASSERT(verticesPerPrimitive > 0);

            const uint32_t maxPrimitivesFirst = (totalIndices / verticesPerPrimitive) / 2;
            const uint32_t firstHalfCount     = maxPrimitivesFirst * verticesPerPrimitive;
            const uint32_t secondHalfCount    = totalIndices - firstHalfCount;

            // Clear fullScreen
            m_renderPasses[0].begin(vk, *m_cmdBuffer, fullScreen, attachmentClearValue);
            m_renderPasses[0].end(vk, *m_cmdBuffer);

            vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &passBarrier);

            m_renderPasses[1].begin(vk, *m_cmdBuffer, leftHalf, attachmentClearValue);
            m_graphicsPipeline.bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdBindIndexBuffer(*m_cmdBuffer, *m_indexBuffer, 0, m_indexType);
            // Overlap the secont half needed to ensure render continuity
            vk.cmdDrawIndexed(*m_cmdBuffer, firstHalfCount + verticesPerPrimitive, 1, 0, 0, 0);
            m_renderPasses[1].end(vk, *m_cmdBuffer);

            vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &passBarrier);

            m_renderPasses[2].begin(vk, *m_cmdBuffer, rightHalf, attachmentClearValue);
            // Overlap the first half needed to ensure render continuity
            vk.cmdDrawIndexed(*m_cmdBuffer, secondHalfCount + verticesPerPrimitive, 1,
                              firstHalfCount - verticesPerPrimitive, 0, 0);

            m_renderPasses[2].end(vk, *m_cmdBuffer);
        }
        else
        {
            m_renderPasses[0].begin(vk, *m_cmdBuffer, fullScreen, attachmentClearValue);

            m_graphicsPipeline.bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdBindIndexBuffer(*m_cmdBuffer, *m_indexBuffer, 0, m_indexType);
            vk.cmdDrawIndexed(*m_cmdBuffer, (uint32_t)m_indices.size(), 1, 0, 0, 0);

            m_renderPasses[0].end(vk, *m_cmdBuffer);
        }

        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

InputAssemblyInstance::~InputAssemblyInstance(void)
{
}

tcu::TestStatus InputAssemblyInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    const VkQueue queue       = m_context.getUniversalQueue();

    submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    return verifyImage();
}

tcu::TestStatus InputAssemblyInstance::verifyImage(void)
{
    const tcu::TextureFormat tcuColorFormat   = mapVkFormat(m_colorFormat);
    const tcu::TextureFormat tcuStencilFormat = tcu::TextureFormat();
    const ColorVertexShader vertexShader;
    const ColorFragmentShader fragmentShader(tcuColorFormat, tcuStencilFormat);
    const rr::Program program(&vertexShader, &fragmentShader);
    ReferenceRenderer refRenderer(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuStencilFormat, &program);
    bool compareOk = false;

    // Render reference image
    {
        // The reference for tessellated patches are drawn using ordinary triangles.
        const rr::PrimitiveType topology = m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST ?
                                               rr::PrimitiveType::PRIMITIVETYPE_TRIANGLES :
                                               mapVkPrimitiveTopology(m_primitiveTopology);
        rr::RenderState renderState(refRenderer.getViewportState(),
                                    m_context.getDeviceProperties().limits.subPixelPrecisionBits);

        if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
            renderState.point.pointSize = 3.0f;

        if (m_primitiveRestartEnable)
        {
            std::vector<uint32_t> indicesRange;

            for (size_t indexNdx = 0; indexNdx < m_indices.size(); indexNdx++)
            {
                const bool isRestart = InputAssemblyTest::isRestartIndex(m_indexType, m_indices[indexNdx]);

                if (!isRestart)
                    indicesRange.push_back(m_indices[indexNdx]);

                if (isRestart || indexNdx == (m_indices.size() - 1))
                {
                    // Draw the range of indices found so far

                    std::vector<Vertex4RGBA> nonIndexedVertices;
                    for (size_t i = 0; i < indicesRange.size(); i++)
                        nonIndexedVertices.push_back(m_vertices[indicesRange[i]]);

                    refRenderer.draw(renderState, topology, nonIndexedVertices);
                    indicesRange.clear();
                }
            }
        }
        else
        {
            std::vector<Vertex4RGBA> nonIndexedVertices;
            for (size_t i = 0; i < m_indices.size(); i++)
                nonIndexedVertices.push_back(m_vertices[m_indices[i]]);

            refRenderer.draw(renderState, topology, nonIndexedVertices);
        }
    }

    // Compare result with reference image
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice vkDevice         = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        SimpleAllocator allocator(
            vk, vkDevice,
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
        de::UniquePtr<tcu::TextureLevel> result(readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator,
                                                                    *m_colorImage, m_colorFormat, m_renderSize)
                                                    .release());

        compareOk = tcu::intThresholdPositionDeviationCompare(
            m_context.getTestContext().getLog(), "IntImageCompare", "Image comparison", refRenderer.getAccess(),
            result->getAccess(), tcu::UVec4(2, 2, 2, 2), tcu::IVec3(1, 1, 0), true, tcu::COMPARE_LOG_RESULT);
    }

    if (compareOk)
        return tcu::TestStatus::pass("Result image matches reference");
    else
        return tcu::TestStatus::fail("Image mismatch");
}

void InputAssemblyInstance::uploadIndexBufferData16(uint16_t *destPtr, const std::vector<uint32_t> &indexBufferData)
{
    for (size_t i = 0; i < indexBufferData.size(); i++)
    {
        DE_ASSERT(indexBufferData[i] <= 0xFFFF);
        destPtr[i] = (uint16_t)indexBufferData[i];
    }
}

void InputAssemblyInstance::uploadIndexBufferData8(uint8_t *destPtr, const std::vector<uint32_t> &indexBufferData)
{
    for (size_t i = 0; i < indexBufferData.size(); i++)
    {
        DE_ASSERT(indexBufferData[i] <= 0xFF);
        destPtr[i] = (uint8_t)indexBufferData[i];
    }
}

uint32_t InputAssemblyInstance::getVerticesPerPrimitive(VkPrimitiveTopology topology)
{
    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return 1;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        return 2;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        return 3;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        return 4;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        return 6;
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        return 3;
    default:
        // Dynamic vertices per primitive are not supported  (return 0)
        return 0;
    }
}

// Utilities for test names

std::string getPrimitiveTopologyCaseName(VkPrimitiveTopology topology)
{
    const std::string fullName = getPrimitiveTopologyName(topology);

    DE_ASSERT(de::beginsWith(fullName, "VK_PRIMITIVE_TOPOLOGY_"));

    return de::toLower(fullName.substr(22));
}

de::MovePtr<tcu::TestCaseGroup> createPrimitiveTopologyTests(tcu::TestContext &testCtx,
                                                             PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> primitiveTopologyTests(new tcu::TestCaseGroup(testCtx, "primitive_topology"));

    de::MovePtr<tcu::TestCaseGroup> indexUint16Tests(new tcu::TestCaseGroup(testCtx, "index_type_uint16"));
    de::MovePtr<tcu::TestCaseGroup> indexUint32Tests(new tcu::TestCaseGroup(testCtx, "index_type_uint32"));
    de::MovePtr<tcu::TestCaseGroup> indexUint8Tests(new tcu::TestCaseGroup(testCtx, "index_type_uint8"));

    for (int topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(InputAssemblyTest::s_primitiveTopologies); topologyNdx++)
    {
        const VkPrimitiveTopology topology = InputAssemblyTest::s_primitiveTopologies[topologyNdx];

        indexUint16Tests->addChild(new PrimitiveTopologyTest(testCtx, getPrimitiveTopologyCaseName(topology),
                                                             pipelineConstructionType, topology, VK_INDEX_TYPE_UINT16));

        indexUint32Tests->addChild(new PrimitiveTopologyTest(testCtx, getPrimitiveTopologyCaseName(topology),
                                                             pipelineConstructionType, topology, VK_INDEX_TYPE_UINT32));

        indexUint8Tests->addChild(new PrimitiveTopologyTest(testCtx, getPrimitiveTopologyCaseName(topology),
                                                            pipelineConstructionType, topology,
                                                            VK_INDEX_TYPE_UINT8_EXT));
    }

    primitiveTopologyTests->addChild(indexUint16Tests.release());
    primitiveTopologyTests->addChild(indexUint32Tests.release());
    primitiveTopologyTests->addChild(indexUint8Tests.release());

    return primitiveTopologyTests;
}

#ifndef CTS_USES_VULKANSC
// RADV had an issue where it would not reset some of the primitive restart state properly when mixing indexed with
// non-indexed draws. The purpose of these tests is to try to exercise some of those circumstances and make sure
// implementations work correctly when mixing indexed and non-indexed draws with primitive restart.
//
// * extraIndexedDraws: we normally proceed to draw with indices first, then switch to normal draws. When this is
//   true, go back to indexed draws after the last non-indexed one, to check the transition in the opposite direction.
//
// * triangleList: use triangle lists instead of triangle strips.
//
// * dynamicTopology: make the topology dynamic to make things a bit more complicated.
//
// * largeNonIndexedDraw: if the implementation incorrectly disables primitive restart, it may run into issues when
//   drawing a large number of vertices in a single draw call. Specifically, when the vertex index goes over the restart
//   index. Since we use 16-bit indices in these tests, largeNonIndexedDraw exchanges the set of non-indexed draws for a
//   single call that draws over one of the quadrants, using a large vertex count, and makes sure only the last couple
//   of triangles actually cover the quadrant, so the implementation needs to reach the end correctly.
//
struct PrimitiveRestartMixParams
{
    PipelineConstructionType constructionType;
    bool extraIndexedDraws; // Draw using indices after the first draw without them; otherwise, skip those quadrants.
    bool triangleList;      // true == triangle list, false == triangle strip
    bool dynamicTopology;
    bool largeNonIndexedDraw; // For the non-indexed draw, use a separate large vertex buffer.
};

class PrimitiveRestartMixTest : public vkt::TestInstance
{
public:
    PrimitiveRestartMixTest(Context &context, const PrimitiveRestartMixParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    ~PrimitiveRestartMixTest() = default;

    tcu::TestStatus iterate() override;

protected:
    const PrimitiveRestartMixParams m_params;
};

class PrimitiveRestartMixCase : public vkt::TestCase
{
public:
    PrimitiveRestartMixCase(tcu::TestContext &testCtx, const std::string &name, const PrimitiveRestartMixParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~PrimitiveRestartMixCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new PrimitiveRestartMixTest(context, m_params);
    }

protected:
    const PrimitiveRestartMixParams m_params;
};

void PrimitiveRestartMixCase::checkSupport(Context &context) const
{
    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);

    if (m_params.dynamicTopology)
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

    if (m_params.triangleList)
        context.requireDeviceFunctionality("VK_EXT_primitive_topology_list_restart");
}

void PrimitiveRestartMixCase::initPrograms(vk::SourceCollections &dst) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=1) in vec4 inColor;\n"
         << "layout (location=0) out vec4 vtxColor;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    vtxColor = inColor;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) in vec4 vtxColor;\n"
         << "layout (location=0) out vec4 fragColor;\n"
         << "void main (void) {\n"
         << "    fragColor = vtxColor;\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

// The idea is drawing once with indices and primitive restart enabled and then drawing again without indices and
// without disabling primitive restart. For this, we will create a framebuffer and draw over each quadrant in each step.
tcu::TestStatus PrimitiveRestartMixTest::iterate()
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(32, 32, 1);
    const auto extent  = makeExtent3D(fbExtent);
    const auto format  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto fbUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    const auto quadrantExtent      = fbExtent / tcu::IVec3(2, 2, 1);
    const auto quadrantExtentU     = quadrantExtent.asUint();
    const auto verticesPerPixel    = (m_params.triangleList ? 6u : 4u);
    const auto totalVertices       = verticesPerPixel * extent.width * extent.height;
    const auto verticesPerQuadrant = verticesPerPixel * quadrantExtentU.x() * quadrantExtentU.y();

    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extent, format, fbUsage, VK_IMAGE_TYPE_2D);

    const std::vector<tcu::Vec4> quadrantColors{
        tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
    };

    std::vector<Vertex4RGBA> vertices;
    std::vector<Vertex4RGBA> extraVertices;

    const auto kRestart = std::numeric_limits<uint16_t>::max(); // Restart index value.
    DE_ASSERT(totalVertices < static_cast<uint32_t>(kRestart)); // Or else we would run into the restart value.
    std::vector<uint16_t> indices;

    const auto normalizeCoords = [](int c, int len)
    { return static_cast<float>(c) / static_cast<float>(len) * 2.0f - 1.0f; };

    const auto appendTrianglesForPixel = [&](int pixX, int pixY, int width, int height, const tcu::Vec4 &color)
    {
        const float xLeft   = normalizeCoords(pixX, width);
        const float xRight  = normalizeCoords(pixX + 1, width);
        const float yTop    = normalizeCoords(pixY, height);
        const float yBottom = normalizeCoords(pixY + 1, height);

        const std::vector<tcu::Vec4> pixelCorners{
            tcu::Vec4(xLeft, yTop, 0.0f, 1.0f),
            tcu::Vec4(xLeft, yBottom, 0.0f, 1.0f),
            tcu::Vec4(xRight, yTop, 0.0f, 1.0f),
            tcu::Vec4(xRight, yBottom, 0.0f, 1.0f),
        };

        if (m_params.triangleList)
        {
            vertices.push_back({pixelCorners.at(0u), color});
            vertices.push_back({pixelCorners.at(1u), color});
            vertices.push_back({pixelCorners.at(2u), color});
            vertices.push_back({pixelCorners.at(2u), color});
            vertices.push_back({pixelCorners.at(1u), color});
            vertices.push_back({pixelCorners.at(3u), color});
        }
        else
        {
            for (const auto &coords : pixelCorners)
                vertices.push_back({coords, color});
        }
    };

    struct Quadrant
    {
        int offsetX;
        int offsetY;
        int width;
        int height;
        bool skipGeometry; // Insert restart indices for this quadrant so no geometry is drawn.
        bool indexedDraw;  // Draw this quadrant with/without indices.
    };

    // clang-format off
    const std::vector<Quadrant> quadrants{
        {0,                  0,                  quadrantExtent.x(), quadrantExtent.y(), false, true},  // Top left.
        {quadrantExtent.x(), quadrantExtent.y(), quadrantExtent.x(), quadrantExtent.y(), true,  true},  // Bottom right.
        {quadrantExtent.x(), 0,                  quadrantExtent.x(), quadrantExtent.y(), false, false}, // Top right.
        {0,                  quadrantExtent.y(), quadrantExtent.x(), quadrantExtent.y(), false, true},  // Bottom left.
    };
    // clang-format on

    DE_ASSERT(quadrants.size() == quadrantColors.size());
    uint32_t vertexCount = 0u;

    const auto vertexCountWithAppendix = static_cast<uint32_t>(kRestart) + 1u;
    vertices.reserve(vertexCountWithAppendix);
    indices.reserve(totalVertices);

    for (size_t i = 0; i < quadrants.size(); ++i)
    {
        const auto &quadrant = quadrants.at(i);
        const auto &color    = quadrantColors.at(i);

        for (int y = 0; y < quadrant.height; ++y)
            for (int x = 0; x < quadrant.width; ++x)
            {
                // Append the vertices for this pixel.
                appendTrianglesForPixel(x + quadrant.offsetX, y + quadrant.offsetY, fbExtent.x(), fbExtent.y(), color);

                // Append the vertex indices for this pixel.
                for (uint32_t j = 0; j < verticesPerPixel; ++j)
                    indices.push_back(static_cast<uint16_t>(vertexCount + j));

                // Make sure indices for quadrants without geometry are set up in ways that restart the primitive.
                if (quadrant.skipGeometry)
                {
                    if (m_params.triangleList)
                    {
                        // Use the primitive restart index for both added triangles.
                        indices.at(indices.size() - 1u) = kRestart;
                        indices.at(indices.size() - 4u) = kRestart;
                    }
                    else
                    {
                        // Similar for the strip, but we overwrite the indices of the last two vertices in the quad.
                        indices.at(indices.size() - 1u) = kRestart;
                        indices.at(indices.size() - 2u) = kRestart;
                    }
                }
                vertexCount += verticesPerPixel;
            }

        // Maybe use an alternative large vertex buffer for quadrants that do not use indexed draws.
        if (m_params.largeNonIndexedDraw && !quadrant.indexedDraw)
        {
            // This should only happen with one quadrant.
            DE_ASSERT(extraVertices.empty());
            DE_ASSERT(m_params.triangleList);

            // We'll add a large list of out-of-screen vertinces and, after that, we'll append a couple of triangles
            // that cover the whole quadrant, to make sure they are used correctly.
            extraVertices.reserve(vertexCountWithAppendix * 2u);

            while (extraVertices.size() <= static_cast<size_t>(vertexCountWithAppendix))
            {
                extraVertices.push_back({{100.0f, 100.0f, 0.0f, 1.0f}, color});
                extraVertices.push_back({{100.0f, 110.0f, 0.0f, 1.0f}, color});
                extraVertices.push_back({{110.0f, 100.0f, 0.0f, 1.0f}, color});
            }

            // Final meaningfull triangles.
            {
                const auto xLeft   = normalizeCoords(quadrant.offsetX, fbExtent.x());
                const auto xRight  = normalizeCoords(quadrant.offsetX + quadrant.width, fbExtent.x());
                const auto yTop    = normalizeCoords(quadrant.offsetY, fbExtent.y());
                const auto yBottom = normalizeCoords(quadrant.offsetY + quadrant.height, fbExtent.y());

                extraVertices.push_back({tcu::Vec4(xLeft, yTop, 0.0f, 1.0f), color});
                extraVertices.push_back({tcu::Vec4(xLeft, yBottom, 0.0f, 1.0f), color});
                extraVertices.push_back({tcu::Vec4(xRight, yTop, 0.0f, 1.0f), color});
                extraVertices.push_back({tcu::Vec4(xRight, yTop, 0.0f, 1.0f), color});
                extraVertices.push_back({tcu::Vec4(xLeft, yBottom, 0.0f, 1.0f), color});
                extraVertices.push_back({tcu::Vec4(xRight, yBottom, 0.0f, 1.0f), color});
            }
        }
    }

    // Make sure the restart index is valid and contains coordinates that will trigger a problem by creating valid
    // triangles that provide coverage to one or more pixel in the framebuffer. As the restart indices will be used for
    // the bottom right quadrant, we use the middle of the right edge of the framebuffer as the invalid coordinate. If
    // it's used, it should provide some coverage.
    while (de::sizeU32(vertices) < vertexCountWithAppendix)
        vertices.push_back({tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), quadrantColors.at(1u)});

    const VkDeviceSize vtxBufferOffset = 0;
    const auto vtxBuffer =
        makeBufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto idxBuffer =
        makeBufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // Extra vertices buffer, if needed.
    std::unique_ptr<BufferWithMemory> extraVtxBuffer;
    if (!extraVertices.empty())
        extraVtxBuffer.reset(
            makeBufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, extraVertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
                .release());

    const std::vector<VkVertexInputBindingDescription> inputBindings{
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(Vertex4RGBA), VK_VERTEX_INPUT_RATE_VERTEX),
    };

    const std::vector<VkVertexInputAttributeDescription> inputAttributes{
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(Vertex4RGBA, position))),
        makeVertexInputAttributeDescription(1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(Vertex4RGBA, color))),
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(inputBindings),
        de::dataOrNull(inputBindings),
        de::sizeU32(inputAttributes),
        de::dataOrNull(inputAttributes),
    };

    const auto goodTopology =
        (m_params.triangleList ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    const auto badTopology =
        (m_params.triangleList ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const auto staticTopology = (m_params.dynamicTopology ? badTopology : goodTopology);

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0u, staticTopology, VK_TRUE,
    };

    std::vector<VkDynamicState> dynamicStates;
    if (m_params.dynamicTopology)
        dynamicStates.push_back(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device);
    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));

    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, format);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(), extent.width,
                                 extent.height);

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                     m_params.constructionType);
    pipeline.setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultMultisampleState()
        .setDynamicState(&dynamicStateCreateInfo)
        .setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader)
        .setupFragmentOutputState(renderPass.get(), 0u)
        .buildPipeline();

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
    pipeline.bind(cmdBuffer);
    if (m_params.dynamicTopology)
        ctx.vkd.cmdSetPrimitiveTopology(cmdBuffer, goodTopology);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vtxBuffer->get(), &vtxBufferOffset);
    ctx.vkd.cmdBindIndexBuffer(cmdBuffer, idxBuffer->get(), 0, VK_INDEX_TYPE_UINT16);

    // Draw loop.
    uint32_t vertexDrawOffset = 0u;
    bool drewWithoutIndices   = false;

    for (size_t i = 0; i < quadrants.size(); ++i)
    {
        const auto &quadrant = quadrants.at(i);

        // If we've already drawn without indices and this quadrant would be drawn with them, but we do not want extra
        // indexed draws, skip the quadrant.
        if (drewWithoutIndices && quadrant.indexedDraw && !m_params.extraIndexedDraws)
            continue;

        bool breakLoop = false; // Helper to break out of both loops.
        for (int y = 0; y < quadrantExtent.y(); ++y)
        {
            for (int x = 0; x < quadrantExtent.x(); ++x)
            {
                const auto firstVertex =
                    static_cast<uint32_t>(y * quadrantExtent.x() + x) * verticesPerPixel + vertexDrawOffset;

                if (quadrant.indexedDraw)
                    ctx.vkd.cmdDrawIndexed(cmdBuffer, verticesPerPixel, 1u, firstVertex, 0, 0u);
                else
                {
                    drewWithoutIndices = true;

                    if (m_params.largeNonIndexedDraw)
                    {
                        // Single draw for the whole quadrant, as a large triangle list.
                        DE_ASSERT(extraVtxBuffer);
                        DE_ASSERT(m_params.triangleList);

                        if (x == 0 && y == 0)
                        {
                            ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &extraVtxBuffer->get(), &vtxBufferOffset);
                            ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(extraVertices), 1u, 0u, 0u);
                            ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vtxBuffer->get(), &vtxBufferOffset);

                            breakLoop = true;
                            break;
                        }
                    }
                    else
                        ctx.vkd.cmdDraw(cmdBuffer, verticesPerPixel, 1u, firstVertex, 0u);
                }
            }

            if (breakLoop)
                break;
        }

        vertexDrawOffset += verticesPerQuadrant;
    }

    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto tcuFormat = mapVkFormat(format);
    tcu::TextureLevel refLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());

    tcu::PixelBufferAccess reference = refLevel.getAccess();
    tcu::clear(reference, clearColor);

    {
        const auto topLeft = tcu::getSubregion(reference, 0, 0, quadrantExtent.x(), quadrantExtent.y());
        tcu::clear(topLeft, quadrantColors.at(0u));

        const auto topRight =
            tcu::getSubregion(reference, quadrantExtent.x(), 0, quadrantExtent.x(), quadrantExtent.y());
        tcu::clear(topRight, quadrantColors.at(2u));

        if (m_params.extraIndexedDraws)
        {
            const auto bottomLeft =
                tcu::getSubregion(reference, 0, quadrantExtent.y(), quadrantExtent.x(), quadrantExtent.y());
            tcu::clear(bottomLeft, quadrantColors.at(3u));
        }
    }

    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, colorBufferAlloc.getHostPtr());

    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_EVERYTHING))
        TCU_FAIL("Unexpected colors found in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

de::MovePtr<tcu::TestCaseGroup> createPrimitiveRestartTests(tcu::TestContext &testCtx,
                                                            PipelineConstructionType pipelineConstructionType)
{
    const VkPrimitiveTopology primitiveRestartTopologies[] = {
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,

        // Supported with VK_EXT_primitive_topology_list_restart
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST};

    // Topologies types capable to perform clear vertex division (list types with fixed vertices per primitive)
    const VkPrimitiveTopology mixedPrimitiveRestartTopologies[] = {
        // Supported with VK_EXT_primitive_topology_list_restart
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST};

    de::MovePtr<tcu::TestCaseGroup> primitiveRestartTests(new tcu::TestCaseGroup(testCtx, "primitive_restart"));

    de::MovePtr<tcu::TestCaseGroup> indexUint16Tests(new tcu::TestCaseGroup(testCtx, "index_type_uint16"));
    de::MovePtr<tcu::TestCaseGroup> indexUint32Tests(new tcu::TestCaseGroup(testCtx, "index_type_uint32"));
    de::MovePtr<tcu::TestCaseGroup> indexUint8Tests(new tcu::TestCaseGroup(testCtx, "index_type_uint8"));

    constexpr struct RestartTest
    {
        RestartType type;
        const char *name;
    } restartTypes[] = {
        {
            RestartType::NORMAL,
            "",
        },
        {
            RestartType::NONE,
            "no_restart_",
        },
        {
            RestartType::ALL,
            "restart_all_",
        },
        {
            RestartType::DIVIDE,
            "divide_draw_",
        },
        {
            RestartType::SECOND_PASS,
            "second_pass_",
        },
    };

    for (int useRestartNdx = 0; useRestartNdx < DE_LENGTH_OF_ARRAY(restartTypes); useRestartNdx++)
    {
        const RestartTest &restartType = restartTypes[useRestartNdx];
        const bool isSplitTest =
            (restartType.type == RestartType::DIVIDE || restartType.type == RestartType::SECOND_PASS);

        // Select appropriate topology array based on test type
        const VkPrimitiveTopology *topologies =
            isSplitTest ? mixedPrimitiveRestartTopologies : primitiveRestartTopologies;
        const int topologyCount = isSplitTest ? DE_LENGTH_OF_ARRAY(mixedPrimitiveRestartTopologies) :
                                                DE_LENGTH_OF_ARRAY(primitiveRestartTopologies);

        for (int topologyNdx = 0; topologyNdx < topologyCount; topologyNdx++)
        {
            const VkPrimitiveTopology topology = topologies[topologyNdx];

            if (topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && restartType.type == RestartType::ALL)
            {
                continue;
            }

            indexUint16Tests->addChild(
                new PrimitiveRestartTest(testCtx, restartType.name + getPrimitiveTopologyCaseName(topology),
                                         pipelineConstructionType, topology, VK_INDEX_TYPE_UINT16, restartType.type));

            indexUint32Tests->addChild(
                new PrimitiveRestartTest(testCtx, restartType.name + getPrimitiveTopologyCaseName(topology),
                                         pipelineConstructionType, topology, VK_INDEX_TYPE_UINT32, restartType.type));

            indexUint8Tests->addChild(new PrimitiveRestartTest(
                testCtx, restartType.name + getPrimitiveTopologyCaseName(topology), pipelineConstructionType, topology,
                VK_INDEX_TYPE_UINT8_EXT, restartType.type));
        }
    }

    // Tests that have primitive restart disabled, but have indices with restart index value.
    if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
    {
        struct
        {
            std::string name;
            std::vector<std::string> requirements;
        } tests[] = {
            {"line_list", {"VK_EXT_primitive_topology_list_restart"}},
            {"line_list_with_adjacency", {"Features.geometryShader", "VK_EXT_primitive_topology_list_restart"}},
            {"line_strip", {}},
            {"line_strip_with_adjacency", {"Features.geometryShader"}},
            {"patch_list", {"VK_EXT_primitive_topology_list_restart", "Features.tessellationShader"}},
            {"point_list", {"VK_EXT_primitive_topology_list_restart"}},
            {"triangle_fan", {}},
            {"triangle_list", {"VK_EXT_primitive_topology_list_restart"}},
            {"triangle_list_with_adjacency", {"Features.geometryShader", "VK_EXT_primitive_topology_list_restart"}},
            {"triangle_strip", {}},
            {"triangle_strip_with_adjacency", {"Features.geometryShader"}}};

        const std::string dataDir = "pipeline/input_assembly/primitive_restart";

        for (auto &test : tests)
        {
            std::string testName = "restart_disabled_" + test.name;
            indexUint16Tests->addChild(cts_amber::createAmberTestCase(testCtx, testName.c_str(), dataDir.c_str(),
                                                                      testName + "_uint16.amber", test.requirements));
            test.requirements.push_back("IndexTypeUint8Features.indexTypeUint8");
            indexUint8Tests->addChild(cts_amber::createAmberTestCase(testCtx, testName.c_str(), dataDir.c_str(),
                                                                     testName + "_uint8.amber", test.requirements));
        }
    }

    primitiveRestartTests->addChild(indexUint16Tests.release());
    primitiveRestartTests->addChild(indexUint32Tests.release());
    primitiveRestartTests->addChild(indexUint8Tests.release());

    {
        de::MovePtr<tcu::TestCaseGroup> restartMixGroup(new tcu::TestCaseGroup(testCtx, "restart_mix"));
        for (const bool extraDraw : {false, true})
            for (const bool triangleList : {false, true})
                for (const bool dynamicTopology : {false, true})
                    for (const bool largeNonIndexedDraw : {false, true})
                    {
                        if (largeNonIndexedDraw && !triangleList)
                            continue;

                        const auto testName = std::string("restart_mix") + (extraDraw ? "_extra_draw" : "") +
                                              (triangleList ? "_triangle_list" : "") +
                                              (dynamicTopology ? "_dynamic_topo" : "") +
                                              (largeNonIndexedDraw ? "_large_non_indexed_draw" : "");
                        const PrimitiveRestartMixParams params{pipelineConstructionType, extraDraw, triangleList,
                                                               dynamicTopology, largeNonIndexedDraw};
                        restartMixGroup->addChild(new PrimitiveRestartMixCase(testCtx, testName, params));
                    }
        primitiveRestartTests->addChild(restartMixGroup.release());
    }

    return primitiveRestartTests;
}
#endif // CTS_USES_VULKANSC

} // namespace

tcu::TestCaseGroup *createInputAssemblyTests(tcu::TestContext &testCtx,
                                             PipelineConstructionType pipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> inputAssemblyTests(new tcu::TestCaseGroup(testCtx, "input_assembly"));

    inputAssemblyTests->addChild(createPrimitiveTopologyTests(testCtx, pipelineConstructionType).release());
#ifndef CTS_USES_VULKANSC
    inputAssemblyTests->addChild(createPrimitiveRestartTests(testCtx, pipelineConstructionType).release());
#endif // CTS_USES_VULKANSC

    return inputAssemblyTests.release();
}

} // namespace pipeline
} // namespace vkt
