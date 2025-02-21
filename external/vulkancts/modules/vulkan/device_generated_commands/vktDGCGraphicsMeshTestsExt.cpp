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
 * \brief Device Generated Commands EXT Mesh Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsMeshTestsExt.hpp"
#include "vktDGCGraphicsMeshConditionalTestsExt.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktDGCUtilExt.hpp"
#include "vktDGCUtilCommon.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"

#include <memory>
#include <map>
#include <iostream>
#include <vector>
#include <limits>
#include <cstddef>
#include <utility>

namespace vkt
{
namespace DGC
{

namespace
{

using namespace vk;

/*
TEST MECHANISM FOR MESH SHADER TESTS

The goal is testing mainly VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_EXT, with pipelines that use mesh shading
and a combination of task shaders, mesh shaders and draw parameters.

For that token, the indirect command data is:

typedef struct VkDrawMeshTasksIndirectCommandEXT {
    uint32_t    groupCountX;
    uint32_t    groupCountY;
    uint32_t    groupCountZ;
} VkDrawMeshTasksIndirectCommandEXT;

The goal is checking that each of those parameters can be varied and taken into account.

The framebuffer will have 32x32 pixels, there will be a triangle covering the center of each pixel.

There will be a storage buffer containing the vertices for each of those 1024 triangles, with triangles for each row
stored together, in row order from top to bottom.

We'll pseudorandomly divide the 32 rows in 8 sequences, and each sequence will handle a number of rows.

The dispatch command will have one main dimension that will be chosen pseudorandomly, with the other 2 staying at 1.

## Not using task shaders

When not using task shaders, the dispatch will launch 1 workgroup per row in the sequence. Push constants will be used
to tell each WG the first starting row, so that the WG index, combined with this "offset", can be used to calculate the
proper row for each WG.

Each WG will contain 32 invocations, and each invocation will prepare the triangle for one of the columns in the row.

## Using task shaders

As before, each WG dispatched will handle 1 row in the image. However, this time the task data will be used to pass
information to mesh shader work groups.

Each task WG will contain 16 invocations, and each one of those will prepare data for 2 pixels in the row.

Each mesh WG will contain only 1 invocation, and will generate geometry for a specific pixel, depending on its WG index.

The data that will be prepared from the task shader is:

struct TaskData {
    uint rowIndex;          // Set by first invocation.
    uint columnIndices[32]; // 2 of these items per invocation.
};

The column indices array will indicate which column will be handled by each mesh WG. It should contain all numbers from
0 to 31, but not neccessarily in that order, so that the mesh WGs handle one of the columns (pixels) each.

To make things even more interesting, we will not simply dispatch 32 mesh WGs per row in this case, but a possibly
smaller number, so that not every pixel in each row is covered.

We could have an input storage buffer with 32 positions, indicating how many columns we'll cover in each row. These can
be pseudorandomly generated.

layout (set=X, binding=Y, std430) readonly buffer CoverageBlock { uint colsPerRow[32]; } cb;
// cb.colsPerRow indexed by row index.

The Indirect Commands Layout will have the following tokens:

* VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT (only in some test variants)
* VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT
* VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_EXT

The work group index will be calculated like this:

    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z +
                                gl_NumWorkGroups.x * gl_WorkGroupID.y +
                                gl_WorkGroupID.x;

## Execution sets

In both cases, the mesh shader will output variables containing both the red color and green color for each triangle,
which will be used by the frag shader.

When using execution sets, some details will vary per shader.

* The blue color will be 1 or 0 depending on the frag shader.
* The red and green colors will vary depending on the mesh shader.
* The (optional) task shader will generate column indices in ascending or descending order.

For VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_COUNT_EXT the tests are basically identical, except that the 8
direct draws will be pseudorandomly divided into 4 groups of indirect draws, and we need to create 4 separate indirect
draw buffers to store the direct draw parameters, varying the stride as well.

*/

using DGCShaderExtPtr = std::unique_ptr<DGCShaderExt>;

constexpr uint32_t kSequenceCountDirect   = 8u;
constexpr uint32_t kSequenceCountIndirect = kSequenceCountDirect / 2u;
constexpr uint32_t kPerTriangleVertices   = 3u;
constexpr uint32_t kWidth                 = 32u;
constexpr uint32_t kHeight                = 32u;

enum class DrawType
{
    DIRECT = 0,
    INDIRECT,
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

bool isGPLMix(PipelineType pipelineType)
{
    return (pipelineType == PipelineType::GPL_MIX_BASE_FAST || pipelineType == PipelineType::GPL_MIX_BASE_OPT);
}

bool isShaderObjects(PipelineType pipelineType)
{
    return (pipelineType == PipelineType::SHADER_OBJECTS);
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

enum class PreprocessType
{
    NONE = 0,
    SAME_STATE_CMD_BUFFER,
    OTHER_STATE_CMD_BUFFER,
};

struct TestParams
{
    DrawType drawType;
    PipelineType pipelineType;
    PreprocessType preprocessType;
    bool taskShader;
    bool useExecutionSet;
    bool unorderedSequences;

    bool indirect(void) const
    {
        return (drawType == DrawType::INDIRECT);
    }

    uint32_t getRandomSeed(void) const
    {
        return (((static_cast<uint32_t>(pipelineType) + 1u) << 24) | (static_cast<uint32_t>(taskShader) << 23) |
                (static_cast<uint32_t>(useExecutionSet) << 22));
    }

    std::vector<float> getBlueColors(void) const
    {
        std::vector<float> blueColors;
        blueColors.push_back(1.0f);
        if (useExecutionSet)
            blueColors.push_back(0.5f);
        return blueColors;
    }

    std::vector<float> getGreenColors(void) const
    {
        std::vector<float> greenColors;
        greenColors.push_back(0.0f);
        if (useExecutionSet)
            greenColors.push_back(1.0f);
        return greenColors;
    }

    std::vector<float> getRedColors(void) const
    {
        std::vector<float> redColors;
        redColors.push_back(0.25f);
        if (useExecutionSet)
            redColors.push_back(0.75f);
        return redColors;
    }

    VkShaderStageFlags getPreRasterStages(void) const
    {
        VkShaderStageFlags stages = VK_SHADER_STAGE_MESH_BIT_EXT;
        if (taskShader)
            stages |= VK_SHADER_STAGE_TASK_BIT_EXT;
        return stages;
    }

    VkShaderStageFlags getAllStages(void) const
    {
        const VkShaderStageFlags stages = (getPreRasterStages() | VK_SHADER_STAGE_FRAGMENT_BIT);
        return stages;
    }

    uint32_t getFragShaderCount(void) const
    {
        return de::sizeU32(getBlueColors());
    }

    uint32_t getMeshShaderCount(void) const
    {
        const auto v1 = getRedColors();
        const auto v2 = getGreenColors();

        DE_ASSERT(v1.size() == v2.size());
        DE_UNREF(v2); // For release builds.

        return de::sizeU32(v1);
    }

    uint32_t getTaskShaderCount(void) const
    {
        if (!taskShader)
            return 0u;
        return (useExecutionSet ? 2u : 1u);
    }

    bool doPreprocess(void) const
    {
        return (preprocessType != PreprocessType::NONE);
    }
};

class DGCMeshDrawInstance : public vkt::TestInstance
{
public:
    DGCMeshDrawInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~DGCMeshDrawInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    TestParams m_params;
};

class DGCMeshDrawCase : public vkt::TestCase
{
public:
    DGCMeshDrawCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DGCMeshDrawCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    TestParams m_params;
};

void DGCMeshDrawCase::checkSupport(Context &context) const
{
    const auto stages                 = m_params.getAllStages();
    const auto bindStages             = (m_params.useExecutionSet ? stages : 0u);
    const bool useESO                 = isShaderObjects(m_params.pipelineType);
    const auto bindStagesPipeline     = (useESO ? 0u : bindStages);
    const auto bindStagesShaderObject = (useESO ? bindStages : 0u);
    const auto &dgcProperties         = context.getDeviceGeneratedCommandsPropertiesEXT();

    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject);
    context.requireDeviceFunctionality("VK_EXT_mesh_shader");

    if (useESO)
    {
        context.requireDeviceFunctionality("VK_EXT_shader_object");

        if (m_params.useExecutionSet && dgcProperties.maxIndirectShaderObjectCount == 0u)
            TCU_THROW(NotSupportedError, "maxIndirectShaderObjectCount is zero");
    }

    if (m_params.indirect())
    {
        if (!dgcProperties.deviceGeneratedCommandsMultiDrawIndirectCount)
            TCU_THROW(NotSupportedError, "deviceGeneratedCommandsMultiDrawIndirectCount not supported");
    }
}

TestInstance *DGCMeshDrawCase::createInstance(Context &context) const
{
    return new DGCMeshDrawInstance(context, m_params);
}

void DGCMeshDrawCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions shaderBuildOpt(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    // Frag shader(s).
    {
        const auto blueColors      = m_params.getBlueColors();
        const auto fragShaderCount = m_params.getFragShaderCount();

        DE_ASSERT(blueColors.size() == fragShaderCount);

        for (size_t i = 0u; i < fragShaderCount; ++i)
        {
            std::ostringstream frag;
            frag << "#version 460\n"
                 << "#extension GL_EXT_mesh_shader : enable\n"
                 << "\n"
                 << "layout (location=0) in perprimitiveEXT float redColor;\n"
                 << "layout (location=1) in flat float greenColor;\n"
                 << "layout (location=0) out vec4 outColor;\n"
                 << "void main(void) {\n"
                 << "    const float blueColor = " << blueColors.at(i) << ";\n"
                 << "    outColor = vec4(redColor, greenColor, blueColor, 1.0);\n"
                 << "}\n";
            const auto shaderName = "frag" + std::to_string(i);
            programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str()) << shaderBuildOpt;
        }
    }

    std::string wgIndexFunc;
    {
        std::ostringstream wgIndexFuncStream;
        wgIndexFuncStream
            << "uint getWorkGroupIndex (void) {\n"
            << "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z +\n"
            << "                                gl_NumWorkGroups.x * gl_WorkGroupID.y +\n"
            << "                                gl_WorkGroupID.x;\n"
            << "    return workGroupIndex;\n"
            << "}\n";
        wgIndexFunc = wgIndexFuncStream.str();
    }

    std::string pcDecl;
    {
        std::ostringstream pcDeclStream;
        pcDeclStream << "layout (push_constant, std430) uniform PushConstantBlock {\n"
                     << "    uint width;\n"
                     << "    uint height;\n"
                     << "    uint baseDrawIndex;\n"
                     << "} pc;\n";
        pcDecl = pcDeclStream.str();
    }

    std::string taskDataDecl;
    {
        std::ostringstream taskDataDeclStream;
        taskDataDeclStream << "struct TaskData {\n"
                           << "    uint rowIndex;          // Set by first task invocation.\n"
                           << "    uint columnIndices[" << kWidth << "]; // 2 of these items per task invocation.\n"
                           << "};\n"
                           << "taskPayloadSharedEXT TaskData td;\n";
        taskDataDecl = taskDataDeclStream.str();
    }

    std::string directDrawDataDecl;
    {
        std::ostringstream directDrawDataDeclStream;
        directDrawDataDeclStream << "layout(set=0, binding=1, std430) readonly buffer DirectDrawBaseRowBlock {\n"
                                 << "    uint baseRow[];\n"
                                 << "} directDrawData;\n";
        directDrawDataDecl = directDrawDataDeclStream.str();
    }

    // Mesh shader(s)
    {
        const auto redColors       = m_params.getRedColors();
        const auto greenColors     = m_params.getGreenColors();
        const auto meshShaderCount = m_params.getMeshShaderCount();

        DE_ASSERT(redColors.size() == greenColors.size());
        DE_ASSERT(redColors.size() == meshShaderCount);

        const auto maxVertices   = (m_params.taskShader ? kPerTriangleVertices : kPerTriangleVertices * kWidth);
        const auto maxPrimitives = (m_params.taskShader ? 1u : kWidth);
        const auto localSize     = (m_params.taskShader ? 1u : 32u);

        for (size_t i = 0u; i < meshShaderCount; ++i)
        {
            std::ostringstream mesh;
            mesh << "#version 460\n"
                 << "#extension GL_EXT_mesh_shader : enable\n"
                 << "\n"
                 << "struct VertexData {\n"
                 << "    vec4 position;\n"
                 << "    vec4 extraData;\n"
                 << "};\n"
                 << "\n"
                 << "layout(set=0, binding=0, std430) readonly buffer VertexDataBlock {\n"
                 << "    VertexData vertexData[];\n"
                 << "} vtxData;\n"
                 << directDrawDataDecl << "\n"
                 << (m_params.taskShader ? taskDataDecl : "") << pcDecl << "\n"
                 << "layout(local_size_x=" << localSize << ") in;\n"
                 << "layout(triangles) out;\n"
                 << "layout(max_vertices=" << maxVertices << ", max_primitives=" << maxPrimitives << ") out;\n"
                 << "\n"
                 << "layout (location=0) out perprimitiveEXT float redColor[];\n"
                 << "layout (location=1) out flat float greenColor[];\n"
                 << "\n"
                 << wgIndexFunc << "\n"
                 << "void main() {\n"
                 << "    const uint triangleVertices = " << kPerTriangleVertices << ";\n"
                 << "    const uint wgIndex = getWorkGroupIndex();\n"
                 << "    const uint rowIndex = "
                 << (m_params.taskShader ?
                         "td.rowIndex" :
                         "directDrawData.baseRow[pc.baseDrawIndex" +
                             std::string(m_params.indirect() ? " + uint(gl_DrawID)" : "") + "] + wgIndex")
                 << ";\n"
                 << "    const uint srcBasePrim = rowIndex * pc.width;\n"
                 << "    const uint srcPrim = srcBasePrim + "
                 << (m_params.taskShader ? "td.columnIndices[wgIndex]" : "gl_LocalInvocationIndex") << ";\n"
                 << "    const uint srcBaseVertex = srcPrim * triangleVertices;\n"
                 << "    const uint dstPrim = gl_LocalInvocationIndex;\n"
                 << "    const uint dstBaseVertex = dstPrim * triangleVertices;\n"
                 << "    SetMeshOutputsEXT(" << maxVertices << ", " << maxPrimitives << ");\n"
                 << "    for (uint i = 0u; i < triangleVertices; ++i) {\n"
                 << "        const uint dstIdx = dstBaseVertex + i;\n"
                 << "        const uint srcIdx = srcBaseVertex + i;\n"
                 << "        gl_MeshVerticesEXT[dstIdx].gl_Position = vtxData.vertexData[srcIdx].position;\n"
                 << "        gl_MeshVerticesEXT[dstIdx].gl_PointSize = 1.0;\n"
                 << "        gl_MeshVerticesEXT[dstIdx].gl_ClipDistance[0] = vtxData.vertexData[srcIdx].extraData.x;\n"
                 << "        gl_MeshVerticesEXT[dstIdx].gl_CullDistance[0] = vtxData.vertexData[srcIdx].extraData.y;\n"
                 << "        greenColor[dstIdx] = " << greenColors.at(i) << ";\n"
                 << "    }\n"
                 << "    gl_PrimitiveTriangleIndicesEXT[dstPrim] = uvec3(dstBaseVertex + 0, dstBaseVertex + 1, "
                    "dstBaseVertex + 2);\n"
                 << "    redColor[dstPrim] = " << redColors.at(i) << ";\n"
                 << "}\n";
            const auto shaderName = "mesh" + std::to_string(i);
            programCollection.glslSources.add(shaderName) << glu::MeshSource(mesh.str()) << shaderBuildOpt;
        }
    }

    // Task shader(s)
    {
        const size_t taskShaderCount = m_params.getTaskShaderCount();
        const auto localSize         = kWidth / 2u; // One invocation per two pixels.

        for (size_t i = 0u; i < taskShaderCount; ++i)
        {
            // i vs (total - 1) - i
            const auto valueOffset = (i == 0u ? 0u : (kWidth - 1u));
            const auto valueFactor = (i == 0u ? 1 : -1);

            std::ostringstream task;
            task << "#version 460\n"
                 << "#extension GL_EXT_mesh_shader : enable\n"
                 << "\n"
                 << directDrawDataDecl
                 << "layout (set=0, binding=2, std430) readonly buffer CoverageBlock { uint colsPerRow[" << kHeight
                 << "]; } cb;\n"
                 << "\n"
                 << taskDataDecl << "\n"
                 << pcDecl << "\n"
                 << "layout(local_size_x=" << localSize << ") in;\n"
                 << "\n"
                 << wgIndexFunc << "\n"
                 << "void main() {\n"
                 << "    const uint wgIndex = getWorkGroupIndex();\n"
                 << "    const uint rowIndex = directDrawData.baseRow[pc.baseDrawIndex" +
                        std::string(m_params.indirect() ? " + uint(gl_DrawID)" : "") + "] + wgIndex;\n"
                 << "    td.rowIndex = rowIndex;\n"
                 << "    const uint baseEntryIdx = gl_LocalInvocationIndex * 2u;\n"
                 << "    for (uint i = 0u; i < 2u; ++i) {\n"
                 << "        const uint idx = baseEntryIdx + i;\n"
                 << "        const uint value = uint(" << valueOffset << " + ((" << valueFactor << ") * int(idx)));\n"
                 << "        td.columnIndices[idx] = value;\n"
                 << "    }\n"
                 << "    EmitMeshTasksEXT(cb.colsPerRow[rowIndex], 1, 1);\n"
                 << "}\n";
            const auto shaderName = "task" + std::to_string(i);
            programCollection.glslSources.add(shaderName) << glu::TaskSource(task.str()) << shaderBuildOpt;
        }
    }
}

using ShaderWrapperPtr = std::unique_ptr<ShaderWrapper>;

Move<VkShaderEXT> makeShaderExt(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkShaderStageFlagBits stage,
                                vk::VkShaderCreateFlagsEXT shaderFlags, const vk::ProgramBinary &shaderBinary,
                                const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
                                const std::vector<vk::VkPushConstantRange> &pushConstantRanges)
{
    if (shaderBinary.getFormat() != PROGRAM_FORMAT_SPIRV)
        TCU_THROW(InternalError, "Program format not supported");

    VkShaderStageFlags nextStage = 0u;
    switch (stage)
    {
    case VK_SHADER_STAGE_TASK_BIT_EXT:
        nextStage |= VK_SHADER_STAGE_MESH_BIT_EXT;
        break;
    case VK_SHADER_STAGE_MESH_BIT_EXT:
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    const VkShaderCreateInfoEXT shaderCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        shaderFlags,                              // VkShaderCreateFlagsEXT flags;
        stage,                                    // VkShaderStageFlagBits stage;
        nextStage,                                // VkShaderStageFlags nextStage;
        VK_SHADER_CODE_TYPE_SPIRV_EXT,            // VkShaderCodeTypeEXT codeType;
        shaderBinary.getSize(),                   // size_t codeSize;
        shaderBinary.getBinary(),                 // const void* pCode;
        "main",                                   // const char* pName;
        de::sizeU32(setLayouts),                  // uint32_t setLayoutCount;
        de::dataOrNull(setLayouts),               // const VkDescriptorSetLayout* pSetLayouts;
        de::sizeU32(pushConstantRanges),          // uint32_t pushConstantRangeCount;
        de::dataOrNull(pushConstantRanges),       // const VkPushConstantRange* pPushConstantRanges;
        nullptr,                                  // const VkSpecializationInfo* pSpecializationInfo;
    };

    shaderBinary.setUsed();
    return createShader(vkd, device, shaderCreateInfo);
}

tcu::TestStatus DGCMeshDrawInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(static_cast<int>(kWidth), static_cast<int>(kHeight), 1);
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto descBufferType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto descBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto pixelCount      = kWidth * kHeight;
    const auto vtxCount        = pixelCount * kPerTriangleVertices;
    const auto allStages       = m_params.getAllStages();
    const auto bindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto sequenceCount =
        ((m_params.drawType == DrawType::DIRECT) ? kSequenceCountDirect : kSequenceCountIndirect);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    // Descriptor set layout.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descBufferType, VK_SHADER_STAGE_MESH_BIT_EXT);
    setLayoutBuilder.addSingleBinding(descBufferType, m_params.getPreRasterStages());
    if (m_params.taskShader)
        setLayoutBuilder.addSingleBinding(descBufferType, VK_SHADER_STAGE_TASK_BIT_EXT);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    // Push constants (must match shaders).
    struct PushConstants
    {
        uint32_t width;
        uint32_t height;
        uint32_t baseDrawIndex;
    };
    const auto pcStages = m_params.getPreRasterStages();
    const auto pcSize   = DE_SIZEOF32(PushConstants);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout. Note the wrapper only needs to know if it uses shader objects or not. The specific type is not
    // important as long as the category is correct.
    PipelineLayoutWrapper pipelineLayout((isShaderObjects(m_params.pipelineType) ?
                                              PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV :
                                              PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC),
                                         ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Vertex data (must match shader).
    struct VertexData
    {
        tcu::Vec4 position;
        tcu::Vec4 extraData; // .x=clip .y=cull
    };

    const auto normalizedCoords = [](int c, float size) { return (static_cast<float>(c) + 0.5f) / size * 2.0f - 1.0f; };

    const auto floatExtent  = fbExtent.asFloat();
    const float pixelWidth  = 2.0f / floatExtent.x();
    const float pixelHeight = 2.0f / floatExtent.y();
    const float horMargin   = pixelWidth / 4.0f;
    const float vertMargin  = pixelHeight / 4.0f;

    // For each of the vertices of the triangle surrounding the pixel center, as offsets from that center.
    const std::vector<tcu::Vec2> vertexOffsets{
        tcu::Vec2(-horMargin, vertMargin),
        tcu::Vec2(horMargin, vertMargin),
        tcu::Vec2(0.0f, -vertMargin),
    };

    // Chosen pseudorandomly for each triangle.
    const std::vector<float> clipDistances{0.75f, 0.0f, -0.5f, 1.25f, 20.0f, 2.0f, 0.25f, 1.0f};
    const std::vector<float> cullDistances{0.75f, 0.0f, 0.5f, 1.25f, 20.0f, 2.0f, -0.25f, 1.0f};

    de::Random rnd(m_params.getRandomSeed());

    std::vector<VertexData> vertices;
    vertices.reserve(vtxCount);

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const auto xCenter = normalizedCoords(x, floatExtent.x());
            const auto yCenter = normalizedCoords(y, floatExtent.y());

            const int clipDistanceIdx = rnd.getInt(0, static_cast<int>(clipDistances.size()) - 1);
            const int cullDistanceIdx = rnd.getInt(0, static_cast<int>(cullDistances.size()) - 1);

            const float clipDistance = clipDistances.at(clipDistanceIdx);
            const float cullDistance = cullDistances.at(cullDistanceIdx);

            for (const auto &offset : vertexOffsets)
            {
                const VertexData vertexData{
                    tcu::Vec4(xCenter + offset.x(), yCenter + offset.y(), 0.0f, 1.0),
                    tcu::Vec4(clipDistance, cullDistance, 0.0f, 0.0f),
                };
                vertices.push_back(vertexData);
            }
        }

    // Coverage block data for the task shader.
    std::vector<uint32_t> coverage(kHeight, kWidth);
    if (m_params.taskShader)
    {
        for (auto &colsPerRow : coverage)
            colsPerRow = static_cast<uint32_t>(rnd.getInt(0, fbExtent.x() - 1));
    }

    // Pseudorandomly distribute rows in sequences of direct draws.
    std::vector<uint32_t> drawRows(kSequenceCountDirect, 0u);
    const int maxPseudoRandomRows = static_cast<int>(kHeight / kSequenceCountDirect);
    {
        uint32_t remainingRows = kHeight;
        for (uint32_t i = 0u; i < kSequenceCountDirect - 1u; ++i)
        {
            const auto rowCount = static_cast<uint32_t>(rnd.getInt(1, maxPseudoRandomRows));
            drawRows.at(i)      = rowCount;
            remainingRows -= rowCount;
        }
        drawRows.back() = remainingRows;
    }

    // Create a vector of base rows for each direct draw, to be used in a descriptor (see directDrawData.baseRow)
    std::vector<uint32_t> baseRows(kSequenceCountDirect, 0u);
    {
        uint32_t prevRows = 0u;
        for (uint32_t i = 0u; i < kSequenceCountDirect; ++i)
        {
            baseRows.at(i) = prevRows;
            prevRows += drawRows.at(i);
        }
    }

    // Descriptor buffers.
    const auto vtxDataSize       = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vtxDataBufferInfo = makeBufferCreateInfo(vtxDataSize, descBufferUsage);
    BufferWithMemory vtxDataBuffer(ctx.vkd, ctx.device, ctx.allocator, vtxDataBufferInfo,
                                   MemoryRequirement::HostVisible);
    auto &vtxDataAlloc = vtxDataBuffer.getAllocation();
    void *vtxDataPtr   = vtxDataAlloc.getHostPtr();
    deMemcpy(vtxDataPtr, de::dataOrNull(vertices), de::dataSize(vertices));

    const auto baseRowsSize       = static_cast<VkDeviceSize>(de::dataSize(baseRows));
    const auto baseRowsBufferInfo = makeBufferCreateInfo(baseRowsSize, descBufferUsage);
    BufferWithMemory baseRowsBuffer(ctx.vkd, ctx.device, ctx.allocator, baseRowsBufferInfo,
                                    MemoryRequirement::HostVisible);
    auto &baseRowsAlloc = baseRowsBuffer.getAllocation();
    void *baseRowsPtr   = baseRowsAlloc.getHostPtr();
    deMemcpy(baseRowsPtr, de::dataOrNull(baseRows), de::dataSize(baseRows));

    const auto covDataSize       = static_cast<VkDeviceSize>(de::dataSize(coverage));
    const auto covDataBufferInfo = makeBufferCreateInfo(covDataSize, descBufferUsage);
    BufferWithMemory covDataBuffer(ctx.vkd, ctx.device, ctx.allocator, covDataBufferInfo,
                                   MemoryRequirement::HostVisible);
    auto &covDataAlloc = covDataBuffer.getAllocation();
    void *covDataPtr   = covDataAlloc.getHostPtr();
    deMemcpy(covDataPtr, de::dataOrNull(coverage), de::dataSize(coverage));

    // Descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descBufferType); // Vertex data.
    poolBuilder.addType(descBufferType); // Base row data.
    if (m_params.taskShader)
        poolBuilder.addType(descBufferType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    using Location = DescriptorSetUpdateBuilder::Location;
    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto vtxBufferDescInfo = makeDescriptorBufferInfo(vtxDataBuffer.get(), 0ull, vtxDataSize);
    const auto baseRowsDescInfo  = makeDescriptorBufferInfo(baseRowsBuffer.get(), 0ull, baseRowsSize);
    setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(0u), descBufferType, &vtxBufferDescInfo);
    setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(1u), descBufferType, &baseRowsDescInfo);
    if (m_params.taskShader)
    {
        const auto covDataBufferDescInfo = makeDescriptorBufferInfo(*covDataBuffer, 0ull, covDataSize);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(2u), descBufferType, &covDataBufferDescInfo);
    }
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Distribute groups of direct draws in single indirect draws when needed (how many direct draws per indirect one?)
    std::vector<uint32_t> directDrawGroupSizes;
    if (m_params.indirect())
    {
        directDrawGroupSizes.resize(kSequenceCountIndirect, 0u);
        const int maxPseudoRandomGroupSize = static_cast<int>(kSequenceCountDirect / kSequenceCountIndirect);
        uint32_t remainingDraws            = kSequenceCountDirect;

        for (uint32_t i = 0u; i < kSequenceCountIndirect - 1u; ++i)
        {
            const auto directDraws     = static_cast<uint32_t>(rnd.getInt(1, maxPseudoRandomGroupSize));
            directDrawGroupSizes.at(i) = directDraws;
            remainingDraws -= directDraws;
        }
        directDrawGroupSizes.back() = remainingDraws;
    }

    // Accumulated base direct draw indices (how many previous direct draws in indirect draw X?)
    // Note: these values go to the push constant token in the indirect case.
    std::vector<uint32_t> prevDirectDraws;
    if (m_params.indirect())
    {
        prevDirectDraws.resize(kSequenceCountIndirect, 0u);
        uint32_t prevDirectDrawCount = 0u;
        for (uint32_t i = 0u; i < kSequenceCountIndirect; ++i)
        {
            prevDirectDraws.at(i) = prevDirectDrawCount;
            prevDirectDrawCount += directDrawGroupSizes.at(i);
        }
    }

    // Renderpass and framebuffer.
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), kWidth, kHeight);

    // Viewport and scissor.
    const std::vector<VkViewport> viewports(1u, makeViewport(apiExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(apiExtent));

    // Pipelines.
    const auto &binaries = m_context.getBinaryCollection();

    // For pipelines, with or without execution sets.
    std::vector<ShaderWrapperPtr> meshModules;
    std::vector<ShaderWrapperPtr> fragModules;
    std::vector<ShaderWrapperPtr> taskModules;
    ShaderWrapper emptyModule;

    // For shader objects without DGC.
    std::vector<Move<VkShaderEXT>> meshShaders;
    std::vector<Move<VkShaderEXT>> fragShaders;
    std::vector<Move<VkShaderEXT>> taskShaders;

    // For shader objects with DGC.
    std::vector<DGCShaderExtPtr> meshDGCShaders;
    std::vector<DGCShaderExtPtr> fragDGCShaders;
    std::vector<DGCShaderExtPtr> taskDGCShaders;

    const std::vector<VkDescriptorSetLayout> setLayouts{*setLayout};
    const std::vector<VkPushConstantRange> pcRanges{pcRange};

    if (isShaderObjects(m_params.pipelineType))
    {
        const VkShaderCreateFlagsEXT meshFlags =
            (m_params.taskShader ? 0u : static_cast<VkShaderCreateFlagsEXT>(VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT));

        if (m_params.useExecutionSet)
        {
            for (uint32_t i = 0u; i < m_params.getMeshShaderCount(); ++i)
            {
                const auto name = "mesh" + std::to_string(i);
                meshDGCShaders.emplace_back(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_MESH_BIT_EXT,
                                                             meshFlags, binaries.get(name), setLayouts, pcRanges, false,
                                                             false));
            }
            for (uint32_t i = 0u; i < m_params.getFragShaderCount(); ++i)
            {
                const auto name = "frag" + std::to_string(i);
                fragDGCShaders.emplace_back(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                                                             binaries.get(name), setLayouts, pcRanges, false, false));
            }
            for (uint32_t i = 0u; i < m_params.getTaskShaderCount(); ++i)
            {
                const auto name = "task" + std::to_string(i);
                taskDGCShaders.emplace_back(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_TASK_BIT_EXT, 0u,
                                                             binaries.get(name), setLayouts, pcRanges, false, false));
            }
        }
        else
        {
            for (uint32_t i = 0u; i < m_params.getMeshShaderCount(); ++i)
            {
                const auto name = "mesh" + std::to_string(i);
                meshShaders.emplace_back(makeShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_MESH_BIT_EXT, meshFlags,
                                                       binaries.get(name), setLayouts, pcRanges));
            }
            for (uint32_t i = 0u; i < m_params.getFragShaderCount(); ++i)
            {
                const auto name = "frag" + std::to_string(i);
                fragShaders.emplace_back(makeShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                                                       binaries.get(name), setLayouts, pcRanges));
            }
            for (uint32_t i = 0u; i < m_params.getTaskShaderCount(); ++i)
            {
                const auto name = "task" + std::to_string(i);
                taskShaders.emplace_back(makeShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_TASK_BIT_EXT, 0u,
                                                       binaries.get(name), setLayouts, pcRanges));
            }
        }
    }
    else
    {
        for (uint32_t i = 0u; i < m_params.getMeshShaderCount(); ++i)
        {
            const auto name = "mesh" + std::to_string(i);
            meshModules.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(name)));
        }
        for (uint32_t i = 0u; i < m_params.getFragShaderCount(); ++i)
        {
            const auto name = "frag" + std::to_string(i);
            fragModules.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(name)));
        }
        for (uint32_t i = 0u; i < m_params.getTaskShaderCount(); ++i)
        {
            const auto name = "task" + std::to_string(i);
            taskModules.emplace_back(new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(name)));
        }
    }

    Move<VkPipeline> normalPipeline;

    using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;
    std::vector<GraphicsPipelineWrapperPtr> dgcPipelines;

    // Shaders that will be used in the different sequences when using indirect execution sets.
    const std::vector<uint32_t> meshShaderIndices{0u, 0u, 1u, 1u, 0u, 1u, 1u, 0u};
    const std::vector<uint32_t> fragShaderIndices{1u, 0u, 1u, 0u, 0u, 0u, 1u, 1u};
    const std::vector<uint32_t> taskShaderIndices{0u, 1u, 0u, 1u, 0u, 0u, 1u, 1u};

    DE_ASSERT(meshShaderIndices.size() >= sequenceCount);
    DE_ASSERT(fragShaderIndices.size() >= sequenceCount);
    DE_ASSERT(taskShaderIndices.size() >= sequenceCount);

    // Actual pipelines.
    if (m_params.useExecutionSet)
    {
        if (isShaderObjects(m_params.pipelineType))
            ; // DGC shaders were prepared above. Nothing to do here.
        else
        {
            dgcPipelines.resize(sequenceCount);

            const auto initialValue = getGeneralConstructionType(m_params.pipelineType);
            std::vector<PipelineConstructionType> constructionTypes(sequenceCount, initialValue);

            if (isGPLMix(m_params.pipelineType))
            {
                const auto altValue = (initialValue == PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY ?
                                           PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY :
                                           PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY);

                for (uint32_t i = 1u; i < sequenceCount; i += 2u)
                    constructionTypes.at(i) = altValue;
            }

            const VkPipelineCreateFlags2KHR creationFlags = VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT;

            for (uint32_t i = 0u; i < sequenceCount; ++i)
            {
                auto &pipeline = dgcPipelines.at(i);
                pipeline.reset(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(), constructionTypes.at(i)));

                const auto &taskModule = (m_params.taskShader ? *taskModules.at(taskShaderIndices.at(i)) : emptyModule);

                pipeline->setPipelineCreateFlags2(creationFlags)
                    .setDefaultRasterizationState()
                    .setDefaultColorBlendState()
                    .setDefaultMultisampleState()
                    .setupPreRasterizationMeshShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u,
                                                          taskModule, *meshModules.at(meshShaderIndices.at(i)))
                    .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, *fragModules.at(fragShaderIndices.at(i)))
                    .setupFragmentOutputState(*renderPass, 0u)
                    .setMonolithicPipelineLayout(pipelineLayout)
                    .buildPipeline();
            }
        }
    }
    else
    {
        // GPL Mix can only be tested with IES. Otherwise there's a single pipeline so mixing is not possible.
        DE_ASSERT(!isGPLMix(m_params.pipelineType));

        if (isShaderObjects(m_params.pipelineType))
            ; // Normal shaders were prepared above. Nothing to do here.
        else
        {
            DE_ASSERT(taskModules.size() <= 1u);
            DE_ASSERT(meshModules.size() == 1u);
            DE_ASSERT(fragModules.size() == 1u);

            const auto taskModule = ((!taskModules.empty()) ? taskModules.at(0u)->getModule() : VK_NULL_HANDLE);
            const auto meshModule = meshModules.at(0u)->getModule();
            const auto fragModule = fragModules.at(0u)->getModule();
            normalPipeline        = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, taskModule, meshModule,
                                                         fragModule, *renderPass, viewports, scissors);
        }
    }

    // Execution set manager. Slots for shader objects: mesh0, mesh1, frag0, frag1, [task0, task1]
    const auto baseMeshShaderIdx = 0u;
    const auto baseFragShaderIdx = 2u;
    const auto baseTaskShaderIdx = 4u;

    ExecutionSetManagerPtr executionSetManager;
    if (m_params.useExecutionSet)
    {
        if (isShaderObjects(m_params.pipelineType))
        {
            std::vector<IESStageInfo> stages;
            stages.emplace_back(meshDGCShaders.at(0u)->get(), setLayouts);
            stages.emplace_back(fragDGCShaders.at(0u)->get(), setLayouts);
            if (m_params.taskShader)
                stages.emplace_back(taskDGCShaders.at(0u)->get(), setLayouts);

            const auto maxShaderCount =
                m_params.getMeshShaderCount() + m_params.getFragShaderCount() + m_params.getTaskShaderCount();
            executionSetManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stages, pcRanges, maxShaderCount);

            for (uint32_t i = 0u; i < sequenceCount; ++i)
            {
                const auto meshIndex = meshShaderIndices.at(i);
                const auto meshSlot  = baseMeshShaderIdx + meshIndex;
                executionSetManager->addShader(meshSlot, meshDGCShaders.at(meshIndex)->get());

                const auto fragIndex = fragShaderIndices.at(i);
                const auto fragSlot  = baseFragShaderIdx + fragIndex;
                executionSetManager->addShader(fragSlot, fragDGCShaders.at(fragIndex)->get());

                if (m_params.taskShader)
                {
                    const auto taskIndex = taskShaderIndices.at(i);
                    const auto taskSlot  = baseTaskShaderIdx + taskIndex;
                    executionSetManager->addShader(taskSlot, taskDGCShaders.at(taskIndex)->get());
                }
            }
        }
        else
        {
            executionSetManager =
                makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipelines.at(0u)->getPipeline(), sequenceCount);
            for (uint32_t i = 0u; i < sequenceCount; ++i)
                executionSetManager->addPipeline(i, dgcPipelines.at(i)->getPipeline());
        }
        executionSetManager->update();
    }

    // Indirect commands layout and DGC data.

    // Push constants will be divided into general push constants and a DGC token.
    const auto baseDrawIndexOffset = static_cast<uint32_t>(offsetof(PushConstants, baseDrawIndex));
    const auto baseDrawIndexSize   = DE_SIZEOF32(uint32_t);

    VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags = 0u;
    if (m_params.doPreprocess())
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;
    if (m_params.unorderedSequences)
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT;

    const VkIndirectExecutionSetInfoTypeEXT executionSetType =
        (isShaderObjects(m_params.pipelineType) ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                                  VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, allStages, *pipelineLayout);
    if (m_params.useExecutionSet)
        cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(), executionSetType, allStages);
    {
        const VkPushConstantRange baseDrawIndexRange{pcRange.stageFlags, pcRange.offset + baseDrawIndexOffset,
                                                     baseDrawIndexSize};
        const auto tokenOffset = cmdsLayoutBuilder.getStreamRange();
        if (m_params.indirect())
            cmdsLayoutBuilder.addPushConstantToken(tokenOffset, baseDrawIndexRange);
        else
            cmdsLayoutBuilder.addSequenceIndexToken(tokenOffset, baseDrawIndexRange);
    }
    if (m_params.indirect())
        cmdsLayoutBuilder.addDrawMeshTasksCountToken(cmdsLayoutBuilder.getStreamRange());
    else
        cmdsLayoutBuilder.addDrawMeshTasksToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Direct draw commands used in the main DGC buffer, or to fill the DGC indirect buffers.
    std::vector<VkDrawMeshTasksIndirectCommandEXT> drawCmds;
    drawCmds.reserve(sequenceCount);
    for (const auto &seqRows : drawRows)
    {
        // We'll pseudorandomly choose the major dimension.
        const auto chosenDim = rnd.getInt(0, 2);
        tcu::UVec3 dispatchSize(1u, 1u, 1u);
        dispatchSize[chosenDim] = seqRows;
        drawCmds.push_back(VkDrawMeshTasksIndirectCommandEXT{dispatchSize.x(), dispatchSize.y(), dispatchSize.z()});
    }

    // Buffer info for each DGC indirect draw, if used.
    using DGCBufferPtr = std::unique_ptr<DGCBuffer>;
    struct IndirectBufferInfo
    {
        IndirectBufferInfo(DGCBuffer *buffer_, uint32_t extraStructs_) : buffer(buffer_), extraStructs(extraStructs_)
        {
        }

        DGCBufferPtr buffer;
        uint32_t extraStructs;
    };

    const auto getStrideBytes = [](uint32_t extraStructs)
    { return (extraStructs + 1u) * DE_SIZEOF32(VkDrawMeshTasksIndirectCommandEXT); };

    // Prepare contents for the buffers used with DGC indirect draws (one buffer per DGC indirect draw, so that the
    // address changes).
    std::vector<IndirectBufferInfo> indirectDrawBuffers;
    if (m_params.indirect())
    {
        constexpr int maxExtraStructs = 3;
        indirectDrawBuffers.reserve(kSequenceCountIndirect);
        for (uint32_t i = 0u; i < kSequenceCountIndirect; ++i)
        {
            // Vary the number of padding structures used so that the stride also varies per DGC indirect draw.
            const uint32_t extraStructs = static_cast<uint32_t>(rnd.getInt(0, maxExtraStructs));
            const uint32_t strideBytes  = getStrideBytes(extraStructs);

            const auto bufferSize = static_cast<VkDeviceSize>(strideBytes * directDrawGroupSizes.at(i));
            DGCBufferPtr buffer(new DGCBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferSize));

            indirectDrawBuffers.emplace_back(buffer.release(), extraStructs);
        }

        // Fill data for indirect buffers, grouping direct draws and storing them with a certain stride.
        const VkDrawMeshTasksIndirectCommandEXT emptyIndirectCmd{0u, 0u, 0u};
        for (uint32_t i = 0u; i < kSequenceCountIndirect; ++i)
        {
            const uint32_t prevDirectDrawCount = prevDirectDraws.at(i);
            const uint32_t curDirectDrawCount  = directDrawGroupSizes.at(i);

            const uint32_t strideItems = (indirectDrawBuffers.at(i).extraStructs + 1u);
            const uint32_t itemCount   = strideItems * curDirectDrawCount;

            std::vector<VkDrawMeshTasksIndirectCommandEXT> bufferContents(itemCount, emptyIndirectCmd);

            uint32_t nextIndex = 0u;
            for (uint32_t j = 0u; j < curDirectDrawCount; ++j)
            {
                bufferContents.at(nextIndex) = drawCmds.at(prevDirectDrawCount + j);
                nextIndex += strideItems;
            }

            auto &buffer = indirectDrawBuffers.at(i).buffer;
            DE_ASSERT(buffer->getSize() == de::dataSize(bufferContents));
            void *bufferDataPtr = buffer->getAllocation().getHostPtr();
            deMemcpy(bufferDataPtr, de::dataOrNull(bufferContents), de::dataSize(bufferContents));
        }
    }

    // DGC indirect draw commands. These go into the main DGC buffer in the indirect case. See below.
    std::vector<VkDrawIndirectCountIndirectCommandEXT> indirectDrawCmds;
    uint32_t maxDrawCount = 0u;
    if (m_params.indirect())
    {
        indirectDrawCmds.reserve(kSequenceCountIndirect);
        for (uint32_t i = 0u; i < kSequenceCountIndirect; ++i)
        {
            const auto &bufferInfo = indirectDrawBuffers.at(i);
            indirectDrawCmds.push_back(VkDrawIndirectCountIndirectCommandEXT{
                bufferInfo.buffer->getDeviceAddress(),
                getStrideBytes(bufferInfo.extraStructs),
                directDrawGroupSizes.at(i),
            });
            if (directDrawGroupSizes.at(i) > maxDrawCount)
                maxDrawCount = directDrawGroupSizes.at(i);
        }
    }
    if (rnd.getBool())
        maxDrawCount *= 2u;

    std::vector<uint32_t> dgcData;
    dgcData.reserve((sequenceCount * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t));

    for (size_t i = 0u; i < sequenceCount; ++i)
    {
        if (m_params.useExecutionSet)
        {
            if (isShaderObjects(m_params.pipelineType))
            {
                // Bit order: fragment, [task], mesh.
                dgcData.push_back(baseFragShaderIdx + fragShaderIndices.at(i));
                if (m_params.taskShader)
                    dgcData.push_back(baseTaskShaderIdx + taskShaderIndices.at(i));
                dgcData.push_back(baseMeshShaderIdx + meshShaderIndices.at(i));
            }
            else
                dgcData.push_back(static_cast<uint32_t>(i));
        }
        if (m_params.indirect())
            pushBackElement(dgcData, prevDirectDraws.at(i)); // Previous number of direct draws.
        else
            pushBackElement(dgcData, std::numeric_limits<uint32_t>::max()); // Placeholder for sequence index token.

        // Push the element corresponding to the type of draw.
        if (m_params.indirect())
            pushBackElement(dgcData, indirectDrawCmds.at(i));
        else
            pushBackElement(dgcData, drawCmds.at(i));
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    auto &dgcBufferAlloc = dgcBuffer.getAllocation();
    void *dgcBufferData  = dgcBufferAlloc.getHostPtr();
    deMemcpy(dgcBufferData, de::dataOrNull(dgcData), de::dataSize(dgcData));

    // Preprocess buffer.
    const auto indirectExecutionSet = (executionSetManager ? executionSetManager->get() : VK_NULL_HANDLE);
    std::vector<VkShaderEXT> shadersVec;
    if (isShaderObjects(m_params.pipelineType) && !m_params.useExecutionSet)
    {
        if (!taskShaders.empty())
            shadersVec.push_back(*taskShaders.at(0u));

        DE_ASSERT(!meshShaders.empty());
        DE_ASSERT(!fragShaders.empty());

        shadersVec.push_back(*meshShaders.at(0u));
        shadersVec.push_back(*fragShaders.at(0u));
    }
    const auto shadersVecPtr = (shadersVec.empty() ? nullptr : &shadersVec);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, indirectExecutionSet, *cmdsLayout,
                                         sequenceCount, maxDrawCount, *normalPipeline, shadersVecPtr);

    // Command buffer.
    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    const auto clearValueColor = makeClearValueColor(clearColor);
    const auto colorSRR        = makeDefaultImageSubresourceRange();

    const DGCGenCmdsInfo cmdsInfo(allStages, indirectExecutionSet, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                  dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  sequenceCount, 0ull, pixelCount, *normalPipeline, shadersVecPtr);

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
        const auto recCmdBuffer = stateCmdBufferPair.first;

        // Only begin each command buffer once.
        if (recCmdBuffer != prevCmdBuffer)
        {
            beginCommandBuffer(ctx.vkd, recCmdBuffer);
            prevCmdBuffer = recCmdBuffer;
        }

        // Preprocessing either does not happen or happens in the second iteration.
        if (stateCmdBufferPair.second != VK_NULL_HANDLE)
        {
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(recCmdBuffer, &cmdsInfo.get(), stateCmdBufferPair.second);
            separateStateCmdBuffer =
                Move<VkCommandBuffer>(); // Delete state cmd buffer immediately as allowed by the spec.

            preprocessToExecuteBarrierExt(ctx.vkd, recCmdBuffer);

            // Break for iteration 1 of PreprocessType::SAME_STATE_CMD_BUFFER. See above.
            if (stateCmdBufferPair.first == stateCmdBufferPair.second)
                break;
        }

        ctx.vkd.cmdBindDescriptorSets(recCmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u,
                                      nullptr);
        {
            // Static part of the push constants: width and height. Does not vary per sequence.
            // It will be complemented by DGC tokens.
            const auto pcValues = fbExtent.swizzle(0, 1).asUint();
            ctx.vkd.cmdPushConstants(recCmdBuffer, *pipelineLayout, pcRange.stageFlags, pcRange.offset,
                                     DE_SIZEOF32(pcValues), &pcValues);
        }

        if (isShaderObjects(m_params.pipelineType))
        {
            std::map<VkShaderStageFlagBits, VkShaderEXT> shaderMap;

            if (m_params.useExecutionSet)
            {
                shaderMap[VK_SHADER_STAGE_TASK_BIT_EXT] =
                    (m_params.taskShader ? taskDGCShaders.at(0u)->get() : VK_NULL_HANDLE);
                shaderMap[VK_SHADER_STAGE_MESH_BIT_EXT] = meshDGCShaders.at(0u)->get();
                shaderMap[VK_SHADER_STAGE_FRAGMENT_BIT] = fragDGCShaders.at(0u)->get();
            }
            else
            {
                shaderMap[VK_SHADER_STAGE_TASK_BIT_EXT] = (m_params.taskShader ? *taskShaders.at(0u) : VK_NULL_HANDLE);
                shaderMap[VK_SHADER_STAGE_MESH_BIT_EXT] = *meshShaders.at(0u);
                shaderMap[VK_SHADER_STAGE_FRAGMENT_BIT] = *fragShaders.at(0u);
            }

            {
                const auto &features = m_context.getDeviceFeatures();

                shaderMap[VK_SHADER_STAGE_VERTEX_BIT] = VK_NULL_HANDLE;

                if (features.geometryShader)
                    shaderMap[VK_SHADER_STAGE_GEOMETRY_BIT] = VK_NULL_HANDLE;

                if (features.tessellationShader)
                {
                    shaderMap[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
                    shaderMap[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
                }
            }
            for (const auto &stageShader : shaderMap)
                ctx.vkd.cmdBindShadersEXT(recCmdBuffer, 1u, &stageShader.first, &stageShader.second);

            bindShaderObjectState(ctx.vkd, getDeviceCreationExtensions(m_context), recCmdBuffer, viewports, scissors,
                                  VK_PRIMITIVE_TOPOLOGY_LAST, 0u, nullptr, nullptr, nullptr, nullptr, nullptr);
        }
        else
        {
            if (m_params.useExecutionSet)
                ctx.vkd.cmdBindPipeline(recCmdBuffer, bindPoint, dgcPipelines.at(0u)->getPipeline());
            else
                ctx.vkd.cmdBindPipeline(recCmdBuffer, bindPoint, *normalPipeline);
        }
    }

    if (isShaderObjects(m_params.pipelineType))
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

#if 1
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(m_params.doPreprocess()), &cmdsInfo.get());
#else
    for (uint32_t i = 0u; i < sequenceCount; ++i)
    {
        if (m_params.useExecutionSet)
        {
            if (isShaderObjects(m_params.pipelineType))
            {
                std::map<VkShaderStageFlagBits, VkShaderEXT> shaderMap;

                if (m_params.taskShader)
                {
                    const auto taskIndex                    = taskShaderIndices.at(i);
                    shaderMap[VK_SHADER_STAGE_TASK_BIT_EXT] = taskDGCShaders.at(taskIndex)->get();
                }
                else
                {
                    shaderMap[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;
                }
                {
                    const auto meshIndex                    = meshShaderIndices.at(i);
                    shaderMap[VK_SHADER_STAGE_MESH_BIT_EXT] = meshDGCShaders.at(meshIndex)->get();
                }
                {
                    const auto fragIndex                    = fragShaderIndices.at(i);
                    shaderMap[VK_SHADER_STAGE_FRAGMENT_BIT] = fragDGCShaders.at(fragIndex)->get();
                }

                for (const auto &stageShader : shaderMap)
                    ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageShader.first, &stageShader.second);
            }
            else
            {
                ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, dgcPipelines.at(i)->getPipeline());
            }
        }
        if (m_params.indirect())
        {
            ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, baseDrawIndexOffset, baseDrawIndexSize,
                                     &prevDirectDraws.at(i));
            ctx.vkd.cmdDrawMeshTasksIndirectEXT(cmdBuffer, indirectDrawBuffers.at(i).buffer->get(), 0ull,
                                                directDrawGroupSizes.at(i),
                                                getStrideBytes(indirectDrawBuffers.at(i).extraStructs));
        }
        else
        {
            ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, baseDrawIndexOffset, baseDrawIndexSize, &i);
            ctx.vkd.cmdDrawMeshTasksEXT(cmdBuffer, drawCmds.at(i).groupCountX, drawCmds.at(i).groupCountY,
                                        drawCmds.at(i).groupCountZ);
        }
    }
#endif

    if (isShaderObjects(m_params.pipelineType))
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

    const auto getDirectDrawIndex = [&drawRows](uint32_t rowIndex)
    {
        uint32_t prevRows = 0u;
        for (uint32_t i = 0u; i < kSequenceCountDirect; ++i)
        {
            const auto seqRows = drawRows.at(i);
            if (rowIndex >= prevRows && rowIndex < prevRows + seqRows)
                return i;
            prevRows += seqRows;
        }
        return std::numeric_limits<uint32_t>::max();
    };

    const auto getIndirectDrawIndex = [&directDrawGroupSizes](uint32_t directDrawIndex)
    {
        uint32_t prevDraws = 0u;
        for (uint32_t i = 0u; i < kSequenceCountIndirect; ++i)
        {
            const auto groupDraws = directDrawGroupSizes.at(i);
            if (directDrawIndex >= prevDraws && directDrawIndex < prevDraws + groupDraws)
                return i;
            prevDraws += groupDraws;
        }
        return std::numeric_limits<uint32_t>::max();
    };

    tcu::clear(reference, clearColor);

    const auto redColors   = m_params.getRedColors();
    const auto greenColors = m_params.getGreenColors();
    const auto blueColors  = m_params.getBlueColors();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const auto ux = static_cast<uint32_t>(x);
            const auto uy = static_cast<uint32_t>(y);

            const auto iesIndex =
                (m_params.indirect() ? getIndirectDrawIndex(getDirectDrawIndex(uy)) : getDirectDrawIndex(uy));
            const auto reversed =
                (m_params.taskShader && m_params.useExecutionSet && taskShaderIndices.at(iesIndex) > 0u);

            // A pixel is not drawn into if the column doesn't have coverage in that row, or if the clip and cull distances are below zero for that triangle.
            bool blank = ((!reversed && ux >= coverage.at(uy)) || (reversed && (kWidth - ux - 1) >= coverage.at(uy)));

            const auto firstVertexIdx = (uy * kWidth + ux) * kPerTriangleVertices;
            const auto &extraData     = vertices.at(firstVertexIdx).extraData;
            const auto &clipDistance  = extraData.x();
            const auto &cullDistance  = extraData.y();
            blank                     = (blank || clipDistance < 0.0f || cullDistance < 0.0f);

            tcu::Vec4 pixelColor = clearColor;
            if (!blank)
            {
                const auto meshShaderIdx = (m_params.useExecutionSet ? meshShaderIndices.at(iesIndex) : 0u);
                const auto fragShaderIdx = (m_params.useExecutionSet ? fragShaderIndices.at(iesIndex) : 0u);

                const auto red   = redColors.at(meshShaderIdx);
                const auto green = greenColors.at(meshShaderIdx);
                const auto blue  = blueColors.at(fragShaderIdx);

                pixelColor = tcu::Vec4(red, green, blue, 1.0f); // Must match shaders, of course.
            }

            reference.setPixel(pixelColor, x, y);
        }

    auto &log                  = m_context.getTestContext().getLog();
    const float thresholdValue = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(thresholdValue, thresholdValue, thresholdValue, 0.0f);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

class NoFragInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        PipelineConstructionType constructionType;
        bool hasTask;
        bool useIES;
        bool preprocess;

        VkShaderStageFlags getShaderStages() const
        {
            VkShaderStageFlags stages = VK_SHADER_STAGE_MESH_BIT_EXT;
            if (hasTask)
                stages |= VK_SHADER_STAGE_TASK_BIT_EXT;
            return stages;
        }

        uint32_t getShadersPerSequence() const
        {
            return (1u + (hasTask ? 1u : 0u)); // Mesh and optional task shader.
        }

        uint32_t getRandomSeed() const
        {
            return (((static_cast<int>(constructionType) + 1u) << 8u) | static_cast<int>(hasTask));
        }

        std::vector<uint32_t> getTaskValues() const
        {
            DE_ASSERT(hasTask);
            std::vector<uint32_t> values{1000000u};
            if (useIES)
                values.push_back(2000000u);
            return values;
        }

        std::vector<uint32_t> getMeshValues() const
        {
            std::vector<uint32_t> values{(hasTask ? 3000000u : 1000000u)};
            if (useIES)
                values.push_back(hasTask ? 4000000u : 2000000u);
            return values;
        }

        uint32_t getWGFactor() const
        {
            return 1000u;
        }

        uint32_t getWorkGroupSize() const
        {
            return 64u;
        }

        uint32_t getOutputArraySize() const
        {
            return 1024u;
        }
    };

    NoFragInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~NoFragInstance() = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class NoFragCase : public vkt::TestCase
{
public:
    NoFragCase(tcu::TestContext &testCtx, const std::string &name, const NoFragInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~NoFragCase() = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new NoFragInstance(context, m_params);
    }

protected:
    const NoFragInstance::Params m_params;
};

void NoFragCase::checkSupport(Context &context) const
{
    const auto ctx = context.getContextCommonData();

    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);

    const auto stages                 = m_params.getShaderStages();
    const auto bindStages             = (m_params.useIES ? stages : 0u);
    const bool useShaderObjects       = isConstructionTypeShaderObject(m_params.constructionType);
    const auto bindStagesPipeline     = (useShaderObjects ? 0u : bindStages);
    const auto bindStagesShaderObject = (useShaderObjects ? bindStages : 0u);

    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject);
}

void NoFragCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const vk::ShaderBuildOptions shaderBuildOpt(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
    const auto wgSize   = m_params.getWorkGroupSize();
    const auto outSize  = m_params.getOutputArraySize();
    const auto wgFactor = std::to_string(m_params.getWGFactor());
    std::string taskDataDecl;

    if (m_params.hasTask)
    {
        std::ostringstream taskDataDeclStream;
        taskDataDeclStream << "struct TaskData {\n"
                           << "    uint globalWorkGroupID;\n"
                           << "};\n"
                           << "taskPayloadSharedEXT TaskData td;\n";
        taskDataDecl = taskDataDeclStream.str();

        const auto taskValues = m_params.getTaskValues();
        for (size_t i = 0u; i < taskValues.size(); ++i)
        {
            std::ostringstream task;
            task << "#version 460\n"
                 << "#extension GL_EXT_mesh_shader : enable\n"
                 << "layout (local_size_x=" << wgSize << ", local_size_y=1, local_size_z=1) in;\n"
                 << "layout (push_constant, std430) uniform PCBlock { uint prevWGCount; } pc;\n"
                 << "layout (set=0, binding=0, std430) buffer OutputBlock { uint values[" << outSize
                 << "]; } taskBuffer;\n"
                 << taskDataDecl << "void main() {\n"
                 << "    const uint globalWorkGroupID = pc.prevWGCount + gl_WorkGroupID.x;\n"
                 << "    const uint slotIndex = globalWorkGroupID * gl_WorkGroupSize.x + gl_LocalInvocationIndex;\n"
                 << "    const uint value = " << taskValues.at(i) << " + globalWorkGroupID * " << wgFactor
                 << " + gl_LocalInvocationIndex;\n"
                 << "    taskBuffer.values[slotIndex] = value;\n"
                 << "    if (gl_LocalInvocationIndex == 0u) {\n"
                 << "        td.globalWorkGroupID = globalWorkGroupID;\n"
                 << "    }\n"
                 << "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
                 << "}\n";
            const auto taskName = "task" + std::to_string(i);
            programCollection.glslSources.add(taskName) << glu::TaskSource(task.str()) << shaderBuildOpt;
        }
    }

    {
        const auto meshValues        = m_params.getMeshValues();
        const auto meshBufferBinding = (m_params.hasTask ? 1u : 0u);

        for (size_t i = 0u; i < meshValues.size(); ++i)
        {
            std::ostringstream mesh;
            mesh << "#version 460\n"
                 << "#extension GL_EXT_mesh_shader : enable\n"
                 << "layout (local_size_x=" << wgSize << ", local_size_y=1, local_size_z=1) in;\n"
                 << (m_params.hasTask ? taskDataDecl :
                                        "layout (push_constant, std430) uniform PCBlock { uint prevWGCount; } pc;\n")
                 << "layout (set=0, binding=" << meshBufferBinding << ", std430) buffer OutputBlock { uint values["
                 << outSize << "]; } meshBuffer;\n"
                 << "layout (points) out;\n"
                 << "layout (max_vertices=1, max_primitives=1) out;\n"
                 << "void main() {\n"
                 << "    const uint globalWorkGroupID = "
                 << (m_params.hasTask ? "td.globalWorkGroupID" : "pc.prevWGCount + gl_WorkGroupID.x") << ";\n"
                 << "    const uint slotIndex = globalWorkGroupID * gl_WorkGroupSize.x + gl_LocalInvocationIndex;\n"
                 << "    const uint value = " << meshValues.at(i) << " + globalWorkGroupID * " << wgFactor
                 << " + gl_LocalInvocationIndex;\n"
                 << "    meshBuffer.values[slotIndex] = value;\n"
                 << "    SetMeshOutputsEXT(0u, 0u);\n"
                 << "}\n";
            const auto meshName = "mesh" + std::to_string(i);
            programCollection.glslSources.add(meshName) << glu::MeshSource(mesh.str()) << shaderBuildOpt;
        }
    }
}

tcu::TestStatus NoFragInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();

    // Main output buffer. This will be used by the mesh or the task shader, whichever is launched.
    const auto arraySize = m_params.getOutputArraySize();
    std::vector<uint32_t> bufferValues(arraySize, 0u);
    const auto outputBufferInfo = makeBufferCreateInfo(de::dataSize(bufferValues), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    BufferWithMemory mainBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = mainBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(bufferValues), de::dataSize(bufferValues));
    }

    // Used by the mesh shader when the task shader is present.
    std::unique_ptr<BufferWithMemory> secondaryBuffer;
    if (m_params.hasTask)
    {
        secondaryBuffer.reset(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, outputBufferInfo, MemoryRequirement::HostVisible));
        auto &alloc = secondaryBuffer->getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(bufferValues), de::dataSize(bufferValues));
    }

    // Descriptor pool, set and pipeline layout.
    const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto mainStage =
        static_cast<VkShaderStageFlags>(m_params.hasTask ? VK_SHADER_STAGE_TASK_BIT_EXT : VK_SHADER_STAGE_MESH_BIT_EXT);
    const auto secondaryStage = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_MESH_BIT_EXT);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, mainStage);
    if (m_params.hasTask)
        setLayoutBuilder.addSingleBinding(descType, secondaryStage);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    const auto pcStages = mainStage;
    const auto pcSize   = DE_SIZEOF32(uint32_t);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device, *setLayout, &pcRange);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, (m_params.hasTask ? 2u : 1u)); // Main and secondary or just main.
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    using Location = DescriptorSetUpdateBuilder::Location;
    {
        const auto descInfo = makeDescriptorBufferInfo(*mainBuffer, 0ull, VK_WHOLE_SIZE);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(0u), descType, &descInfo);
    }
    if (m_params.hasTask)
    {
        const auto descInfo = makeDescriptorBufferInfo(secondaryBuffer->get(), 0ull, VK_WHOLE_SIZE);
        setUpdateBuilder.writeSingle(*descriptorSet, Location::binding(1u), descType, &descInfo);
    }
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Pipelines.
    using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;
    std::vector<GraphicsPipelineWrapperPtr> pipelines;
    const auto pipelineCount = de::sizeU32(m_params.getMeshValues());

    const auto &binaries = m_context.getBinaryCollection();
    const tcu::IVec3 extent(1, 1, 1);
    const auto apiExtent = makeExtent3D(extent);
    const std::vector<VkViewport> viewports{makeViewport(extent)};
    const std::vector<VkRect2D> scissors{makeRect2D(extent)};

    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, 0u, nullptr, nullptr, apiExtent.width, apiExtent.height);

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_TRUE, // Discard rasterization results.
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };

    const auto pipelineCreationFlags = (m_params.useIES ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT : 0);
    const auto shaderCreateFlags     = (m_params.useIES ? VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT : 0);
    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();

    for (uint32_t i = 0u; i < pipelineCount; ++i)
    {
        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(), m_params.constructionType));
        auto &pipeline = *pipelines.back();

        const auto iStr     = std::to_string(i);
        const auto meshName = "mesh" + iStr;
        const auto taskName = "task" + iStr;

        ShaderWrapper meshShader(ctx.vkd, ctx.device, binaries.get(meshName));
        ShaderWrapperPtr taskShader;
        taskShader.reset(m_params.hasTask ? (new ShaderWrapper(ctx.vkd, ctx.device, binaries.get(taskName))) :
                                            new ShaderWrapper());

        pipeline.setPipelineCreateFlags2(pipelineCreationFlags)
            .setShaderCreateFlags(shaderCreateFlags)
            .setDefaultDepthStencilState()
            .setDefaultMultisampleState()
            .setupPreRasterizationMeshShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u,
                                                  *taskShader, meshShader, &rasterizationStateCreateInfo)
            .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, ShaderWrapper())
            .setupFragmentOutputState(renderPass.get(), 0u, &colorBlendStateCreateInfo)
            .buildPipeline();
    }

    std::vector<uint32_t> dispatchSizes;
    const auto groupSize = m_params.getWorkGroupSize();
    DE_ASSERT(arraySize % groupSize == 0u);
    const auto totalGroups = arraySize / groupSize;

    const uint32_t seed = m_params.getRandomSeed();
    de::Random rnd(seed);
    dispatchSizes.push_back(static_cast<uint32_t>(rnd.getInt(1, static_cast<int>(totalGroups) - 1)));
    dispatchSizes.push_back(totalGroups - dispatchSizes.front());

    // Push constant values in each iteration.
    std::vector<uint32_t> pcValues;
    pcValues.reserve(dispatchSizes.size());
    uint32_t prevGroupCount = 0u;
    for (size_t i = 0u; i < dispatchSizes.size(); ++i)
    {
        pcValues.push_back(prevGroupCount);
        prevGroupCount += dispatchSizes.at(i);
    }

    // DGC pieces.
    const auto sequenceCount      = de::sizeU32(dispatchSizes);
    const auto shadersPerSequence = m_params.getShadersPerSequence();
    const auto shaderStages       = m_params.getShaderStages();
    const bool useESO             = isConstructionTypeShaderObject(m_params.constructionType);

    const auto cmdsLayoutFlags =
        (m_params.preprocess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, *pipelineLayout);
    if (m_params.useIES)
    {
        const auto iesType = (useESO ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                       VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
        cmdsLayoutBuilder.addExecutionSetToken(0u, iesType, shaderStages);
    }
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawMeshTasksToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<uint32_t> dgcData;
    dgcData.reserve((sequenceCount * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t));
    for (uint32_t i = 0u; i < de::sizeU32(dispatchSizes); ++i)
    {
        if (m_params.useIES)
        {
            if (useESO)
            {
                dgcData.push_back(i * shadersPerSequence);
                if (m_params.hasTask)
                {
                    DE_ASSERT(shadersPerSequence == 2u);
                    dgcData.push_back(i * shadersPerSequence + 1u);
                }
            }
            else
                dgcData.push_back(i);
        }
        dgcData.push_back(pcValues.at(i));      // Push constant token value.
        dgcData.push_back(dispatchSizes.at(i)); // Dispatch X.
        dgcData.push_back(1u);                  // Dispatch Y.
        dgcData.push_back(1u);                  // Dispatch Z.
    }

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc = dgcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;

    VkPipeline preprocessPipeline = VK_NULL_HANDLE;
    std::vector<VkShaderEXT> preprocessShaders;

    if (m_params.useIES)
    {
        if (useESO)
        {
            const std::vector<VkDescriptorSetLayout> setLayouts{*setLayout};
            const std::vector<VkPushConstantRange> pcRanges{pcRange};

            std::vector<IESStageInfo> stages;
            if (m_params.hasTask)
                stages.push_back(IESStageInfo{pipelines.front()->getShader(VK_SHADER_STAGE_TASK_BIT_EXT), setLayouts});
            stages.push_back(IESStageInfo{pipelines.front()->getShader(VK_SHADER_STAGE_MESH_BIT_EXT), setLayouts});
            DE_ASSERT(shadersPerSequence == de::sizeU32(stages));

            const uint32_t maxShaderCount = sequenceCount * shadersPerSequence;
            iesManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stages, pcRanges, maxShaderCount);

            // Task,Mesh,Task,Mesh or Mesh,Mesh
            for (uint32_t i = 0u; i < sequenceCount; ++i)
            {
                const auto &pipeline = *pipelines.at(i);
                if (m_params.hasTask)
                    iesManager->addShader(i * shadersPerSequence, pipeline.getShader(VK_SHADER_STAGE_TASK_BIT_EXT));
                iesManager->addShader(i * shadersPerSequence + (m_params.hasTask ? 1u : 0u),
                                      pipeline.getShader(VK_SHADER_STAGE_MESH_BIT_EXT));
            }
        }
        else
        {
            iesManager =
                makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, pipelines.front()->getPipeline(), sequenceCount);
            for (uint32_t i = 0u; i < sequenceCount; ++i)
                iesManager->addPipeline(i, pipelines.at(i)->getPipeline());
        }

        iesManager->update();
        iesHandle = iesManager->get();
    }
    else
    {
        if (useESO)
        {
            if (m_params.hasTask)
                preprocessShaders.push_back(pipelines.front()->getShader(VK_SHADER_STAGE_TASK_BIT_EXT));
            preprocessShaders.push_back(pipelines.front()->getShader(VK_SHADER_STAGE_MESH_BIT_EXT));
        }
        else
            preprocessPipeline = pipelines.front()->getPipeline();
    }

    const auto preprocessShadersPtr = (preprocessShaders.empty() ? nullptr : &preprocessShaders);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, sequenceCount, 0u,
                                         preprocessPipeline, preprocessShadersPtr);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto preprocessCmdBuffer = (m_params.preprocess ? allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool,
                                                                                  VK_COMMAND_BUFFER_LEVEL_PRIMARY) :
                                                            Move<VkCommandBuffer>());

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u));
#if 1
    {
        pipelines.front()->bind(cmdBuffer); // Bind initial state.
        const DGCGenCmdsInfo cmdsInfo(shaderStages, iesHandle, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                      dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(),
                                      preprocessBuffer.getSize(), sequenceCount, 0ull, 0u, preprocessPipeline,
                                      preprocessShadersPtr);

        if (m_params.preprocess)
        {
            beginCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmdBuffer, &cmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmdBuffer);
            endCommandBuffer(ctx.vkd, *preprocessCmdBuffer);
        }
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(m_params.preprocess), &cmdsInfo.get());
    }
#else
    for (size_t i = 0u; i < dispatchSizes.size(); ++i)
    {
        pipelines.at(i % pipelines.size())->bind(cmdBuffer);
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, 0u, pcSize, &pcValues.at(i));
        ctx.vkd.cmdDrawMeshTasksEXT(cmdBuffer, dispatchSizes.at(i), 1u, 1u);
    }
#endif
    renderPass.end(ctx.vkd, cmdBuffer);
    {
        const auto barrier              = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
        if (m_params.hasTask)
            stageFlags |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT;
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, stageFlags, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmdBuffer);

    const auto mainValues      = (m_params.hasTask ? m_params.getTaskValues() : m_params.getMeshValues());
    const auto secondaryValues = m_params.getMeshValues();
    const auto wgFactor        = m_params.getWGFactor();

    // Main buffer verification.
    auto &log = m_context.getTestContext().getLog();
    bool fail = false;

    struct BufferVerification
    {
        std::string bufferName;
        const BufferWithMemory *pBuffer;
        const std::vector<uint32_t> *pBaseValues;
    };

    std::vector<BufferVerification> verifications;
    verifications.push_back(BufferVerification{"binding=0", &mainBuffer, &mainValues});
    if (m_params.hasTask)
        verifications.push_back(BufferVerification{"binding=1", secondaryBuffer.get(), &secondaryValues});

    for (const auto &verification : verifications)
    {
        auto &alloc = verification.pBuffer->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);

        memcpy(de::dataOrNull(bufferValues), alloc.getHostPtr(), de::dataSize(bufferValues));

        prevGroupCount = 0u;
        for (size_t i = 0u; i < dispatchSizes.size(); ++i)
        {
            const auto wgCount = dispatchSizes.at(i);
            for (uint32_t j = 0u; j < wgCount; ++j)
            {
                const auto wgIndex = prevGroupCount + j;
                for (uint32_t k = 0u; k < groupSize; ++k)
                {
                    const auto expectedValue =
                        verification.pBaseValues->at(i % verification.pBaseValues->size()) + wgIndex * wgFactor + k;
                    const auto arrayIndex  = wgIndex * groupSize + k;
                    const auto resultValue = bufferValues.at(arrayIndex);

                    if (expectedValue != resultValue)
                    {
                        log << tcu::TestLog::Message << "Unexpected value in " << verification.bufferName
                            << " buffer index " << arrayIndex << ": expected " << expectedValue << " but found "
                            << resultValue << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }
            }

            prevGroupCount += wgCount;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected values found in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCGraphicsMeshTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "mesh"));
    GroupPtr directGroup(new tcu::TestCaseGroup(testCtx, "token_draw"));
    GroupPtr indirectGroup(new tcu::TestCaseGroup(testCtx, "token_draw_count"));
    GroupPtr miscGroup(new tcu::TestCaseGroup(testCtx, "misc"));

    struct PipelineCase
    {
        PipelineType pipelineType;
        const char *name;
    };

    const PipelineCase pipelineCases[] = {
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

    for (const auto &drawType : {DrawType::DIRECT, DrawType::INDIRECT})
        for (const auto &pipelineCase : pipelineCases)
            for (const bool taskShader : {false, true})
                for (const bool useExecutionSet : {false, true})
                {
                    if (isGPLMix(pipelineCase.pipelineType) && !useExecutionSet)
                        continue;

                    for (const auto &preprocessCase : preprocessCases)
                        for (const bool unorderedSequences : {false, true})
                        {
                            const TestParams params{
                                drawType,   pipelineCase.pipelineType, preprocessCase.preprocessType,
                                taskShader, useExecutionSet,           unorderedSequences,
                            };

                            const auto testName = std::string(pipelineCase.name) +
                                                  (taskShader ? "_with_task_shader" : "") +
                                                  (useExecutionSet ? "_with_execution_set" : "") +
                                                  preprocessCase.suffix + (unorderedSequences ? "_unordered" : "");

                            auto &targetGroup = (drawType == DrawType::DIRECT ? directGroup : indirectGroup);
                            targetGroup->addChild(new DGCMeshDrawCase(testCtx, testName, params));
                        }
                }

    struct PCaseMatch
    {
        PCaseMatch(PipelineType pType_) : pType(pType_)
        {
        }
        bool operator()(const PipelineCase &pCase)
        {
            return pCase.pipelineType == pType;
        }
        PipelineType pType;
    };
    const auto first =
        std::find_if(std::begin(pipelineCases), std::end(pipelineCases), PCaseMatch(PipelineType::MONOLITHIC));
    const auto last =
        std::find_if(std::begin(pipelineCases), std::end(pipelineCases), PCaseMatch(PipelineType::GPL_FAST));

    for (auto i = first; i <= last; ++i)
        for (const bool hasTask : {false, true})
            for (const bool useIES : {false, true})
                for (const bool preprocess : {false, true})
                {
                    const NoFragInstance::Params params{
                        getGeneralConstructionType(i->pipelineType),
                        hasTask,
                        useIES,
                        preprocess,
                    };
                    const auto testName = std::string("no_frag_shader_") + i->name + (hasTask ? "_with_task" : "") +
                                          (useIES ? "_with_ies" : "") + (preprocess ? "_preprocess" : "");
                    miscGroup->addChild(new NoFragCase(testCtx, testName, params));
                }

    mainGroup->addChild(directGroup.release());
    mainGroup->addChild(indirectGroup.release());
    mainGroup->addChild(miscGroup.release());
    mainGroup->addChild(createDGCGraphicsMeshConditionalTestsExt(testCtx));

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
