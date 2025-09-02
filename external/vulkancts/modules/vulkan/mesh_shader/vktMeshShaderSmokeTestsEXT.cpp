/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Mesh Shader Smoke Tests for VK_EXT_mesh_shader
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderSmokeTestsEXT.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "deRandom.hpp"

#include <utility>
#include <vector>
#include <string>
#include <sstream>
#include <set>
#include <memory>
#include <map>

namespace vkt
{
namespace MeshShader
{

namespace
{

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

using namespace vk;

std::string commonMeshFragShader()
{
    std::string frag = "#version 450\n"
                       "#extension GL_EXT_mesh_shader : enable\n"
                       "\n"
                       "layout (location=0) in perprimitiveEXT vec4 triangleColor;\n"
                       "layout (location=0) out vec4 outColor;\n"
                       "\n"
                       "void main ()\n"
                       "{\n"
                       "    outColor = triangleColor;\n"
                       "}\n";
    return frag;
}

tcu::Vec4 getClearColor()
{
    return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

void makeMeshGraphicsPipeline(
    GraphicsPipelineWrapper &maker, const PipelineLayoutWrapper &pipelineLayout, const ShaderWrapper taskShader,
    const ShaderWrapper meshShader, const ShaderWrapper fragShader, const VkRenderPass renderPass,
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors, const uint32_t subpass = 0u,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo            = nullptr,
    VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateStateCreateInfo = nullptr)
{
#ifndef CTS_USES_VULKANSC
    maker.setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setupPreRasterizationMeshShaderState(viewports, scissors, pipelineLayout, renderPass, subpass, taskShader,
                                              meshShader, nullptr, nullptr, nullptr, fragmentShadingRateStateCreateInfo)
        .setupFragmentShaderState(pipelineLayout, renderPass, subpass, fragShader, depthStencilStateCreateInfo)
        .setupFragmentOutputState(renderPass, subpass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();
#else
    DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
}

struct MeshTriangleRendererParams
{
    PipelineConstructionType constructionType;
    std::vector<tcu::Vec4> vertexCoords;
    std::vector<uint32_t> vertexIndices;
    uint32_t taskCount;
    tcu::Vec4 expectedColor;
    bool rasterizationDisabled;

    MeshTriangleRendererParams(PipelineConstructionType constructionType_, std::vector<tcu::Vec4> vertexCoords_,
                               std::vector<uint32_t> vertexIndices_, uint32_t taskCount_,
                               const tcu::Vec4 &expectedColor_, bool rasterizationDisabled_ = false)
        : constructionType(constructionType_)
        , vertexCoords(std::move(vertexCoords_))
        , vertexIndices(std::move(vertexIndices_))
        , taskCount(taskCount_)
        , expectedColor(expectedColor_)
        , rasterizationDisabled(rasterizationDisabled_)
    {
    }

    MeshTriangleRendererParams(MeshTriangleRendererParams &&other)
        : MeshTriangleRendererParams(other.constructionType, std::move(other.vertexCoords),
                                     std::move(other.vertexIndices), other.taskCount, other.expectedColor,
                                     other.rasterizationDisabled)
    {
    }
};

class MeshOnlyTriangleCase : public vkt::TestCase
{
public:
    MeshOnlyTriangleCase(tcu::TestContext &testCtx, const std::string &name, PipelineConstructionType constructionType,
                         bool rasterizationDisabled = false)
        : vkt::TestCase(testCtx, name)
        , m_constructionType(constructionType)
        , m_rasterizationDisabled(rasterizationDisabled)
    {
    }
    virtual ~MeshOnlyTriangleCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const PipelineConstructionType m_constructionType;
    const bool m_rasterizationDisabled;
};

class MeshTaskTriangleCase : public vkt::TestCase
{
public:
    MeshTaskTriangleCase(tcu::TestContext &testCtx, const std::string &name, PipelineConstructionType constructionType)
        : vkt::TestCase(testCtx, name)
        , m_constructionType(constructionType)
    {
    }
    virtual ~MeshTaskTriangleCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const PipelineConstructionType m_constructionType;
};

// Note: not actually task-only. The task shader will not emit mesh shader work groups.
class TaskOnlyTriangleCase : public vkt::TestCase
{
public:
    TaskOnlyTriangleCase(tcu::TestContext &testCtx, const std::string &name, PipelineConstructionType constructionType)
        : vkt::TestCase(testCtx, name)
        , m_constructionType(constructionType)
    {
    }
    virtual ~TaskOnlyTriangleCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const PipelineConstructionType m_constructionType;
};

class MeshTriangleRenderer : public vkt::TestInstance
{
public:
    MeshTriangleRenderer(Context &context, MeshTriangleRendererParams params)
        : vkt::TestInstance(context)
        , m_params(std::move(params))
    {
    }
    virtual ~MeshTriangleRenderer(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    MeshTriangleRendererParams m_params;
};

void MeshOnlyTriangleCase::checkSupport(Context &context) const
{
    checkTaskMeshShaderSupportEXT(context, false, true);
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_constructionType);
}

void MeshTaskTriangleCase::checkSupport(Context &context) const
{
    checkTaskMeshShaderSupportEXT(context, true, true);
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_constructionType);
}

void TaskOnlyTriangleCase::checkSupport(Context &context) const
{
    checkTaskMeshShaderSupportEXT(context, true, true);
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_constructionType);
}

void MeshOnlyTriangleCase::initPrograms(SourceCollections &dst) const
{
    const auto buildOptions = getMinMeshEXTBuildOptions(dst.usedVulkanVersion);

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         // We will actually output a single triangle and most invocations will do no work.
         << "layout(local_size_x=8, local_size_y=4, local_size_z=4) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         // Unique vertex coordinates.
         << "layout (set=0, binding=0) uniform CoordsBuffer {\n"
         << "    vec4 coords[3];\n"
         << "} cb;\n"
         // Unique vertex indices.
         << "layout (set=0, binding=1, std430) readonly buffer IndexBuffer {\n"
         << "    uint indices[3];\n"
         << "} ib;\n"
         << "\n"
         // Triangle color.
         << "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    SetMeshOutputsEXT(3u, 1u);\n"
         << "    triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "\n"
         << "    const uint vertexIndex = gl_LocalInvocationIndex;\n"
         << "    if (vertexIndex < 3u)\n"
         << "    {\n"
         << "        const uint coordsIndex = ib.indices[vertexIndex];\n"
         << "        gl_MeshVerticesEXT[vertexIndex].gl_Position = cb.coords[coordsIndex];\n"
         << "    }\n"
         << "    if (vertexIndex == 0u)\n"
         << "    {\n"
         << "        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
         << "    }\n"
         << "}\n";
    dst.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

    dst.glslSources.add("frag") << glu::FragmentSource(commonMeshFragShader()) << buildOptions;
}

void MeshTaskTriangleCase::initPrograms(SourceCollections &dst) const
{
    const auto buildOptions = getMinMeshEXTBuildOptions(dst.usedVulkanVersion);

    std::string taskDataDecl = "struct TaskData {\n"
                               "    uint triangleIndex;\n"
                               "};\n"
                               "taskPayloadSharedEXT TaskData td;\n";

    std::ostringstream task;
    task
        // Each work group spawns 1 task each (2 in total) and each task will draw 1 triangle.
        << "#version 460\n"
        << "#extension GL_EXT_mesh_shader : enable\n"
        << "\n"
        << "layout(local_size_x=8, local_size_y=4, local_size_z=4) in;\n"
        << "\n"
        << taskDataDecl << "\n"
        << "void main ()\n"
        << "{\n"
        << "    if (gl_LocalInvocationIndex == 0u)\n"
        << "    {\n"
        << "        td.triangleIndex = gl_WorkGroupID.x;\n"
        << "    }\n"
        << "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
        << "}\n";
    ;
    dst.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;

    std::ostringstream mesh;
    mesh << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         // We will actually output a single triangle and most invocations will do no work.
         << "layout(local_size_x=8, local_size_y=4, local_size_z=4) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         // Unique vertex coordinates.
         << "layout (set=0, binding=0) uniform CoordsBuffer {\n"
         << "    vec4 coords[4];\n"
         << "} cb;\n"
         // Unique vertex indices.
         << "layout (set=0, binding=1, std430) readonly buffer IndexBuffer {\n"
         << "    uint indices[6];\n"
         << "} ib;\n"
         << "\n"
         // Triangle color.
         << "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
         << "\n"
         << taskDataDecl << "\n"
         << "void main ()\n"
         << "{\n"
         << "    SetMeshOutputsEXT(3u, 1u);\n"
         << "\n"
         // Each "active" invocation will copy one vertex.
         << "    const uint triangleVertex = gl_LocalInvocationIndex;\n"
         << "    const uint indexArrayPos  = td.triangleIndex * 3u + triangleVertex;\n"
         << "\n"
         << "    if (triangleVertex < 3u)\n"
         << "    {\n"
         << "        const uint coordsIndex = ib.indices[indexArrayPos];\n"
         // Copy vertex coordinates.
         << "        gl_MeshVerticesEXT[triangleVertex].gl_Position = cb.coords[coordsIndex];\n"
         // Index renumbering: final indices will always be 0, 1, 2.
         << "    }\n"
         << "    if (triangleVertex == 0u)\n"
         << "    {\n"
         << "        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
         << "        triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "    }\n"
         << "}\n";
    dst.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

    dst.glslSources.add("frag") << glu::FragmentSource(commonMeshFragShader()) << buildOptions;
}

void TaskOnlyTriangleCase::initPrograms(SourceCollections &dst) const
{
    const auto buildOptions = getMinMeshEXTBuildOptions(dst.usedVulkanVersion);

    // The task shader does not spawn any mesh shader invocations.
    std::ostringstream task;
    task << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout(local_size_x=1) in;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    EmitMeshTasksEXT(0u, 0u, 0u);\n"
         << "}\n";
    dst.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;

    // Same shader as the mesh only case, but it should not be launched.
    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         // We will actually output a single triangle and most invocations will do no work.
         << "layout(local_size_x=8, local_size_y=4, local_size_z=4) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         << "layout (set=0, binding=0) uniform CoordsBuffer {\n"
         << "    vec4 coords[3];\n"
         << "} cb;\n"
         << "layout (set=0, binding=1, std430) readonly buffer IndexBuffer {\n"
         << "    uint indices[3];\n"
         << "} ib;\n"
         << "\n"
         << "layout (location=0) out perprimitiveEXT vec4 triangleColor[];\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    SetMeshOutputsEXT(3u, 1u);\n"
         << "    triangleColor[0] = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "\n"
         << "    const uint vertexIndex = gl_LocalInvocationIndex;\n"
         << "    if (vertexIndex < 3u)\n"
         << "    {\n"
         << "        const uint coordsIndex = ib.indices[vertexIndex];\n"
         << "        gl_MeshVerticesEXT[vertexIndex].gl_Position = cb.coords[coordsIndex];\n"
         << "    }\n"
         << "    if (vertexIndex == 0u)\n"
         << "    {\n"
         << "        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
         << "    }\n"
         << "}\n";
    dst.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

    dst.glslSources.add("frag") << glu::FragmentSource(commonMeshFragShader()) << buildOptions;
}

TestInstance *MeshOnlyTriangleCase::createInstance(Context &context) const
{
    const std::vector<tcu::Vec4> vertexCoords = {
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 3.0f, 0.0f, 1.0f),
        tcu::Vec4(3.0f, -1.0f, 0.0f, 1.0f),
    };
    const std::vector<uint32_t> vertexIndices = {0u, 1u, 2u};
    const auto expectedColor = (m_rasterizationDisabled ? getClearColor() : tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
    MeshTriangleRendererParams params(m_constructionType, std::move(vertexCoords), std::move(vertexIndices), 1u,
                                      expectedColor, m_rasterizationDisabled);

    return new MeshTriangleRenderer(context, std::move(params));
}

TestInstance *MeshTaskTriangleCase::createInstance(Context &context) const
{
    const std::vector<tcu::Vec4> vertexCoords = {
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };
    const std::vector<uint32_t> vertexIndices = {2u, 0u, 1u, 1u, 3u, 2u};
    MeshTriangleRendererParams params(m_constructionType, std::move(vertexCoords), std::move(vertexIndices), 2u,
                                      tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    return new MeshTriangleRenderer(context, std::move(params));
}

TestInstance *TaskOnlyTriangleCase::createInstance(Context &context) const
{
    const std::vector<tcu::Vec4> vertexCoords = {
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 3.0f, 0.0f, 1.0f),
        tcu::Vec4(3.0f, -1.0f, 0.0f, 1.0f),
    };
    const std::vector<uint32_t> vertexIndices = {0u, 1u, 2u};
    // Note we expect the clear color.
    MeshTriangleRendererParams params(m_constructionType, std::move(vertexCoords), std::move(vertexIndices), 1u,
                                      getClearColor());

    return new MeshTriangleRenderer(context, std::move(params));
}

tcu::TestStatus MeshTriangleRenderer::iterate()
{
    const auto &vki           = m_context.getInstanceInterface();
    const auto &vkd           = m_context.getDeviceInterface();
    const auto physicalDevice = m_context.getPhysicalDevice();
    const auto device         = m_context.getDevice();
    auto &alloc               = m_context.getDefaultAllocator();
    const auto qIndex         = m_context.getUniversalQueueFamilyIndex();
    const auto queue          = m_context.getUniversalQueue();

    const auto vertexBufferStages = VK_SHADER_STAGE_MESH_BIT_EXT;
    const auto vertexBufferSize   = static_cast<VkDeviceSize>(de::dataSize(m_params.vertexCoords));
    const auto vertexBufferUsage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    const auto vertexBufferLoc    = DescriptorSetUpdateBuilder::Location::binding(0u);
    const auto vertexBufferType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    const auto indexBufferStages = VK_SHADER_STAGE_MESH_BIT_EXT;
    const auto indexBufferSize   = static_cast<VkDeviceSize>(de::dataSize(m_params.vertexIndices));
    const auto indexBufferUsage  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto indexBufferLoc    = DescriptorSetUpdateBuilder::Location::binding(1u);
    const auto indexBufferType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    // Vertex buffer.
    const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    BufferWithMemory vertexBuffer(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
    auto &vertexBufferAlloc   = vertexBuffer.getAllocation();
    void *vertexBufferDataPtr = vertexBufferAlloc.getHostPtr();

    deMemcpy(vertexBufferDataPtr, m_params.vertexCoords.data(), static_cast<size_t>(vertexBufferSize));
    flushAlloc(vkd, device, vertexBufferAlloc);

    // Index buffer.
    const auto indexBufferInfo = makeBufferCreateInfo(indexBufferSize, indexBufferUsage);
    BufferWithMemory indexBuffer(vkd, device, alloc, indexBufferInfo, MemoryRequirement::HostVisible);
    auto &indexBufferAlloc   = indexBuffer.getAllocation();
    void *indexBufferDataPtr = indexBufferAlloc.getHostPtr();

    deMemcpy(indexBufferDataPtr, m_params.vertexIndices.data(), static_cast<size_t>(indexBufferSize));
    flushAlloc(vkd, device, indexBufferAlloc);

    // Color buffer.
    const auto colorBufferFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorBufferExtent = makeExtent3D(8u, 8u, 1u);
    const auto colorBufferUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        colorBufferFormat,                   // VkFormat format;
        colorBufferExtent,                   // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        colorBufferUsage,                    // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory colorBuffer(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);

    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorBufferView =
        makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, colorBufferFormat, colorSRR);

    // Render pass.
    const auto renderPass = makeRenderPass(vkd, device, colorBufferFormat);

    // Framebuffer.
    const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorBufferView.get(),
                                             colorBufferExtent.width, colorBufferExtent.height);

    // Set layout.
    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(vertexBufferType, vertexBufferStages);
    layoutBuilder.addSingleBinding(indexBufferType, indexBufferStages);
    const auto setLayout = layoutBuilder.build(vkd, device);

    // Descriptor pool.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vertexBufferType);
    poolBuilder.addType(indexBufferType);
    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // Descriptor set.
    const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

    // Update descriptor set.
    DescriptorSetUpdateBuilder updateBuilder;
    const auto vertexBufferDescInfo = makeDescriptorBufferInfo(vertexBuffer.get(), 0ull, vertexBufferSize);
    const auto indexBufferDescInfo  = makeDescriptorBufferInfo(indexBuffer.get(), 0ull, indexBufferSize);
    updateBuilder.writeSingle(descriptorSet.get(), vertexBufferLoc, vertexBufferType, &vertexBufferDescInfo);
    updateBuilder.writeSingle(descriptorSet.get(), indexBufferLoc, indexBufferType, &indexBufferDescInfo);
    updateBuilder.update(vkd, device);

    // Pipeline layout.
    const PipelineLayoutWrapper pipelineLayout(m_params.constructionType, vkd, device, setLayout.get());

    // Shader modules.
    ShaderWrapper taskModule;
    ShaderWrapper fragModule;
    const auto &binaries = m_context.getBinaryCollection();

    if (binaries.contains("task"))
        taskModule = ShaderWrapper(vkd, device, binaries.get("task"), 0u);
    if (!m_params.rasterizationDisabled)
        fragModule = ShaderWrapper(vkd, device, binaries.get("frag"), 0u);
    const auto meshModule = ShaderWrapper(vkd, device, binaries.get("mesh"), 0u);

    // Graphics pipeline.
    std::vector<VkViewport> viewports(1u, makeViewport(colorBufferExtent));
    std::vector<VkRect2D> scissors(1u, makeRect2D(colorBufferExtent));
    GraphicsPipelineWrapper pipelineMaker(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(),
                                          m_params.constructionType);

    makeMeshGraphicsPipeline(pipelineMaker, pipelineLayout, taskModule, meshModule, fragModule, renderPass.get(),
                             viewports, scissors);
    const auto pipeline = pipelineMaker.getPipeline();

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    // Output buffer.
    const auto tcuFormat      = mapVkFormat(colorBufferFormat);
    const auto outBufferSize  = static_cast<VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) *
                                                         colorBufferExtent.width * colorBufferExtent.height);
    const auto outBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto outBufferInfo  = makeBufferCreateInfo(outBufferSize, outBufferUsage);
    BufferWithMemory outBuffer(vkd, device, alloc, outBufferInfo, MemoryRequirement::HostVisible);
    auto &outBufferAlloc = outBuffer.getAllocation();
    void *outBufferData  = outBufferAlloc.getHostPtr();

    // Draw triangle.
    beginCommandBuffer(vkd, cmdBuffer);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), getClearColor());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkd.cmdDrawMeshTasksEXT(cmdBuffer, m_params.taskCount, 1u, 1u);
    endRenderPass(vkd, cmdBuffer);

    // Copy color buffer to output buffer.
    const tcu::IVec3 imageDim(static_cast<int>(colorBufferExtent.width), static_cast<int>(colorBufferExtent.height),
                              static_cast<int>(colorBufferExtent.depth));
    const tcu::IVec2 imageSize(imageDim.x(), imageDim.y());

    copyImageToBuffer(vkd, cmdBuffer, colorBuffer.get(), outBuffer.get(), imageSize);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Invalidate alloc.
    invalidateAlloc(vkd, device, outBufferAlloc);
    tcu::ConstPixelBufferAccess outPixels(tcuFormat, imageDim, outBufferData);

    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f); // The color can be represented exactly.

    if (!tcu::floatThresholdCompare(log, "Result", "", m_params.expectedColor, outPixels, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Failed; check log for details");

    return tcu::TestStatus::pass("Pass");
}

VkExtent3D gradientImageExtent()
{
    return makeExtent3D(256u, 256u, 1u);
}

struct GradientParams
{
    tcu::Maybe<FragmentSize> fragmentSize;
    PipelineConstructionType constructionType;

    GradientParams(const tcu::Maybe<FragmentSize> &fragmentSize_, PipelineConstructionType constructionType_)
        : fragmentSize(fragmentSize_)
        , constructionType(constructionType_)
    {
    }
};

void checkMeshSupport(Context &context, GradientParams params)
{
    checkTaskMeshShaderSupportEXT(context, false, true);

    if (static_cast<bool>(params.fragmentSize))
    {
        const auto &features = context.getMeshShaderFeaturesEXT();
        if (!features.primitiveFragmentShadingRateMeshShader)
            TCU_THROW(NotSupportedError, "Primitive fragment shading rate not supported in mesh shaders");
    }

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          params.constructionType);
}

void initGradientPrograms(vk::SourceCollections &programCollection, GradientParams params)
{
    const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);
    const auto extent       = gradientImageExtent();

    std::ostringstream frag;
    frag << "#version 450\n"
         << "\n"
         << "layout (location=0) in  vec4 inColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    outColor = inColor;\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    std::string fragmentSizeStr;
    const auto useFragmentSize = static_cast<bool>(params.fragmentSize);

    if (useFragmentSize)
    {
        const auto &fragSize = params.fragmentSize.get();
        fragmentSizeStr      = getGLSLShadingRateMask(fragSize);

        const auto val = getSPVShadingRateValue(fragSize);
        DE_ASSERT(val != 0);
        DE_UNREF(val); // For release builds.
    }

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n";

    if (useFragmentSize)
        mesh << "#extension GL_EXT_fragment_shading_rate : enable\n";

    mesh << "\n"
         << "layout(local_size_x=4) in;\n"
         << "layout(triangles) out;\n"
         << "layout(max_vertices=256, max_primitives=256) out;\n"
         << "\n"
         << "layout (location=0) out vec4 outColor[];\n"
         << "\n";

    if (useFragmentSize)
    {
        mesh << "perprimitiveEXT out gl_MeshPerPrimitiveEXT {\n"
             << "   int gl_PrimitiveShadingRateEXT;\n"
             << "} gl_MeshPrimitivesEXT[];\n"
             << "\n";
    }

    mesh << "void main ()\n"
         << "{\n"
         << "    SetMeshOutputsEXT(4u, 2u);\n"
         << "\n"
         << "    const uint vertex    = gl_LocalInvocationIndex;\n"
         << "    const uint primitive = gl_LocalInvocationIndex;\n"
         << "\n"
         << "    const vec4 topLeft      = vec4(-1.0, -1.0, 0.0, 1.0);\n"
         << "    const vec4 botLeft      = vec4(-1.0,  1.0, 0.0, 1.0);\n"
         << "    const vec4 topRight     = vec4( 1.0, -1.0, 0.0, 1.0);\n"
         << "    const vec4 botRight     = vec4( 1.0,  1.0, 0.0, 1.0);\n"
         << "    const vec4 positions[4] = vec4[](topLeft, botLeft, topRight, botRight);\n"
         << "\n"
         // Green changes according to the width.
         // Blue changes according to the height.
         // Value 0 at the center of the first pixel and value 1 at the center of the last pixel.
         << "    const float width      = " << extent.width << ";\n"
         << "    const float height     = " << extent.height << ";\n"
         << "    const float halfWidth  = (1.0 / (width - 1.0)) / 2.0;\n"
         << "    const float halfHeight = (1.0 / (height - 1.0)) / 2.0;\n"
         << "    const float minGreen   = -halfWidth;\n"
         << "    const float maxGreen   = 1.0+halfWidth;\n"
         << "    const float minBlue    = -halfHeight;\n"
         << "    const float maxBlue    = 1.0+halfHeight;\n"
         << "    const vec4  colors[4]  = vec4[](\n"
         << "        vec4(0, minGreen, minBlue, 1.0),\n"
         << "        vec4(0, minGreen, maxBlue, 1.0),\n"
         << "        vec4(0, maxGreen, minBlue, 1.0),\n"
         << "        vec4(0, maxGreen, maxBlue, 1.0)\n"
         << "    );\n"
         << "\n"
         << "    const uvec3 indices[2] = uvec3[](\n"
         << "        uvec3(0, 1, 2),\n"
         << "        uvec3(1, 3, 2)\n"
         << "    );\n"
         << "    if (vertex < 4u)\n"
         << "    {\n"
         << "        gl_MeshVerticesEXT[vertex].gl_Position = positions[vertex];\n"
         << "        outColor[vertex] = colors[vertex];\n"
         << "    }\n"
         << "    if (primitive < 2u)\n"
         << "    {\n";

    if (useFragmentSize)
    {
        mesh << "        gl_MeshPrimitivesEXT[primitive].gl_PrimitiveShadingRateEXT = " << fragmentSizeStr << ";\n";
    }

    mesh << "        gl_PrimitiveTriangleIndicesEXT[primitive] = indices[primitive];\n"
         << "    }\n"
         << "}\n";
    ;
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

std::string coordColorFormat(int x, int y, const tcu::Vec4 &color)
{
    std::ostringstream msg;
    msg << "[" << x << ", " << y << "]=(" << color.x() << ", " << color.y() << ", " << color.z() << ", " << color.w()
        << ")";
    return msg.str();
}

tcu::TestStatus testFullscreenGradient(Context &context, GradientParams params)
{
    const auto &vki                = context.getInstanceInterface();
    const auto &vkd                = context.getDeviceInterface();
    const auto physicalDevice      = context.getPhysicalDevice();
    const auto device              = context.getDevice();
    auto &alloc                    = context.getDefaultAllocator();
    const auto qIndex              = context.getUniversalQueueFamilyIndex();
    const auto queue               = context.getUniversalQueue();
    const auto useFragmentSize     = static_cast<bool>(params.fragmentSize);
    const auto defaultFragmentSize = FragmentSize::SIZE_1X1;
    const auto rateSize = getShadingRateSize(useFragmentSize ? params.fragmentSize.get() : defaultFragmentSize);

    // Color buffer.
    const auto colorBufferFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorBufferExtent =
        makeExtent3D(256u, 256u, 1u); // Big enough for a detailed gradient, small enough to get unique colors.
    const auto colorBufferUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        colorBufferFormat,                   // VkFormat format;
        colorBufferExtent,                   // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        colorBufferUsage,                    // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory colorBuffer(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);

    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorBufferView =
        makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, colorBufferFormat, colorSRR);

    // Render pass.
    const auto renderPass = makeRenderPass(vkd, device, colorBufferFormat);

    // Framebuffer.
    const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), colorBufferView.get(),
                                             colorBufferExtent.width, colorBufferExtent.height);

    // Set layout.
    DescriptorSetLayoutBuilder layoutBuilder;
    const auto setLayout = layoutBuilder.build(vkd, device);

    // Pipeline layout.
    const PipelineLayoutWrapper pipelineLayout(params.constructionType, vkd, device, setLayout.get());

    // Shader modules.
    ShaderWrapper taskModule;
    const auto &binaries = context.getBinaryCollection();

    const auto meshModule = ShaderWrapper(vkd, device, binaries.get("mesh"), 0u);
    const auto fragModule = ShaderWrapper(vkd, device, binaries.get("frag"), 0u);

    using ShadingRateInfoPtr = de::MovePtr<VkPipelineFragmentShadingRateStateCreateInfoKHR>;
    ShadingRateInfoPtr pNext;
    if (useFragmentSize)
    {
        pNext  = ShadingRateInfoPtr(new VkPipelineFragmentShadingRateStateCreateInfoKHR);
        *pNext = initVulkanStructure();

        pNext->fragmentSize = getShadingRateSize(
            FragmentSize::SIZE_1X1); // 1x1 will not be used as the primitive rate in tests with fragment size.
        pNext->combinerOps[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR;
        pNext->combinerOps[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
    }

    // Graphics pipeline.
    std::vector<VkViewport> viewports(1u, makeViewport(colorBufferExtent));
    std::vector<VkRect2D> scissors(1u, makeRect2D(colorBufferExtent));
    GraphicsPipelineWrapper pipelineMaker(vki, vkd, physicalDevice, device, context.getDeviceExtensions(),
                                          params.constructionType);

    makeMeshGraphicsPipeline(pipelineMaker, pipelineLayout, taskModule, meshModule, fragModule, renderPass.get(),
                             viewports, scissors, 0u, nullptr, pNext.get());
    const auto pipeline = pipelineMaker.getPipeline();

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    // Output buffer.
    const auto tcuFormat      = mapVkFormat(colorBufferFormat);
    const auto outBufferSize  = static_cast<VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) *
                                                         colorBufferExtent.width * colorBufferExtent.height);
    const auto outBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto outBufferInfo  = makeBufferCreateInfo(outBufferSize, outBufferUsage);
    BufferWithMemory outBuffer(vkd, device, alloc, outBufferInfo, MemoryRequirement::HostVisible);
    auto &outBufferAlloc = outBuffer.getAllocation();
    void *outBufferData  = outBufferAlloc.getHostPtr();

    // Draw triangles.
    beginCommandBuffer(vkd, cmdBuffer);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), getClearColor());
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
    endRenderPass(vkd, cmdBuffer);

    // Copy color buffer to output buffer.
    const tcu::IVec3 imageDim(static_cast<int>(colorBufferExtent.width), static_cast<int>(colorBufferExtent.height),
                              static_cast<int>(colorBufferExtent.depth));
    const tcu::IVec2 imageSize(imageDim.x(), imageDim.y());

    copyImageToBuffer(vkd, cmdBuffer, colorBuffer.get(), outBuffer.get(), imageSize);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Invalidate alloc.
    invalidateAlloc(vkd, device, outBufferAlloc);
    tcu::ConstPixelBufferAccess outPixels(tcuFormat, imageDim, outBufferData);

    // Create reference image.
    tcu::TextureLevel refLevel(tcuFormat, imageDim.x(), imageDim.y(), imageDim.z());
    tcu::PixelBufferAccess refAccess(refLevel);
    for (int y = 0; y < imageDim.y(); ++y)
        for (int x = 0; x < imageDim.x(); ++x)
        {
            const tcu::IVec4 color(0, x, y, 255);
            refAccess.setPixel(color, x, y);
        }

    const tcu::TextureFormat maskFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
    tcu::TextureLevel errorMask(maskFormat, imageDim.x(), imageDim.y(), imageDim.z());
    tcu::PixelBufferAccess errorAccess(errorMask);
    const tcu::Vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    auto &log = context.getTestContext().getLog();

    // Each block needs to have the same color and be equal to one of the pixel colors of that block in the reference image.
    const auto blockWidth  = static_cast<int>(rateSize.width);
    const auto blockHeight = static_cast<int>(rateSize.height);

    tcu::clear(errorAccess, green);
    bool globalFail = false;

    const auto diagonalEdge = (imageDim.x() + imageDim.y()) / 2 - 1;

    for (int y = 0; y < imageDim.y() / blockHeight; ++y)
        for (int x = 0; x < imageDim.x() / blockWidth; ++x)
        {
            bool blockFail = false;
            std::vector<tcu::Vec4> candidates;

            candidates.reserve(rateSize.width * rateSize.height);

            const auto cornerY     = y * blockHeight;
            const auto cornerX     = x * blockWidth;
            const auto cornerColor = outPixels.getPixel(cornerX, cornerY);
            bool edgeBlock         = false;

            for (int blockY = 0; blockY < blockHeight; ++blockY)
                for (int blockX = 0; blockX < blockWidth; ++blockX)
                {
                    const auto absY     = cornerY + blockY;
                    const auto absX     = cornerX + blockX;
                    const auto resColor = outPixels.getPixel(absX, absY);

                    candidates.push_back(refAccess.getPixel(absX, absY));

                    if ((absY + absX) == diagonalEdge)
                    {
                        // Block uniformity is not guaranteed if the VRS block spans across multiple primitives
                        edgeBlock = ((blockHeight > 1) || (blockWidth > 1));
                    }

                    if ((cornerColor != resColor) && (!edgeBlock))
                    {
                        std::ostringstream msg;
                        msg << "Block not uniform: " << coordColorFormat(cornerX, cornerY, cornerColor) << " vs "
                            << coordColorFormat(absX, absY, resColor);
                        log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;

                        blockFail = true;
                    }
                }

            if (!de::contains(begin(candidates), end(candidates), cornerColor))
            {
                std::ostringstream msg;
                msg << "Block color does not match any reference color at [" << cornerX << ", " << cornerY << "]";
                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                blockFail = true;
            }

            if (blockFail)
            {
                const auto blockAccess = tcu::getSubregion(errorAccess, cornerX, cornerY, blockWidth, blockHeight);
                tcu::clear(blockAccess, red);
                globalFail = true;
            }
        }

    if (globalFail)
    {
        log << tcu::TestLog::Image("Result", "", outPixels);
        log << tcu::TestLog::Image("Reference", "", refAccess);
        log << tcu::TestLog::Image("ErrorMask", "", errorAccess);

        TCU_FAIL("Color mismatch; check log for more details");
    }

    return tcu::TestStatus::pass("Pass");
}

// Smoke test that emits one triangle per pixel plus one more global background triangle, but doesn't use every triangle. It only
// draws half the front triangles. It gets information from a mix of vertex buffers, per primitive buffers and push constants.
struct PartialUsageParams
{
    PipelineConstructionType constructionType;
    bool compactVertices;
};

class PartialUsageCase : public vkt::TestCase
{
public:
    static constexpr uint32_t kWidth            = 16u;
    static constexpr uint32_t kHeight           = 16u;
    static constexpr uint32_t kLocalInvocations = 64u;
    static constexpr uint32_t kMaxPrimitives    = kLocalInvocations;
    static constexpr uint32_t kMaxVertices      = kMaxPrimitives * 3u;
    static constexpr uint32_t kNumWorkGroups    = 2u;
    static constexpr uint32_t kTotalPrimitives  = kNumWorkGroups * kMaxPrimitives;

    PartialUsageCase(tcu::TestContext &testCtx, const std::string &name, const PartialUsageParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~PartialUsageCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    struct IndexAndColor
    {
        uint32_t index;
        float color;
    };

    struct PushConstants
    {
        uint32_t totalTriangles;
        float depth;
        float red;
    };

protected:
    PartialUsageParams m_params;
};

class PartialUsageInstance : public vkt::TestInstance
{
public:
    PartialUsageInstance(Context &context, PipelineConstructionType constructionType)
        : vkt::TestInstance(context)
        , m_constructionType(constructionType)
    {
    }
    virtual ~PartialUsageInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const PipelineConstructionType m_constructionType;
};

void PartialUsageCase::checkSupport(Context &context) const
{
    checkTaskMeshShaderSupportEXT(context, true, true);
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_params.constructionType);
}

TestInstance *PartialUsageCase::createInstance(Context &context) const
{
    return new PartialUsageInstance(context, m_params.constructionType);
}

void PartialUsageCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto buildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

    // The task shader will always emit two mesh shader work groups, which may do some work.
    std::ostringstream task;
    task << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=1) in;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    EmitMeshTasksEXT(" << kNumWorkGroups << ", 1u, 1u);\n"
         << "}\n";
    programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << buildOptions;

    // The frag shader will color the output with the indicated color;
    std::ostringstream frag;
    frag << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout (location=0) perprimitiveEXT in vec4 primitiveColor;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << "    outColor = primitiveColor;\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str()) << buildOptions;

    // The mesh shader reads primitive indices and vertices data from buffers and push constants. The primitive data block contains
    // primitive indices and primitive colors that must be read by the current invocation using an index that depends on its global
    // invocation index. The primitive index allows access into the triangle vertices buffer. Depending on the current work group
    // index and total number of triangles (set by push constants), the current invocation may have to emit a primitive or not.
    //
    // In addition, the non-compacted variant emits some extra unused vertices at the start of the array.
    const auto kExtraVertices       = (m_params.compactVertices ? 0u : kLocalInvocations);
    const auto kLocationMaxVertices = kMaxVertices + kExtraVertices;

    if (!m_params.compactVertices)
        DE_ASSERT(kLocationMaxVertices <= 256u);

    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
         << "layout (triangles) out;\n"
         << "layout (max_vertices=" << kLocationMaxVertices << ", max_primitives=" << kMaxPrimitives << ") out;\n"
         << "\n"
         << "layout (location=0) perprimitiveEXT out vec4 primitiveColor[];\n"
         << "\n"
         << "layout (set=0, binding=0, std430) readonly buffer VerticesBlock {\n"
         << "    vec2 coords[];\n" // 3 vertices per triangle.
         << "} vertex;\n"
         << "\n"
         << "struct IndexAndColor {\n"
         << "    uint  index;\n" // Triangle index (for accessing the coordinates buffer above).
         << "    float color;\n" // Triangle blue color component.
         << "};\n"
         << "\n"
         << "layout (set=0, binding=1, std430) readonly buffer PrimitiveDataBlock {\n"
         << "    IndexAndColor data[];\n"
         << "} primitive;\n"
         << "\n"
         << "layout (push_constant, std430) uniform PushConstantBlock {\n"
         << "    uint  totalTriangles;\n" // How many triangles in total we have to emit.
         << "    float depth;\n"          // Triangle depth (allows painting the background with a different color).
         << "    float red;\n"            // Triangle red color component.
         << "} pc;\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         // First primitive for this work group, plus the work group primitive and vertex count.
         << "    const uint firstPrimitive   = gl_WorkGroupID.x * gl_WorkGroupSize.x;\n"
         << "    const uint wgTriangleCount  = ((pc.totalTriangles >= firstPrimitive) ? min(pc.totalTriangles - "
            "firstPrimitive, "
         << kLocalInvocations << ") : 0u);\n"
         << "    const uint wgVertexCount    = wgTriangleCount * 3u + " << kExtraVertices << "u;\n"
         << "\n";

    if (!m_params.compactVertices)
    {
        // Produce extra unused vertices.
        mesh << "    {\n"
             << "        const float proportion = float(gl_LocalInvocationIndex) / float(gl_WorkGroupSize.x);\n"
             << "        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vec4(proportion, 1.0 - proportion, "
                "pc.depth, 1.0);\n"
             << "    }\n"
             << "\n";
    }

    mesh
        << "    SetMeshOutputsEXT(wgVertexCount, wgTriangleCount);\n"
        << "\n"
        // Calculate global invocation primitive id, and use it to access the per-primitive buffer. From there, get the primitive index in the
        // vertex buffer and the blue color component.
        << "    if (gl_LocalInvocationIndex < wgTriangleCount) {\n"
        << "        const uint  primitiveID         = firstPrimitive + gl_LocalInvocationIndex;\n"
        << "        const uint  primitiveIndex      = primitive.data[primitiveID].index;\n"
        << "        const float blue                = primitive.data[primitiveID].color;\n"
        << "        const uint  firstVertexIndex    = primitiveIndex * 3u;\n"
        << "        const uvec3 globalVertexIndices = uvec3(firstVertexIndex, firstVertexIndex+1u, "
           "firstVertexIndex+2u);\n"
        << "        const uint  localPrimitiveID    = gl_LocalInvocationIndex;\n"
        << "        const uint  firstLocalVertex    = localPrimitiveID * 3u + " << kExtraVertices << "u;\n"
        << "        const uvec3 localVertexIndices  = uvec3(firstLocalVertex, firstLocalVertex+1u, "
           "firstLocalVertex+2u);\n"
        << "\n"
        << "        gl_MeshVerticesEXT[localVertexIndices.x].gl_Position = vec4(vertex.coords[globalVertexIndices.x], "
           "pc.depth, 1.0);\n"
        << "        gl_MeshVerticesEXT[localVertexIndices.y].gl_Position = vec4(vertex.coords[globalVertexIndices.y], "
           "pc.depth, 1.0);\n"
        << "        gl_MeshVerticesEXT[localVertexIndices.z].gl_Position = vec4(vertex.coords[globalVertexIndices.z], "
           "pc.depth, 1.0);\n"
        << "\n"
        << "        gl_PrimitiveTriangleIndicesEXT[localPrimitiveID] = localVertexIndices;\n"
        << "        primitiveColor[localPrimitiveID]                 = vec4(pc.red, 0.0, blue, 1.0f);\n"
        << "    }\n"
        << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

inline float pixelToFBCoords(uint32_t pixelId, uint32_t totalPixels)
{
    return (static_cast<float>(pixelId) + 0.5f) / static_cast<float>(totalPixels) * 2.0f - 1.0f;
}

tcu::TestStatus PartialUsageInstance::iterate()
{
    const auto &vki             = m_context.getInstanceInterface();
    const auto &vkd             = m_context.getDeviceInterface();
    const auto physicalDevice   = m_context.getPhysicalDevice();
    const auto device           = m_context.getDevice();
    const auto queueIndex       = m_context.getUniversalQueueFamilyIndex();
    const auto queue            = m_context.getUniversalQueue();
    auto &alloc                 = m_context.getDefaultAllocator();
    const auto bufferUsage      = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto bufferDescType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto bufferDescStages = VK_SHADER_STAGE_MESH_BIT_EXT;
    const auto pcSize           = static_cast<VkDeviceSize>(sizeof(PartialUsageCase::PushConstants));
    const auto pcStages         = bufferDescStages;
    const auto pcRange          = makePushConstantRange(pcStages, 0u, static_cast<uint32_t>(pcSize));
    const auto fbExtent         = makeExtent3D(PartialUsageCase::kWidth, PartialUsageCase::kHeight, 1u);
    const tcu::IVec3 iExtent(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height),
                             static_cast<int>(fbExtent.depth));
    const auto colorFormat         = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorTcuFormat      = mapVkFormat(colorFormat);
    const auto dsFormat            = VK_FORMAT_D16_UNORM;
    const auto vertexSize          = sizeof(tcu::Vec2);
    const auto verticesPerTriangle = 3u;
    const auto pixelCount          = fbExtent.width * fbExtent.height * fbExtent.depth;
    const auto vertexCount         = pixelCount * verticesPerTriangle;
    const auto triangleSize        = vertexSize * verticesPerTriangle;
    const auto colorThreshold      = 0.005f; // 1/255 < 0.005 < 2/255
    const float fgRed              = 0.0f;
    const float bgRed              = 1.0f;
    const float bgBlue             = 1.0f;

    // Quarter of the pixel width and height in framebuffer coordinates.
    const float pixelWidth4  = 2.0f / (static_cast<float>(fbExtent.width) * 4.0f);
    const float pixelHeight4 = 2.0f / (static_cast<float>(fbExtent.height) * 4.0f);

    // Offsets for each triangle vertex from the pixel center.
    //    +-------------------+
    //    |         2         |
    //    |         x         |
    //    |        x x        |
    //    |       x   x       |
    //    |      x  x  x      |
    //    |     x       x     |
    //    |    xxxxxxxxxxx    |
    //    |   0           1   |
    //    +-------------------+
    const std::vector<tcu::Vec2> offsets{
        tcu::Vec2(-pixelWidth4, +pixelHeight4),
        tcu::Vec2(+pixelWidth4, +pixelHeight4),
        tcu::Vec2(0.0f, -pixelHeight4),
    };

    // We'll use two draw calls: triangles on the front and triangle that sets the background color, so we need two vertex buffers
    // and two primitive data buffers.
    const auto vertexBufferFrontSize = static_cast<VkDeviceSize>(triangleSize * pixelCount);
    const auto vertexBufferFrontInfo = makeBufferCreateInfo(vertexBufferFrontSize, bufferUsage);
    BufferWithMemory vertexBufferFront(vkd, device, alloc, vertexBufferFrontInfo, MemoryRequirement::HostVisible);
    auto &vertexBufferFrontAlloc = vertexBufferFront.getAllocation();
    void *vertexBufferFrontData  = vertexBufferFrontAlloc.getHostPtr();

    std::vector<tcu::Vec2> trianglePerPixel;
    trianglePerPixel.reserve(vertexCount);

    // Fill front vertex buffer.
    for (uint32_t y = 0u; y < PartialUsageCase::kHeight; ++y)
        for (uint32_t x = 0u; x < PartialUsageCase::kWidth; ++x)
            for (uint32_t v = 0u; v < verticesPerTriangle; ++v)
            {
                const auto &offset = offsets.at(v);
                const auto xCoord  = pixelToFBCoords(x, PartialUsageCase::kWidth) + offset.x();
                const auto yCoord  = pixelToFBCoords(y, PartialUsageCase::kHeight) + offset.y();
                trianglePerPixel.emplace_back(xCoord, yCoord);
            }
    deMemcpy(vertexBufferFrontData, trianglePerPixel.data(), de::dataSize(trianglePerPixel));

    // For the front triangles we will select some pixels randomly.
    using IndexAndColor = PartialUsageCase::IndexAndColor;

    std::set<uint32_t> selectedPixels;
    std::vector<IndexAndColor> indicesAndColors;
    de::Random rnd(1646058327u);
    const auto maxId           = static_cast<int>(pixelCount) - 1;
    const auto fTotalTriangles = static_cast<float>(PartialUsageCase::kTotalPrimitives);

    while (selectedPixels.size() < PartialUsageCase::kTotalPrimitives)
    {
        const auto pixelId = static_cast<uint32_t>(rnd.getInt(0, maxId));
        if (!selectedPixels.count(pixelId))
        {
            selectedPixels.insert(pixelId);

            const float colorVal = static_cast<float>(selectedPixels.size()) / fTotalTriangles;
            const IndexAndColor indexAndColor{pixelId, colorVal};

            indicesAndColors.push_back(indexAndColor);
        }
    }

    const auto primDataBufferFrontSize = static_cast<VkDeviceSize>(de::dataSize(indicesAndColors));
    const auto primDataBufferFrontInfo = makeBufferCreateInfo(primDataBufferFrontSize, bufferUsage);
    BufferWithMemory primDataBufferFront(vkd, device, alloc, primDataBufferFrontInfo, MemoryRequirement::HostVisible);
    auto &primDataBufferFrontAlloc = primDataBufferFront.getAllocation();
    void *primDataBufferFrontData  = primDataBufferFrontAlloc.getHostPtr();
    deMemcpy(primDataBufferFrontData, indicesAndColors.data(), de::dataSize(indicesAndColors));

    // Generate reference image based on the previous data.
    tcu::TextureLevel referenceLevel(colorTcuFormat, iExtent.x(), iExtent.y(), iExtent.z());
    tcu::PixelBufferAccess referenceAccess = referenceLevel.getAccess();
    const tcu::Vec4 bgColor(bgRed, 0.0f, bgBlue, 1.0f);

    tcu::clear(referenceAccess, bgColor);
    for (const auto &indexAndColor : indicesAndColors)
    {
        const int xCoord = static_cast<int>(indexAndColor.index % fbExtent.width);
        const int yCoord = static_cast<int>(indexAndColor.index / fbExtent.width);
        const tcu::Vec4 color(fgRed, 0.0f, indexAndColor.color, 1.0f);

        referenceAccess.setPixel(color, xCoord, yCoord);
    }

    // Background buffers. These will only contain one triangle.
    const std::vector<tcu::Vec2> backgroundTriangle{
        tcu::Vec2(-1.0f, -1.0f),
        tcu::Vec2(-1.0f, 3.0f),
        tcu::Vec2(3.0f, -1.0f),
    };

    const PartialUsageCase::IndexAndColor backgroundTriangleData{0u, bgBlue};

    const auto vertexBufferBackSize = static_cast<VkDeviceSize>(de::dataSize(backgroundTriangle));
    const auto vertexBufferBackInfo = makeBufferCreateInfo(vertexBufferBackSize, bufferUsage);
    BufferWithMemory vertexBufferBack(vkd, device, alloc, vertexBufferBackInfo, MemoryRequirement::HostVisible);
    auto &vertexBufferBackAlloc = vertexBufferBack.getAllocation();
    void *vertexBufferBackData  = vertexBufferBackAlloc.getHostPtr();
    deMemcpy(vertexBufferBackData, backgroundTriangle.data(), de::dataSize(backgroundTriangle));

    const auto primDataBufferBackSize = static_cast<VkDeviceSize>(sizeof(backgroundTriangleData));
    const auto primDataBufferBackInfo = makeBufferCreateInfo(primDataBufferBackSize, bufferUsage);
    BufferWithMemory primDataBufferBack(vkd, device, alloc, primDataBufferBackInfo, MemoryRequirement::HostVisible);
    auto &primDataBufferBackAlloc = primDataBufferBack.getAllocation();
    void *primDataBufferBackData  = primDataBufferBackAlloc.getHostPtr();
    deMemcpy(primDataBufferBackData, &backgroundTriangleData, sizeof(backgroundTriangleData));

    // Descriptor pool and descriptor sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(bufferDescType, 4u);
    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(bufferDescType, bufferDescStages);
    setLayoutBuilder.addSingleBinding(bufferDescType, bufferDescStages);
    const auto setLayout = setLayoutBuilder.build(vkd, device);

    const auto setFront = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());
    const auto setBack  = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

    // Update descriptor sets.
    DescriptorSetUpdateBuilder updateBuilder;
    {
        const auto bufferInfo = makeDescriptorBufferInfo(vertexBufferFront.get(), 0ull, vertexBufferFrontSize);
        updateBuilder.writeSingle(setFront.get(), DescriptorSetUpdateBuilder::Location::binding(0u), bufferDescType,
                                  &bufferInfo);
    }
    {
        const auto bufferInfo = makeDescriptorBufferInfo(primDataBufferFront.get(), 0ull, primDataBufferFrontSize);
        updateBuilder.writeSingle(setFront.get(), DescriptorSetUpdateBuilder::Location::binding(1u), bufferDescType,
                                  &bufferInfo);
    }
    {
        const auto bufferInfo = makeDescriptorBufferInfo(vertexBufferBack.get(), 0ull, vertexBufferBackSize);
        updateBuilder.writeSingle(setBack.get(), DescriptorSetUpdateBuilder::Location::binding(0u), bufferDescType,
                                  &bufferInfo);
    }
    {
        const auto bufferInfo = makeDescriptorBufferInfo(primDataBufferBack.get(), 0ull, primDataBufferBackSize);
        updateBuilder.writeSingle(setBack.get(), DescriptorSetUpdateBuilder::Location::binding(1u), bufferDescType,
                                  &bufferInfo);
    }
    updateBuilder.update(vkd, device);

    // Pipeline layout.
    const PipelineLayoutWrapper pipelineLayout(m_constructionType, vkd, device, setLayout.get(), &pcRange);

    // Shader modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto taskShader = ShaderWrapper(vkd, device, binaries.get("task"));
    const auto meshShader = ShaderWrapper(vkd, device, binaries.get("mesh"));
    const auto fragShader = ShaderWrapper(vkd, device, binaries.get("frag"));

    // Render pass.
    const auto renderPass = makeRenderPass(vkd, device, colorFormat, dsFormat);

    // Color and depth/stencil buffers.
    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        VK_FORMAT_UNDEFINED,                 // VkFormat format;
        fbExtent,                            // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        0u,                                  // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    std::unique_ptr<ImageWithMemory> colorAttachment;
    {
        auto colorAttCreateInfo   = imageCreateInfo;
        colorAttCreateInfo.format = colorFormat;
        colorAttCreateInfo.usage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        colorAttachment.reset(new ImageWithMemory(vkd, device, alloc, colorAttCreateInfo, MemoryRequirement::Any));
    }

    std::unique_ptr<ImageWithMemory> dsAttachment;
    {
        auto dsAttCreateInfo   = imageCreateInfo;
        dsAttCreateInfo.format = dsFormat;
        dsAttCreateInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        dsAttachment.reset(new ImageWithMemory(vkd, device, alloc, dsAttCreateInfo, MemoryRequirement::Any));
    }

    const auto colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto dsSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);

    const auto colorView =
        makeImageView(vkd, device, colorAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);
    const auto dsView = makeImageView(vkd, device, dsAttachment->get(), VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSRR);

    // Create verification buffer.
    const auto verificationBufferSize =
        static_cast<VkDeviceSize>(tcu::getPixelSize(colorTcuFormat) * iExtent.x() * iExtent.y() * iExtent.z());
    const auto verificationBufferInfo = makeBufferCreateInfo(verificationBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory verificationBuffer(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
    auto &verificationBufferAlloc = verificationBuffer.getAllocation();
    void *verificationBufferData  = verificationBufferAlloc.getHostPtr();

    // Framebuffer.
    const std::vector<VkImageView> fbViews{colorView.get(), dsView.get()};
    const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), static_cast<uint32_t>(fbViews.size()),
                                             de::dataOrNull(fbViews), fbExtent.width, fbExtent.height);

    // Viewports and scissors.
    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Pipeline.
    const VkStencilOpState stencilOpState              = {};
    const VkPipelineDepthStencilStateCreateInfo dsInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        VK_TRUE,                                                    // VkBool32 depthTestEnable;
        VK_TRUE,                                                    // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_LESS,                                         // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        stencilOpState,                                             // VkStencilOpState front;
        stencilOpState,                                             // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        1.0f,                                                       // float maxDepthBounds;
    };

    GraphicsPipelineWrapper pipelineMaker(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(),
                                          m_constructionType);
    makeMeshGraphicsPipeline(pipelineMaker, pipelineLayout, taskShader, meshShader, fragShader, renderPass.get(),
                             viewports, scissors, 0u, &dsInfo);
    const auto pipeline = pipelineMaker.getPipeline();

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    // Draw the triangles in the front, then the triangle in the back.
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const float clearDepth      = 1.0f;
    const uint32_t clearStencil = 0u;

    const PartialUsageCase::PushConstants pcFront = {PartialUsageCase::kTotalPrimitives, 0.0f, fgRed};
    const PartialUsageCase::PushConstants pcBack  = {1u, 0.5f, bgRed};

    beginCommandBuffer(vkd, cmdBuffer);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor, clearDepth,
                    clearStencil);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Front triangles.
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &setFront.get(),
                              0u, nullptr);
    vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, static_cast<uint32_t>(pcSize), &pcFront);
    vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);

    // Back triangles.
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &setBack.get(),
                              0u, nullptr);
    vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, static_cast<uint32_t>(pcSize), &pcBack);
    vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);

    endRenderPass(vkd, cmdBuffer);

    // Copy color attachment to verification buffer.
    const auto colorToTransferBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorAttachment->get(), colorSRR);
    const auto transferToHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    const auto copyRegion            = makeBufferImageCopy(fbExtent, colorSRL);

    cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, &colorToTransferBarrier);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorAttachment->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             verificationBuffer.get(), 1u, &copyRegion);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &transferToHostBarrier);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Verify color attachment.
    invalidateAlloc(vkd, device, verificationBufferAlloc);

    tcu::ConstPixelBufferAccess resultAccess(colorTcuFormat, iExtent, verificationBufferData);
    auto &log = m_context.getTestContext().getLog();
    const tcu::Vec4 errorThreshold(colorThreshold, 0.0f, colorThreshold, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, errorThreshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Result does not match reference -- check log for details");

    return tcu::TestStatus::pass("Pass");
}

// Create a classic and a mesh shading pipeline using graphics pipeline libraries. Both pipelines will use the same fragment shader
// pipeline library, and the fragment shader will use the gl_Layer built-in, which is per-primitive in mesh shaders and per-vertex
// in vertex shaders.
struct SharedFragLibraryParams
{
    PipelineConstructionType constructionType;
    bool primitiveID; // false: test gl_Layer; true: test gl_PrimitiveID.
    bool meshFirst;   // false: classic + mesh, true: mesh + classic.
    bool extraInput;  // true: add custom extra input in the frag shader.
};

class SharedFragLibraryCase : public vkt::TestCase
{
public:
    SharedFragLibraryCase(tcu::TestContext &testCtx, const std::string &name, const SharedFragLibraryParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~SharedFragLibraryCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

    static std::vector<tcu::Vec4> getLayerColors(void);

protected:
    const SharedFragLibraryParams m_params;
};

class SharedFragLibraryInstance : public vkt::TestInstance
{
public:
    SharedFragLibraryInstance(Context &context, const SharedFragLibraryParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~SharedFragLibraryInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const SharedFragLibraryParams m_params;
};

std::vector<tcu::Vec4> SharedFragLibraryCase::getLayerColors(void)
{
    std::vector<tcu::Vec4> layerColors{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f),
        tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };

    return layerColors;
}

void SharedFragLibraryCase::checkSupport(Context &context) const
{
    checkTaskMeshShaderSupportEXT(context, false /*requireTask*/, true /*requireMesh*/);

    if (m_params.primitiveID)
    {
        // When using gl_PrimitiveID in the frag shader, glslang will add "OpCapability Geometry" to it.
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    }
    else
    {
        if (context.getUsedApiVersion() < VK_API_VERSION_1_2)
            context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");
        else
        {
            // More fine-grained: we do not need shaderViewportIndex.
            const auto &vk12Features = context.getDeviceVulkan12Features();
            if (!vk12Features.shaderOutputLayer)
                TCU_THROW(NotSupportedError, "shaderOutputLayer not supported");
        }
    }

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);
}

void SharedFragLibraryCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto meshBuildOptions = getMinMeshEXTBuildOptions(programCollection.usedVulkanVersion);

    const std::string vtxPositions = "vec2 positions[4] = vec2[](\n"
                                     "    vec2(-1.0, -1.0),\n"
                                     "    vec2(-1.0,  1.0),\n"
                                     "    vec2( 1.0, -1.0),\n"
                                     "    vec2( 1.0,  1.0)\n"
                                     ");\n";

    const bool useLayer = (!m_params.primitiveID);

    // The vertex shader emits geometry to layer 1 when using layers.
    std::ostringstream vert;
    vert << "#version 450\n"
         << "#extension GL_ARB_shader_viewport_layer_array : enable\n"
         << (m_params.extraInput ? "layout (location=0) out float multiplier;\n" : "") << "\n"
         << vtxPositions << "void main ()\n"
         << "{\n"
         << "    gl_Position = vec4(positions[gl_VertexIndex % 4], 0.0, 1.0);\n"
         << (useLayer ? "    gl_Layer = 1;\n" : "")
         << (m_params.extraInput ? "    multiplier = (gl_VertexIndex < 1024 ? 1.0 : 0.0);\n" : "") // 1.0 in practice.
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("vert_1_2")
        << glu::VertexSource(vert.str())
        << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_5, 0u, true);

    // The mesh shader emits geometry to layer 2 when using layers.
    std::ostringstream mesh;
    mesh << "#version 450\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (triangles) out;\n"
         << "layout (max_vertices=4, max_primitives=2) out;\n"
         << "\n"
         << "perprimitiveEXT out gl_MeshPerPrimitiveEXT {\n"
         << (useLayer ? "   int gl_Layer;\n" : "   int gl_PrimitiveID;\n") << "} gl_MeshPrimitivesEXT[];\n"
         << "\n"
         << vtxPositions << "\n"
         << (m_params.extraInput ? "    layout (location=0) out float multiplier[];\n" : "") << "void main ()\n"
         << "{\n"
         << "    SetMeshOutputsEXT(4u, 2u);\n"
         << "    for (uint i = 0; i < 4; ++i) {\n"
         << "        gl_MeshVerticesEXT[i].gl_Position = vec4(positions[i], 0.0, 1.0);\n"
         << (m_params.extraInput ? "        multiplier[i] = (gl_NumWorkGroups[0] < 1024 ? 1.0 : 0.0);\n" :
                                   "") // 1.0 in practice.
         << "    }\n"
         << "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
         << "    gl_PrimitiveTriangleIndicesEXT[1] = uvec3(2, 1, 3);\n"
         << (useLayer ? "    gl_MeshPrimitivesEXT[0].gl_Layer = 2;\n" :
                        "    gl_MeshPrimitivesEXT[0].gl_PrimitiveID = 0;\n")
         << (useLayer ? "    gl_MeshPrimitivesEXT[1].gl_Layer = 2;\n" :
                        "    gl_MeshPrimitivesEXT[1].gl_PrimitiveID = 1;\n")
         << "}\n";
    programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << meshBuildOptions;

    // The frag shader uses the gl_Layer built-in to choose an output color.
    const auto outColors = getLayerColors();
    DE_ASSERT(outColors.size() == 3);

    std::ostringstream frag;
    frag << "#version 450\n"
         << "\n"
         << (m_params.extraInput ? "layout (location=0) in float multiplier;\n" : "")
         << "layout (location=0) out vec4 outColor;\n"
         << "\n"
         << "vec4 outColors[3] = vec4[](\n"
         << "    vec4" << outColors.at(0) << ",\n"
         << "    vec4" << outColors.at(1) << ",\n"
         << "    vec4" << outColors.at(2) << "\n"
         << ");\n"
         << "\n"
         << "void main ()\n"
         << "{\n"
         << (useLayer ? "    const vec4 baseColor = outColors[gl_Layer];\n" :
                        "    const vec4 baseColor = vec4(outColors[2].xy, gl_PrimitiveID, 1.0);\n")
         << "    outColor = baseColor" << (m_params.extraInput ? " * multiplier" : "") << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *SharedFragLibraryCase::createInstance(Context &context) const
{
    return new SharedFragLibraryInstance(context, m_params);
}

VkGraphicsPipelineLibraryCreateInfoEXT makeLibCreateInfo(VkGraphicsPipelineLibraryFlagsEXT flags, void *pNext = nullptr)
{
    const VkGraphicsPipelineLibraryCreateInfoEXT createInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, // VkStructureType sType;
        pNext,                                                       // void* pNext;
        flags,                                                       // VkGraphicsPipelineLibraryFlagsEXT flags;
    };

    return createInfo;
}

tcu::TestStatus SharedFragLibraryInstance::iterate(void)
{
    const auto &vki        = m_context.getInstanceInterface();
    const auto physDev     = m_context.getPhysicalDevice();
    const auto &vkd        = m_context.getDeviceInterface();
    const auto &device     = m_context.getDevice();
    const auto queueIndex  = m_context.getUniversalQueueFamilyIndex();
    const auto queue       = m_context.getUniversalQueue();
    auto &alloc            = m_context.getDefaultAllocator();
    const auto layerColors = SharedFragLibraryCase::getLayerColors();
    const auto &clearColor = layerColors.front();
    const auto layerCount  = (m_params.primitiveID ? 1u : static_cast<uint32_t>(layerColors.size()));
    const auto height      = (m_params.primitiveID ? 2u : 1u);
    const auto fbExtent    = makeExtent3D(1u, height, 1u);
    const tcu::IVec3 iExtent(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height),
                             static_cast<int>(layerCount));
    const auto fbFormat        = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat       = mapVkFormat(fbFormat);
    const auto fbUsage         = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const bool optimized       = (m_params.constructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY);
    const auto libExtraFlags   = (optimized ? VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT : 0);
    const auto libCompileFlags = (VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | libExtraFlags);
    const auto pipelineLinkFlags = (optimized ? VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT : 0);
    const bool useESO            = isConstructionTypeShaderObject(m_params.constructionType);
    const auto colorBufferSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount);

    // When using layer, use a single framebuffer and draw twice in the same render pass.
    // When using primitive ID, use two framebuffers. Draw to the first one with a normal pipeline and to the second one with a mesh pipeline.
    // Also, when using primitive ID, the framebuffer will be 2 pixels tall so each triangle in the strip draws to a separate pixel.
    enum class DrawType
    {
        CLASSIC = 0,
        MESH    = 1,
    };
    using DrawTypeVec = std::vector<DrawType>;
    std::vector<DrawTypeVec> rpSetups; // One or two render pass instances.

    std::vector<DrawType> usedDraws{DrawType::CLASSIC, DrawType::MESH};
    if (m_params.meshFirst)
        std::swap(usedDraws.front(), usedDraws.back());

    if (m_params.primitiveID)
    {
        rpSetups.push_back(DrawTypeVec{usedDraws.front()});
        rpSetups.push_back(DrawTypeVec{usedDraws.back()});
    }
    else
    {
        rpSetups.push_back(DrawTypeVec{usedDraws.front(), usedDraws.back()});
    }
    const auto fbCount = de::sizeU32(rpSetups);

    // Color buffer(s).
    using ImageWithBufferPtr = std::unique_ptr<ImageWithBuffer>;
    std::vector<ImageWithBufferPtr> colorBuffers;
    colorBuffers.reserve(fbCount);
    for (uint32_t i = 0u; i < fbCount; ++i)
        colorBuffers.emplace_back(new ImageWithBuffer(vkd, device, alloc, fbExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D,
                                                      colorBufferSRR, layerCount));

    // Render pass. Note for ESO we will not use its begin/end methods because we have multiple framebuffers.
    RenderPassWrapper renderPass(m_params.constructionType, vkd, device, fbFormat);

    // Framebuffer(s).
    std::vector<Move<VkFramebuffer>> framebuffers;
    if (!useESO)
    {
        framebuffers.reserve(fbCount);
        for (uint32_t i = 0u; i < fbCount; ++i)
        {
            framebuffers.push_back(makeFramebuffer(vkd, device, renderPass.get(), colorBuffers.at(i)->getImageView(),
                                                   fbExtent.width, fbExtent.height, layerCount));
        }
    }

    // Pipeline layout (common).
    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, vkd, device);

    // Shader modules.
    const auto &binaries = m_context.getBinaryCollection();
    const auto &vertBinary =
        (m_context.contextSupports(VK_API_VERSION_1_2) ? binaries.get("vert_1_2") : binaries.get("vert"));
    const auto &meshBinary = binaries.get("mesh");

    ShaderWrapper vertModule(vkd, device, vertBinary);
    ShaderWrapper meshModule(vkd, device, meshBinary);
    ShaderWrapper fragModule(vkd, device, binaries.get("frag"));

    Move<VkPipeline> fragOutputLib;
    Move<VkPipeline> fragShaderLib;
    Move<VkPipeline> preRastClassicLib;
    Move<VkPipeline> preRastMeshLib;
    Move<VkPipeline> vertexInputLib;
    Move<VkPipeline> classicPipeline;
    Move<VkPipeline> meshPipeline;

    std::unique_ptr<GraphicsPipelineWrapper> pipelineWrapper;
    Move<VkShaderEXT> vertShader;
    Move<VkShaderEXT> meshShader;

    // Pipeline state. We can reuse this for ESO and GPL.

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const VkColorComponentFlags colorComponentFlags =
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,             // VkBool32                 blendEnable
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor            srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor            dstColorBlendFactor
        VK_BLEND_OP_ADD,      // VkBlendOp                colorBlendOp
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor            srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor            dstAlphaBlendFactor
        VK_BLEND_OP_ADD,      // VkBlendOp                alphaBlendOp
        colorComponentFlags,  // VkColorComponentFlags    colorWriteMask
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendState = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_CLEAR,                                        // VkLogicOp logicOp;
        1u,                                                       // uint32_t attachmentCount;
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f},   // float blendConstants[4];
    };

    const VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                                sType
        nullptr,               // const void*                                    pNext
        0u,                    // VkPipelineMultisampleStateCreateFlags        flags
        VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                        rasterizationSamples
        VK_FALSE,              // VkBool32                                        sampleShadingEnable
        1.0f,                  // float                                        minSampleShading
        nullptr,               // const VkSampleMask*                            pSampleMask
        VK_FALSE,              // VkBool32                                        alphaToCoverageEnable
        VK_FALSE               // VkBool32                                        alphaToOneEnable
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = initVulkanStructure();
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo   = initVulkanStructure();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = initVulkanStructure();
    inputAssemblyStateCreateInfo.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        0u,                                                    // VkPipelineViewportStateCreateFlags flags;
        static_cast<uint32_t>(viewports.size()),               // uint32_t viewportCount;
        de::dataOrNull(viewports),                             // const VkViewport* pViewports;
        static_cast<uint32_t>(scissors.size()),                // uint32_t scissorCount;
        de::dataOrNull(scissors),                              // const VkRect2D* pScissors;
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,                                       // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,                                          // VkCullModeFlags cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f,                                                       // float lineWidth;
    };

    if (useESO)
    {
        // For ESO, we will create a graphics pipeline wrapper with the right module for the first draw, and
        // a separate module for the second draw. We will use the pipeline wrapper to bind shaders and state correctly
        // for the first draw and manually unbind shaders and bind the correct ones for the second draw.
        pipelineWrapper.reset(new GraphicsPipelineWrapper(vki, vkd, physDev, device, m_context.getDeviceExtensions(),
                                                          m_params.constructionType));
        auto &pipeline = *pipelineWrapper;
        pipeline.setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo);
        if (m_params.meshFirst)
            pipeline.setupPreRasterizationMeshShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u,
                                                          ShaderWrapper(), meshModule, &rasterizationStateCreateInfo);
        else
            pipeline.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u,
                                                      vertModule, &rasterizationStateCreateInfo);
        pipeline.setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragModule,
                                          &depthStencilStateCreateInfo, &multisampleState);
        pipeline.setupFragmentOutputState(renderPass.get(), 0u, &colorBlendState, &multisampleState);
        pipeline.buildPipeline();

        const VkShaderCreateInfoEXT shaderCreateInfoTemplate = {
            VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
            nullptr,
            0u,
            VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_SHADER_CODE_TYPE_SPIRV_EXT,
            0ull,
            nullptr,
            "main",
            0u,
            nullptr,
            0u,
            nullptr,
            nullptr,
        };

        if (m_params.meshFirst)
        {
            VkShaderCreateInfoEXT vertShaderCreateInfo = shaderCreateInfoTemplate;
            vertShaderCreateInfo.stage                 = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderCreateInfo.codeSize              = vertBinary.getSize();
            vertShaderCreateInfo.pCode                 = vertBinary.getBinary();

            vertShader = createShader(vkd, device, vertShaderCreateInfo);
        }
        else
        {
            VkShaderCreateInfoEXT meshShaderCreateInfo = shaderCreateInfoTemplate;
            meshShaderCreateInfo.stage                 = VK_SHADER_STAGE_MESH_BIT_EXT;
            meshShaderCreateInfo.codeSize              = meshBinary.getSize();
            meshShaderCreateInfo.pCode                 = meshBinary.getBinary();
            meshShaderCreateInfo.flags                 = VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT;

            meshShader = createShader(vkd, device, meshShaderCreateInfo);
        }
    }
    else
    {
        // Create classic modules directly.
        vertModule.createModule();
        meshModule.createModule();
        fragModule.createModule();

        // Fragment output state library (common).
        const auto fragOutputLibInfo =
            makeLibCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);

        VkGraphicsPipelineCreateInfo fragOutputInfo = initVulkanStructure();
        fragOutputInfo.layout                       = pipelineLayout.get();
        fragOutputInfo.renderPass                   = renderPass.get();
        fragOutputInfo.pColorBlendState             = &colorBlendState;
        fragOutputInfo.pMultisampleState            = &multisampleState;
        fragOutputInfo.flags                        = libCompileFlags;
        fragOutputInfo.pNext                        = &fragOutputLibInfo;

        fragOutputLib = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &fragOutputInfo);

        // Fragment shader lib (shared among the classic and mesh pipelines).
        const VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits stage;
            fragModule.getModule(),                              // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        const auto fragShaderLibInfo = makeLibCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);

        VkGraphicsPipelineCreateInfo fragShaderInfo = initVulkanStructure();
        fragShaderInfo.layout                       = pipelineLayout.get();
        fragShaderInfo.renderPass                   = renderPass.get();
        fragShaderInfo.pMultisampleState            = &multisampleState;
        fragShaderInfo.pDepthStencilState           = &depthStencilStateCreateInfo;
        fragShaderInfo.stageCount                   = 1u;
        fragShaderInfo.pStages                      = &fragShaderStageCreateInfo;
        fragShaderInfo.flags                        = libCompileFlags;
        fragShaderInfo.pNext                        = &fragShaderLibInfo;

        fragShaderLib = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &fragShaderInfo);

        // Vertex input state (common, but should be unused by the mesh shading pipeline).
        const auto vertexInputLibInfo = makeLibCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);

        VkGraphicsPipelineCreateInfo vertexInputInfo = initVulkanStructure();
        vertexInputInfo.layout                       = pipelineLayout.get();
        vertexInputInfo.pVertexInputState            = &vertexInputStateCreateInfo;
        vertexInputInfo.pInputAssemblyState          = &inputAssemblyStateCreateInfo;
        vertexInputInfo.flags                        = libCompileFlags;
        vertexInputInfo.pNext                        = &vertexInputLibInfo;

        vertexInputLib = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &vertexInputInfo);

        // Pre-rasterization shader state: common pieces.
        const auto preRastLibInfo = makeLibCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);

        VkGraphicsPipelineCreateInfo preRastShaderInfo = initVulkanStructure();
        preRastShaderInfo.layout                       = pipelineLayout.get();
        preRastShaderInfo.pViewportState               = &viewportStateCreateInfo;
        preRastShaderInfo.pRasterizationState          = &rasterizationStateCreateInfo;
        preRastShaderInfo.renderPass                   = renderPass.get();
        preRastShaderInfo.flags                        = libCompileFlags;
        preRastShaderInfo.pNext                        = &preRastLibInfo;
        preRastShaderInfo.stageCount                   = 1u;

        // Vertex stage info.
        const VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits stage;
            vertModule.getModule(),                              // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        // Mesh stage info.
        const VkPipelineShaderStageCreateInfo meshShaderStageCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_MESH_BIT_EXT,                        // VkShaderStageFlagBits stage;
            meshModule.getModule(),                              // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        // Pre-rasterization shader libs.
        preRastShaderInfo.pStages = &vertShaderStageCreateInfo;
        preRastClassicLib         = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &preRastShaderInfo);

        preRastShaderInfo.pStages = &meshShaderStageCreateInfo;
        preRastMeshLib            = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &preRastShaderInfo);

        // Pipelines.
        const std::vector<VkPipeline> classicLibs{vertexInputLib.get(), preRastClassicLib.get(), fragShaderLib.get(),
                                                  fragOutputLib.get()};
        const std::vector<VkPipeline> meshLibs{vertexInputLib.get(), preRastMeshLib.get(), fragShaderLib.get(),
                                               fragOutputLib.get()};

        const VkPipelineLibraryCreateInfoKHR classicLinkInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            static_cast<uint32_t>(classicLibs.size()),          // uint32_t libraryCount;
            de::dataOrNull(classicLibs),                        // const VkPipeline* pLibraries;
        };

        const VkPipelineLibraryCreateInfoKHR meshLinkInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            static_cast<uint32_t>(meshLibs.size()),             // uint32_t libraryCount;
            de::dataOrNull(meshLibs),                           // const VkPipeline* pLibraries;
        };

        VkGraphicsPipelineCreateInfo classicPipelineCreateInfo = initVulkanStructure();
        classicPipelineCreateInfo.flags                        = pipelineLinkFlags;
        classicPipelineCreateInfo.layout                       = pipelineLayout.get();
        classicPipelineCreateInfo.pNext                        = &classicLinkInfo;

        VkGraphicsPipelineCreateInfo meshPipelineCreateInfo = initVulkanStructure();
        meshPipelineCreateInfo.flags                        = pipelineLinkFlags;
        meshPipelineCreateInfo.layout                       = pipelineLayout.get();
        meshPipelineCreateInfo.pNext                        = &meshLinkInfo;

        classicPipeline = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &classicPipelineCreateInfo);
        meshPipeline    = createGraphicsPipeline(vkd, device, VK_NULL_HANDLE, &meshPipelineCreateInfo);
    }

    // Record commands with both pipelines.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    bool esoStateBound = false;

    if (useESO)
    {
        // Transition color attachment layouts manually.
        const auto dstAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        const auto dstStage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        std::vector<VkImageMemoryBarrier> barriers;
        barriers.reserve(colorBuffers.size());

        for (const auto &colorBuffer : colorBuffers)
            barriers.push_back(makeImageMemoryBarrier(0u, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer->getImage(),
                                                      colorBufferSRR));

        cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, 0u, dstStage, barriers.data(), barriers.size());
    }

    // Draw using both pipelines.
    for (size_t i = 0u; i < rpSetups.size(); ++i)
    {
        const auto &rpSetup = rpSetups.at(i);

        if (useESO)
        {
            const VkRenderingAttachmentInfo colorRenderingAttachment = {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                nullptr,
                colorBuffers.at(i)->getImageView(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_RESOLVE_MODE_NONE_KHR,
                VK_NULL_HANDLE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                makeClearValueColor(clearColor),
            };

            const VkRenderingInfo renderingInfo = {
                VK_STRUCTURE_TYPE_RENDERING_INFO, nullptr, 0u,      scissors.at(0u), layerCount, 0u, 1u,
                &colorRenderingAttachment,        nullptr, nullptr,
            };
            vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
        }
        else
            beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffers.at(i).get(), scissors.at(0), clearColor);

        for (const auto &drawType : rpSetup)
        {
            if (drawType == DrawType::CLASSIC)
            {
                if (useESO)
                {
                    if (!esoStateBound)
                    {
                        DE_ASSERT(!m_params.meshFirst); // The wrapper should contain the classic graphics state.
                        pipelineWrapper->bind(cmdBuffer);
                        esoStateBound = true;
                    }
                    else
                    {
                        const std::map<VkShaderStageFlagBits, VkShaderEXT> stagesToBind{
                            std::make_pair(VK_SHADER_STAGE_MESH_BIT_EXT, VK_NULL_HANDLE),
                            std::make_pair(VK_SHADER_STAGE_VERTEX_BIT, *vertShader),
                        };
                        for (const auto &stageShader : stagesToBind)
                            vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageShader.first, &stageShader.second);
                        vkd.cmdSetPrimitiveTopology(cmdBuffer, inputAssemblyStateCreateInfo.topology);
                        vkd.cmdSetPrimitiveRestartEnable(cmdBuffer,
                                                         inputAssemblyStateCreateInfo.primitiveRestartEnable);
                        vkd.cmdSetVertexInputEXT(cmdBuffer, 0u, nullptr, 0u, nullptr);
                    }
                }
                else
                    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, classicPipeline.get());
                vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
            }
            else if (drawType == DrawType::MESH)
            {
                if (useESO)
                {
                    if (!esoStateBound)
                    {
                        DE_ASSERT(m_params.meshFirst); // The wrapper should contain the mesh graphics state.
                        pipelineWrapper->bind(cmdBuffer);
                        esoStateBound = true;
                    }
                    else
                    {
                        const std::map<VkShaderStageFlagBits, VkShaderEXT> stagesToBind{
                            std::make_pair(VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE),
                            std::make_pair(VK_SHADER_STAGE_MESH_BIT_EXT, *meshShader),
                        };
                        for (const auto &stageShader : stagesToBind)
                            vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageShader.first, &stageShader.second);
                    }
                }
                else
                    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline.get());
                vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
            }
            else
                DE_ASSERT(false);
        }

        if (useESO)
            vkd.cmdEndRendering(cmdBuffer);
        else
            endRenderPass(vkd, cmdBuffer);
    }

    // Copy color buffers to verification buffers.
    for (uint32_t i = 0u; i < fbCount; ++i)
    {
        copyImageToBuffer(vkd, cmdBuffer, colorBuffers.at(i)->getImage(), colorBuffers.at(i)->getBuffer(),
                          iExtent.swizzle(0, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, layerCount);
    }

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Validate color buffer.
    for (uint32_t i = 0u; i < fbCount; ++i)
        invalidateAlloc(vkd, device, colorBuffers.at(i)->getBufferAllocation());

    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    for (uint32_t i = 0u; i < fbCount; ++i)
    {
        Allocation &colorDataAlloc = colorBuffers.at(i)->getBufferAllocation();
        tcu::ConstPixelBufferAccess resultAccess(tcuFormat, iExtent, colorDataAlloc.getHostPtr());

        for (int z = 0; z < iExtent.z(); ++z)
        {
            for (int y = 0; y < iExtent.y(); ++y)
                for (int x = 0; x < iExtent.x(); ++x)
                {
                    const auto expectedColor =
                        (m_params.primitiveID ?
                             tcu::Vec4(layerColors.at(2u).x(), layerColors.at(2u).y(), static_cast<float>(y), 1.0f) :
                             layerColors.at(z));
                    const auto resultColor = resultAccess.getPixel(x, y, z);
                    if (resultColor != expectedColor)
                    {
                        std::ostringstream msg;
                        msg << "Unexpected color at framebuffer " << i << " coordinates (x=" << x << ", y=" << y
                            << ", layer=" << z << "): expected " << expectedColor << " but found " << resultColor;
                        log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                        fail = true;
                    }
                }
        }
    }

    if (fail)
        TCU_FAIL("Failed; check log for details --");
    return tcu::TestStatus::pass("Pass");
}

struct DepthOnlyParams
{
    enum class Geometry
    {
        POINTS = 0,
        TRIANGLES
    };

    PipelineConstructionType constructionType;
    Geometry geometry;
    bool stepByStepPosition;

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(64, 32, 1);
    }

    tcu::Vec2 getXYOffset() const
    {
        return tcu::Vec2(1000.0f, 2000.0f);
    }
};

void depthOnlySupport(Context &context, DepthOnlyParams params)
{
    checkTaskMeshShaderSupportEXT(context, false, true);

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, params.constructionType);
}

void depthOnlyPrograms(vk::SourceCollections &dst, DepthOnlyParams params)
{
    // Note we must explicitly omit the fragment shader.
    // Each working group will handle a full row, 1 primitive per invocation, each covering 1 pixel.
    const bool isTriangles                  = (params.geometry == DepthOnlyParams::Geometry::TRIANGLES);
    const auto extent                       = params.getExtent().asUint();
    const auto xyOffset                     = params.getXYOffset();
    const uint32_t kPrimitiveVertices       = (isTriangles ? 3u : 1u);
    const uint32_t kMaxVerticesPerWorkGroup = extent.x() * kPrimitiveVertices;
    const std::string outPrimitive          = (isTriangles ? "triangles" : "points");
    const std::string indexBuiltIn = (isTriangles ? "gl_PrimitiveTriangleIndicesEXT" : "gl_PrimitivePointIndicesEXT");
    const std::string indexValues =
        (isTriangles ? "uvec3(baseOutVertex, baseOutVertex + 1u, baseOutVertex + 2u)" : "baseOutVertex");
    std::ostringstream mesh;
    mesh << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "layout (" << outPrimitive << ") out;\n"
         << "layout (max_vertices=" << kMaxVerticesPerWorkGroup << ", max_primitives=" << extent.x() << ") out;\n"
         << "layout (local_size_x=" << extent.x() << ", local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) readonly buffer PositionsArray { vec4 position[]; } vtxData;\n"
         << "void main (void) {\n"
         << "    const uint primitiveVertices = " << kPrimitiveVertices << ";\n"
         << "    const uint row = gl_WorkGroupID.x;\n"
         << "    const uint col = gl_LocalInvocationIndex;\n"
         << "    const uint pixelIdx = row * " << extent.x() << " + col;\n"
         << "    const uint baseInVertex = pixelIdx * primitiveVertices;\n"
         << "    const uint baseOutVertex = col * primitiveVertices;\n"
         << "    SetMeshOutputsEXT(" << kMaxVerticesPerWorkGroup << ", " << extent.x() << ");\n"
         << "    for (uint i = 0u; i < primitiveVertices; ++i) {\n"
         << "        const uint inIndex = baseInVertex + i;\n"
         << "        const uint outIndex = baseOutVertex + i;\n"
         << "        vec4 outPos;\n"
         << "        outPos.x = vtxData.position[inIndex].x - " << xyOffset.x() << ";\n"
         << "        outPos.y = vtxData.position[inIndex].y - " << xyOffset.y() << ";\n"
         << "        outPos.z = vtxData.position[inIndex].z;\n"
         << "        outPos.w = 1.0;\n";

    if (params.stepByStepPosition) // This caused issues in the past for some drivers.
    {
        mesh << "        gl_MeshVerticesEXT[outIndex].gl_Position.x = outPos.x;\n"
             << "        gl_MeshVerticesEXT[outIndex].gl_Position.y = outPos.y;\n"
             << "        gl_MeshVerticesEXT[outIndex].gl_Position.z = outPos.z;\n"
             << "        gl_MeshVerticesEXT[outIndex].gl_Position.w = outPos.w;\n";
    }
    else
        mesh << "        gl_MeshVerticesEXT[outIndex].gl_Position = outPos;\n";

    mesh << "        gl_MeshVerticesEXT[outIndex].gl_PointSize = 1.0;\n"
         << "    }\n"
         << "    " << indexBuiltIn << "[col] = " << indexValues << ";\n"
         << "}\n";
    const auto buildOptions = getMinMeshEXTBuildOptions(dst.usedVulkanVersion);
    dst.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
}

tcu::TestStatus depthOnlyRun(Context &context, DepthOnlyParams params)
{
    const auto ctx                    = context.getContextCommonData();
    const auto xyOffset               = params.getXYOffset();
    const auto fbExtent               = params.getExtent();
    const auto apiExtent              = makeExtent3D(fbExtent);
    const auto depthFormat            = VK_FORMAT_D16_UNORM;
    const auto usage                  = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto depthSRR               = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
    const uint32_t kPrimitiveVertices = (params.geometry == DepthOnlyParams::Geometry::POINTS ? 1u : 3u);
    const uint32_t kPixelCount        = apiExtent.width * apiExtent.height;
    const uint32_t kVertexCount       = kPixelCount * kPrimitiveVertices;

    // Depth buffer.
    ImageWithBuffer depthBuffer(ctx.vkd, ctx.device, ctx.allocator, apiExtent, depthFormat, usage, VK_IMAGE_TYPE_2D,
                                depthSRR);

    const auto floatExtent  = fbExtent.asFloat();
    const float pixelWidth  = 2.0f / floatExtent.x();
    const float pixelHeight = 2.0f / floatExtent.y();
    const float horMargin   = pixelWidth * 0.25f;
    const float vertMargin  = pixelHeight * 0.25f;

    std::vector<tcu::Vec4> positions;
    positions.reserve(kVertexCount);

    std::vector<float> pixelDepths;
    pixelDepths.reserve(kPixelCount);

    const std::vector<tcu::Vec2> positionMargins{
        tcu::Vec2(0.0f, -vertMargin),
        tcu::Vec2(-horMargin, vertMargin),
        tcu::Vec2(horMargin, vertMargin),
    };

    de::Random rnd(1738233594u + static_cast<uint32_t>(params.constructionType));
    for (uint32_t i = 0u; i < kPixelCount; ++i)
        pixelDepths.push_back(rnd.getFloat());

    for (uint32_t y = 0u; y < apiExtent.height; ++y)
        for (uint32_t x = 0u; x < apiExtent.width; ++x)
        {
            const uint32_t pixelId = y * apiExtent.width + x;
            const float &depth     = pixelDepths.at(pixelId);

            const float xCenter = (static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f;
            const float yCenter = (static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0f - 1.0f;

            for (uint32_t i = 0u; i < kPrimitiveVertices; ++i)
            {
                const auto &margin = positionMargins.at(i);
                positions.emplace_back(xCenter + margin.x() + xyOffset.x(), yCenter + margin.y() + xyOffset.y(), depth,
                                       1.0f);
            }
        }

    // "Vertex" buffer.
    const auto vertBufferSize = static_cast<VkDeviceSize>(de::dataSize(positions));
    const auto vertBufferInfo = makeBufferCreateInfo(vertBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory vertBuffer(ctx.vkd, ctx.device, ctx.allocator, vertBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = vertBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(positions), de::dataSize(positions));
    }

    // Descriptor set stuff.
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_MESH_BIT_EXT);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, stageFlags);
    const auto setLayout     = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto bufferDescInfo = makeDescriptorBufferInfo(*vertBuffer, 0ull, VK_WHOLE_SIZE);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                 &bufferDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Pipeline.
    ShaderWrapper meshShader(ctx.vkd, ctx.device, context.getBinaryCollection().get("mesh"));

    PipelineLayoutWrapper pipelineLayout(params.constructionType, ctx.vkd, ctx.device, *setLayout);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    RenderPassWrapper renderPass(params.constructionType, ctx.vkd, ctx.device, VK_FORMAT_UNDEFINED, depthFormat);

    const auto depthBufferImage = depthBuffer.getImage();
    const auto depthBufferView  = depthBuffer.getImageView();
    renderPass.createFramebuffer(ctx.vkd, ctx.device, 1u, &depthBufferImage, &depthBufferView, apiExtent.width,
                                 apiExtent.height);

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_FALSE,
        VK_FALSE, // Do not discard rasterization.
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };

    // Stencil not used so we provide some default values.
    const auto stencilOpState =
        makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0u, 0u, 0u);

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_TRUE,            // Enable depth test.
        VK_TRUE,            // Enable depth writes.
        VK_COMPARE_OP_LESS, // We'll clear to 1.0.
        VK_FALSE,
        VK_FALSE,
        stencilOpState,
        stencilOpState,
        0.0f,
        0.0f,
    };

    // No color attachments.
    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, context.getDeviceExtensions(),
                                     params.constructionType);
    pipeline.setDefaultMultisampleState()
        .setupPreRasterizationMeshShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u,
                                              ShaderWrapper(), meshShader, &rasterizationStateCreateInfo)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, ShaderWrapper(), &depthStencilStateCreateInfo)
        .setupFragmentOutputState(renderPass.get(), 0u, &colorBlendStateCreateInfo)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto clearValue = makeClearValueDepthStencil(1.0f, 0u);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearValue);
    pipelineLayout.bindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, 1u, &descriptorSet.get(), 0u,
                                      nullptr);
    pipeline.bind(cmdBuffer);
    ctx.vkd.cmdDrawMeshTasksEXT(cmdBuffer, apiExtent.height, 1u, 1u);
    renderPass.end(ctx.vkd, cmdBuffer);
    {
        const auto srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        const auto oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        copyImageToBuffer(ctx.vkd, cmdBuffer, depthBufferImage, depthBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                          srcAccess, oldLayout, 1u, depthSRR.aspectMask, depthSRR.aspectMask);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto tcuFormat = mapVkFormat(depthFormat);
    tcu::TextureLevel refLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    tcu::PixelBufferAccess refAccess = refLevel.getAccess();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const auto pixelIdx = static_cast<uint32_t>(y * fbExtent.x() + x);
            refAccess.setPixDepth(pixelDepths.at(pixelIdx), x, y);
        }

    auto &depthAlloc = depthBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, depthAlloc);

    tcu::ConstPixelBufferAccess resAccess(tcuFormat, fbExtent, depthAlloc.getHostPtr());

    const float threshold = 0.000025f; // 1/65535 < this value < 2/65535
    auto &log             = context.getTestContext().getLog();
    if (!tcu::dsThresholdCompare(log, "Result", "", refAccess, resAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected results found in depth buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createMeshShaderSmokeTestsEXT(tcu::TestContext &testCtx)
{
    struct
    {
        PipelineConstructionType constructionType;
        const char *name;
    } constructionTypes[] = {
        {PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, "monolithic"},
        {PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY, "optimized_lib"},
        {PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, "fast_lib"},
        {PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, "shader_objects"},
    };

    GroupPtr smokeTests(new tcu::TestCaseGroup(testCtx, "smoke"));

    for (const auto &constructionCase : constructionTypes)
    {
        const auto &cType = constructionCase.constructionType;

        GroupPtr constructionGroup(new tcu::TestCaseGroup(testCtx, constructionCase.name));

        if (!isConstructionTypeShaderObject(cType))
        {
            constructionGroup->addChild(new MeshOnlyTriangleCase(testCtx, "mesh_shader_triangle", cType));
            constructionGroup->addChild(new MeshOnlyTriangleCase(testCtx, "mesh_shader_triangle_rasterization_disabled",
                                                                 cType, true /*rasterizationDisabled*/));
            constructionGroup->addChild(new MeshTaskTriangleCase(testCtx, "mesh_task_shader_triangle", cType));
            constructionGroup->addChild(new TaskOnlyTriangleCase(testCtx, "task_only_shader_triangle", cType));

            for (int i = 0; i < 2; ++i)
            {
                const bool compaction        = (i == 0);
                const std::string nameSuffix = (compaction ? "" : "_without_compaction");
                const PartialUsageParams params{cType, compaction};

                constructionGroup->addChild(new PartialUsageCase(testCtx, "partial_usage" + nameSuffix, params));
            }

            addFunctionCaseWithPrograms(constructionGroup.get(), "fullscreen_gradient", checkMeshSupport,
                                        initGradientPrograms, testFullscreenGradient,
                                        GradientParams(tcu::nothing<FragmentSize>(), cType));
            addFunctionCaseWithPrograms(constructionGroup.get(), "fullscreen_gradient_fs2x2", checkMeshSupport,
                                        initGradientPrograms, testFullscreenGradient,
                                        GradientParams(tcu::just(FragmentSize::SIZE_2X2), cType));
            addFunctionCaseWithPrograms(constructionGroup.get(), "fullscreen_gradient_fs2x1", checkMeshSupport,
                                        initGradientPrograms, testFullscreenGradient,
                                        GradientParams(tcu::just(FragmentSize::SIZE_2X1), cType));
        }

        if (cType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
        {
            const bool isESOCase = isConstructionTypeShaderObject(cType);

            for (const auto usePrimitiveID : {false, true})
                for (const auto meshFirst : {false, true})
                    for (const auto extraInput : {false, true})
                    {
                        SharedFragLibraryParams params{cType, usePrimitiveID, meshFirst, extraInput};
                        const auto testName = std::string() +
                                              (isESOCase ? "shared_frag_shader" : "shared_frag_library") +
                                              (usePrimitiveID ? "_primid" : "") + (meshFirst ? "_mesh_first" : "") +
                                              (extraInput ? "_extra_input" : "");
                        constructionGroup->addChild(new SharedFragLibraryCase(testCtx, testName, params));
                    }
        }

        {
            for (const auto geometry : {DepthOnlyParams::Geometry::POINTS, DepthOnlyParams::Geometry::TRIANGLES})
                for (const auto stepByStepPosition : {false, true})
                {
                    const auto testName = std::string("depth_only") +
                                          ((geometry == DepthOnlyParams::Geometry::POINTS) ? "_points" : "_triangles") +
                                          (stepByStepPosition ? "_position_components" : "");

                    const DepthOnlyParams params{
                        constructionCase.constructionType,
                        geometry,
                        stepByStepPosition,
                    };

                    addFunctionCaseWithPrograms(constructionGroup.get(), testName, depthOnlySupport, depthOnlyPrograms,
                                                depthOnlyRun, params);
                }
        }

        smokeTests->addChild(constructionGroup.release());
    }

    return smokeTests.release();
}

} // namespace MeshShader
} // namespace vkt
