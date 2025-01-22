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
 * \brief Device Generated Commands EXT Graphics Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsDrawTestsExt.hpp"
#include "util/vktShaderObjectUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilCommon.hpp"
#include "vktDGCUtilExt.hpp"

#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "vktTestCaseUtil.hpp"

#include <sstream>
#include <map>
#include <memory>
#include <algorithm>
#include <iterator>
#include <bitset>
#include <utility>

namespace vkt
{
namespace DGC
{

namespace
{

using namespace vk;

/*
TEST MECHANISM FOR VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_EXT

2x2 pixel framebuffer, with 3 sequences: lets call them 0, 1, 2

Each sequence will draw triangles over the following pixels:

0 1
1 2

The indirect commands layout will contain:

1. VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT (optional)
2. VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT
3. VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_EXT
4. VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_EXT

The order of the two intermediate elements will be chosen pseudorandomly.

The structure in the indirect commands buffer needs to be:

typedef struct VkDrawIndirectCommand {
    uint32_t  vertexCount;
    uint32_t  instanceCount;
    uint32_t  firstVertex;
    uint32_t  firstInstance;
} VkDrawIndirectCommand;

And we want to check if each of those 4 parameters is properly taken into account.

We'll create vertex coordinates for 4 triangles (12 vertices in total) each covering one of the framebuffer pixels.

Lets call these triangles A, B, C and D, by default drawn as:

A B
C D

Over the 4 pixels of the framebuffer.

To make things more interesting and use a different vertex buffer in each sequence, we'll create a vertex buffer for each of the 3
sequences with different configurations, so that the stride and size also varies a bit. ZZ represents Zeros.

Buffer 0: A0 A1 A2                              # Smaller buffer.
Buffer 1: B0 ZZ B1 ZZ B2 ZZ C0 ZZ C1 ZZ C2 ZZ   # Wider stride.
Buffer 2: ZZ ZZ ZZ ZZ ZZ ZZ ZZ ZZ ZZ D0 D1 D2   # Non-zero first vertex.

The vertex count for each sequence varies: 3, 6, 3.

The instance count could change also for each sequence: 1, 1, 2.
  - And we could pass the instance index to the fragment shader and use it as the green channel.

The first vertex will vary in each call: 0, 0, 9

The first instance can also vary per call: 0 1 0

The push constants will determine the red channel, and will vary in each sequence.

The test setup is good for cases where VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT is not used. If it is, how can we make a
difference there?

We can provide 2 fragment shaders: one stores 0 in the blue channel and another one stores 1. We can make the
sequences use pipelines 0 0 1, for example. What about the vertex shader? One could mirror coordinates in the X dimension for the
*first* triangle *only* and another one would not. With this, we can determine the expected color for each of the 4 pixels.

We can add variations that use tessellation or geometry, and in that case the tessellation or geometry shaders would be the ones
flipping coordinates in the X dimension for the first triangle. As VertexIndex can only be used in the vertex shader, the
determination of which triangle to flip will be done in the vertex shader, which will pass a flag to the tessellation or geometry
shader indicating which triangle needs flipping.

*/

/*
TEST MECHANISM FOR VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT

Note: very similar to a non-indexed draw as explained above.

2x2 pixel framebuffer, with 3 sequences: lets call them 0, 1, 2

Each sequence will draw triangles over the following pixels:

0 1
1 2

The indirect commands layout will contain:

1. VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT (optional)
2. VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT
3. VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_EXT
4. VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_EXT

The order of the two intermediate elements will be chosen pseudorandomly.

The structure in the indirect commands buffer needs to be:

typedef struct VkDrawIndexedIndirectCommand {
    uint32_t    indexCount;
    uint32_t    instanceCount;
    uint32_t    firstIndex;
    int32_t     vertexOffset;
    uint32_t    firstInstance;
} VkDrawIndexedIndirectCommand;

And we want to check if each of those 5 parameters is properly taken into account.

We'll create vertex coordinates for 4 triangles (12 vertices in total) each covering one of the framebuffer pixels.

Lets call these triangles A, B, C and D, by default drawn as:

A B
C D

Over the 4 pixels of the framebuffer.

We'll use a single vertex buffer with all vertices stored consecutively for triangles A, B, C and D.

To make things more interesting and use a different index buffer in each sequence, we'll create an index buffer for each of the 3
sequences with different configurations, so that the size and index type also varies. ZZ represents an invalid large index value.

Buffer 0:  0  1  2                              # Different sizes.
Buffer 1:  3  4  5  6  7  8                     #
Buffer 2: ZZ ZZ ZZ ZZ ZZ ZZ ZZ ZZ ZZ 29 30 31   # Non-zero first vertex, plus indices will be stored as 16-bit instead of 32-bit.

Parameters for each sequence:

indexCount: 3 6 3
instanceCount: 1 1 2
firstIndex: 0 0 9
vertexOffset: 0 0 -20
firstInstance: 0 1 0

The push constants will determine the red channel, and will vary in each sequence.

The test setup is good for cases where VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT is not used. If it is, wow can we make a
difference there?

We can provide 2 fragment shaders: one stores 0 in the blue channel and another one stores 1. We can make the
sequences use pipelines 0 0 1, for example. What about the vertex shader? One could mirror coordinates in the X dimension for the
*first* and *second* triangles and another one would not. With this, we can determine the expected color for each of the 4 pixels.

We can add variations that use tessellation or geometry, and in that case the tessellation or geometry shaders would be the ones
flipping coordinates in the X dimension for the first triangle. As VertexIndex can only be used in the vertex shader, the
determination of which triangle to flip will be done in the vertex shader, which will pass a flag to the tessellation or geometry
shader indicating which triangle needs flipping.

*/

using DGCShaderExtPtr = std::unique_ptr<DGCShaderExt>;

constexpr int kPerTriangleVertices   = 3;
constexpr float kVertNormalRedOffset = 0.125f;
constexpr float kVertFlipRedOffset   = 0.25f;

enum class TestType
{
    DRAW = 0,
    DRAW_SIMPLE, // No vertex or index buffer tokens.
    DRAW_INDEXED,
    DRAW_INDEXED_DX, // Using VK_INDIRECT_COMMANDS_INPUT_MODE_DXGI_INDEX_BUFFER_EXT.
};

bool isIndexed(TestType testType)
{
    return (testType == TestType::DRAW_INDEXED || testType == TestType::DRAW_INDEXED_DX);
}

enum class ExtraStages
{
    NONE = 0,
    TESSELLATION,
    GEOMETRY,
};

enum class PipelineType
{
    MONOLITHIC = 0,
    SHADER_OBJECTS,
    GPL_FAST,
    GPL_OPTIMIZED,
    GPL_MIX_BASE_FAST,
    GPL_MIX_BASE_OPT,
};

enum class PreprocessType
{
    NONE = 0,
    SAME_STATE_CMD_BUFFER,
    OTHER_STATE_CMD_BUFFER,
};

bool isGPL(PipelineType pipelineType)
{
    return (pipelineType == PipelineType::GPL_FAST || pipelineType == PipelineType::GPL_OPTIMIZED ||
            pipelineType == PipelineType::GPL_MIX_BASE_FAST || pipelineType == PipelineType::GPL_MIX_BASE_OPT);
}

PipelineConstructionType getGeneralConstructionType(PipelineType pipelineType)
{
    PipelineConstructionType constructionType =
        PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY; // Actually invalid.
    switch (pipelineType)
    {
    case PipelineType::MONOLITHIC:
        constructionType = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC;
        break;
    case PipelineType::SHADER_OBJECTS:
        constructionType = PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV;
        break;
    case PipelineType::GPL_FAST:
    case PipelineType::GPL_MIX_BASE_FAST:
        constructionType = PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY;
        break;
    case PipelineType::GPL_OPTIMIZED:
    case PipelineType::GPL_MIX_BASE_OPT:
        constructionType = PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY;
        break;
    default:
        break;
    }
    return constructionType;
}

struct DrawTestParams
{
    TestType testType;
    ExtraStages extraStages;
    PipelineType pipelineType;
    PreprocessType preprocessType;
    bool checkDrawParams;
    bool useExecutionSet;
    bool unorderedSequences;

    bool hasExtraStages(void) const
    {
        return (extraStages != ExtraStages::NONE);
    }

    bool isShaderObjects(void) const
    {
        return (pipelineType == PipelineType::SHADER_OBJECTS);
    }

    VkShaderStageFlags getStageFlags(void) const
    {
        VkShaderStageFlags stages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        if (extraStages == ExtraStages::TESSELLATION)
            stages |= (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        else if (extraStages == ExtraStages::GEOMETRY)
            stages |= (VK_SHADER_STAGE_GEOMETRY_BIT);
        else if (extraStages == ExtraStages::NONE)
            ;
        else
            DE_ASSERT(false);

        return stages;
    }

    VkIndirectCommandsInputModeFlagsEXT getInputModeFlags(void) const
    {
        VkIndirectCommandsInputModeFlagsEXT flags = 0u;

        if (isIndexed(testType))
        {
            flags |= (testType == TestType::DRAW_INDEXED ? VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER_EXT :
                                                           VK_INDIRECT_COMMANDS_INPUT_MODE_DXGI_INDEX_BUFFER_EXT);
        }

        return flags;
    }

    bool doPreprocess(void) const
    {
        return (preprocessType != PreprocessType::NONE);
    }
};

class DGCDrawInstance : public vkt::TestInstance
{
public:
    DGCDrawInstance(Context &context, DrawTestParams params) : TestInstance(context), m_params(params)
    {
    }
    virtual ~DGCDrawInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    DrawTestParams m_params;
};

class DGCDrawCase : public vkt::TestCase
{
public:
    DGCDrawCase(tcu::TestContext &testCtx, const std::string &name, const DrawTestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DGCDrawCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    DrawTestParams m_params;
};

TestInstance *DGCDrawCase::createInstance(Context &context) const
{
    return new DGCDrawInstance(context, m_params);
}

void DGCDrawCase::checkSupport(Context &context) const
{
    const auto stages                 = m_params.getStageFlags();
    const auto bindStages             = (m_params.useExecutionSet ? stages : 0u);
    const bool useESO                 = m_params.isShaderObjects();
    const auto bindStagesPipeline     = (useESO ? 0u : bindStages);
    const auto bindStagesShaderObject = (useESO ? bindStages : 0u);
    const auto modeFlags              = m_params.getInputModeFlags();

    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject, modeFlags);

    if (m_params.extraStages == ExtraStages::TESSELLATION)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if (m_params.extraStages == ExtraStages::GEOMETRY)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    if (m_params.checkDrawParams)
        context.requireDeviceFunctionality("VK_KHR_shader_draw_parameters");

    if (isGPL(m_params.pipelineType))
    {
        DE_ASSERT(m_params.useExecutionSet); // The code is not prepared otherwise.
        context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");
    }

    const auto &dgcProperties = context.getDeviceGeneratedCommandsPropertiesEXT();

    if (useESO)
    {
        context.requireDeviceFunctionality("VK_EXT_shader_object");

        if (m_params.useExecutionSet && dgcProperties.maxIndirectShaderObjectCount == 0u)
            TCU_THROW(NotSupportedError, "maxIndirectShaderObjectCount is zero");
    }
}

void DGCDrawCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const bool flipFirstTriangleHoriz = m_params.useExecutionSet;
    const bool passFlipFactorDown     = (flipFirstTriangleHoriz && m_params.hasExtraStages());
    const bool checkDrawParams        = m_params.checkDrawParams; // Start at location=2 for simplicity.

    // Normal vertex shader, always used.
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "layout (location=0) out flat int instanceIndex;\n"
             << "layout (location=1) out flat float redOffset;\n"
             << (checkDrawParams ? "layout (location=3) out flat int drawIndex;\n" : "")
             << (checkDrawParams ? "layout (location=4) out flat int baseVertex;\n" : "")
             << (checkDrawParams ? "layout (location=5) out flat int baseInstance;\n" : "") << "void main (void) {\n"
             << "    gl_Position = inPos;\n"
             << "    instanceIndex = gl_InstanceIndex;\n"
             << "    redOffset = " << kVertNormalRedOffset << ";\n"
             << (checkDrawParams ? "    drawIndex = gl_DrawID;\n" : "")
             << (checkDrawParams ? "    baseVertex = gl_BaseVertex;\n" : "")
             << (checkDrawParams ? "    baseInstance = gl_BaseInstance;\n" : "") << "}\n";
        programCollection.glslSources.add("vert_normal") << glu::VertexSource(vert.str());
    }

    // Vertex shader that flips the X coordinates of the first triangle in each draw.
    // Used for the first two sequences when using execution sets.
    if (flipFirstTriangleHoriz)
    {
        // For indexed draws, the vertex index matches the index value, so we have to flip the first 2 triangles in the list.
        // For non-indexed draws, the vertex index is per-draw, so we have to flip the first 1 triangle in the first 2 draws.
        const auto flippedTriangles     = ((m_params.testType == TestType::DRAW) ? 1u : 2u);
        const auto flippedVertexIndices = flippedTriangles * kPerTriangleVertices;

        std::ostringstream vert;
        vert << "#version 460\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "layout (location=0) out flat int instanceIndex;\n"
             << "layout (location=1) out flat float redOffset;\n"
             << (passFlipFactorDown ? "layout (location=2) out flat float xCoordFactor;\n" : "")
             << (checkDrawParams ? "layout (location=3) out flat int drawIndex;\n" : "")
             << (checkDrawParams ? "layout (location=4) out flat int baseVertex;\n" : "")
             << (checkDrawParams ? "layout (location=5) out flat int baseInstance;\n" : "") << "void main (void) {\n"
             << "    const bool passFlipFactorDown = " << (passFlipFactorDown ? "true" : "false") << ";\n"
             << "    const bool flippedTriangle = (gl_VertexIndex < " << flippedVertexIndices << ");\n"
             << (passFlipFactorDown ? "    xCoordFactor = (flippedTriangle ? -1.0 : 1.0);\n" : "")
             << (checkDrawParams ? "    drawIndex = gl_DrawID;\n" : "")
             << (checkDrawParams ? "    baseVertex = gl_BaseVertex;\n" : "")
             << (checkDrawParams ? "    baseInstance = gl_BaseInstance;\n" : "")
             << "    const float xCoord = ((flippedTriangle && !passFlipFactorDown) ? (inPos.x * -1.0) : inPos.x);\n"
             << "    gl_Position = vec4(xCoord, inPos.yzw);\n"
             << "    instanceIndex = gl_InstanceIndex;\n"
             << "    redOffset = " << kVertFlipRedOffset << ";\n"
             << "}\n";
        programCollection.glslSources.add("vert_flip") << glu::VertexSource(vert.str());
    }

    // The normal fragment shader uses 0 for the blue channel and an alternative one uses 1 in the blue channel, if needed.
    std::map<std::string, float> shaderNameBlueMap;
    shaderNameBlueMap["frag_normal"] = 0.0f;
    if (m_params.useExecutionSet)
        shaderNameBlueMap["frag_alt"] = 1.0f;

    for (const auto &kvPair : shaderNameBlueMap)
    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (push_constant, std430) uniform PushConstantBlock {\n"
             << "    float redValue;\n"
             << (checkDrawParams ? "    int drawIndex;\n" : "") << (checkDrawParams ? "    int baseVertex;\n" : "")
             << (checkDrawParams ? "    int baseInstance;\n" : "") << "} pc;\n"
             << "layout (location=0) in flat int instanceIndex;\n"
             << "layout (location=1) in flat float redOffset;\n"
             << (checkDrawParams ? "layout (location=3) in flat int drawIndex;\n" : "")
             << (checkDrawParams ? "layout (location=4) in flat int baseVertex;\n" : "")
             << (checkDrawParams ? "layout (location=5) in flat int baseInstance;\n" : "")
             << "layout (location=0) out vec4 outColor;\n"
             << "void main (void) {\n"
             << "    bool drawParamsOK = true;\n"
             << (checkDrawParams ? "    drawParamsOK = (drawParamsOK && (drawIndex == pc.drawIndex));\n" : "")
             << (checkDrawParams ? "    drawParamsOK = (drawParamsOK && (baseVertex == pc.baseVertex));\n" : "")
             << (checkDrawParams ? "    drawParamsOK = (drawParamsOK && (baseInstance == pc.baseInstance));\n" : "")
             << "    const float alphaValue = (drawParamsOK ? 1.0 : 0.0);\n"
             << "    outColor = vec4(pc.redValue + redOffset, float(instanceIndex), " << kvPair.second
             << ", alphaValue);\n"
             << "}\n";
        programCollection.glslSources.add(kvPair.first) << glu::FragmentSource(frag.str());
    }

    if (m_params.extraStages == ExtraStages::GEOMETRY)
    {
        // We have to create one or two geometry shaders, depending on if we need to flip the first triangle.
        std::map<std::string, bool> shaderNameFlipMap;
        shaderNameFlipMap["geom_normal"] = false;
        if (flipFirstTriangleHoriz)
            shaderNameFlipMap["geom_flip"] = true;

        for (const auto &kvPair : shaderNameFlipMap)
        {
            const auto &shaderName = kvPair.first;
            const auto flip        = kvPair.second;

            std::ostringstream geom;
            geom << "#version 460\n"
                 << "layout (triangles) in;\n"
                 << "layout (triangle_strip, max_vertices=3) out;\n"
                 << "in gl_PerVertex\n"
                 << "{\n"
                 << "    vec4 gl_Position;\n"
                 << "} gl_in[3];\n"
                 << "out gl_PerVertex\n"
                 << "{\n"
                 << "    vec4 gl_Position;\n"
                 << "};\n"
                 << "layout (location=0) in int inInstanceIndex[3];\n"
                 << "layout (location=1) in float inRedOffset[3];\n"
                 << (checkDrawParams ? "layout (location=3) in int inDrawIndex[3];\n" : "")
                 << (checkDrawParams ? "layout (location=4) in int inBaseVertex[3];\n" : "")
                 << (checkDrawParams ? "layout (location=5) in int inBaseInstance[3];\n" : "")
                 << "layout (location=0) out flat int outInstanceIndex;\n"
                 << "layout (location=1) out flat float outRedOffset;\n"
                 << (checkDrawParams ? "layout (location=3) out flat int outDrawIndex;\n" : "")
                 << (checkDrawParams ? "layout (location=4) out flat int outBaseVertex;\n" : "")
                 << (checkDrawParams ? "layout (location=5) out flat int outBaseInstance;\n" : "")
                 << (flip ? "layout (location=2) in float inXCoordFactor[3];\n" : "") << "void main() {\n"
                 << "    for (int i = 0; i < 3; ++i) {\n"
                 << "        const float xCoordFactor = " << (flip ? "inXCoordFactor[i]" : "1.0") << ";\n"
                 << "        gl_Position = vec4(gl_in[i].gl_Position.x * xCoordFactor, gl_in[i].gl_Position.yzw);\n"
                 << "        outInstanceIndex = inInstanceIndex[i];\n"
                 << "        outRedOffset = inRedOffset[i];\n"
                 << (checkDrawParams ? "        outDrawIndex = inDrawIndex[i];\n" : "")
                 << (checkDrawParams ? "        outBaseVertex = inBaseVertex[i];\n" : "")
                 << (checkDrawParams ? "        outBaseInstance = inBaseInstance[i];\n" : "")
                 << "        EmitVertex();\n"
                 << "    }\n"
                 << "}\n";
            programCollection.glslSources.add(shaderName) << glu::GeometrySource(geom.str());
        }
    }

    if (m_params.extraStages == ExtraStages::TESSELLATION)
    {
        // Same as in the geometry shader case.
        std::map<std::string, bool> shaderNameFlipMap;
        shaderNameFlipMap["tesc_normal"] = false;
        if (flipFirstTriangleHoriz)
            shaderNameFlipMap["tesc_flip"] = true;

        for (const auto &kvPair : shaderNameFlipMap)
        {
            const auto &shaderName = kvPair.first;
            const auto flip        = kvPair.second;

            std::ostringstream tesc;
            tesc << "#version 460\n"
                 << "layout (vertices=3) out;\n"
                 << "in gl_PerVertex\n"
                 << "{\n"
                 << "    vec4  gl_Position;\n"
                 << "} gl_in[gl_MaxPatchVertices];\n"
                 << "out gl_PerVertex\n"
                 << "{\n"
                 << "    vec4  gl_Position;\n"
                 << "} gl_out[];\n"
                 << "layout (location=0) in int inInstanceIndex[gl_MaxPatchVertices];\n"
                 << "layout (location=1) in float inRedOffset[gl_MaxPatchVertices];\n"
                 << (checkDrawParams ? "layout (location=3) in int inDrawIndex[gl_MaxPatchVertices];\n" : "")
                 << (checkDrawParams ? "layout (location=4) in int inBaseVertex[gl_MaxPatchVertices];\n" : "")
                 << (checkDrawParams ? "layout (location=5) in int inBaseInstance[gl_MaxPatchVertices];\n" : "")
                 << "layout (location=0) out int outInstanceIndex[];\n"
                 << "layout (location=1) out float outRedOffset[];\n"
                 << (checkDrawParams ? "layout (location=3) out int outDrawIndex[];\n" : "")
                 << (checkDrawParams ? "layout (location=4) out int outBaseVertex[];\n" : "")
                 << (checkDrawParams ? "layout (location=5) out int outBaseInstance[];\n" : "")
                 << (flip ? "layout (location=2) in float inXCoordFactor[gl_MaxPatchVertices];\n" : "")
                 << "void main (void)\n"
                 << "{\n"
                 << "    const float xCoordFactor = " << (flip ? "inXCoordFactor[gl_InvocationID]" : "1.0") << ";\n"
                 << "    gl_TessLevelInner[0] = 1.0;\n"
                 << "    gl_TessLevelInner[1] = 1.0;\n"
                 << "    gl_TessLevelOuter[0] = 1.0;\n"
                 << "    gl_TessLevelOuter[1] = 1.0;\n"
                 << "    gl_TessLevelOuter[2] = 1.0;\n"
                 << "    gl_TessLevelOuter[3] = 1.0;\n"
                 << "    gl_out[gl_InvocationID].gl_Position = vec4(gl_in[gl_InvocationID].gl_Position.x * "
                    "xCoordFactor, gl_in[gl_InvocationID].gl_Position.yzw);\n"
                 << "    outInstanceIndex[gl_InvocationID] = inInstanceIndex[gl_InvocationID];\n"
                 << "    outRedOffset[gl_InvocationID] = inRedOffset[gl_InvocationID];\n"
                 << (checkDrawParams ? "    outDrawIndex[gl_InvocationID] = inDrawIndex[gl_InvocationID];\n" : "")
                 << (checkDrawParams ? "    outBaseVertex[gl_InvocationID] = inBaseVertex[gl_InvocationID];\n" : "")
                 << (checkDrawParams ? "    outBaseInstance[gl_InvocationID] = inBaseInstance[gl_InvocationID];\n" : "")
                 << "}\n";

            programCollection.glslSources.add(shaderName) << glu::TessellationControlSource(tesc.str());
        }

        // Tessellation evaluation is always the same.
        std::ostringstream tese;
        tese << "#version 460\n"
             << "layout (triangles, fractional_odd_spacing, cw) in;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "  vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "  vec4 gl_Position;\n"
             << "};\n"
             << "layout (location=0) in int inInstanceIndex[];\n"
             << "layout (location=1) in float inRedOffset[];\n"
             << (checkDrawParams ? "layout (location=3) in int inDrawIndex[];\n" : "")
             << (checkDrawParams ? "layout (location=4) in int inBaseVertex[];\n" : "")
             << (checkDrawParams ? "layout (location=5) in int inBaseInstance[];\n" : "")
             << "layout (location=0) out flat int outInstanceIndex;\n"
             << "layout (location=1) out flat float outRedOffset;\n"
             << (checkDrawParams ? "layout (location=3) out flat int outDrawIndex;\n" : "")
             << (checkDrawParams ? "layout (location=4) out flat int outBaseVertex;\n" : "")
             << (checkDrawParams ? "layout (location=5) out flat int outBaseInstance;\n" : "") << "void main (void)\n"
             << "{\n"
             << "    gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
             << "                  (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
             << "                  (gl_TessCoord.z * gl_in[2].gl_Position);\n"
             << "    outInstanceIndex = inInstanceIndex[0];\n"
             << "    outRedOffset = inRedOffset[0];\n"
             << (checkDrawParams ? "    outDrawIndex = inDrawIndex[0];\n" : "")
             << (checkDrawParams ? "    outBaseVertex = inBaseVertex[0];\n" : "")
             << (checkDrawParams ? "    outBaseInstance = inBaseInstance[0];\n" : "") << "}\n";

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }
}

// Generates float values for a color, given a starting point, a maximum value and a step.
// E.g. SequentialColorGenerator (start=128, max=255, step=2) generates 128/255, 130/255, 132/255, etc.
class SequentialColorGenerator
{
protected:
    float m_current;
    float m_max;
    float m_step;

public:
    SequentialColorGenerator(uint32_t start, uint32_t max, uint32_t step)
        : m_current(static_cast<float>(start))
        , m_max(static_cast<float>(max))
        , m_step(static_cast<float>(step))
    {
    }

    float operator()(void)
    {
        const float gen = m_current / m_max;
        m_current += m_step;
        return gen;
    }
};

// Creates a shader module if the shader exists.
using ShaderWrapperPtr = std::unique_ptr<ShaderWrapper>;

ShaderWrapperPtr maybeCreateModule(const DeviceInterface &vkd, VkDevice device, const BinaryCollection &binaries,
                                   const std::string &name)
{
    ShaderWrapperPtr shaderPtr;

    if (binaries.contains(name))
        shaderPtr.reset(new ShaderWrapper(vkd, device, binaries.get(name)));
    else
        shaderPtr.reset(new ShaderWrapper());

    return shaderPtr;
}

DGCShaderExtPtr maybeCreateShader(const DeviceInterface &vkd, VkDevice device, const BinaryCollection &binaries,
                                  const std::string &name, VkShaderStageFlagBits stage,
                                  const VkPushConstantRange *pcRange, bool tessFeature, bool geomFeature)
{
    DGCShaderExtPtr shaderPtr;
    if (binaries.contains(name))
    {
        const std::vector<VkDescriptorSetLayout> setLayouts;

        std::vector<VkPushConstantRange> pcRanges;
        if (pcRange)
            pcRanges.push_back(*pcRange);

        shaderPtr.reset(new DGCShaderExt(vkd, device, stage, 0u, binaries.get(name), setLayouts, pcRanges, tessFeature,
                                         geomFeature, nullptr, nullptr));
    }
    return shaderPtr;
}

using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

struct VertexBufferInfo
{
    BufferWithMemoryPtr buffer;
    VkDeviceAddress address;
    uint32_t size;
    uint32_t stride;

    VertexBufferInfo() : buffer(), address(0ull), size(0u), stride(0u)
    {
    }
};

std::vector<VertexBufferInfo> makeVertexBuffers(const DeviceInterface &vkd, const VkDevice device, Allocator &allocator,
                                                const std::vector<tcu::Vec4> &vertices, uint32_t sequenceCount,
                                                uint32_t pixelCount)
{
    // Return vector.
    std::vector<VertexBufferInfo> ret;

    DE_ASSERT(sequenceCount == 1u || sequenceCount == 3u); // We don't know how to do more cases yet.

    const auto vertexBufferUsage   = (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto vertexBufferMemReqs = (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
    const auto vertexSize = static_cast<uint32_t>(sizeof(std::remove_reference<decltype(vertices)>::type::value_type));

    if (sequenceCount == 1u)
    {
        // Flat buffer containing all vertices.
        const auto vertexBufferSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
        const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
        BufferWithMemoryPtr vertexBuffer(
            new BufferWithMemory(vkd, device, allocator, vertexBufferInfo, vertexBufferMemReqs));

        auto &vertexBufferAlloc = vertexBuffer->getAllocation();
        void *vertexBufferData  = vertexBufferAlloc.getHostPtr();

        deMemcpy(vertexBufferData, de::dataOrNull(vertices), de::dataSize(vertices));

        ret.resize(1u);
        auto &bufferInfo = ret.at(0u);

        bufferInfo.buffer.reset(vertexBuffer.release());
        bufferInfo.address = getBufferDeviceAddress(vkd, device, bufferInfo.buffer->get());
        bufferInfo.size    = static_cast<uint32_t>(vertexBufferSize);
        bufferInfo.stride  = vertexSize;
    }
    else
    {
        // Vertex buffers: one per sequence, with the first one containing the
        // first triangle and the last one containg the last triangle.
        std::vector<BufferWithMemoryPtr> vertexBuffers(sequenceCount);

        const auto vtxBufferStrideNormal = vertexSize;
        const auto vtxBufferStrideWide   = vtxBufferStrideNormal * 2u;

        const std::vector<uint32_t> vertexBufferSizes{
            vtxBufferStrideNormal * kPerTriangleVertices,
            vtxBufferStrideWide * kPerTriangleVertices * (pixelCount - 2u), // Wider stride and 2 triangles.
            vtxBufferStrideNormal * kPerTriangleVertices * pixelCount,      // Large vertex offset.
        };

        const std::vector<uint32_t> vertexBufferStrides{
            vtxBufferStrideNormal,
            vtxBufferStrideWide, // The second vertex buffer has a wider stride. See long comment above.
            vtxBufferStrideNormal,
        };

        for (uint32_t i = 0u; i < sequenceCount; ++i)
        {
            const auto vertexBufferCreateInfo = makeBufferCreateInfo(vertexBufferSizes.at(i), vertexBufferUsage);
            vertexBuffers.at(i).reset(
                new BufferWithMemory(vkd, device, allocator, vertexBufferCreateInfo, vertexBufferMemReqs));
        }

        // Device addresses for the vertex buffers.
        std::vector<VkDeviceAddress> vertexBufferAddresses;
        vertexBufferAddresses.reserve(vertexBuffers.size());
        std::transform(begin(vertexBuffers), end(vertexBuffers), std::back_inserter(vertexBufferAddresses),
                       [&vkd, &device](const BufferWithMemoryPtr &vtxBuffer)
                       { return getBufferDeviceAddress(vkd, device, vtxBuffer->get()); });

        std::vector<tcu::Vec4 *> vertexBufferDataPtrs;
        vertexBufferDataPtrs.reserve(vertexBuffers.size());
        std::transform(begin(vertexBuffers), end(vertexBuffers), std::back_inserter(vertexBufferDataPtrs),
                       [](const BufferWithMemoryPtr &bufferPtr)
                       { return reinterpret_cast<tcu::Vec4 *>(bufferPtr->getAllocation().getHostPtr()); });

        const tcu::Vec4 zeroedVertex(0.0f, 0.0f, 0.0f, 0.0f);

        // First vertex buffer.
        uint32_t nextVertex = 0u;
        for (uint32_t i = 0u; i < kPerTriangleVertices; ++i)
            vertexBufferDataPtrs.at(0u)[i] = vertices.at(nextVertex++);

        // Second vertex buffer.
        {
            uint32_t nextPos = 0u;
            for (uint32_t i = 0u; i < pixelCount - 2u /*remove the first and last triangles*/; ++i)
                for (uint32_t j = 0u; j < kPerTriangleVertices; ++j)
                {
                    vertexBufferDataPtrs.at(1u)[nextPos++] = vertices.at(nextVertex++);
                    vertexBufferDataPtrs.at(1u)[nextPos++] =
                        zeroedVertex; // Padding between vertices for the wider stride.
                }
        }

        // Third vertex buffer.
        {
            uint32_t nextPos = 0u;

            // Padding at the beginning.
            for (uint32_t i = 0u; i < nextVertex; ++i)
                vertexBufferDataPtrs.at(2u)[nextPos++] = zeroedVertex;

            // Vertices for triangle D.
            for (uint32_t i = 0u; i < kPerTriangleVertices; ++i)
                vertexBufferDataPtrs.at(2u)[nextPos++] = vertices.at(nextVertex++);
        }

        // Prepare return vector.
        ret.resize(sequenceCount);

        for (uint32_t i = 0u; i < sequenceCount; ++i)
        {
            auto &bufferInfo = ret.at(i);

            bufferInfo.buffer.reset(vertexBuffers.at(i).release());
            bufferInfo.address = vertexBufferAddresses.at(i);
            bufferInfo.size    = vertexBufferSizes.at(i);
            bufferInfo.stride  = vertexBufferStrides.at(i);
        }
    }

    return ret;
}

struct IndexBufferInfo
{
    BufferWithMemoryPtr buffer;
    VkDeviceAddress address;
    uint32_t size;
    VkIndexType indexType;
    int32_t vertexOffset;
};

std::vector<IndexBufferInfo> makeIndexBuffers(const DeviceInterface &vkd, const VkDevice device, Allocator &allocator,
                                              uint32_t sequenceCount, uint32_t pixelCount)
{
    DE_ASSERT(sequenceCount == 0u || sequenceCount == 3u);

    std::vector<IndexBufferInfo> ret;
    if (sequenceCount == 0u)
        return ret;

    const auto indexBufferUsage   = (VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto indexBufferMemReqs = (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    // Buffer contents.
    constexpr uint16_t kInvalidIndex16 = std::numeric_limits<uint16_t>::max() / uint16_t{2};
    const uint32_t indexCountMiddle    = (pixelCount - 2u) * kPerTriangleVertices;

    std::vector<uint32_t> firstBuffer;
    firstBuffer.reserve(kPerTriangleVertices);
    for (uint32_t i = 0u; i < kPerTriangleVertices; ++i)
        firstBuffer.push_back(i);

    std::vector<uint32_t> secondBuffer;
    secondBuffer.reserve(indexCountMiddle);
    for (uint32_t i = 0u; i < indexCountMiddle; ++i)
        secondBuffer.push_back(i + kPerTriangleVertices);

    std::vector<uint16_t> thirdBuffer;
    thirdBuffer.reserve(pixelCount * kPerTriangleVertices);

    const uint32_t prevVertexCount = (pixelCount - 1u) * kPerTriangleVertices;
    const uint16_t vertexOffset    = 20;

    for (uint32_t i = 0u; i < prevVertexCount; ++i)
        thirdBuffer.push_back(kInvalidIndex16);
    for (uint32_t i = 0u; i < kPerTriangleVertices; ++i)
        thirdBuffer.push_back(static_cast<uint16_t>(i + prevVertexCount + vertexOffset));

    // Data pointers.
    const std::vector<void *> bufferDataPtrs{
        de::dataOrNull(firstBuffer),
        de::dataOrNull(secondBuffer),
        de::dataOrNull(thirdBuffer),
    };

    // Buffer sizes.
    const std::vector<uint32_t> bufferSizes{
        static_cast<uint32_t>(de::dataSize(firstBuffer)),
        static_cast<uint32_t>(de::dataSize(secondBuffer)),
        static_cast<uint32_t>(de::dataSize(thirdBuffer)),
    };

    // Index types.
    const std::vector<VkIndexType> indexTypes{
        VK_INDEX_TYPE_UINT32,
        VK_INDEX_TYPE_UINT32,
        VK_INDEX_TYPE_UINT16,
    };

    // Actual buffers.
    std::vector<BufferWithMemoryPtr> indexBuffers;
    indexBuffers.reserve(sequenceCount);
    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        const auto createInfo = makeBufferCreateInfo(static_cast<VkDeviceSize>(bufferSizes.at(i)), indexBufferUsage);
        indexBuffers.emplace_back(new BufferWithMemory(vkd, device, allocator, createInfo, indexBufferMemReqs));

        auto &alloc   = indexBuffers.back()->getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, bufferDataPtrs.at(i), static_cast<size_t>(bufferSizes.at(i)));
        flushAlloc(vkd, device, alloc);
    }

    // Device addresses.
    std::vector<VkDeviceAddress> addresses;
    addresses.reserve(sequenceCount);

    for (uint32_t i = 0u; i < sequenceCount; ++i)
        addresses.push_back(getBufferDeviceAddress(vkd, device, indexBuffers.at(i)->get()));

    // Vertex offsets.
    const std::vector<int32_t> vertexOffsets{0, 0, -vertexOffset};

    ret.resize(sequenceCount);

    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        auto &bufferInfo = ret.at(i);

        bufferInfo.buffer.reset(indexBuffers.at(i).release());
        bufferInfo.address      = addresses.at(i);
        bufferInfo.size         = bufferSizes.at(i);
        bufferInfo.indexType    = indexTypes.at(i);
        bufferInfo.vertexOffset = vertexOffsets.at(i);
    }

    return ret;
}

tcu::TestStatus DGCDrawInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const tcu::Vec3 floatExtent = fbExtent.asFloat();
    const auto pixelCountI      = fbExtent.x() * fbExtent.y() * fbExtent.z();
    const auto vkExtent         = makeExtent3D(fbExtent);
    const auto pixelCountU      = vkExtent.width * vkExtent.height * vkExtent.depth;
    const auto colorFormat      = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto sequenceCount = 3u;
    const auto stageFlags    = m_params.getStageFlags();
    const auto bindPoint     = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    // Vertex data: 1 triangle per pixel.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCountI * kPerTriangleVertices);

    const float pixWidth  = 2.0f / floatExtent.x();
    const float pixHeight = 2.0f / floatExtent.y();
    const float horMargin = pixWidth / 4.0f;
    const float verMargin = pixHeight / 4.0f;

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float xCenter = ((static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f) - 1.0f;
            const float yCenter = ((static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0f) - 1.0f;

            vertices.emplace_back(xCenter - horMargin, yCenter + verMargin, 0.0f, 1.0f);
            vertices.emplace_back(xCenter + horMargin, yCenter + verMargin, 0.0f, 1.0f);
            vertices.emplace_back(xCenter, yCenter - verMargin, 0.0f, 1.0f);
        }

    // Create vertex and index buffers.
    const auto vertSeqCount  = (m_params.testType == TestType::DRAW ? sequenceCount : 1u);
    const auto indexSeqCount = (isIndexed(m_params.testType) ? sequenceCount : 0u);

    const auto vertexBuffers =
        makeVertexBuffers(ctx.vkd, ctx.device, ctx.allocator, vertices, vertSeqCount, pixelCountU);
    const auto indexBuffers = makeIndexBuffers(ctx.vkd, ctx.device, ctx.allocator, indexSeqCount, pixelCountU);

    // Push constants.
    const auto drawParamsCount = (m_params.checkDrawParams ? 3u /*DrawIndex, BaseVertex, BaseInstance*/ : 0u);
    const auto pcStages        = (VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto pcSize          = DE_SIZEOF32(float) + // Must match fragment shader.
                        drawParamsCount * DE_SIZEOF32(int32_t);
    const auto pcRange = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout. Note the wrapper only needs to know if it uses shader objects or not. The specific type is not
    // important as long as the category is correct.
    PipelineLayoutWrapper pipelineLayout((m_params.isShaderObjects() ?
                                              PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV :
                                              PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC),
                                         ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    const uint32_t randomSeed =
        ((static_cast<uint32_t>(m_params.extraStages) << 24) | (static_cast<uint32_t>(m_params.checkDrawParams) << 16) |
         (static_cast<uint32_t>(m_params.useExecutionSet) << 15));
    de::Random rnd(randomSeed);

    // Indirect commands layout.
    VkIndirectCommandsLayoutUsageFlagsEXT layoutFlags = 0u;
    if (m_params.doPreprocess())
        layoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;
    if (m_params.unorderedSequences)
        layoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT;

    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(layoutFlags, stageFlags, *pipelineLayout);
    if (m_params.useExecutionSet)
    {
        const auto infoType = (m_params.isShaderObjects() ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                                            VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
        cmdsLayoutBuilder.addExecutionSetToken(0u, infoType, stageFlags);
    }

    const bool pcFirst = rnd.getBool();

    if (pcFirst)
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);

    if (m_params.testType == TestType::DRAW)
    {
        cmdsLayoutBuilder.addVertexBufferToken(cmdsLayoutBuilder.getStreamRange(), 0u);
    }
    else if (isIndexed(m_params.testType))
    {
        const auto mode = m_params.getInputModeFlags();
        {
            constexpr auto kBitCount = sizeof(VkIndirectCommandsInputModeFlagsEXT) * 8u;
            const std::bitset<kBitCount> bitMask(mode);
            DE_UNREF(bitMask); // For release mode.
            DE_ASSERT(bitMask.count() == 1u);
        }
        const auto modeBits = static_cast<VkIndirectCommandsInputModeFlagBitsEXT>(mode);
        cmdsLayoutBuilder.addIndexBufferToken(cmdsLayoutBuilder.getStreamRange(), modeBits);
    }
    else if (m_params.testType == TestType::DRAW_SIMPLE)
        ; // No vertex or index buffer tokens.
    else
        DE_ASSERT(false);

    if (!pcFirst)
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);

    if (m_params.testType == TestType::DRAW || m_params.testType == TestType::DRAW_SIMPLE)
        cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    else if (isIndexed(m_params.testType))
        cmdsLayoutBuilder.addDrawIndexedToken(cmdsLayoutBuilder.getStreamRange());
    else
        DE_ASSERT(false);

    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Device-generated commands data.
    std::vector<uint32_t> dgcData;
    dgcData.reserve((sequenceCount * cmdsLayoutBuilder.getStreamStride()) / sizeof(uint32_t));

    // Red color values.
    SequentialColorGenerator colorGenerator(128u, 255u, 5u);
    std::vector<float> redValues(sequenceCount, -1.0f);
    std::generate(begin(redValues), end(redValues), colorGenerator);

    // Draw commands.
    const auto middleVertexCount = (pixelCountU - 2u) * kPerTriangleVertices;    // For the second sequence.
    const auto firstVertexAtEnd  = de::sizeU32(vertices) - kPerTriangleVertices; // First vertex for the final sequence.

    std::vector<VkDrawIndirectCommand> drawCmds;
    std::vector<VkDrawIndexedIndirectCommand> drawIndexedCmds;

    if (m_params.testType == TestType::DRAW)
    {
        drawCmds.reserve(sequenceCount);

        // For the rationale behind these numbers, see the explanation above. The idea is to vary every number.
        drawCmds.push_back(VkDrawIndirectCommand{kPerTriangleVertices, 1u, 0u, 0u});
        drawCmds.push_back(VkDrawIndirectCommand{middleVertexCount, 1u, 0u, 1u});
        drawCmds.push_back(VkDrawIndirectCommand{kPerTriangleVertices, 2u, firstVertexAtEnd, 0u});
    }
    else if (m_params.testType == TestType::DRAW_SIMPLE)
    {
        drawCmds.reserve(sequenceCount);

        // Alternative to the one above with a single vertex buffer, so the middle draw uses a different firstVertex.
        drawCmds.push_back(VkDrawIndirectCommand{kPerTriangleVertices, 1u, 0u, 0u});
        drawCmds.push_back(VkDrawIndirectCommand{middleVertexCount, 1u, kPerTriangleVertices, 1u});
        drawCmds.push_back(VkDrawIndirectCommand{kPerTriangleVertices, 2u, firstVertexAtEnd, 0u});
    }
    else if (isIndexed(m_params.testType))
    {
        drawIndexedCmds.reserve(sequenceCount);

        const std::vector<int32_t> offsets{
            indexBuffers.at(0).vertexOffset,
            indexBuffers.at(1).vertexOffset,
            indexBuffers.at(2).vertexOffset,
        };

        // For the rationale behind these numbers, see the explanation above. The idea is to vary every number.
        drawIndexedCmds.push_back(VkDrawIndexedIndirectCommand{kPerTriangleVertices, 1u, 0u, offsets.at(0), 0u});
        drawIndexedCmds.push_back(VkDrawIndexedIndirectCommand{middleVertexCount, 1u, 0u, offsets.at(1), 1u});
        drawIndexedCmds.push_back(
            VkDrawIndexedIndirectCommand{kPerTriangleVertices, 2u, firstVertexAtEnd, offsets.at(2), 0u});
    }
    else
        DE_ASSERT(false);

    std::vector<VkBindVertexBufferIndirectCommandEXT> bindVertexBufferCmds;
    std::vector<VkBindIndexBufferIndirectCommandEXT> bindIndexBufferCmds;

    if (m_params.testType == TestType::DRAW)
    {
        bindVertexBufferCmds.reserve(sequenceCount);

        for (uint32_t i = 0u; i < sequenceCount; ++i)
        {
            bindVertexBufferCmds.emplace_back(VkBindVertexBufferIndirectCommandEXT{
                vertexBuffers.at(i).address,
                vertexBuffers.at(i).size,
                vertexBuffers.at(i).stride,
            });
        }
    }
    else if (isIndexed(m_params.testType))
    {
        bindIndexBufferCmds.reserve(sequenceCount);

        for (uint32_t i = 0u; i < sequenceCount; ++i)
        {
            if (m_params.testType == TestType::DRAW_INDEXED)
            {
                bindIndexBufferCmds.emplace_back(VkBindIndexBufferIndirectCommandEXT{
                    indexBuffers.at(i).address,
                    indexBuffers.at(i).size,
                    indexBuffers.at(i).indexType,
                });
            }
            else
            {
                IndexBufferViewD3D12 cmd(indexBuffers.at(i).address, indexBuffers.at(i).size,
                                         indexBuffers.at(i).indexType);
                pushBackElement(bindIndexBufferCmds, cmd);
            }
        }
    }
    else if (m_params.testType == TestType::DRAW_SIMPLE)
        ; // No vertex or index buffer bind tokens.
    else
        DE_ASSERT(false);

    // Closure to avoid code duplication.
    const auto pushPushConstants = [&dgcData, &redValues, &drawCmds, &drawIndexedCmds, this](uint32_t i)
    {
        pushBackElement(dgcData, redValues.at(i));
        if (this->m_params.checkDrawParams)
        {
            pushBackElement(dgcData, static_cast<int32_t>(0)); // For non-count commands, DrawIndex stays at 0.

            const auto &testType = this->m_params.testType;

            if (testType == TestType::DRAW || testType == TestType::DRAW_SIMPLE)
            {
                pushBackElement(dgcData, drawCmds.at(i).firstVertex);
                pushBackElement(dgcData, drawCmds.at(i).firstInstance);
            }
            else if (isIndexed(this->m_params.testType))
            {
                pushBackElement(dgcData, drawIndexedCmds.at(i).vertexOffset);
                pushBackElement(dgcData, drawIndexedCmds.at(i).firstInstance);
            }
            else
                DE_ASSERT(false);
        }
    };

    // Rationale behind execution sets
    //
    // For pipelines, we'll create a different pipeline per sequence with the
    // right shaders, and store them in order in the execution set. This means
    // sequence i will use element i in the execution set.
    //
    // For shader objects, the execution set will contain 2 vertex shaders and 2
    // fragment shaders (plus optionally 2 tessellation control shaders and 1
    // tessellation evaluation shader, or 2 geometry shaders). For stages with 2
    // shaders, the first one will be the "normal" one and the second one will
    // be the "alternative" one, so in each sequence we need a different set of
    // numbers that will match what we'll be using for pipelines, which means,
    // per stage:
    //
    // vert: 1 1 0 (flip, flip, normal)
    // tesc: 1 1 0 (flip, flip, normal)
    // tese: 0 0 0 (we only have 1)
    // geom: 1 1 0 (flip, flip, normal)
    // frag: 0 1 1 (normal, alt, alt)
    //
    // However, as each shader needs to have a unique index, we'll offset those
    // values by a base value calculated according to the stages we will be
    // using.
    //
    // Also, in the indirect commands buffer, the indices for a sequence need to
    // appear in the order of the stages in the pipeline.
    //

    // Base unique indices for each stage.
    const auto invalidIndex  = std::numeric_limits<uint32_t>::max() / 2u; // Divided by 2 to avoid overflows.
    const auto vertBaseIndex = 0u;
    const auto fragBaseIndex = 2u;
    auto tescBaseIndex       = invalidIndex;
    auto teseBaseIndex       = invalidIndex;
    auto geomBaseIndex       = invalidIndex;

    if (m_params.extraStages == ExtraStages::TESSELLATION)
    {
        tescBaseIndex = 4u;
        teseBaseIndex = 6u;
    }
    if (m_params.extraStages == ExtraStages::GEOMETRY)
        geomBaseIndex = 4u;

    const std::vector<uint32_t> vertIndicesESO{vertBaseIndex + 1u, vertBaseIndex + 1u, vertBaseIndex + 0u};
    const std::vector<uint32_t> tescIndicesESO{tescBaseIndex + 1u, tescBaseIndex + 1u, tescBaseIndex + 0u};
    const std::vector<uint32_t> teseIndicesESO{teseBaseIndex + 0u, teseBaseIndex + 0u, teseBaseIndex + 0u};
    const std::vector<uint32_t> geomIndicesESO{geomBaseIndex + 1u, geomBaseIndex + 1u, geomBaseIndex + 0u};
    const std::vector<uint32_t> fragIndicesESO{fragBaseIndex + 0u, fragBaseIndex + 1u, fragBaseIndex + 1u};

    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        if (m_params.useExecutionSet)
        {
            if (m_params.isShaderObjects())
            {
                pushBackElement(dgcData, vertIndicesESO.at(i));
                if (m_params.extraStages == ExtraStages::TESSELLATION)
                {
                    pushBackElement(dgcData, tescIndicesESO.at(i));
                    pushBackElement(dgcData, teseIndicesESO.at(i));
                }
                if (m_params.extraStages == ExtraStages::GEOMETRY)
                    pushBackElement(dgcData, geomIndicesESO.at(i));
                pushBackElement(dgcData, fragIndicesESO.at(i));
            }
            else
                pushBackElement(dgcData, i);
        }

        if (pcFirst)
            pushPushConstants(i);

        if (m_params.testType == TestType::DRAW)
            pushBackElement(dgcData, bindVertexBufferCmds.at(i));
        else if (isIndexed(m_params.testType))
            pushBackElement(dgcData, bindIndexBufferCmds.at(i));
        else if (m_params.testType == TestType::DRAW_SIMPLE)
            ; // No vertex or index buffer bind tokens.
        else
            DE_ASSERT(false);

        if (!pcFirst)
            pushPushConstants(i);

        if (m_params.testType == TestType::DRAW || m_params.testType == TestType::DRAW_SIMPLE)
            pushBackElement(dgcData, drawCmds.at(i));
        else if (isIndexed(m_params.testType))
            pushBackElement(dgcData, drawIndexedCmds.at(i));
        else
            DE_ASSERT(false);
    }

    // Buffer holding the device-generated commands.
    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);

    auto &dgcBufferAlloc   = dgcBuffer.getAllocation();
    void *dgcBufferDataPtr = dgcBufferAlloc.getHostPtr();

    deMemcpy(dgcBufferDataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    flushAlloc(ctx.vkd, ctx.device, dgcBufferAlloc);

    // Prepare single pipeline, shaders or indirect execution set.
    const auto &binaries = m_context.getBinaryCollection();

    ShaderWrapperPtr vertNormal;
    ShaderWrapperPtr vertFlip;
    ShaderWrapperPtr tescNormal;
    ShaderWrapperPtr tescFlip;
    ShaderWrapperPtr tese;
    ShaderWrapperPtr geomNormal;
    ShaderWrapperPtr geomFlip;
    ShaderWrapperPtr fragNormal;
    ShaderWrapperPtr fragAlt;

    DGCShaderExtPtr vertNormalShader;
    DGCShaderExtPtr vertFlipShader;
    DGCShaderExtPtr tescNormalShader;
    DGCShaderExtPtr tescFlipShader;
    DGCShaderExtPtr teseShader;
    DGCShaderExtPtr geomNormalShader;
    DGCShaderExtPtr geomFlipShader;
    DGCShaderExtPtr fragNormalShader;
    DGCShaderExtPtr fragAltShader;

    const auto &meshFeatures = m_context.getMeshShaderFeaturesEXT();
    const auto &features     = m_context.getDeviceFeatures();

    const auto tessFeature = (features.tessellationShader == VK_TRUE);
    const auto geomFeature = (features.geometryShader == VK_TRUE);

    if (m_params.isShaderObjects())
    {
        vertNormalShader = maybeCreateShader(ctx.vkd, ctx.device, binaries, "vert_normal", VK_SHADER_STAGE_VERTEX_BIT,
                                             nullptr, tessFeature, geomFeature);
        vertFlipShader   = maybeCreateShader(ctx.vkd, ctx.device, binaries, "vert_flip", VK_SHADER_STAGE_VERTEX_BIT,
                                             nullptr, tessFeature, geomFeature);
        tescNormalShader =
            maybeCreateShader(ctx.vkd, ctx.device, binaries, "tesc_normal", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                              nullptr, tessFeature, geomFeature);
        tescFlipShader   = maybeCreateShader(ctx.vkd, ctx.device, binaries, "tesc_flip",
                                             VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, nullptr, tessFeature, geomFeature);
        teseShader       = maybeCreateShader(ctx.vkd, ctx.device, binaries, "tese",
                                             VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, nullptr, tessFeature, geomFeature);
        geomNormalShader = maybeCreateShader(ctx.vkd, ctx.device, binaries, "geom_normal", VK_SHADER_STAGE_GEOMETRY_BIT,
                                             nullptr, tessFeature, geomFeature);
        geomFlipShader   = maybeCreateShader(ctx.vkd, ctx.device, binaries, "geom_flip", VK_SHADER_STAGE_GEOMETRY_BIT,
                                             nullptr, tessFeature, geomFeature);
        fragNormalShader = maybeCreateShader(ctx.vkd, ctx.device, binaries, "frag_normal", VK_SHADER_STAGE_FRAGMENT_BIT,
                                             &pcRange, tessFeature, geomFeature);
        fragAltShader    = maybeCreateShader(ctx.vkd, ctx.device, binaries, "frag_alt", VK_SHADER_STAGE_FRAGMENT_BIT,
                                             &pcRange, tessFeature, geomFeature);
    }
    else
    {
        vertNormal = maybeCreateModule(ctx.vkd, ctx.device, binaries, "vert_normal");
        vertFlip   = maybeCreateModule(ctx.vkd, ctx.device, binaries, "vert_flip");
        tescNormal = maybeCreateModule(ctx.vkd, ctx.device, binaries, "tesc_normal");
        tescFlip   = maybeCreateModule(ctx.vkd, ctx.device, binaries, "tesc_flip");
        tese       = maybeCreateModule(ctx.vkd, ctx.device, binaries, "tese");
        geomNormal = maybeCreateModule(ctx.vkd, ctx.device, binaries, "geom_normal");
        geomFlip   = maybeCreateModule(ctx.vkd, ctx.device, binaries, "geom_flip");
        fragNormal = maybeCreateModule(ctx.vkd, ctx.device, binaries, "frag_normal");
        fragAlt    = maybeCreateModule(ctx.vkd, ctx.device, binaries, "frag_alt");
    }

    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (!m_params.isShaderObjects())
    {
        renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
        framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width,
                                      vkExtent.height);
    }

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const bool hasTessellation = (m_params.extraStages == ExtraStages::TESSELLATION);
    const auto primitiveTopology =
        (hasTessellation ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const auto patchControlPoints = (hasTessellation ? 3u : 0u);

    Move<VkPipeline> normalPipeline;

    using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;
    std::vector<GraphicsPipelineWrapperPtr> dgcPipelines;

    const auto vertexBinding =
        makeVertexInputBindingDescription(0u, 0u /*stride will come from DGC*/, VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttrib = makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &vertexBinding, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        1u,             // uint32_t vertexAttributeDescriptionCount;
        &vertexAttrib,  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE};

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
        de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
        de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
    };

    // Prepare indirect execution set at the same time as the pipelines.
    ExecutionSetManagerPtr executionSetManager;

    if (m_params.useExecutionSet)
    {
        if (m_params.isShaderObjects())
        {
            const std::vector<VkDescriptorSetLayout> noSetLayouts; // No set layouts being used in these tests.
            std::vector<IESStageInfo> stageInfos;
            uint32_t maxShaderCount = 0u;

            stageInfos.reserve(5u); // Potentially vert, tesc, tese, geom, frag.

            const auto addStage = [&maxShaderCount, &stageInfos, &noSetLayouts](VkShaderEXT shader, uint32_t maxShaders)
            {
                stageInfos.emplace_back(IESStageInfo{shader, noSetLayouts});
                maxShaderCount += maxShaders;
            };

            addStage(vertNormalShader->get(), 2u);
            addStage(fragNormalShader->get(), 2u);

            if (m_params.extraStages == ExtraStages::TESSELLATION)
            {
                addStage(tescNormalShader->get(), 2u);
                addStage(teseShader->get(), 1u);
            }

            if (m_params.extraStages == ExtraStages::GEOMETRY)
                addStage(geomNormalShader->get(), 2u);

            const std::vector<vk::VkPushConstantRange> pcRanges(1u, pcRange);

            // Execution set for shader objects. Note we store the normal shader
            // with index 0 and the alternative with index 1. This matches the
            // rationale we're following for shader objects described before,
            // and the expected contents of the indirect commands buffer.
            {
                executionSetManager =
                    makeExecutionSetManagerShader(ctx.vkd, ctx.device, stageInfos, pcRanges, maxShaderCount);

                executionSetManager->addShader(vertBaseIndex + 0u, vertNormalShader->get());
                executionSetManager->addShader(vertBaseIndex + 1u, vertFlipShader->get());

                executionSetManager->addShader(fragBaseIndex + 0u, fragNormalShader->get());
                executionSetManager->addShader(fragBaseIndex + 1u, fragAltShader->get());

                if (m_params.extraStages == ExtraStages::TESSELLATION)
                {
                    executionSetManager->addShader(tescBaseIndex + 0u, tescNormalShader->get());
                    executionSetManager->addShader(tescBaseIndex + 1u, tescFlipShader->get());

                    executionSetManager->addShader(teseBaseIndex + 0u, teseShader->get());
                }

                if (m_params.extraStages == ExtraStages::GEOMETRY)
                {
                    executionSetManager->addShader(geomBaseIndex + 0u, geomNormalShader->get());
                    executionSetManager->addShader(geomBaseIndex + 1u, geomFlipShader->get());
                }

                executionSetManager->update();
            }
        }
        else
        {
            dgcPipelines.resize(sequenceCount);

            const auto initialValue = getGeneralConstructionType(m_params.pipelineType);
            std::vector<PipelineConstructionType> constructionTypes(sequenceCount, initialValue);

            if (m_params.pipelineType == PipelineType::GPL_MIX_BASE_OPT)
                constructionTypes.at(1u) = PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY;
            else if (m_params.pipelineType == PipelineType::GPL_MIX_BASE_FAST)
                constructionTypes.at(1u) = PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY;

            const VkPipelineCreateFlags2KHR creationFlags = VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT;

            {
                auto &pipeline = dgcPipelines.at(0u);
                pipeline.reset(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(), constructionTypes.at(0u)));

                pipeline->setDefaultTopology(primitiveTopology)
                    .setPipelineCreateFlags2(creationFlags)
                    .setDefaultRasterizationState()
                    .setDefaultColorBlendState()
                    .setDefaultMultisampleState()
                    .setDefaultPatchControlPoints(patchControlPoints)
                    .setDynamicState(&dynamicStateCreateInfo)
                    .setupVertexInputState(&vertexInputStateCreateInfo)
                    .setupPreRasterizationShaderState2(viewports, scissors, pipelineLayout, *renderPass, 0u, *vertFlip,
                                                       nullptr, *tescFlip, *tese, *geomFlip)
                    .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, *fragNormal)
                    .setupFragmentOutputState(*renderPass, 0u)
                    .setMonolithicPipelineLayout(pipelineLayout)
                    .buildPipeline();
            }

            {
                auto &pipeline = dgcPipelines.at(1u);
                pipeline.reset(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(), constructionTypes.at(1u)));

                pipeline->setDefaultTopology(primitiveTopology)
                    .setPipelineCreateFlags2(creationFlags)
                    .setDefaultRasterizationState()
                    .setDefaultColorBlendState()
                    .setDefaultMultisampleState()
                    .setDefaultPatchControlPoints(patchControlPoints)
                    .setDynamicState(&dynamicStateCreateInfo)
                    .setupVertexInputState(&vertexInputStateCreateInfo)
                    .setupPreRasterizationShaderState2(viewports, scissors, pipelineLayout, *renderPass, 0u, *vertFlip,
                                                       nullptr, *tescFlip, *tese, *geomFlip)
                    .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, *fragAlt)
                    .setupFragmentOutputState(*renderPass, 0u)
                    .setMonolithicPipelineLayout(pipelineLayout)
                    .buildPipeline();
            }

            {
                auto &pipeline = dgcPipelines.at(2u);
                pipeline.reset(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(), constructionTypes.at(2u)));

                pipeline->setDefaultTopology(primitiveTopology)
                    .setPipelineCreateFlags2(creationFlags)
                    .setDefaultRasterizationState()
                    .setDefaultColorBlendState()
                    .setDefaultMultisampleState()
                    .setDefaultPatchControlPoints(patchControlPoints)
                    .setDynamicState(&dynamicStateCreateInfo)
                    .setupVertexInputState(&vertexInputStateCreateInfo)
                    .setupPreRasterizationShaderState2(viewports, scissors, pipelineLayout, *renderPass, 0u,
                                                       *vertNormal, nullptr, *tescNormal, *tese, *geomNormal)
                    .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, *fragAlt)
                    .setupFragmentOutputState(*renderPass, 0u)
                    .setMonolithicPipelineLayout(pipelineLayout)
                    .buildPipeline();
            }

            executionSetManager =
                makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipelines.at(0u)->getPipeline(), sequenceCount);
            for (uint32_t i = 0u; i < sequenceCount; ++i)
                executionSetManager->addPipeline(i, dgcPipelines.at(i)->getPipeline());
            executionSetManager->update();
        }
    }
    else
    {
        if (!m_params.isShaderObjects())
        {
            normalPipeline = makeGraphicsPipeline(
                ctx.vkd, ctx.device, *pipelineLayout, vertNormal->getModule(), tescNormal->getModule(),
                tese->getModule(), geomNormal->getModule(), fragNormal->getModule(), *renderPass, viewports, scissors,
                primitiveTopology, 0u, patchControlPoints, &vertexInputStateCreateInfo, nullptr, nullptr, nullptr,
                nullptr, &dynamicStateCreateInfo);
        }
    }

    const auto indirectExecutionSet = (executionSetManager ? executionSetManager->get() : VK_NULL_HANDLE);

    // Preprocess buffer.
    std::vector<VkShaderEXT> shadersVec;
    if (m_params.isShaderObjects() && !m_params.useExecutionSet)
    {
        shadersVec.reserve(5u); // At most: vert, tesc, tese, geom, frag.
        if (vertNormalShader)
            shadersVec.push_back(vertNormalShader->get());
        if (tescNormalShader)
            shadersVec.push_back(tescNormalShader->get());
        if (teseShader)
            shadersVec.push_back(teseShader->get());
        if (geomNormalShader)
            shadersVec.push_back(geomNormalShader->get());
        if (fragNormalShader)
            shadersVec.push_back(fragNormalShader->get());
    }
    const std::vector<VkShaderEXT> *shadersVecPtr = (shadersVec.empty() ? nullptr : &shadersVec);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, indirectExecutionSet, *cmdsLayout,
                                         sequenceCount, 0u, *normalPipeline, shadersVecPtr);

    // Record commands.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const auto clearValueColor = makeClearValueColor(clearColor);
    const auto colorSRR        = makeDefaultImageSubresourceRange();

    // Will be used for both preprocessing and execution.
    const DGCGenCmdsInfo generatedCommandsInfo(
        stageFlags,                          // VkShaderStageFlags shaderStages;
        indirectExecutionSet,                // VkIndirectExecutionSetEXT indirectExecutionSet;
        *cmdsLayout,                         // VkIndirectCommandsLayoutEXT indirectCommandsLayout;
        dgcBuffer.getDeviceAddress(),        // VkDeviceAddress indirectAddress;
        dgcBufferSize,                       // VkDeviceSize indirectAddressSize;
        preprocessBuffer.getDeviceAddress(), // VkDeviceAddress preprocessAddress;
        preprocessBuffer.getSize(),          // VkDeviceSize preprocessSize;
        sequenceCount,                       // uint32_t maxSequenceCount;
        0ull,                                // VkDeviceAddress sequenceCountAddress;
        0u,                                  // uint32_t maxDrawCount;
        *normalPipeline, shadersVecPtr);

    // When preprocessing, we need to use a separate command buffer to record state.
    // The preprocessing step needs to happen outside the render pass.
    Move<VkCommandBuffer> separateStateCmdBuffer;

    // A command buffer we want to record state into.
    // .first is the command buffer itself.
    // .second, if not NULL, means we'll record a preprocess command with it as the state command buffer.
    using StateCmdBuffer                 = std::pair<VkCommandBuffer, VkCommandBuffer>;
    const VkCommandBuffer kNullCmdBuffer = VK_NULL_HANDLE; // Workaround for emplace_back below.
    std::vector<StateCmdBuffer> stateCmdBuffers;

    // Sequences and iterations for the different cases:
    //     - PreprocessType::NONE
    //         - Only one loop iteration.
    //         - Iteration 0: .first = main cmd buffer, .second = NULL
    //             - No preprocess, bind state
    //         - Execute.
    //     - PreprocessType::OTHER_STATE_CMD_BUFFER
    //         - Iteration 0: .first = state cmd buffer, .second = NULL
    //             - No preprocess, bind state
    //         - Iteration 1: .first = main cmd buffer, .second = state cmd buffer
    //             - Preprocess with state cmd buffer, bind state on main
    //         - Execute.
    //     - PreprocessType::SAME_STATE_CMD_BUFFER
    //         - Iteration 0: .first = main cmd buffer, .second = NULL
    //             - No preprocess, bind state
    //         - Iteration 1: .first = main cmd buffer, .second = main cmd buffer
    //             - Preprocess with main cmd buffer, break
    //         - Execute.
    switch (m_params.preprocessType)
    {
    case PreprocessType::NONE:
        stateCmdBuffers.emplace_back(cmdBuffer, kNullCmdBuffer);
        break;
    case PreprocessType::SAME_STATE_CMD_BUFFER:
        stateCmdBuffers.emplace_back(cmdBuffer, kNullCmdBuffer);
        stateCmdBuffers.emplace_back(cmdBuffer, cmdBuffer);
        break;
    case PreprocessType::OTHER_STATE_CMD_BUFFER:
        separateStateCmdBuffer =
            allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        stateCmdBuffers.emplace_back(*separateStateCmdBuffer, kNullCmdBuffer);
        stateCmdBuffers.emplace_back(cmdBuffer, *separateStateCmdBuffer);
        break;
    default:
        DE_ASSERT(false);
    }

    // Record pre-execution state to all needed command buffers.
    VkCommandBuffer prevCmdBuffer = VK_NULL_HANDLE;
    for (const auto &stateCmdBufferPair : stateCmdBuffers)
    {
        const auto &recCmdBuffer = stateCmdBufferPair.first;

        // Only begin each command buffer once.
        if (recCmdBuffer != prevCmdBuffer)
        {
            beginCommandBuffer(ctx.vkd, recCmdBuffer);
            prevCmdBuffer = recCmdBuffer;
        }

        if (stateCmdBufferPair.second != VK_NULL_HANDLE)
        {
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(recCmdBuffer, &generatedCommandsInfo.get(),
                                                      stateCmdBufferPair.second);
            separateStateCmdBuffer =
                Move<VkCommandBuffer>(); // Delete state command buffer right away as allowed by the spec.

            preprocessToExecuteBarrierExt(ctx.vkd, recCmdBuffer);

            // Break for iteration 1 of PreprocessType::SAME_STATE_CMD_BUFFER. See above.
            if (stateCmdBufferPair.first == stateCmdBufferPair.second)
                break;
        }

        if (m_params.isShaderObjects())
        {
            // Bind shaders.
            std::map<VkShaderStageFlagBits, VkShaderEXT> shadersToBind{
                std::make_pair(VK_SHADER_STAGE_VERTEX_BIT, vertNormalShader->get()),
                std::make_pair(VK_SHADER_STAGE_FRAGMENT_BIT, fragNormalShader->get()),
            };

            if (m_params.extraStages == ExtraStages::TESSELLATION)
            {
                shadersToBind[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = tescNormalShader->get();
                shadersToBind[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = teseShader->get();
            }
            else if (features.tessellationShader)
            {
                shadersToBind[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
                shadersToBind[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
            }

            if (m_params.extraStages == ExtraStages::GEOMETRY)
                shadersToBind[VK_SHADER_STAGE_GEOMETRY_BIT] = geomNormalShader->get();
            else if (features.geometryShader)
                shadersToBind[VK_SHADER_STAGE_GEOMETRY_BIT] = VK_NULL_HANDLE;

            if (meshFeatures.meshShader)
                shadersToBind[VK_SHADER_STAGE_MESH_BIT_EXT] = VK_NULL_HANDLE;
            if (meshFeatures.taskShader)
                shadersToBind[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;

            for (const auto &stageAndShader : shadersToBind)
                ctx.vkd.cmdBindShadersEXT(recCmdBuffer, 1u, &stageAndShader.first, &stageAndShader.second);
        }
        else
        {
            if (m_params.useExecutionSet)
            {
                DE_ASSERT(!dgcPipelines.empty());
                ctx.vkd.cmdBindPipeline(recCmdBuffer, bindPoint, dgcPipelines.at(0u)->getPipeline());
            }
            else
            {
                // Bind shaders and state.
                DE_ASSERT(*normalPipeline != VK_NULL_HANDLE);
                ctx.vkd.cmdBindPipeline(recCmdBuffer, bindPoint, *normalPipeline);
            }
        }

        if (m_params.isShaderObjects())
        {
            // Bind state for shader objects. This is needed with and without execution sets.
            vkt::shaderobjutil::bindShaderObjectState(
                ctx.vkd, vkt::shaderobjutil::getDeviceCreationExtensions(m_context), recCmdBuffer, viewports, scissors,
                primitiveTopology, patchControlPoints, &vertexInputStateCreateInfo, nullptr, nullptr, nullptr, nullptr);
        }

        if (isIndexed(m_params.testType) || m_params.testType == TestType::DRAW_SIMPLE)
        {
            const VkBuffer vertexBuffer           = vertexBuffers.at(0u).buffer->get();
            const VkDeviceSize vertexBufferOffset = 0ull;
            const VkDeviceSize vertexBufferSize   = static_cast<VkDeviceSize>(vertexBuffers.at(0u).size);
            const VkDeviceSize vertexBufferStride = static_cast<VkDeviceSize>(vertexBuffers.at(0u).stride);

            ctx.vkd.cmdBindVertexBuffers2(recCmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset, &vertexBufferSize,
                                          &vertexBufferStride);
        }
    }

    if (m_params.isShaderObjects())
    {
        const auto clearLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        const auto renderingLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        const auto preClearBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                            clearLayout, colorBuffer.getImage(), colorSRR);

        const auto postClearBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
            clearLayout, renderingLayout, colorBuffer.getImage(), colorSRR);

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
        ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), clearLayout, &clearValueColor.color, 1u,
                                   &colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);

        beginRendering(ctx.vkd, cmdBuffer, colorBuffer.getImageView(), scissors.at(0u), clearValueColor /*unused*/,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    else
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);

    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(m_params.doPreprocess()),
                                           &generatedCommandsInfo.get());

    if (m_params.isShaderObjects())
        endRendering(ctx.vkd, cmdBuffer);
    else
        endRenderPass(ctx.vkd, cmdBuffer);

    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify results.
    auto &resultsBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, resultsBufferAlloc);

    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtent, resultsBufferAlloc.getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto reference = referenceLevel.getAccess();

    // The first and second triangles will have their pixels swapped when using execution sets, because of the X coordinate flip.
    // The last two sequences should have blue 1 when using execution sets.
    // Except for the first pixel, all others have green 1, either because of the instance count or because of the first instance value.
    const auto firstX       = (m_params.useExecutionSet ? 1 : 0);
    const auto secondX      = (m_params.useExecutionSet ? 0 : 1);
    const auto blueAlt      = (m_params.useExecutionSet ? 1.0f : 0.0f);
    const auto redOffsetAlt = (m_params.useExecutionSet ? kVertFlipRedOffset : kVertNormalRedOffset);

    reference.setPixel(tcu::Vec4(redValues.at(0u) + redOffsetAlt, 0.0f, 0.0f, 1.0f), firstX, 0);
    reference.setPixel(tcu::Vec4(redValues.at(1u) + redOffsetAlt, 1.0f, blueAlt, 1.0f), secondX, 0);
    reference.setPixel(tcu::Vec4(redValues.at(1u) + redOffsetAlt, 1.0f, blueAlt, 1.0f), 0, 1);
    reference.setPixel(tcu::Vec4(redValues.at(2u) + kVertNormalRedOffset, 1.0f, blueAlt, 1.0f), 1, 1);

    auto &log                  = m_context.getTestContext().getLog();
    const float thresholdValue = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(thresholdValue, 0.0f, 0.0f, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_EVERYTHING))
        TCU_FAIL("Unexpected results in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

void checkBasicDGCGraphicsSupport(Context &context, bool)
{
    const VkShaderStageFlags stages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    checkDGCExtSupport(context, stages);
}

// The fragment shader uses a push constant for the geometry color but, in addition to that, if pushConstantToken is
// true we're also going to use a push constant for the point size.
void basicGraphicsPrograms(SourceCollections &dst, bool pcToken)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << (pcToken ? "layout (push_constant, std430) uniform PCBlock { layout(offset=16) float ptSize; } pc;\n" : "")
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    const float pointSize = " << (pcToken ? "pc.ptSize" : "1.0") << ";\n"
         << "    gl_PointSize = pointSize;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (push_constant, std430) uniform PCBlock { vec4 color; } pc;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = pc.color;\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

// indexedDrawWithoutIndexTokenRun tests indexed draws without an index buffer token.
tcu::TestStatus indexedDrawWithoutIndexTokenRun(Context &context, bool pcToken)
{
    const auto &ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(4, 4, 1);
    const auto floatExtent = fbExtent.asFloat();
    const auto pixelCount  = static_cast<uint32_t>(fbExtent.x() * fbExtent.y() * fbExtent.z());
    const auto pixelCountF = static_cast<float>(pixelCount);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto fbFormat    = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat   = mapVkFormat(fbFormat);
    const auto fbUsage     = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f);
    const tcu::Vec4 solidThreshold(0.0f, 0.0f, 0.0f, 0.0f);      // When using 0 and 1 only, we expect exact results.
    const tcu::Vec4 gradientThreshold(0.0f, 0.0f, 0.005f, 0.0f); // Allow a small mistake in the blue component.
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    // Color buffer with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    // Vertices.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float xCenter = (static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f;
            const float yCenter = (static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0f - 1.0f;
            vertices.emplace_back(xCenter, yCenter, 0.0f, 1.0f);
        }

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto &vbAlloc = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));

    // Indices. To make sure these are used we're going duplicate every even index and skip odd indices. And, on top
    // of that, we're going to apply an offset to each point.
    std::vector<int32_t> offsets;
    offsets.reserve(pixelCount);
    for (uint32_t i = 0u; i < pixelCount; ++i)
        offsets.push_back(100 + i);

    std::vector<uint32_t> indices;
    indices.reserve(pixelCount);
    for (uint32_t i = 0u; i < pixelCount; ++i)
        indices.push_back((i / 2u) * 2u + offsets.at(i));

    const auto ibSize = static_cast<VkDeviceSize>(de::dataSize(indices));
    const auto ibInfo = makeBufferCreateInfo(ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    BufferWithMemory indexBuffer(ctx.vkd, ctx.device, ctx.allocator, ibInfo, MemoryRequirement::HostVisible);
    const auto &ibAlloc = indexBuffer.getAllocation();
    void *ibData        = ibAlloc.getHostPtr();

    deMemcpy(ibData, de::dataOrNull(indices), de::dataSize(indices));

    // Pipeline, render pass, framebuffer...
    std::vector<VkPushConstantRange> pcRanges;
    if (pcToken)
        pcRanges.push_back(
            makePushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, DE_SIZEOF32(geomColor), DE_SIZEOF32(float)));
    pcRanges.push_back(makePushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0u, DE_SIZEOF32(geomColor)));

    const auto pipelineLayout =
        makePipelineLayout(ctx.vkd, ctx.device, 0u, nullptr, de::sizeU32(pcRanges), de::dataOrNull(pcRanges));
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, fbFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    // The default values works for the current setup, including the vertex input data format.
    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule, *renderPass, viewports,
                                               scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    // Indirect commands layout.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    if (pcToken)
    {
        // The color will be provided with a push constant token.
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRanges.back());
    }
    cmdsLayoutBuilder.addDrawIndexedToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC Buffer.
    std::vector<VkDrawIndexedIndirectCommand> drawCmds;
    drawCmds.reserve(pixelCount);
    for (size_t i = 0u; i < vertices.size(); ++i)
        drawCmds.push_back(VkDrawIndexedIndirectCommand{1u, 1u, static_cast<uint32_t>(i), -offsets.at(i), 0u});

    std::vector<uint32_t> dgcData;
    dgcData.reserve(pixelCount * (cmdsLayoutBuilder.getStreamStride() / DE_SIZEOF32(uint32_t)));
    for (size_t i = 0u; i < drawCmds.size(); ++i)
    {
        if (pcToken)
        {
            // Color pc token, making a gradient.
            const float blueComp = static_cast<float>(i) / pixelCountF;
            const tcu::Vec4 color(0.0f, 0.0f, blueComp, 1.0f);
            pushBackElement(dgcData, color);
        }
        pushBackElement(dgcData, drawCmds.at(i));
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    auto &dgcBufferAlloc = dgcBuffer.getAllocation();
    void *dgcBufferData  = dgcBufferAlloc.getHostPtr();
    deMemcpy(dgcBufferData, de::dataOrNull(dgcData), de::dataSize(dgcData));

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout,
                                         de::sizeU32(drawCmds), 0u, *pipeline);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    ctx.vkd.cmdBindIndexBuffer(cmdBuffer, indexBuffer.get(), 0ull, vk::VK_INDEX_TYPE_UINT32);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    if (pcToken)
    {
        // The fixed push constant will contain the point size for the vertex shader.
        const auto &pcRange = pcRanges.front();
        const float ptSz    = 1.0f;
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcRange.stageFlags, pcRange.offset, pcRange.size, &ptSz);
    }
    else
    {
        // A fixed geometry color in this case, for the fragment shader.
        const auto &pcRange = pcRanges.back();
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcRange.stageFlags, pcRange.offset, pcRange.size,
                                 &geomColor);
    }
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    {
        DGCGenCmdsInfo cmdsInfo(stageFlags, VK_NULL_HANDLE, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                de::sizeU32(drawCmds), 0ull, 0u, *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
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
    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const int pixelIdx   = y * fbExtent.x() + x;
            const bool drawnOver = (pixelIdx % 2 == 0); // Only even pixels are drawn into.

            tcu::Vec4 color(0.0f, 0.0f, 0.0f, 0.0f);

            if (pcToken)
            {
                // The passed color will be in the pc token and will change with each draw, forming a gradient. See above.
                if (drawnOver)
                {
                    // The +1 in the pixelIdx is because even points are drawn twice and the second color prevails.
                    const float blueComp = static_cast<float>(pixelIdx + 1) / pixelCountF;
                    color                = tcu::Vec4(0.0f, 0.0f, blueComp, 1.0f);
                }
                else
                    color = clearColor;
            }
            else
            {
                // Fixed color in this case.
                color = (drawnOver ? geomColor : clearColor);
            }
            referenceAccess.setPixel(color, x, y);
        }

    auto &log             = context.getTestContext().getLog();
    const auto &threshold = (pcToken ? gradientThreshold : solidThreshold);
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCGraphicsDrawTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "draw"));

    GroupPtr drawTokenGroup(new tcu::TestCaseGroup(testCtx, "token_draw"));
    GroupPtr drawIndexedTokenGroup(new tcu::TestCaseGroup(testCtx, "token_draw_indexed"));

    const struct
    {
        ExtraStages extraStages;
        const char *name;
    } extraStageCases[] = {
        {ExtraStages::NONE, ""},
        {ExtraStages::TESSELLATION, "_with_tess"},
        {ExtraStages::GEOMETRY, "_with_geom"},
    };

    const struct
    {
        PipelineType pipelineType;
        const std::string name;
    } pipelineTypeCases[] = {
        {PipelineType::MONOLITHIC, "monolithic"},
        {PipelineType::SHADER_OBJECTS, "shader_objects"},
        {PipelineType::GPL_FAST, "gpl_fast"},
        {PipelineType::GPL_OPTIMIZED, "gpl_optimized"},
        {PipelineType::GPL_MIX_BASE_FAST, "gpl_mix_base_fast"},
        {PipelineType::GPL_MIX_BASE_OPT, "gpl_mix_base_opt"},
    };

    const struct
    {
        PreprocessType preprocessType;
        const char *suffix;
    } preprocessCases[] = {
        {PreprocessType::NONE, ""},
        {PreprocessType::SAME_STATE_CMD_BUFFER, "_preprocess_same_state_cmd_buffer"},
        {PreprocessType::OTHER_STATE_CMD_BUFFER, "_preprocess_separate_state_cmd_buffer"},
    };

    for (const auto testType :
         {TestType::DRAW_SIMPLE, TestType::DRAW, TestType::DRAW_INDEXED, TestType::DRAW_INDEXED_DX})
        for (const auto &pipelineTypeCase : pipelineTypeCases)
            for (const auto useExecutionSet : {false, true})
            {
                if (isGPL(pipelineTypeCase.pipelineType) && !useExecutionSet)
                    continue;

                for (const auto &extraStageCase : extraStageCases)
                    for (const bool checkDrawParams : {false, true})
                        for (const auto &preprocessCase : preprocessCases)
                            for (const auto unordered : {false, true})
                            {
                                const DrawTestParams params{testType,
                                                            extraStageCase.extraStages,
                                                            pipelineTypeCase.pipelineType,
                                                            preprocessCase.preprocessType,
                                                            checkDrawParams,
                                                            useExecutionSet,
                                                            unordered};
                                const std::string testName = pipelineTypeCase.name + extraStageCase.name +
                                                             (useExecutionSet ? "_with_execution_set" : "") +
                                                             (checkDrawParams ? "_check_draw_params" : "") +
                                                             preprocessCase.suffix + (unordered ? "_unordered" : "") +
                                                             (testType == TestType::DRAW_SIMPLE ? "_simple" : "") +
                                                             (testType == TestType::DRAW_INDEXED_DX ? "_dx_index" : "");

                                const auto group =
                                    (isIndexed(testType) ? drawIndexedTokenGroup.get() : drawTokenGroup.get());

                                group->addChild(new DGCDrawCase(testCtx, testName, params));
                            }
            }

    for (const auto pcToken : {false, true})
    {
        const std::string testName =
            std::string("indexed_draw_without_index_buffer_token") + (pcToken ? "_with_pc_token" : "");
        addFunctionCaseWithPrograms(drawIndexedTokenGroup.get(), testName, checkBasicDGCGraphicsSupport,
                                    basicGraphicsPrograms, indexedDrawWithoutIndexTokenRun, pcToken);
    }

    mainGroup->addChild(drawTokenGroup.release());
    mainGroup->addChild(drawIndexedTokenGroup.release());

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
