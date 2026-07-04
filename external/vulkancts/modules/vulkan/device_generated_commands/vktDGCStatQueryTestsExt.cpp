/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Valve Corporation.
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
 * \brief Device Generated Commands EXT Stat Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCStatQueryTestsExt.hpp"
#include "vktDGCUtilCommon.hpp"
#include "vktDGCUtilExt.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"

#include "tcuImageCompare.hpp"

#include <bitset>
#include <map>
#include <memory>
#include <numeric>
#include <vector>

namespace vkt
{
namespace DGC
{

namespace
{

using namespace vk;

struct TestParams
{
    VkQueryPipelineStatisticFlags stats;
    PipelineConstructionType constructionType;
    bool useExecutionSet;
    bool preprocess;

    bool hasAnyStats(VkQueryPipelineStatisticFlags stats_) const
    {
        return ((stats & stats_) != 0u);
    }

    bool needsMeshShading() const
    {
        return hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT |
                           VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT);
    }

    bool needsTessellation() const
    {
        const auto shaderStages = getShaderStageFlags();
        return (shaderStages &
                (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
    }

    VkShaderStageFlags getShaderStageFlags() const
    {
        VkShaderStageFlags flags = 0u;

        if (hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT))
        {
            flags |= VK_SHADER_STAGE_COMPUTE_BIT;
            return flags;
        }

        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;

        if (!needsMeshShading())
            flags |= VK_SHADER_STAGE_VERTEX_BIT;

        if (hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT))
            flags |= (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

        if (hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
                        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT))
            flags |= VK_SHADER_STAGE_GEOMETRY_BIT;

        if (hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT))
            flags |= VK_SHADER_STAGE_TASK_BIT_EXT;

        if (hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT))
            flags |= VK_SHADER_STAGE_MESH_BIT_EXT;

        return flags;
    }

    uint32_t getShaderVariantCount(VkShaderStageFlagBits stage) const
    {
        const auto shaderStages = getShaderStageFlags();

        if ((shaderStages & stage) == 0u)
            return 0u; // Stage not needed.

        if (!useExecutionSet)
            return 1u; // Needed but not using IES, so 1 variant always.

        // The rest depends on the stat we are interested in.
        if (stage == VK_SHADER_STAGE_VERTEX_BIT &&
            hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
                        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
                        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
                        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT))
            return 2u;
        else if (stage == VK_SHADER_STAGE_GEOMETRY_BIT &&
                 hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT))
            return 2u;
        else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT &&
                 hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT))
            return 2u;
        else if (stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT &&
                 hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT))
            return 2u;
        else if (stage == VK_SHADER_STAGE_COMPUTE_BIT &&
                 hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT))
            return 2u;
        else if (stage == VK_SHADER_STAGE_TASK_BIT_EXT &&
                 hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT))
            return 2u;
        else if (stage == vk::VK_SHADER_STAGE_MESH_BIT_EXT &&
                 hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT))
            return 2u;

        return 1u;
    }

    tcu::Vec4 getClearColor() const
    {
        // Shader colors always have alpha 1.0, so this one is different.
        return tcu::Vec4(0.0f);
    }

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(8, 8, 1);
    }

    uint32_t getCompWorkGroupSize() const
    {
        return 64u;
    }

    uint32_t getCompWorkGroupCount() const
    {
        return 64u;
    }
};
using TestParamsPtr = std::shared_ptr<const TestParams>;

void checkSupport(Context &context, TestParamsPtr params)
{
    const auto ctx = context.getContextCommonData();

    const auto shaderStages           = params->getShaderStageFlags();
    const bool useESO                 = isConstructionTypeShaderObject(params->constructionType);
    const auto bindStagesPipeline     = (params->useExecutionSet && !useESO ? shaderStages : 0u);
    const auto bindStagesShaderObject = (params->useExecutionSet && useESO ? shaderStages : 0u);
    checkDGCExtSupport(context, shaderStages, bindStagesPipeline, bindStagesShaderObject);

    if (params->needsMeshShading())
    {
        const auto &meshFeatures = context.getMeshShaderFeaturesEXT();
        if (!(meshFeatures.taskShader && meshFeatures.meshShader && meshFeatures.meshShaderQueries))
            TCU_THROW(NotSupportedError, "Mesh shader queries not supported");
    }

    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, params->constructionType);

    if (shaderStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if (shaderStages & VK_SHADER_STAGE_GEOMETRY_BIT)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY);
}

void initPrograms(SourceCollections &dst, TestParamsPtr params)
{
    {
        const auto vertShaderCount = params->getShaderVariantCount(VK_SHADER_STAGE_VERTEX_BIT);
        for (uint32_t i = 0u; i < vertShaderCount; ++i)
        {
            std::ostringstream vert;
            vert << "#version 460\n"
                 << "layout (location=0) in vec4 inPos;\n"
                 << "layout (location=0) out vec4 outColor;\n"
                 << "out gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "    float gl_PointSize;\n"
                 << "};\n"
                 << "void main(void) {\n"
                 << "    gl_Position = inPos;\n"
                 << "    gl_PointSize = 1.0;\n"
                 << "    outColor = vec4(" << i << ".0, 0.0, 1.0, 1.0);\n"
                 << "}\n";
            const auto shaderName = "vert" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::VertexSource(vert.str());
        }
    }

    {
        const auto tescShaderCount = params->getShaderVariantCount(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        for (uint32_t i = 0u; i < tescShaderCount; ++i)
        {
            const bool forceGreen = (i > 0u);
            std::ostringstream tesc;
            tesc << "#version 460\n"
                 << "#extension GL_EXT_tessellation_shader : require\n"
                 << "layout (vertices=3) out;\n"
                 << "layout (location=0) in vec4 inColor[];\n"
                 << "layout (location=0) out vec4 outColor[];\n"
                 << "in gl_PerVertex\n"
                 << "{\n"
                 << "    vec4 gl_Position;\n"
                 << "    float gl_PointSize;\n"
                 << "} gl_in[gl_MaxPatchVertices];\n"
                 << "out gl_PerVertex\n"
                 << "{\n"
                 << "    vec4 gl_Position;\n"
                 << "    float gl_PointSize;\n"
                 << "} gl_out[];\n"
                 << "void main(void) {\n"
                 << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                 << "    gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
                 << "    gl_TessLevelOuter[0] = 1.0;\n"
                 << "    gl_TessLevelOuter[1] = 1.0;\n"
                 << "    gl_TessLevelOuter[2] = 1.0;\n"
                 << "    gl_TessLevelOuter[3] = 1.0;\n"
                 << "    gl_TessLevelInner[0] = 1.0;\n"
                 << "    gl_TessLevelInner[1] = 1.0;\n"
                 << (forceGreen ? "    outColor[gl_InvocationID] = vec4(inColor[gl_InvocationID].r, 1.0, "
                                  "inColor[gl_InvocationID].ba);\n" :
                                  "    outColor[gl_InvocationID] = inColor[gl_InvocationID];\n")
                 << "}\n";
            const auto shaderName = "tesc" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::TessellationControlSource(tesc.str());
        }
    }

    {
        const auto teseShaderCount = params->getShaderVariantCount(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        for (uint32_t i = 0u; i < teseShaderCount; ++i)
        {
            const bool invert = (i > 0u);
            std::ostringstream tese;
            tese << "#version 460\n"
                 << "#extension GL_EXT_tessellation_shader : require\n"
                 << "layout (triangles) in;\n"
                 << "in gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "    float gl_PointSize;\n"
                 << "} gl_in[gl_MaxPatchVertices];\n"
                 << "out gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "    float gl_PointSize;\n"
                 << "};\n"
                 << "layout (location=0) in vec4 inColor[];\n"
                 << "layout (location=0) out vec4 outColor;\n"
                 << "void main(void) {\n"
                 << "    gl_Position = (gl_in[0].gl_Position * gl_TessCoord.x +\n"
                 << "                   gl_in[1].gl_Position * gl_TessCoord.y +\n"
                 << "                   gl_in[2].gl_Position * gl_TessCoord.z);\n"
                 << "    gl_PointSize = gl_in[0].gl_PointSize;\n"
                 << (invert ? "    outColor = vec4(1.0 - inColor[0].r, 1.0 - inColor[0].g, 1.0 - inColor[0].b, "
                              "inColor[0].a);\n" :
                              "    outColor = inColor[0];\n")
                 << "}\n";
            const auto shaderName = "tese" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::TessellationEvaluationSource(tese.str());
        }
    }

    {
        const auto geomShaderCount = params->getShaderVariantCount(VK_SHADER_STAGE_GEOMETRY_BIT);
        for (uint32_t i = 0u; i < geomShaderCount; ++i)
        {
            const bool swizzle = (i > 0u);
            std::ostringstream geom;
            geom << "#version 460\n"
                 << "layout (triangles) in;\n"
                 << "layout (triangle_strip, max_vertices=3) out;\n"
                 << "in gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "    float gl_PointSize;\n"
                 << "} gl_in[3];\n"
                 << "out gl_PerVertex {\n"
                 << "    vec4 gl_Position;\n"
                 << "    float gl_PointSize;\n"
                 << "};\n"
                 << "layout (location=0) in vec4 inColor[];\n"
                 << "layout (location=0) out vec4 outColor;\n"
                 << "void main() {\n"
                 << "    for (uint i = 0; i < 3; ++i) {\n"
                 << "        gl_Position = gl_in[i].gl_Position;\n"
                 << "        gl_PointSize = gl_in[i].gl_PointSize;\n"
                 << (swizzle ? "        outColor = inColor[i].gbra;\n" : "        outColor = inColor[i];\n")
                 << "        EmitVertex();\n"
                 << "    }\n"
                 << "}\n";
            const auto shaderName = "geom" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::GeometrySource(geom.str());
        }
    }

    {
        const auto fragShaderCount = params->getShaderVariantCount(VK_SHADER_STAGE_FRAGMENT_BIT);
        for (uint32_t i = 0u; i < fragShaderCount; ++i)
        {
            const bool swizzle = (i > 0u);
            std::ostringstream frag;
            frag << "#version 460\n"
                 << "layout (location=0) in vec4 inColor;\n"
                 << "layout (location=0) out vec4 outColor;\n"
                 << "void main(void) {\n"
                 << (swizzle ? "    outColor = inColor.gbra;\n" : "    outColor = inColor;\n") << "}\n";
            const auto shaderName = "frag" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
        }
    }

    {
        const auto compShaderCount = params->getShaderVariantCount(VK_SHADER_STAGE_COMPUTE_BIT);
        for (uint32_t i = 0u; i < compShaderCount; ++i)
        {
            std::ostringstream comp;
            comp << "#version 460\n"
                 << "layout (local_size_x=64) in;\n"
                 << "layout (push_constant, std430) uniform PCBlock { uint wgOffset; } pc;\n"
                 << "layout (set=0, binding=0, std430) buffer BufferBlock {\n"
                 << "    uint values[];\n"
                 << "} ssbo;\n"
                 << "void main(void) {\n"
                 << "    uint global_id = (gl_WorkGroupID.x + pc.wgOffset) * gl_WorkGroupSize.x + "
                    "gl_LocalInvocationIndex;\n"
                 << "    ssbo.values[global_id] = " << (i + 1) << "u;\n"
                 << "}\n";
            const auto shaderName = "comp" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::ComputeSource(comp.str());
        }
    }

    {
        const auto buildOptions    = ShaderBuildOptions(dst.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
        const auto taskShaderCount = params->getShaderVariantCount(VK_SHADER_STAGE_TASK_BIT_EXT);
        for (uint32_t i = 0u; i < taskShaderCount; ++i)
        {
            std::ostringstream task;
            task << "#version 460\n"
                 << "#extension GL_EXT_mesh_shader : enable\n"
                 << "layout (local_size_x=32) in;\n"
                 << "layout (push_constant, std430) uniform PCBlock { uint rowOffset; } pc;\n"
                 << "struct TaskPayload {\n"
                 << "    vec4 color;\n"
                 << "    uint row;\n"
                 << "};\n"
                 << "taskPayloadSharedEXT TaskPayload payload;\n"
                 << "void main(void) {\n"
                 << "    payload.color = vec4(" << i << ".0, 0.0, 1.0, 1.0);\n"
                 << "    payload.row = gl_WorkGroupID.x + pc.rowOffset;\n"
                 << "    EmitMeshTasksEXT(8u, 1u, 1u);\n"
                 << "}\n";
            const auto shaderName = "task" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::TaskSource(task.str()) << buildOptions;
        }
    }

    {
        const auto buildOptions    = ShaderBuildOptions(dst.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
        const auto meshShaderCount = params->getShaderVariantCount(VK_SHADER_STAGE_MESH_BIT_EXT);
        for (uint32_t i = 0u; i < meshShaderCount; ++i)
        {
            const bool forceGreen = (i > 0u);
            std::ostringstream mesh;
            mesh << "#version 460\n"
                 << "#extension GL_EXT_mesh_shader : enable\n"
                 << "layout (local_size_x=64) in;\n"
                 << "struct TaskPayload {\n"
                 << "    vec4 color;\n"
                 << "    uint row;\n"
                 << "};\n"
                 << "taskPayloadSharedEXT TaskPayload payload;\n"
                 << "layout (triangles) out;\n"
                 << "layout (max_vertices=3, max_primitives=1) out;\n"
                 << "layout (set=0, binding=0, std430) readonly buffer PositionsBlock {\n"
                 << "    vec4 positions[];\n"
                 << "} ssbo;\n"
                 << "layout (location=0) out vec4 outColor[];\n"
                 << "void main(void) {\n"
                 << "    uint pixel_id = payload.row * 8 + gl_WorkGroupID.x;\n"
                 << "    uint vertex_id = pixel_id * 3 + gl_LocalInvocationIndex;\n"
                 << "    SetMeshOutputsEXT(3u, 1u);\n"
                 << "    if (gl_LocalInvocationIndex < 3)\n"
                 << "    {\n"
                 << "        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = ssbo.positions[vertex_id];\n"
                 << (forceGreen ?
                         "        outColor[gl_LocalInvocationIndex] = vec4(payload.color.r, 1.0, payload.color.ba);\n" :
                         "        outColor[gl_LocalInvocationIndex] = payload.color;\n")
                 << "    }\n"
                 << "    if (gl_LocalInvocationIndex == 0)\n"
                 << "        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
                 << "}\n";
            const auto shaderName = "mesh" + std::to_string(i);
            dst.glslSources.add(shaderName) << glu::MeshSource(mesh.str()) << buildOptions;
        }
    }
}

using BufferWithMemoryPtr        = std::unique_ptr<BufferWithMemory>;
using ImageWithBufferPtr         = std::unique_ptr<ImageWithBuffer>;
using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;

tcu::TestStatus iterate(Context &context, TestParamsPtr params)
{
    const auto ctx          = context.getContextCommonData();
    const auto isComp       = params->hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT);
    const auto isMesh       = params->needsMeshShading();
    const auto isTess       = params->needsTessellation();
    const auto clearColor   = params->getClearColor();
    const auto shaderStages = params->getShaderStageFlags();
    const auto useESO       = isConstructionTypeShaderObject(params->constructionType);
    const auto extent       = params->getExtent();
    const auto extentVk     = makeExtent3D(extent);
    const auto extentU      = extent.asUint();
    const auto pixelCount   = extentU.x() * extentU.y() * extentU.z();
    const auto colorFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto imageType        = VK_IMAGE_TYPE_2D;
    const auto hasFragStats     = params->hasAnyStats(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT);
    const bool useTriangleStrip = hasFragStats; // We'll use triangle strips for the frag shader stat case.

    ImageWithBufferPtr colorBuffer;
    if (!isComp)
        colorBuffer.reset(
            new ImageWithBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, colorFormat, colorUsage, imageType));

    const auto compShaderCount     = params->getShaderVariantCount(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto compBufferUsage     = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto compWorkGroupSize   = params->getCompWorkGroupSize();
    const auto compWorkGroupCount  = params->getCompWorkGroupCount();
    const auto compBufferItemCount = compWorkGroupSize * compWorkGroupCount;
    const auto compBufferSize      = static_cast<VkDeviceSize>(compBufferItemCount * DE_SIZEOF32(uint32_t));
    const auto vertBufferUsage     = static_cast<VkBufferUsageFlags>(isMesh ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT :
                                                                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    BufferWithMemoryPtr compBuffer;
    BufferWithMemoryPtr vertBuffer;
    std::vector<tcu::Vec4> vertices;

    if (isComp)
    {
        const auto compBufferInfo = makeBufferCreateInfo(compBufferSize, compBufferUsage);
        compBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, compBufferInfo, HostIntent::R));
    }
    else
    {
        if (useTriangleStrip)
        {
            // Two half-screen quads (horizontal division).
            vertices.reserve(8u);

            // clang-format off
            vertices.emplace_back(-1.0f, -1.0f, 0.0f, 1.0f);
            vertices.emplace_back(-1.0f,  0.0f, 0.0f, 1.0f);
            vertices.emplace_back( 1.0f, -1.0f, 0.0f, 1.0f);
            vertices.emplace_back( 1.0f,  0.0f, 0.0f, 1.0f);

            vertices.emplace_back(-1.0f,  0.0f, 0.0f, 1.0f);
            vertices.emplace_back(-1.0f,  1.0f, 0.0f, 1.0f);
            vertices.emplace_back( 1.0f,  0.0f, 0.0f, 1.0f);
            vertices.emplace_back( 1.0f,  1.0f, 0.0f, 1.0f);
            // clang-format on
        }
        else
        {
            // One triangle per pixel for the other cases.
            const auto floatExtent   = extent.asFloat();
            const auto triangleCount = extentU.x() * extentU.y();
            const auto vertexCount   = triangleCount * 3u;

            const float halfPixelHoriz = (2.0f / floatExtent.x()) * 0.5f;
            const float halfPixelVert  = (2.0f / floatExtent.y()) * 0.5f;

            const auto normalize = [](int x, float total)
            { return (static_cast<float>(x) + 0.5f) / total * 2.0f - 1.0f; };

            vertices.reserve(vertexCount);
            for (int y = 0; y < extent.y(); ++y)
                for (int x = 0; x < extent.x(); ++x)
                {
                    const auto xCenter = normalize(x, floatExtent.x());
                    const auto yCenter = normalize(y, floatExtent.y());

                    vertices.emplace_back(xCenter - halfPixelHoriz, yCenter + halfPixelVert, 0.0f, 1.0f);
                    vertices.emplace_back(xCenter + halfPixelHoriz, yCenter + halfPixelVert, 0.0f, 1.0f);
                    vertices.emplace_back(xCenter, yCenter - halfPixelVert, 0.0f, 1.0f);
                }
        }

        const auto vertBufferSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
        const auto vertBufferInfo = makeBufferCreateInfo(vertBufferSize, vertBufferUsage);
        vertBuffer.reset(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, vertBufferInfo, HostIntent::W));
        {
            auto &alloc = vertBuffer->getAllocation();
            memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }
    }

    const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    Move<VkDescriptorPool> descPool;
    Move<VkDescriptorSetLayout> setLayout;
    Move<VkDescriptorSet> descSet;

    // Both the compute and task shaders need a uint32_t push constant.
    const auto pcSize     = DE_SIZEOF32(uint32_t);
    const auto pcRange    = makePushConstantRange(shaderStages, 0u, pcSize);
    const auto pcRangePtr = (isMesh || isComp ? &pcRange : nullptr);

    if (isComp || isMesh)
    {
        // These are the only stages that have descriptors.
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(descType);
        descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(descType, shaderStages);
        setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
        descSet   = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

        DescriptorSetUpdateBuilder setUpdateBuilder;
        const auto binding      = DescriptorSetUpdateBuilder::Location::binding;
        const auto bufferHandle = (isMesh ? vertBuffer->get() : compBuffer->get());
        const auto bufferDesc   = makeDescriptorBufferInfo(bufferHandle, 0ull, VK_WHOLE_SIZE);
        setUpdateBuilder.writeSingle(*descSet, binding(0u), descType, &bufferDesc);
        setUpdateBuilder.update(ctx.vkd, ctx.device);
    }

    PipelineLayoutWrapper pipelineLayout(params->constructionType, ctx.vkd, ctx.device, *setLayout, pcRangePtr);

    const auto &binaries = context.getBinaryCollection();
    std::vector<ShaderWrapper> vertShaders;
    std::vector<ShaderWrapper> tescShaders;
    std::vector<ShaderWrapper> teseShaders;
    std::vector<ShaderWrapper> geomShaders;
    std::vector<ShaderWrapper> fragShaders;
    std::vector<ShaderWrapper> taskShaders;
    std::vector<ShaderWrapper> meshShaders;

    std::vector<Move<VkShaderModule>> compModules;
    std::vector<Move<VkShaderEXT>> compShaders;
    std::vector<Move<VkPipeline>> compPipelines;

    // Make all shaders that we generated in initPrograms(), filling the vectors above.
    struct PrefixAndVec
    {
        const char *prefix;
        std::vector<ShaderWrapper> *vec;
    };

    const std::map<VkShaderStageFlagBits, PrefixAndVec> shaderVecMap{
        std::make_pair(VK_SHADER_STAGE_VERTEX_BIT, PrefixAndVec{"vert", &vertShaders}),
        std::make_pair(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, PrefixAndVec{"tesc", &tescShaders}),
        std::make_pair(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, PrefixAndVec{"tese", &teseShaders}),
        std::make_pair(VK_SHADER_STAGE_GEOMETRY_BIT, PrefixAndVec{"geom", &geomShaders}),
        std::make_pair(VK_SHADER_STAGE_FRAGMENT_BIT, PrefixAndVec{"frag", &fragShaders}),
        std::make_pair(VK_SHADER_STAGE_TASK_BIT_EXT, PrefixAndVec{"task", &taskShaders}),
        std::make_pair(VK_SHADER_STAGE_MESH_BIT_EXT, PrefixAndVec{"mesh", &meshShaders}),
    };

    uint32_t maxSequenceCount     = 2u; // We will have at least 2 sequences.
    uint32_t maxGraphicsPipelines = 0u;
    for (const auto &stageAndVec : shaderVecMap)
    {
        const auto stageShaderCount = params->getShaderVariantCount(stageAndVec.first);
        stageAndVec.second.vec->reserve(stageShaderCount);
        maxGraphicsPipelines = std::max(maxGraphicsPipelines, stageShaderCount);

        for (uint32_t i = 0u; i < stageShaderCount; ++i)
        {
            const auto shaderName = stageAndVec.second.prefix + std::to_string(i); // e.g. "vert0"
            stageAndVec.second.vec->emplace_back(ctx.vkd, ctx.device, binaries.get(shaderName));
        }
    }
    maxSequenceCount = std::max(maxSequenceCount, maxGraphicsPipelines);

    // We need to store some information about each shader stage to easily access it when builing pipelines, DGC buffer
    // contents, etc. Some parts of this are a bit tricky.
    struct StageInfo
    {
        // A sequential index (id) for each shader stage that is actually used. If not used, this is -1. E.g. if we use
        // vertex, geom and frag, vertex gets 0, geom gets 1 and frag gets 2. tessellation control, evaluation, mesh and
        // task get -1. This index is useful as an offset to mix shader stages together in each sequence.
        int stageId;

        // Returns true if the stage has an assigned positive stage id.
        bool hasStage() const
        {
            return (stageId >= 0);
        }

        // The vector of shaders for this stage.
        const std::vector<ShaderWrapper> *shaderVec;

        // Based on stageId, give me the index (in shaderVec) that will be used for sequence i. -1 if not used.
        int getShaderIdxForSeq(uint32_t seqIndex) const
        {
            // By adding the stage id to the i counter we mix stages together if needed. For example, if the test has 2
            // vertex shaders and 2 fragment shaders, this logic will mix vert0 with frag1 in the first pipeline, and
            // vert1 with frag0 in the second pipeline.
            if (stageId < 0)
                return stageId;
            return static_cast<int>((static_cast<uint32_t>(stageId) + seqIndex) % shaderVec->size());
        }

        // Similar to the previous method, but giving the shader directly. If getSequenceIndex would return -1, get a
        // default empty shader.
        const ShaderWrapper &getSequenceShader(uint32_t seqIndex) const
        {
            const static ShaderWrapper defaultShader;
            const auto index = getShaderIdxForSeq(seqIndex);
            if (index < 0)
                return defaultShader;
            return shaderVec->at(static_cast<size_t>(index));
        }
    };

    // Assign a sequential index (id) to each graphics shader stage in use. This will be useful later for several things.
    // Similarly, index the different shader vectors by their flag bits.
    std::map<VkShaderStageFlagBits, StageInfo> stageInfos;
    {
        int nextId            = 0;
        const int vertStageId = (vertShaders.empty() ? -1 : nextId++);
        const int tescStageId = (tescShaders.empty() ? -1 : nextId++);
        const int teseStageId = (teseShaders.empty() ? -1 : nextId++);
        const int geomStageId = (geomShaders.empty() ? -1 : nextId++);
        const int fragStageId = (fragShaders.empty() ? -1 : nextId++);
        const int taskStageId = (taskShaders.empty() ? -1 : nextId++);
        const int meshStageId = (meshShaders.empty() ? -1 : nextId++);

        stageInfos[VK_SHADER_STAGE_VERTEX_BIT]                  = StageInfo{vertStageId, &vertShaders};
        stageInfos[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = StageInfo{tescStageId, &tescShaders};
        stageInfos[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = StageInfo{teseStageId, &teseShaders};
        stageInfos[VK_SHADER_STAGE_GEOMETRY_BIT]                = StageInfo{geomStageId, &geomShaders};
        stageInfos[VK_SHADER_STAGE_FRAGMENT_BIT]                = StageInfo{fragStageId, &fragShaders};
        stageInfos[VK_SHADER_STAGE_TASK_BIT_EXT]                = StageInfo{taskStageId, &taskShaders};
        stageInfos[VK_SHADER_STAGE_MESH_BIT_EXT]                = StageInfo{meshStageId, &meshShaders};
    }

    std::unique_ptr<RenderPassWrapper> renderPass;
    std::vector<GraphicsPipelineWrapperPtr> graphicsPipelines;
    graphicsPipelines.reserve(maxGraphicsPipelines);

    // DGC part.
    ExecutionSetManagerPtr iesManager;
    const auto shaderCreateFlags =
        static_cast<VkShaderCreateFlagsEXT>(params->useExecutionSet ? VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT : 0);
    const auto pipelineCreateFlags = static_cast<VkPipelineCreateFlags2>(
        params->useExecutionSet ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT : 0);

    const std::vector<VkShaderStageFlagBits> allStageBits{
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_TASK_BIT_EXT,
        VK_SHADER_STAGE_MESH_BIT_EXT,
    };

    // Shader indices used for each DGC sequence when using shader objects.
    std::vector<std::vector<uint32_t>> iesShaderObjIndices;

    if (isComp)
    {
        // We don't have wrappers for compute shaders, so we create them separately.
        maxSequenceCount = std::max(maxSequenceCount, compShaderCount);

        if (useESO)
            compShaders.reserve(compShaderCount);
        else
        {
            compModules.reserve(compShaderCount);
            compPipelines.reserve(compShaderCount);
        }

        for (uint32_t i = 0u; i < compShaderCount; ++i)
        {
            const auto shaderName = "comp" + std::to_string(i);
            auto &binary          = binaries.get(shaderName);
            binary.setUsed();

            if (useESO)
            {
                DE_ASSERT(binary.getFormat() == PROGRAM_FORMAT_SPIRV);

                const VkShaderCreateInfoEXT createInfo{
                    VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                    nullptr,
                    shaderCreateFlags,
                    VK_SHADER_STAGE_COMPUTE_BIT,
                    0u,
                    VK_SHADER_CODE_TYPE_SPIRV_EXT,
                    binary.getSize(),
                    binary.getBinary(),
                    "main",
                    1u,
                    &setLayout.get(),
                    1u,
                    pcRangePtr,
                    nullptr,
                };
                compShaders.emplace_back(createShader(ctx.vkd, ctx.device, createInfo));
            }
            else
            {
                compModules.emplace_back(createShaderModule(ctx.vkd, ctx.device, binary));
                const VkPipelineCreateFlags2CreateInfo pipelineFlagsInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
                    nullptr,
                    pipelineCreateFlags,
                };
                compPipelines.emplace_back(makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, 0u,
                                                               &pipelineFlagsInfo, compModules.back().get(), 0u));
            }
        }

        if (params->useExecutionSet)
        {
            if (useESO)
            {
                const std::vector<vk::VkPushConstantRange> pcRanges(1u, pcRange);
                const std::vector<VkDescriptorSetLayout> setLayouts(1u, *setLayout);
                const std::vector<IESStageInfo> iesStages{
                    IESStageInfo(compShaders.front().get(), setLayouts),
                };
                iesManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, iesStages, pcRanges, compShaderCount);
                for (uint32_t i = 0u; i < compShaderCount; ++i)
                    iesManager->addShader(i, compShaders.at(i).get());
            }
            else
            {
                iesManager =
                    makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, compPipelines.front().get(), compShaderCount);
                for (uint32_t i = 0u; i < compShaderCount; ++i)
                    iesManager->addPipeline(i, compPipelines.at(i).get());
            }

            iesManager->update();
        }
    }
    else
    {
        // Prepare graphics pipeline wrappers, mixing existing shaders together.

        const std::vector<VkViewport> viewports(1u, makeViewport(extent));
        const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

        renderPass.reset(new RenderPassWrapper(params->constructionType, ctx.vkd, ctx.device, colorFormat));
        renderPass->createFramebuffer(ctx.vkd, ctx.device, colorBuffer->getImage(), colorBuffer->getImageView(),
                                      extentVk.width, extentVk.height);

        for (uint32_t i = 0u; i < maxGraphicsPipelines; ++i)
        {
            graphicsPipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                                       context.getDeviceExtensions(),
                                                                       params->constructionType));
            auto &pipeline = *graphicsPipelines.back();

            const auto topology =
                (useTriangleStrip ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP :
                                    (isTess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));

            const auto &vertShader = stageInfos.at(VK_SHADER_STAGE_VERTEX_BIT).getSequenceShader(i);
            const auto &tescShader = stageInfos.at(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT).getSequenceShader(i);
            const auto &teseShader = stageInfos.at(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT).getSequenceShader(i);
            const auto &geomShader = stageInfos.at(VK_SHADER_STAGE_GEOMETRY_BIT).getSequenceShader(i);
            const auto &fragShader = stageInfos.at(VK_SHADER_STAGE_FRAGMENT_BIT).getSequenceShader(i);
            const auto &taskShader = stageInfos.at(VK_SHADER_STAGE_TASK_BIT_EXT).getSequenceShader(i);
            const auto &meshShader = stageInfos.at(VK_SHADER_STAGE_MESH_BIT_EXT).getSequenceShader(i);

            pipeline.setDefaultTopology(topology);
            pipeline.setDefaultPatchControlPoints(3u);
            pipeline.setDefaultRasterizationState();
            pipeline.setDefaultDepthStencilState();
            pipeline.setDefaultMultisampleState();
            pipeline.setDefaultColorBlendState();
            pipeline.setPipelineCreateFlags2(pipelineCreateFlags);
            pipeline.setShaderCreateFlags(shaderCreateFlags);
            if (!isMesh)
                pipeline.setupVertexInputState();
            if (isMesh)
                pipeline.setupPreRasterizationMeshShaderState(viewports, scissors, pipelineLayout, renderPass->get(),
                                                              0u, taskShader, meshShader);
            else
                pipeline.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass->get(), 0u,
                                                          vertShader, nullptr, tescShader, teseShader, geomShader);
            pipeline.setupFragmentShaderState(pipelineLayout, renderPass->get(), 0u, fragShader);
            pipeline.setupFragmentOutputState(renderPass->get(), 0u);
            pipeline.buildPipeline();
        }

        if (params->useExecutionSet)
        {
            if (useESO)
            {
                std::vector<IESStageInfo> iesStageInfos;

                std::vector<VkDescriptorSetLayout> setLayouts;
                if (*setLayout != VK_NULL_HANDLE)
                    setLayouts.push_back(*setLayout);
                std::vector<vk::VkPushConstantRange> pcRanges;
                if (pcRangePtr)
                    pcRanges.push_back(*pcRangePtr);

                for (const auto stageBit : allStageBits)
                {
                    if (!stageInfos.at(stageBit).shaderVec->empty())
                        iesStageInfos.emplace_back(graphicsPipelines.front()->getShader(stageBit), setLayouts);
                }

                const auto maxShaderCount = maxGraphicsPipelines * 5u; // At most 5 stages per pipeline.
                iesManager =
                    makeExecutionSetManagerShader(ctx.vkd, ctx.device, iesStageInfos, pcRanges, maxShaderCount);

                uint32_t nextSlot = 0u;
                for (uint32_t i = 0u; i < maxGraphicsPipelines; ++i)
                {
                    iesShaderObjIndices.emplace_back();
                    auto &seqIndices = iesShaderObjIndices.back();
                    auto &pipeline   = graphicsPipelines.at(i);

                    for (const auto stageBit : allStageBits) // Note allStageBits has the stages in the proper order.
                    {
                        if (stageInfos.at(stageBit).hasStage())
                        {
                            const auto thisSlot = nextSlot++;
                            iesManager->addShader(thisSlot, pipeline->getShader(stageBit));
                            seqIndices.push_back(thisSlot);
                        }
                    }
                }
            }
            else
            {
                iesManager = makeExecutionSetManagerPipeline(
                    ctx.vkd, ctx.device, graphicsPipelines.front()->getPipeline(), maxGraphicsPipelines);
                for (uint32_t i = 0u; i < maxGraphicsPipelines; ++i)
                    iesManager->addPipeline(i, graphicsPipelines.at(i)->getPipeline());
            }

            iesManager->update();
        }
    }

    VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags = 0u;
    if (params->preprocess)
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, *pipelineLayout);

    if (params->useExecutionSet)
    {
        const auto iesSetType = (useESO ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                          VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
        cmdsLayoutBuilder.addExecutionSetToken(0u, iesSetType, shaderStages);
    }
    if (isComp || isMesh)
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    if (isComp)
        cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    else if (isMesh)
        cmdsLayoutBuilder.addDrawMeshTasksToken(cmdsLayoutBuilder.getStreamRange());
    else
        cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<uint32_t> dgcData;
    dgcData.reserve((maxSequenceCount * cmdsLayoutBuilder.getStreamStride()) / DE_SIZEOF32(uint32_t));

    const std::vector<uint32_t> meshWorkGroupCounts{3u, 5u}; // We make them not even.
    DE_ASSERT(meshWorkGroupCounts.size() >= maxGraphicsPipelines);

    DE_ASSERT(de::sizeU32(vertices) % maxSequenceCount == 0u);
    const auto vertexCountPerSeq = de::sizeU32(vertices) / maxSequenceCount;

    DE_ASSERT(compWorkGroupCount % maxSequenceCount == 0u);
    const auto compDispatchX = (compWorkGroupCount / maxSequenceCount);

    for (uint32_t i = 0u; i < maxSequenceCount; ++i)
    {
        if (params->useExecutionSet)
        {
            if (isComp || !useESO)
                pushBackElement(dgcData, i);
            else
            {
                const auto shaderIndices = iesShaderObjIndices.at(i);
                for (const auto idx : shaderIndices)
                    pushBackElement(dgcData, idx);
            }
        }
        if (isComp)
        {
            const uint32_t wgOffset = i * compDispatchX;
            pushBackElement(dgcData, wgOffset);

            // Each dispatch fills a chunk of the output buffer.
            const VkDispatchIndirectCommand dispatch{
                compDispatchX,
                1u,
                1u,
            };
            pushBackElement(dgcData, dispatch);
        }
        else
        {
            if (isMesh)
            {
                const auto prevWGs = std::accumulate(meshWorkGroupCounts.begin(), meshWorkGroupCounts.begin() + i, 0u);
                pushBackElement(dgcData, prevWGs);

                const VkDrawMeshTasksIndirectCommandEXT drawCmd{
                    meshWorkGroupCounts.at(i),
                    1u,
                    1u,
                };
                pushBackElement(dgcData, drawCmd);
            }
            else
            {
                const VkDrawIndirectCommand drawCmd{
                    vertexCountPerSeq,
                    1u,
                    i * vertexCountPerSeq,
                    0u,
                };
                pushBackElement(dgcData, drawCmd);
            }
        }
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    {
        auto &alloc = dgcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    const auto iesHandle = (iesManager ? iesManager->get() : VK_NULL_HANDLE);
    const auto preprocessPipeline =
        ((iesHandle != VK_NULL_HANDLE || useESO) ?
             VK_NULL_HANDLE :
             (isComp ? compPipelines.front().get() : graphicsPipelines.front()->getPipeline()));
    std::vector<VkShaderEXT> preprocessShadersVec;
    if (useESO && iesHandle == VK_NULL_HANDLE)
    {
        if (isComp)
        {
            for (const auto &compShader : compShaders)
                preprocessShadersVec.push_back(*compShader);
        }
        else
        {
            for (const auto stageBit : allStageBits)
            {
                if (stageInfos.at(stageBit).hasStage())
                    preprocessShadersVec.push_back(graphicsPipelines.front()->getShader(stageBit));
            }
        }
    }
    const auto preprocessShaders = (preprocessShadersVec.empty() ? nullptr : &preprocessShadersVec);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, maxSequenceCount,
                                         0u, preprocessPipeline, preprocessShaders);

    const DGCGenCmdsInfo genCmdsInfo(shaderStages, iesHandle, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                     dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(),
                                     preprocessBuffer.getSize(), maxSequenceCount, 0ull, 0u, preprocessPipeline,
                                     preprocessShaders);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;
    Move<VkCommandBuffer> preprocessCmd;
    if (params->preprocess)
        preprocessCmd = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const VkQueryPoolCreateInfo qpCreateInfo{
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr, 0u, VK_QUERY_TYPE_PIPELINE_STATISTICS, 1u, params->stats,
    };
    const auto queryPool = createQueryPool(ctx.vkd, ctx.device, &qpCreateInfo);
    const auto resetCmd  = allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    beginCommandBuffer(ctx.vkd, *resetCmd);
    ctx.vkd.cmdResetQueryPool(*resetCmd, *queryPool, 0u, 1u);
    endCommandBuffer(ctx.vkd, *resetCmd);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, *resetCmd);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    if (isComp)
    {
        const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descSet.get(), 0u, nullptr);
        if (useESO)
        {
            const auto shaderStage = VK_SHADER_STAGE_COMPUTE_BIT;
            ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &shaderStage, &compShaders.front().get());
        }
        else
        {
            ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, compPipelines.front().get());
        }
        ctx.vkd.cmdBeginQuery(cmdBuffer, *queryPool, 0u, 0u);
        if (params->preprocess)
        {
            beginCommandBuffer(ctx.vkd, *preprocessCmd);
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmd, &genCmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmd);
            endCommandBuffer(ctx.vkd, *preprocessCmd);
        }
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(params->preprocess), &genCmdsInfo.get());
        ctx.vkd.cmdEndQuery(cmdBuffer, *queryPool, 0u);
        const auto hostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &hostBarrier);
    }
    else
    {
        const auto renderArea = makeRect2D(extent);
        const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;

        renderPass->begin(ctx.vkd, cmdBuffer, renderArea, clearColor);
        if (isMesh)
            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descSet.get(), 0u, nullptr);
        else
        {
            const VkDeviceSize vertBufferOffset = 0ull;
            ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertBuffer->get(), &vertBufferOffset);
        }
        graphicsPipelines.front()->bind(cmdBuffer);
        ctx.vkd.cmdBeginQuery(cmdBuffer, *queryPool, 0u, 0u);
        if (params->preprocess)
        {
            beginCommandBuffer(ctx.vkd, *preprocessCmd);
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(*preprocessCmd, &genCmdsInfo.get(), cmdBuffer);
            preprocessToExecuteBarrierExt(ctx.vkd, *preprocessCmd);
            endCommandBuffer(ctx.vkd, *preprocessCmd);
        }
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, makeVkBool(params->preprocess), &genCmdsInfo.get());
        ctx.vkd.cmdEndQuery(cmdBuffer, *queryPool, 0u);
        renderPass->end(ctx.vkd, cmdBuffer);
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer->getImage(), colorBuffer->getBuffer(), extent.swizzle(0, 1));
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitAndWaitWithPreprocess(ctx.vkd, ctx.device, ctx.queue, cmdBuffer, *preprocessCmd);

    auto &log = context.getTestContext().getLog();
    bool fail = false;

    if (isComp)
    {
        // Verify output buffer.
        std::vector<uint32_t> result(compBufferItemCount, 0u);
        auto &alloc = compBuffer->getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        memcpy(de::dataOrNull(result), alloc.getHostPtr(), de::dataSize(result));

        std::vector<uint32_t> reference(compBufferItemCount, 0u);

        for (uint32_t i = 0u; i < maxSequenceCount; ++i)
        {
            const auto shaderIdx = std::min(i, compShaderCount - 1u);
            for (uint32_t j = 0u; j < compDispatchX; ++j)
                for (uint32_t k = 0u; k < compWorkGroupSize; ++k)
                {
                    const auto wgIdx  = compDispatchX * i + j;
                    const auto idx    = wgIdx * compWorkGroupSize + k;
                    reference.at(idx) = (shaderIdx + 1u);
                }
        }

        for (uint32_t i = 0u; i < compBufferItemCount; ++i)
        {
            const auto &res = result.at(i);
            const auto &ref = reference.at(i);

            if (res != ref)
            {
                fail = true;
                std::ostringstream msg;
                msg << "Unexpected value in buffer item " << i << ": expected " << ref << " but found " << res;
                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            }
        }
    }
    else
    {
        // Verify color buffer.
        auto &bufferAlloc = colorBuffer->getBufferAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

        const auto tcuFormat = mapVkFormat(colorFormat);
        tcu::ConstPixelBufferAccess result(tcuFormat, extent, bufferAlloc.getHostPtr());

        tcu::TextureLevel refLevel(tcuFormat, extent.x(), extent.y(), extent.z());
        tcu::PixelBufferAccess reference = refLevel.getAccess();

        DE_ASSERT(pixelCount % maxSequenceCount == 0u);
        const auto pixelsPerSeq = static_cast<int>(pixelCount / maxSequenceCount);

        const auto getMeshSeqIndex = [&](int row)
        {
            const auto rowU   = static_cast<uint32_t>(row);
            uint32_t prevRows = 0u;
            uint32_t seqIndex = 0u;
            for (uint32_t i = 0u; i < de::sizeU32(meshWorkGroupCounts); ++i)
            {
                const auto &rowCount = meshWorkGroupCounts.at(i);
                if (prevRows + rowCount > rowU)
                    return seqIndex;
                ++seqIndex;
                prevRows += rowCount;
            }

            DE_ASSERT(false);
            return std::numeric_limits<uint32_t>::max();
        };
        for (int y = 0; y < extent.y(); ++y)
            for (int x = 0; x < extent.x(); ++x)
            {
                const auto pixelIndex   = y * extent.x() + x;
                const auto seqIndex     = (isMesh ? getMeshSeqIndex(y) : pixelIndex / pixelsPerSeq);
                tcu::Vec4 expectedColor = clearColor;

                // Mimic the different color transformations that the shader stages would do.
                if (isMesh)
                {
                    // Base value for R changes in the task shader.
                    const auto taskId = stageInfos.at(VK_SHADER_STAGE_TASK_BIT_EXT).getShaderIdxForSeq(seqIndex);
                    expectedColor     = tcu::Vec4(static_cast<float>(taskId), 0.0f, 1.0f, 1.0f);

                    const auto meshId = stageInfos.at(VK_SHADER_STAGE_MESH_BIT_EXT).getShaderIdxForSeq(seqIndex);
                    if (meshId > 0)
                        expectedColor.y() = 1.0f;
                }
                else
                {
                    // Base value for R changes in the vertex shader.
                    const auto vertId = stageInfos.at(VK_SHADER_STAGE_VERTEX_BIT).getShaderIdxForSeq(seqIndex);
                    expectedColor     = tcu::Vec4(static_cast<float>(vertId), 0.0f, 1.0f, 1.0f);

                    // Maybe force green.
                    const auto tescId =
                        stageInfos.at(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT).getShaderIdxForSeq(seqIndex);
                    if (tescId > 0)
                        expectedColor.y() = 1.0f;

                    // Maybe invert RGB.
                    const auto teseId =
                        stageInfos.at(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT).getShaderIdxForSeq(seqIndex);
                    if (teseId > 0)
                        expectedColor = tcu::Vec4(1.0f, 1.0f, 1.0f, 2.0f) - expectedColor;

                    // Maybe swizzle.
                    const auto geomId = stageInfos.at(VK_SHADER_STAGE_GEOMETRY_BIT).getShaderIdxForSeq(seqIndex);
                    if (geomId > 0)
                        expectedColor = expectedColor.swizzle(1, 2, 0, 3);
                }

                // Maybe swizzle again.
                const auto fragId = stageInfos.at(VK_SHADER_STAGE_FRAGMENT_BIT).getShaderIdxForSeq(seqIndex);
                if (fragId > 0)
                    expectedColor = expectedColor.swizzle(1, 2, 0, 3);

                reference.setPixel(expectedColor, x, y);
            }

        const tcu::Vec4 threshold(0.0f);
        if (!tcu::floatThresholdCompare(log, "Color", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }

    // Verify queries.
    {
        const std::vector<VkQueryPipelineStatisticFlagBits> allStats{
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT,
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT,
            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
            VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT,
            VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT,
            VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT,
        };

        const std::bitset<32> statBits(params->stats);
        std::vector<uint32_t> queryResults(statBits.count(), 0u);

        const auto stride      = static_cast<VkDeviceSize>(sizeof(uint32_t));
        const auto resultFlags = static_cast<VkQueryResultFlags>(VK_QUERY_RESULT_WAIT_BIT);

        ctx.vkd.getQueryPoolResults(ctx.device, *queryPool, 0u, 1u, de::dataSize(queryResults),
                                    de::dataOrNull(queryResults), stride, resultFlags);
        uint32_t nextResultIdx = 0u;

        for (const auto stat : allStats)
        {
            if (!params->hasAnyStats(static_cast<VkQueryPipelineStatisticFlags>(stat)))
                continue;

            const auto result = queryResults.at(nextResultIdx++);

            if (stat == VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT)
            {
                const auto expected = de::sizeU32(vertices);
                if (result != expected)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected " << expected << " input assembly vertices but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT)
            {
                const auto expected = (useTriangleStrip ? 2u : pixelCount);
                if (result != expected)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected " << expected << " input assembly primitives but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT)
            {
                const auto minimum = de::sizeU32(vertices);
                if (result < minimum)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected at least " << minimum << " vertex shader invocations but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT ||
                     stat == VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT)
            {
                const auto primitives = (useTriangleStrip ? 2u : pixelCount);
                const bool needsExact = (stat == VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT);
                const bool bad        = (needsExact ? (result != primitives) : (result < primitives));
                if (bad)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected " << (needsExact ? "" : "at least") << " " << primitives << " geometry shader "
                        << (needsExact ? "primitives" : "invocations") << " but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT)
            {
                // Implementations are allowed to reuse fragment shader invocations to shade different fragments under
                // some circumstances:
                //
                // - The frag shader statically computes the same value for different framebuffer locations, and
                // - It does not write to any storage resources.
                //
                // The spec does not mention a minimum number of invocations, but in practice we're tying this to
                // enabling fragment shading rate support automatically. We'll suppose implementations not supporting
                // fragment shading rate will not do this, and those supporting it will not run less invocations than
                // the whole framebuffer divided into areas of maxFramentSize pixels. If this proves problematic, we can
                // relax the check later.
                const auto maxFragmentSize   = (context.isDeviceFunctionalitySupported("VK_KHR_fragment_shading_rate") ?
                                                    context.getFragmentShadingRateProperties().maxFragmentSize :
                                                    makeExtent2D(1u, 1u));
                const auto maxFragmentWidth  = std::max(maxFragmentSize.width, 1u);  // In case the driver reports zero.
                const auto maxFragmentHeight = std::max(maxFragmentSize.height, 1u); // Ditto.
                const auto minCols           = extentVk.width / maxFragmentWidth;
                const auto minRows           = extentVk.height / maxFragmentHeight;
                const auto minimum           = std::max(minCols * minRows * extentVk.depth, 1u); // At least one.

                if (result < minimum)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected at least " << minimum << " fragment shader invocations but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT)
            {
                const auto expected = pixelCount;
                if (result != expected)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected " << expected << " tessellation control shader patches but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT)
            {
                const auto minimum = de::sizeU32(vertices);
                if (result < minimum)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected at least " << minimum << " tessellation evaluation shader invocations but found "
                        << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT)
            {
                const auto minimum = compWorkGroupCount * compWorkGroupSize;
                if (result < minimum)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected at least " << minimum << " compute shader invocations but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT)
            {
                const auto taskWorkGroupSize = 32u;
                const auto expected          = extentU.y() * taskWorkGroupSize;
                if (result != expected)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected " << expected << " task shader invocations but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else if (stat == VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT)
            {
                const auto meshWorkGroupSize = 64u;
                const auto expected          = pixelCount * meshWorkGroupSize;
                if (result != expected)
                {
                    fail = true;
                    std::ostringstream msg;
                    msg << "Expected " << expected << " mesh shader invocations but found " << result;
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                }
            }
            else
                DE_ASSERT(false);
        }
    }

    if (fail)
        TCU_FAIL("Unexpected results; check log for details -- ");

    return tcu::TestStatus::pass("Pass");
}

struct StatCase
{
    VkQueryPipelineStatisticFlags stats;
    std::string name;
};

struct ConstructionCase
{
    PipelineConstructionType constructionType;
    std::string name;
};

void populateStatQueryTestGroup(tcu::TestCaseGroup *mainGroup)
{
    const std::vector<StatCase> statCases{
        // clang-format off
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT), "input_vert"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT), "input_prim"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT), "vert_inv"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT), "geom_inv"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT), "geom_prim"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT), "frag_inv"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT), "tesc_patch"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT), "tese_inv"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT), "comp_inv"},
        StatCase{static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT | VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT), "task_mesh_inv"},
        // clang-format on
    };

    const std::vector<ConstructionCase> constructionCases{
        ConstructionCase{PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, "monolithic"},
        ConstructionCase{PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, "fast_lib"},
        ConstructionCase{PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, "shader_obj"},
    };

    for (const auto &statCase : statCases)
        for (const auto &constructionCase : constructionCases)
            for (const bool useExecutionSet : {false, true})
                for (const bool preprocess : {false, true})
                {
                    TestParamsPtr params(new TestParams{
                        statCase.stats,
                        constructionCase.constructionType,
                        useExecutionSet,
                        preprocess,
                    });
                    const auto testName = statCase.name + "_" + constructionCase.name +
                                          (useExecutionSet ? "_ies" : "") + (preprocess ? "_preprocess" : "");
                    addFunctionCaseWithPrograms(mainGroup, testName, checkSupport, initPrograms, iterate, params);
                }
}

} // anonymous namespace

tcu::TestCaseGroup *createDGCStatQueryTestsExt(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "stat_query", populateStatQueryTestGroup);
}

} // namespace DGC
} // namespace vkt
