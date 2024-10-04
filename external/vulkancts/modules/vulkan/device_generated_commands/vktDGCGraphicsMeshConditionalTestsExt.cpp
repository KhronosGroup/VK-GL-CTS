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
 * \brief Device Generated Commands EXT Conditional Mesh Rendering Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsMeshConditionalTestsExt.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktDGCUtilExt.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDGCUtilCommon.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include <vector>
#include <memory>
#include <sstream>
#include <string>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

constexpr int kWidth  = 2u;
constexpr int kHeight = 4u;

using DGCComputePipelinePtr = std::unique_ptr<DGCComputePipelineExt>;
using DGCBufferPtr          = std::unique_ptr<DGCBuffer>;

enum class HasTask
{
    NO = 0,
    YES,
};

struct TestParams
{
    HasTask hasTask;          // Use a task shader or not.
    bool pipelineToken;       // Use a DGC indirect pipeline.
    bool indirectCountBuffer; // Use an indirect count buffer.
    bool conditionValue;      // Value for the condition buffer.
    bool inverted;            // Inverted condition?

    bool useTaskShader(void) const
    {
        return (hasTask == HasTask::YES);
    }
};

struct ConditionalPreprocessParams
{
    bool conditionValue;
    bool inverted;
};

VkShaderStageFlags getShaderStages(bool taskShader)
{
    VkShaderStageFlags shaderStages = (VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT);
    if (taskShader)
        shaderStages |= VK_SHADER_STAGE_TASK_BIT_EXT;
    return shaderStages;
}

inline void checkExtensionSupport(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_mesh_shader");
    context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
}

void checkDGCGraphicsMeshSupport(Context &context, bool pipelineToken, bool taskShader)
{
    const VkShaderStageFlags shaderStages = getShaderStages(taskShader);
    const VkShaderStageFlags bindStages   = (pipelineToken ? shaderStages : 0u);
    checkDGCExtSupport(context, shaderStages, bindStages);
}

void checkConditionalDGCGraphicsMeshSupport(Context &context, TestParams params)
{
    checkExtensionSupport(context);
    checkDGCGraphicsMeshSupport(context, params.pipelineToken, params.useTaskShader());
}

void checkConditionalPreprocessMeshSupport(Context &context, ConditionalPreprocessParams)
{
    checkExtensionSupport(context);
    checkDGCGraphicsMeshSupport(context, false, false);
}

// Store the push constant value in the output buffer.
void onePointPerPixelPrograms(SourceCollections &dst, bool taskShader)
{
    const vk::ShaderBuildOptions shaderBuildOpt(dst.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    // Frag shader is common.
    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (push_constant, std430) uniform PCBlock { vec4 color; } pc;\n"
         << "void main (void) {\n"
         << "    outColor = pc.color;\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());

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

    std::string taskDataDecl;
    {
        std::ostringstream taskDataDeclStream;
        taskDataDeclStream << "struct TaskData {\n"
                           << "    uint baseVertex;\n"
                           << "};\n"
                           << "taskPayloadSharedEXT TaskData td;\n";
        taskDataDecl = taskDataDeclStream.str();
    }

    std::string bindingDecl;
    {
        std::ostringstream bindingDeclStream;
        bindingDeclStream << "layout(set=0, binding=0, std430) readonly buffer VertexDataBlock {\n"
                          << "    vec4 positions[];\n"
                          << "} vertices;\n";
        bindingDecl = bindingDeclStream.str();
    }

    if (taskShader)
    {
        // Task shader will handle one row each, dispatching one mesh shader per column.
        // Each mesh shader will output a single point.
        std::ostringstream task;
        task << "#version 460\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "layout(local_size_x=1) in;\n"
             << wgIndexFunc << taskDataDecl << "void main (void) {\n"
             << "    td.baseVertex = getWorkGroupIndex() * " << kWidth << ";\n" // One task WG per row.
             << "    EmitMeshTasksEXT(1, 1, " << kWidth << ");\n"               // Dispatch one mesh WG per column.
             << "}\n";
        dst.glslSources.add("task") << glu::TaskSource(task.str()) << shaderBuildOpt;
    }

    // This is mostly common, but each mesh workgroup handles a full row or a single column depening on the presence of
    // a task shader.
    // * With task shader: one mesh WG per column, outputs 1 point.
    // * Without task shader: one mesh WG per row, each outputs kWidth points.
    const auto meshLocalSize = (taskShader ? 1u : kWidth);
    const auto vertIndexExpr =
        (taskShader ? "td.baseVertex + getWorkGroupIndex()" :
                      "getWorkGroupIndex() * " + std::to_string(kWidth) + " + gl_LocalInvocationIndex");

    std::ostringstream mesh;
    mesh << "#version 460\n"
         << "#extension GL_EXT_mesh_shader : enable\n"
         << "layout(local_size_x=" << meshLocalSize << ") in;\n"
         << "layout(points) out;\n"
         << "layout(max_vertices=" << meshLocalSize << ", max_primitives=" << meshLocalSize << ") out;\n"
         << bindingDecl << wgIndexFunc << (taskShader ? taskDataDecl : "") << "void main() {\n"
         << "    SetMeshOutputsEXT(" << meshLocalSize << ", " << meshLocalSize << ");\n"
         << "    const uint vertIndex = " << vertIndexExpr << ";\n"
         << "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = vertices.positions[vertIndex];\n"
         << "    gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_PointSize = 1.0;\n"
         << "    gl_PrimitivePointIndicesEXT[gl_LocalInvocationIndex] = gl_LocalInvocationIndex;\n"
         << "}\n";
    dst.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << shaderBuildOpt;
}

void conditionalMeshPrograms(SourceCollections &dst, TestParams params)
{
    onePointPerPixelPrograms(dst, params.useTaskShader());
}

void conditionalPreprocessMeshPrograms(SourceCollections &dst, ConditionalPreprocessParams)
{
    onePointPerPixelPrograms(dst, false);
}

void beginConditionalRendering(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkBuffer conditionBuffer,
                               bool inverted)
{
    uint32_t flags = 0u;
    if (inverted)
        flags |= VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT;

    const VkConditionalRenderingBeginInfoEXT beginInfo = {
        VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT, // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        conditionBuffer,                                        // VkBuffer buffer;
        0ull,                                                   // VkDeviceSize offset;
        flags,                                                  // VkConditionalRenderingFlagsEXT flags;
    };
    vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &beginInfo);
}

// Binds the normal (non-DGC) pipeline if it's not the null handle.
void bindPipelineIfPresent(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint,
                           VkPipeline normalPipeline, VkPipeline dgcPipeline)
{
    DE_ASSERT(normalPipeline != VK_NULL_HANDLE || dgcPipeline != VK_NULL_HANDLE);
    DE_ASSERT((normalPipeline == VK_NULL_HANDLE) != (dgcPipeline == VK_NULL_HANDLE));

    if (normalPipeline != VK_NULL_HANDLE)
        vkd.cmdBindPipeline(cmdBuffer, bindPoint, normalPipeline);
    if (dgcPipeline != VK_NULL_HANDLE)
        vkd.cmdBindPipeline(cmdBuffer, bindPoint, dgcPipeline);
}

using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

BufferWithMemoryPtr makeVertexBuffer(const DeviceInterface &vkd, VkDevice device, Allocator &alloc,
                                     const tcu::IVec2 &extent, VkBufferUsageFlags usage)
{
    const auto pixelCount = extent.x() * extent.y();

    // Vertex data.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    const auto floatExtent = extent.asFloat();

    const auto normalizeCoords = [](int c, float sz) { return (static_cast<float>(c) + 0.5f) / sz * 2.0f - 1.0f; };

    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const float xCenter = normalizeCoords(x, floatExtent.x());
            const float yCenter = normalizeCoords(y, floatExtent.y());
            vertices.push_back(tcu::Vec4(xCenter, yCenter, 0.0f, 1.0f));
        }

    // Create buffer.
    const auto bufferSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto createInfo = makeBufferCreateInfo(bufferSize, usage);
    BufferWithMemoryPtr buffer(new BufferWithMemory(vkd, device, alloc, createInfo, MemoryRequirement::HostVisible));
    auto &bufferAlloc = buffer->getAllocation();
    void *dataPtr     = bufferAlloc.getHostPtr();
    deMemcpy(dataPtr, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(vkd, device, bufferAlloc);

    return buffer;
}

tcu::TestStatus conditionalDispatchRun(Context &context, TestParams params)
{
    const auto &ctx       = context.getContextCommonData();
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto stageFlags = getShaderStages(params.useTaskShader());
    const tcu::IVec3 fbExtent(kWidth, kHeight, 1);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto imageType = VK_IMAGE_TYPE_2D;
    const auto u32Size   = DE_SIZEOF32(uint32_t);
    const auto descType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto descUsage = (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType);

    // Vertex buffer.
    auto vertexBuffer = makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, fbExtent.swizzle(0, 1), descUsage);

    // Descriptor set layout.
    const auto bufferStages = VK_SHADER_STAGE_MESH_BIT_EXT;
    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(descType, bufferStages);
    const auto setLayout = layoutBuilder.build(ctx.vkd, ctx.device);

    // Descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    // Update descriptor set.
    using Location = DescriptorSetUpdateBuilder::Location;
    DescriptorSetUpdateBuilder updateBuilder;
    const auto vertexBufferDescInfo = makeDescriptorBufferInfo(vertexBuffer->get(), 0ull, VK_WHOLE_SIZE);
    updateBuilder.writeSingle(*descriptorSet, Location::binding(0u), descType, &vertexBufferDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    // Push constants
    const tcu::Vec4 pcValue(0.0f, 0.0f, 1.0f, 1.0f); // Blue.
    const auto pcSize   = DE_SIZEOF32(pcValue);
    const auto pcStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shaders.
    const auto &binaries  = context.getBinaryCollection();
    const auto taskModule = (params.useTaskShader() ? createShaderModule(ctx.vkd, ctx.device, binaries.get("task")) :
                                                      Move<VkShaderModule>());
    const auto meshModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("mesh"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    // Render pass and framebuffer.
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);
    const tcu::Vec4 clearValue(0.0f, 0.0f, 0.0f, 1.0f);

    // Pipeline, multiple options.
    Move<VkPipeline> dgcPipeline;
    Move<VkPipeline> normalPipeline;

    // These will be used for the DGC pipeline case, but not the normal case.
    const auto pipelineCreateFlags =
        static_cast<VkPipelineCreateFlags2KHR>(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT);

    const VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlagsInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR, //   VkStructureType             sType;
        nullptr,                                                   //   const void*                 pNext;
        pipelineCreateFlags,                                       //   VkPipelineCreateFlags2KHR   flags;
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    {
        Move<VkPipeline> &createdPipeline = (params.pipelineToken ? dgcPipeline : normalPipeline);

        // pNext for flags.
        const void *pNext = (params.pipelineToken ? &pipelineCreateFlagsInfo : nullptr);

        createdPipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *taskModule, *meshModule,
                                               *fragModule, *renderPass, viewports, scissors, 0u, nullptr, nullptr,
                                               nullptr, nullptr, nullptr, 0u, pNext);
    }

    // Indirect commands layout. Push constant followed by dispatch, optionally preceded by a pipeline bind.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    if (params.pipelineToken)
        cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(),
                                               VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, stageFlags);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawMeshTasksToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(cmdsLayoutBuilder.getStreamStride() / u32Size);
    if (params.pipelineToken)
        genCmdsData.push_back(0u);
    pushBackElement(genCmdsData, pcValue);
    {
        const VkDrawMeshTasksIndirectCommandEXT drawCmd{1u, kHeight, 1u};
        pushBackElement(genCmdsData, drawCmd);
    }

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Conditional rendering buffer.
    const auto conditionBufferSize = u32Size;
    const auto conditionBufferInfo =
        makeBufferCreateInfo(conditionBufferSize, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
    BufferWithMemory conditionBuffer(ctx.vkd, ctx.device, ctx.allocator, conditionBufferInfo,
                                     MemoryRequirement::HostVisible);
    const uint32_t conditionBufferValue =
        (params.conditionValue ? 1024u : 0u); // Avoid using value 1, just to make it interesting.
    auto &conditionBufferAlloc = conditionBuffer.getAllocation();
    void *conditionBufferData  = conditionBufferAlloc.getHostPtr();

    deMemcpy(conditionBufferData, &conditionBufferValue, sizeof(conditionBufferValue));
    flushAlloc(ctx.vkd, ctx.device, conditionBufferAlloc);

    // Indirect execution set.
    ExecutionSetManagerPtr executionSetManager;
    if (params.pipelineToken)
    {
        executionSetManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipeline.get(), 1u);
        // Lets rely on the initial value.
        //executionSetManager->addPipeline(0u, dgcPipeline->get());
        executionSetManager->update();
    }
    const VkIndirectExecutionSetEXT executionSetHandle =
        (executionSetManager ? executionSetManager->get() : VK_NULL_HANDLE);

    // Preprocess buffer for 256 sequences (actually only using one, but we'll pretend we may use more).
    // Note the minimum property requirements are large enough so that 256 sequences should fit.
    const auto potentialSequenceCount = 256u;
    const auto actualSequenceCount    = 1u;
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, executionSetHandle, *cmdsLayout,
                                         potentialSequenceCount, 0u, *normalPipeline);

    // (Optional) Sequence count buffer.
    DGCBufferPtr sequenceCountBuffer;
    if (params.indirectCountBuffer)
    {
        sequenceCountBuffer.reset(new DGCBuffer(ctx.vkd, ctx.device, ctx.allocator, u32Size));

        auto &allocation = sequenceCountBuffer->getAllocation();
        void *dataptr    = allocation.getHostPtr();

        deMemcpy(dataptr, &actualSequenceCount, sizeof(actualSequenceCount));
        flushAlloc(ctx.vkd, ctx.device, allocation);
    }

    // Generated commands info.
    const auto sequenceCountBufferAddress =
        (params.indirectCountBuffer ? sequenceCountBuffer->getDeviceAddress() : 0ull);
    const auto infoSequencesCount = (params.indirectCountBuffer ? potentialSequenceCount : actualSequenceCount);

    const DGCGenCmdsInfo cmdsInfo(stageFlags, executionSetHandle, *cmdsLayout, genCmdsBuffer.getDeviceAddress(),
                                  genCmdsBufferSize, preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  infoSequencesCount, sequenceCountBufferAddress, 0u, *normalPipeline);

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    {
        // Everything is recorded on the primary command buffer.
        beginCommandBuffer(ctx.vkd, cmdBuffer);
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearValue);
        beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
        bindPipelineIfPresent(ctx.vkd, cmdBuffer, bindPoint, *normalPipeline, *dgcPipeline);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
        ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
        endRenderPass(ctx.vkd, cmdBuffer);
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
        endCommandBuffer(ctx.vkd, cmdBuffer);
    }

    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify results.
    const auto tcuFormat = mapVkFormat(colorFormat);

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess     = referenceLevel.getAccess();
    const auto expectedValue = ((params.conditionValue != params.inverted) ? pcValue : clearValue);
    tcu::clear(referenceAccess, expectedValue);

    auto &bufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);
    const void *resultData = bufferAlloc.getHostPtr();
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resultData);

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected output found in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

// These tests try to check conditional rendering does not affect preprocessing.
tcu::TestStatus conditionalPreprocessRun(Context &context, ConditionalPreprocessParams params)
{
    const auto &ctx      = context.getContextCommonData();
    const auto dgcStages = (VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto seqCount  = 1u;

    const tcu::IVec3 fbExtent(kWidth, kHeight, 1);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto imageType = VK_IMAGE_TYPE_2D;
    const auto u32Size   = DE_SIZEOF32(uint32_t);
    const auto descType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto descUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType);

    // Vertex buffer.
    auto vertexBuffer = makeVertexBuffer(ctx.vkd, ctx.device, ctx.allocator, fbExtent.swizzle(0, 1), descUsage);

    // Descriptor set layout.
    const auto bufferStages = VK_SHADER_STAGE_MESH_BIT_EXT;
    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(descType, bufferStages);
    const auto setLayout = layoutBuilder.build(ctx.vkd, ctx.device);

    // Descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    // Update descriptor set.
    using Location = DescriptorSetUpdateBuilder::Location;
    DescriptorSetUpdateBuilder updateBuilder;
    const auto vertexBufferDescInfo = makeDescriptorBufferInfo(vertexBuffer->get(), 0ull, VK_WHOLE_SIZE);
    updateBuilder.writeSingle(*descriptorSet, Location::binding(0u), descType, &vertexBufferDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    // Push constants
    const tcu::Vec4 pcValue(0.0f, 0.0f, 1.0f, 1.0f); // Blue.
    const auto pcSize   = DE_SIZEOF32(pcValue);
    const auto pcStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shaders.
    const auto &binaries  = context.getBinaryCollection();
    const auto meshModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("mesh"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    // Render pass and framebuffer.
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);
    const tcu::Vec4 clearValue(0.0f, 0.0f, 0.0f, 1.0f);

    // Pipeline, multiple options.
    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const auto normalPipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, VK_NULL_HANDLE, *meshModule,
                                                     *fragModule, *renderPass, viewports, scissors);

    // Indirect commands layout. Push constant followed by dispatch.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT,
                                                       dgcStages, *pipelineLayout);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawMeshTasksToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(cmdsLayoutBuilder.getStreamStride() / u32Size);
    pushBackElement(genCmdsData, pcValue);
    {
        // Draw full-screen triangle.
        const VkDrawMeshTasksIndirectCommandEXT drawCmd{1u, 1u, kHeight};
        pushBackElement(genCmdsData, drawCmd);
    }

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Conditional rendering buffer.
    const auto conditionBufferSize = u32Size;
    const auto conditionBufferInfo =
        makeBufferCreateInfo(conditionBufferSize, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
    BufferWithMemory conditionBuffer(ctx.vkd, ctx.device, ctx.allocator, conditionBufferInfo,
                                     MemoryRequirement::HostVisible);
    const uint32_t conditionValue =
        (params.conditionValue ? 1024u : 0u); // Avoid using value 1, just to make it interesting.
    auto &conditionBufferAlloc = conditionBuffer.getAllocation();
    void *conditionBufferData  = conditionBufferAlloc.getHostPtr();

    deMemcpy(conditionBufferData, &conditionValue, sizeof(conditionValue));
    flushAlloc(ctx.vkd, ctx.device, conditionBufferAlloc);

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, seqCount, 0u,
                                         *normalPipeline);

    // Generated commands info.
    const DGCGenCmdsInfo cmdsInfo(dgcStages, VK_NULL_HANDLE, *cmdsLayout, genCmdsBuffer.getDeviceAddress(),
                                  genCmdsBufferSize, preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  seqCount, 0ull, 0u, *normalPipeline);

    // Command pool and buffer.
    const auto cmdPool = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto preprocessCmdBuffer =
        allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto executeCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    auto cmdBuffer = *preprocessCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
    ctx.vkd.cmdPreprocessGeneratedCommandsEXT(cmdBuffer, &cmdsInfo.get(), cmdBuffer);
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    preprocessToExecuteBarrierExt(ctx.vkd, cmdBuffer);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    cmdBuffer = *executeCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearValue);
    beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_TRUE, &cmdsInfo.get());
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify results.
    const auto tcuFormat = mapVkFormat(colorFormat);

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess      = referenceLevel.getAccess();
    const auto &expectedValue = ((params.inverted != params.conditionValue) ? pcValue : clearValue);
    tcu::clear(referenceAccess, expectedValue);

    auto &bufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);
    const void *resultData = bufferAlloc.getHostPtr();
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resultData);

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected output found in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCGraphicsMeshConditionalTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "conditional_rendering"));
    GroupPtr generalGroup(new tcu::TestCaseGroup(testCtx, "general"));
    GroupPtr preprocessGroup(new tcu::TestCaseGroup(testCtx, "preprocess"));

    for (const auto pipelineToken : {false, true})
        for (const auto indirectCountBuffer : {false, true})
            for (const auto conditionValue : {false, true})
                for (const auto inverted : {false, true})
                    for (const auto hasTask : {false, true})
                    {
                        const TestParams params{
                            (hasTask ? HasTask::YES : HasTask::NO),
                            pipelineToken,
                            indirectCountBuffer,
                            conditionValue,
                            inverted,
                        };

                        const std::string testName =
                            std::string() + (pipelineToken ? "pipeline_token" : "classic_bind") +
                            (indirectCountBuffer ? "_with_count_buffer" : "_without_count_buffer") +
                            (conditionValue ? "_condition_true" : "_condition_false") +
                            (inverted ? "_inverted_flag" : "") + (hasTask ? "_with_task_shader" : "");

                        addFunctionCaseWithPrograms(generalGroup.get(), testName,
                                                    checkConditionalDGCGraphicsMeshSupport, conditionalMeshPrograms,
                                                    conditionalDispatchRun, params);
                    }

    // Preprocessing tests.
    for (const auto conditionValue : {false, true})
        for (const auto inverted : {false, true})
        {
            const ConditionalPreprocessParams params{
                conditionValue,
                inverted,
            };

            const std::string testName = std::string() + (conditionValue ? "condition_true" : "condition_false") +
                                         (inverted ? "_inverted_flag" : "");

            addFunctionCaseWithPrograms(preprocessGroup.get(), testName, checkConditionalPreprocessMeshSupport,
                                        conditionalPreprocessMeshPrograms, conditionalPreprocessRun, params);
        }

    mainGroup->addChild(generalGroup.release());
    mainGroup->addChild(preprocessGroup.release());
    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
